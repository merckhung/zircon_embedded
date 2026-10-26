#include <elf.h>
#include <zircon/syscalls/log.h>

#include "src/shim/shim_common.h"
#include "src/shim/signals.h"
#include "src/shim/vfs.h"

zx_handle_t g_debuglog = ZX_HANDLE_INVALID;
zx_handle_t g_bootfs_vmo = ZX_HANDLE_INVALID;
zx_handle_t g_bootdata_vmo = ZX_HANDLE_INVALID;
zx_handle_t g_vdso_vmo = ZX_HANDLE_INVALID;
zx_handle_t g_mmio_rsrc = ZX_HANDLE_INVALID;
zx_handle_t g_ioport_rsrc = ZX_HANDLE_INVALID;
zx_handle_t g_irq_rsrc = ZX_HANDLE_INVALID;
zx_handle_t g_sys_rsrc = ZX_HANDLE_INVALID;
zx_handle_t g_vmex_rsrc = ZX_HANDLE_INVALID;
zx_handle_t g_debug_rsrc = ZX_HANDLE_INVALID;

std::recursive_mutex g_proc_mtx;
std::map<int, std::shared_ptr<LinuxProcess>> g_processes;
std::condition_variable_any g_wait4_cv;
std::mutex g_poll_mtx;
std::condition_variable g_poll_cv;
std::atomic<uint64_t> g_poll_seq{0};

namespace {

// Linux allocates PIDs and TIDs from one number space.
std::atomic<int> g_next_pid{2};

struct BootfsHeader {
  uint32_t magic;
  uint32_t dirsize;
  uint32_t reserved0;
  uint32_t reserved1;
};

struct BootfsDirent {
  uint32_t name_len;
  uint32_t data_len;
  uint32_t data_off;
  char name[];
};

zx_handle_t DupHandle(zx_handle_t h) {
  zx_handle_t out = ZX_HANDLE_INVALID;
  zx_handle_duplicate(h, ZX_RIGHT_SAME_RIGHTS, &out);
  return out;
}

zx_handle_t FindBootfsFileVmo(zx_handle_t bootfs_vmo, zx_handle_t vmex_rsrc,
                              const char* target_name, uint32_t* out_size) {
  if (out_size) *out_size = 0;
  BootfsHeader hdr{};
  if (zx_vmo_read(bootfs_vmo, &hdr, 0, sizeof(hdr)) != ZX_OK) {
    return ZX_HANDLE_INVALID;
  }
  std::vector<uint8_t> dir(hdr.dirsize);
  if (zx_vmo_read(bootfs_vmo, dir.data(), sizeof(hdr), hdr.dirsize) != ZX_OK) {
    return ZX_HANDLE_INVALID;
  }
  size_t off = 0;
  while (off + sizeof(BootfsDirent) <= dir.size()) {
    const auto* d = reinterpret_cast<const BootfsDirent*>(dir.data() + off);
    if (strcmp(d->name, target_name) == 0) {
      if (out_size) *out_size = d->data_len;
      zx_handle_t child = ZX_HANDLE_INVALID;
      uint64_t aligned = (d->data_len + 4095ULL) & ~4095ULL;
      if (zx_vmo_create_child(bootfs_vmo,
                              ZX_VMO_CHILD_SNAPSHOT_AT_LEAST_ON_WRITE,
                              d->data_off, aligned, &child) != ZX_OK) {
        return ZX_HANDLE_INVALID;
      }
      uint64_t fsize = d->data_len;
      zx_object_set_property(child, ZX_PROP_VMO_CONTENT_SIZE, &fsize,
                             sizeof(fsize));
      zx_object_set_property(child, ZX_PROP_NAME, target_name,
                             strlen(target_name));
      if (vmex_rsrc != ZX_HANDLE_INVALID) {
        zx_vmo_replace_as_executable(child, vmex_rsrc, &child);
      }
      return child;
    }
    off += (sizeof(BootfsDirent) + d->name_len + 3U) & ~3U;
  }
  return ZX_HANDLE_INVALID;
}

struct LoadedZirconElf {
  zx_handle_t vmar = ZX_HANDLE_INVALID;
  uintptr_t vmar_base = 0;
  uintptr_t entry = 0;
};

LoadedZirconElf MapZirconElf(zx_handle_t root_vmar, zx_handle_t elf_vmo) {
  LoadedZirconElf res{};
  Elf64_Ehdr ehdr{};
  if (zx_vmo_read(elf_vmo, &ehdr, 0, sizeof(ehdr)) != ZX_OK) return res;
  std::vector<Elf64_Phdr> phdrs(ehdr.e_phnum);
  if (zx_vmo_read(elf_vmo, phdrs.data(), ehdr.e_phoff,
                  ehdr.e_phnum * sizeof(Elf64_Phdr)) != ZX_OK) {
    return res;
  }

  uintptr_t low = UINTPTR_MAX, high = 0;
  for (const auto& ph : phdrs) {
    if (ph.p_type != PT_LOAD) continue;
    uintptr_t s = ph.p_vaddr & ~4095ULL;
    uintptr_t e = (ph.p_vaddr + ph.p_memsz + 4095ULL) & ~4095ULL;
    if (s < low) low = s;
    if (e > high) high = e;
  }
  zx_vmar_allocate(root_vmar,
                   ZX_VM_CAN_MAP_SPECIFIC | ZX_VM_CAN_MAP_READ |
                       ZX_VM_CAN_MAP_WRITE | ZX_VM_CAN_MAP_EXECUTE,
                   0, high - low, &res.vmar, &res.vmar_base);
  res.entry = ehdr.e_entry + (res.vmar_base - low);

  for (const auto& ph : phdrs) {
    if (ph.p_type != PT_LOAD) continue;
    uintptr_t adj = ph.p_offset & 4095ULL;
    uint64_t f_off = ph.p_offset - adj;
    size_t f_sz = ph.p_filesz + adj;
    uintptr_t v_off = (ph.p_vaddr - adj) - low;
    size_t v_sz = ph.p_memsz + adj;
    size_t f_end = (f_sz + 4095ULL) & ~4095ULL;
    size_t v_end = (v_sz + 4095ULL) & ~4095ULL;

    zx_vm_option_t perm = ZX_VM_SPECIFIC | ZX_VM_ALLOW_FAULTS;
    if (ph.p_flags & (PF_R | PF_W | PF_X)) perm |= ZX_VM_PERM_READ;
    if (ph.p_flags & PF_W) perm |= ZX_VM_PERM_WRITE;
    if (ph.p_flags & PF_X) perm |= ZX_VM_PERM_EXECUTE;

    if (f_end > 0) {
      zx_handle_t mvmo = elf_vmo;
      bool close_mvmo = false;
      uint64_t moff = f_off;
      if ((v_sz > f_sz && (f_sz & 4095ULL) != 0) || (ph.p_flags & PF_W)) {
        zx_vmo_create_child(elf_vmo, ZX_VMO_CHILD_SNAPSHOT_AT_LEAST_ON_WRITE,
                            f_off, f_end, &mvmo);
        close_mvmo = true;
        moff = 0;
        if (v_sz > f_sz && f_end > f_sz) {
          std::vector<uint8_t> zeros(f_end - f_sz, 0);
          zx_vmo_write(mvmo, zeros.data(), f_sz, zeros.size());
        }
      }
      uintptr_t m = 0;
      zx_status_t map_st = zx_vmar_map(res.vmar, perm, v_off, mvmo, moff, f_end, &m);
      if (map_st != ZX_OK) {
        char err[128];
        snprintf(err, sizeof(err), "[MapZirconElf] vmar_map failed: status=%d perm=0x%x v_off=0x%lx len=0x%lx\n",
                 map_st, perm, (unsigned long)v_off, (unsigned long)f_end);
        zx_debug_write(err, strlen(err));
      }
      if (close_mvmo) zx_handle_close(mvmo);
    }
    if (v_end > f_end) {
      zx_handle_t bss = ZX_HANDLE_INVALID;
      zx_vmo_create(v_end - f_end, 0, &bss);
      uintptr_t m = 0;
      zx_vmar_map(res.vmar, perm, v_off + f_end, bss, 0, v_end - f_end, &m);
      zx_handle_close(bss);
    }
  }
  return res;
}

void WriteBootstrapMsg(zx_handle_t ch, const std::vector<const char*>& args,
                       const std::vector<zx_handle_t>& handles,
                       const std::vector<uint32_t>& hinfo) {
  std::vector<zx_handle_t> valid_handles;
  std::vector<uint32_t> valid_hinfo;
  for (size_t i = 0; i < handles.size() && i < hinfo.size(); ++i) {
    if (handles[i] != ZX_HANDLE_INVALID) {
      valid_handles.push_back(handles[i]);
      valid_hinfo.push_back(hinfo[i]);
    }
  }
  size_t abytes = 0;
  for (const char* a : args) abytes += strlen(a) + 1;
  uint32_t hoff = (sizeof(zx_proc_args_t) + 3U) & ~3U;
  uint32_t aoff =
      hoff + static_cast<uint32_t>(valid_hinfo.size() * sizeof(uint32_t));
  uint32_t tot = (aoff + static_cast<uint32_t>(abytes) + 7U) & ~7U;

  std::vector<uint8_t> msg(tot, 0);
  auto* pa = reinterpret_cast<zx_proc_args_t*>(msg.data());
  pa->protocol = ZX_PROCARGS_PROTOCOL;
  pa->version = ZX_PROCARGS_VERSION;
  pa->handle_info_off = hoff;
  pa->args_off = args.empty() ? 0 : aoff;
  pa->args_num = static_cast<uint32_t>(args.size());
  memcpy(msg.data() + hoff, valid_hinfo.data(),
         valid_hinfo.size() * sizeof(uint32_t));
  size_t cur = aoff;
  for (const char* a : args) {
    size_t l = strlen(a) + 1;
    memcpy(msg.data() + cur, a, l);
    cur += l;
  }
  zx_channel_write(ch, 0, msg.data(), static_cast<uint32_t>(msg.size()),
                   valid_handles.data(),
                   static_cast<uint32_t>(valid_handles.size()));
}

// The kernel queues its log in the debuglog and a kernel thread drains it to
// the UART asynchronously, while the shim and the Linux console write to the
// UART directly (zx_debug_write). Shortly after boot the drainer is still
// transmitting the boot log, so the two streams would interleave character
// by character. Estimate when the drainer finishes from the volume queued so
// far and the UART rate (115200 baud, 10 bits per byte), and wait until then.
void WaitForKernelLogDrain() {
  if (g_sys_rsrc == ZX_HANDLE_INVALID) return;
  zx_handle_t rsrc = ZX_HANDLE_INVALID;
  if (zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM, ZX_RSRC_SYSTEM_DEBUGLOG_BASE, 1,
                         nullptr, 0, &rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t log = ZX_HANDLE_INVALID;
  zx_status_t st = zx_debuglog_create(rsrc, ZX_LOG_FLAG_READABLE, &log);
  zx_handle_close(rsrc);
  if (st != ZX_OK) return;
  // Each record is printed as "[sssss.mmm] ppppp:ttttt> " + data (+ "\r").
  constexpr uint64_t kPrefixBytes = 26;
  uint64_t bytes = 0;
  alignas(zx_log_record_t) uint8_t buf[ZX_LOG_RECORD_MAX];
  for (int i = 0; i < 100000; ++i) {
    if (zx_debuglog_read(log, 0, buf, sizeof(buf)) < 0) break;
    bytes += kPrefixBytes + reinterpret_cast<const zx_log_record_t*>(buf)->datalen;
  }
  zx_handle_close(log);
  constexpr uint64_t kUartBytesPerSec = 115200 / 10;
  // The drainer has been transmitting since (about) boot.
  const zx_time_t drained_at = static_cast<zx_time_t>(bytes * 1000000000ULL / kUartBytesPerSec);
  zx_nanosleep(drained_at);
}

}  // namespace

void ShimLog(const char* s) {
  size_t len = strlen(s);
  zx_debug_write(s, len);
}

void ShimLogf(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  ShimLog(buf);
}

int AllocateNextPid() { return g_next_pid.fetch_add(1); }
int AllocateNextTid() { return g_next_pid.fetch_add(1); }

void NotifyPollWaiters() {
  {
    std::lock_guard<std::mutex> lock(g_poll_mtx);
    g_poll_seq.fetch_add(1, std::memory_order_release);
  }
  g_poll_cv.notify_all();
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  g_debuglog = zx_take_startup_handle(PA_HND(PA_FD, 0x8000));
  g_bootfs_vmo = zx_take_startup_handle(PA_HND(PA_VMO_BOOTFS, 0));
  g_bootdata_vmo = zx_take_startup_handle(PA_HND(PA_VMO_BOOTDATA, 0));
  g_vdso_vmo = zx_take_startup_handle(PA_HND(PA_VMO_VDSO, 0));
  g_mmio_rsrc = zx_take_startup_handle(PA_HND(PA_MMIO_RESOURCE, 0));
  g_ioport_rsrc = zx_take_startup_handle(PA_HND(PA_IOPORT_RESOURCE, 0));
  g_irq_rsrc = zx_take_startup_handle(PA_HND(PA_IRQ_RESOURCE, 0));
  g_sys_rsrc = zx_take_startup_handle(PA_HND(PA_SYSTEM_RESOURCE, 0));

  // Probe whether this process has ZX_PROCESS_SHARED enabled
  zx_handle_t probe_p = ZX_HANDLE_INVALID, probe_v = ZX_HANDLE_INVALID;
  if (zx_process_create_shared(zx_process_self(), 0, "probe", 5, &probe_p,
                               &probe_v) == ZX_ERR_INVALID_ARGS) {
    zx_handle_t vmex = ZX_HANDLE_INVALID;
    zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                       ZX_RSRC_SYSTEM_VMEX_BASE, 1, "vmex", 4, &vmex);
    zx_handle_t prog_vmo =
        FindBootfsFileVmo(g_bootfs_vmo, vmex, "bin/zircon_linux_init", nullptr);
    zx_handle_t ld_vmo =
        FindBootfsFileVmo(g_bootfs_vmo, vmex, "lib/ld.so.1", nullptr);

    zx_handle_t cproc = ZX_HANDLE_INVALID, cvmar = ZX_HANDLE_INVALID,
                cthrd = ZX_HANDLE_INVALID;
    zx_process_create(zx_job_default(), "zircon_linux_init", 17,
                      ZX_PROCESS_SHARED, &cproc, &cvmar);
    zx_thread_create(cproc, "main", 4, 0, &cthrd);

    LoadedZirconElf ld_elf = MapZirconElf(cvmar, ld_vmo);
    LoadedZirconElf vdso_elf = MapZirconElf(cvmar, g_vdso_vmo);

    zx_handle_t svmo = ZX_HANDLE_INVALID;
    zx_vmo_create(256 * 1024, 0, &svmo);
    uintptr_t sbase = 0;
    zx_vmar_map(cvmar, ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0, svmo, 0,
                256 * 1024, &sbase);
#if defined(__x86_64__)
    uintptr_t sp = ((sbase + 256 * 1024) & ~15ULL) - 8;
#else
    uintptr_t sp = (sbase + 256 * 1024) & ~15ULL;
#endif

    zx_handle_t ch_tx = ZX_HANDLE_INVALID, ch_rx = ZX_HANDLE_INVALID;
    zx_channel_create(0, &ch_tx, &ch_rx);
    zx_handle_t ld_tx = ZX_HANDLE_INVALID, ld_rx = ZX_HANDLE_INVALID;
    zx_channel_create(0, &ld_tx, &ld_rx);

    WriteBootstrapMsg(ch_tx, {},
                      {prog_vmo, DupHandle(g_debuglog), DupHandle(cproc),
                       DupHandle(cvmar), ld_elf.vmar, DupHandle(cthrd), ld_rx},
                      {PA_HND(PA_VMO_EXECUTABLE, 0), PA_HND(PA_FD, 0x8000),
                       PA_HND(PA_PROC_SELF, 0), PA_HND(PA_VMAR_ROOT, 0),
                       PA_HND(PA_VMAR_LOADED, 0), PA_HND(PA_THREAD_SELF, 0),
                       PA_HND(PA_LDSVC_LOADER, 0)});

    WriteBootstrapMsg(
        ch_tx, {"zircon_linux_init", "--shared"},
        {DupHandle(cproc), DupHandle(cvmar), DupHandle(cthrd),
         DupHandle(zx_job_default()), DupHandle(g_debuglog), g_bootfs_vmo,
         g_bootdata_vmo, g_vdso_vmo, g_mmio_rsrc, g_ioport_rsrc, g_irq_rsrc,
         g_sys_rsrc},
        {PA_HND(PA_PROC_SELF, 0), PA_HND(PA_VMAR_ROOT, 0),
         PA_HND(PA_THREAD_SELF, 0), PA_HND(PA_JOB_DEFAULT, 0),
         PA_HND(PA_FD, 0x8000), PA_HND(PA_VMO_BOOTFS, 0),
         PA_HND(PA_VMO_BOOTDATA, 0), PA_HND(PA_VMO_VDSO, 0),
         PA_HND(PA_MMIO_RESOURCE, 0), PA_HND(PA_IOPORT_RESOURCE, 0),
         PA_HND(PA_IRQ_RESOURCE, 0), PA_HND(PA_SYSTEM_RESOURCE, 0)});

    zx_process_start(cproc, cthrd, ld_elf.entry, sp, ch_rx, vdso_elf.vmar_base);
    zx_signals_t obs = 0;
    zx_object_wait_one(cproc, ZX_PROCESS_TERMINATED, ZX_TIME_INFINITE, &obs);
    return 0;
  }
  zx_handle_close(probe_p);
  zx_handle_close(probe_v);

  WaitForKernelLogDrain();

  // Wall clock from the CMOS RTC; also installs libc's UTC reference clock
  // (std::chrono::system_clock, timed condition_variable waits).
  TimeInitFromRtc();

  ShimLogf(
      "\n================================================================\n"
      "[zircon-init] Fuchsia Zircon (%s) + 64-bit Linux Syscall Shim\n"
      "================================================================\n",
      zx_system_get_version_string().c_str);

  if (g_sys_rsrc != ZX_HANDLE_INVALID) {
    zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                       ZX_RSRC_SYSTEM_VMEX_BASE, 1, "vmex", 4, &g_vmex_rsrc);
    zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                       ZX_RSRC_SYSTEM_DEBUG_BASE, 1, "debug", 5, &g_debug_rsrc);
  }

  Vfs::Init();
  uint32_t tar_size = 0;
  zx_handle_t tar_vmo = FindBootfsFileVmo(g_bootfs_vmo, ZX_HANDLE_INVALID,
                                          "data/rootfs.tar", &tar_size);
  if (tar_vmo != ZX_HANDLE_INVALID && tar_size > 0) {
    uintptr_t tar_ptr = 0;
    size_t aligned_tar = (tar_size + 4095ULL) & ~4095ULL;
    if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ, 0, tar_vmo, 0,
                    aligned_tar, &tar_ptr) == ZX_OK) {
      size_t count = Vfs::LoadTarImage(
          reinterpret_cast<const uint8_t*>(tar_ptr), tar_size);
      ShimLogf("[zircon-init] Unpacked data/rootfs.tar (%u KB, %zu files)\n",
               tar_size / 1024, count);
      zx_vmar_unmap(zx_vmar_root_self(), tar_ptr, aligned_tar);
    }
    zx_vmo_op_range(tar_vmo, ZX_VMO_OP_DECOMMIT, 0, aligned_tar, nullptr, 0);
    zx_handle_close(tar_vmo);
  }
  if (g_bootfs_vmo != ZX_HANDLE_INVALID) {
    uint64_t bsize = 0;
    if (zx_vmo_get_size(g_bootfs_vmo, &bsize) == ZX_OK && bsize > 0) {
      zx_vmo_op_range(g_bootfs_vmo, ZX_VMO_OP_DECOMMIT, 0, bsize, nullptr, 0);
    }
    zx_handle_close(g_bootfs_vmo);
    g_bootfs_vmo = ZX_HANDLE_INVALID;
  }

  InitHardware();

  // Launch Linux PID 1 (/init)
  auto pid1 = std::make_shared<LinuxProcess>();
  pid1->pid = 1;
  pid1->ppid = 0;
  pid1->pgid = 1;
  pid1->sid = 1;
  zx_process_create_shared(zx_process_self(), 0, "linux-pid-1", 11,
                           &pid1->zx_proc, &pid1->restricted_vmar);
  zx_info_vmar_t vinfo{};
  zx_object_get_info(pid1->restricted_vmar, ZX_INFO_VMAR, &vinfo, sizeof(vinfo),
                     nullptr, nullptr);
  pid1->vmar_base = vinfo.base;
  pid1->vmar_len = vinfo.len;

  for (int fd = 0; fd <= 2; ++fd) {
    auto con = std::make_shared<FileDescription>();
    con->kind = FdKind::kSerialConsole;
    con->flags = kLinuxO_RDWR;  // Linux opens /dev/console O_RDWR for init
    con->path = "/dev/console";
    pid1->fds[fd] = OpenFd{con, false};
  }

  {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    g_processes[1] = pid1;
  }

  std::string init_path = Vfs::Lookup("/", "/init", true, 0) ? "/init" : "/bin/sh";
  std::vector<std::string> args = {init_path};
  // Minimal generic environment; the rootfs /init script sets up anything
  // session-specific.
  std::vector<std::string> env = {
      "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
      "HOME=/root",
      "TERM=vt100",
  };

  auto tid1 = std::make_shared<LinuxThread>();
  tid1->tid = 1;
  tid1->proc = pid1.get();
  {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    pid1->threads.push_back(tid1);
  }

  zx_handle_t prev = thrd_set_zx_process(pid1->zx_proc);
  std::thread t1([pid1, tid1, init_path, args, env]() {
    uintptr_t entry = 0, sp = 0;
    int64_t rc = LoadLinuxExecutable(pid1.get(), init_path, args, env, &entry, &sp);
    if (rc < 0) {
      ShimLogf("[zircon-init] Failed to load %s: %lld\n", init_path.c_str(),
               static_cast<long long>(rc));
      // Linux kills a process whose exec fails; there is no init to run.
      GroupExit(pid1.get(), kSIGKILL);
      {
        std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
        pid1->threads.clear();
      }
      OnLastThreadExit(pid1.get());
      return;
    }
    memset(&tid1->saved_sig_state, 0, sizeof(tid1->saved_sig_state));
#if defined(__x86_64__)
    tid1->saved_sig_state.ip = entry;
    tid1->saved_sig_state.rsp = sp;
    tid1->saved_sig_state.flags = 0;
#elif defined(__aarch64__)
    tid1->saved_sig_state.pc = entry;
    tid1->saved_sig_state.sp = sp;
    tid1->saved_sig_state.cpsr = 0;
#endif
    ShimLogf("[zircon-init] Starting Linux PID 1 (%s) at entry=0x%llx sp=0x%llx\n",
             init_path.c_str(), static_cast<unsigned long long>(entry),
             static_cast<unsigned long long>(sp));
    RunLinuxThreadLoop(pid1, tid1);
  });
  thrd_set_zx_process(prev);
  t1.detach();

  {
    std::unique_lock<std::recursive_mutex> lock(g_proc_mtx);
    g_wait4_cv.wait(lock, [&] { return pid1->zombie.load(); });
  }
  const int st = pid1->exit_status;
  if ((st & 0x7f) == 0) {
    ShimLogf("[zircon-init] Linux PID 1 exited with code %d\n", (st >> 8) & 0xff);
  } else {
    ShimLogf("[zircon-init] Linux PID 1 killed by signal %d\n", st & 0x7f);
  }
  return 0;
}
