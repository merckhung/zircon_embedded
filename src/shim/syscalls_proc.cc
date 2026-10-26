#include <algorithm>

#include "src/shim/arm64_syscall_map.h"
#include "src/shim/shim_common.h"
#include "src/shim/signals.h"
#include "src/shim/vfs.h"

namespace {

// clone(2) flags.
constexpr uint64_t kCsignal = 0x000000ffULL;
constexpr uint64_t kCloneVm = 0x00000100ULL;
constexpr uint64_t kCloneFs = 0x00000200ULL;
constexpr uint64_t kCloneFiles = 0x00000400ULL;
constexpr uint64_t kCloneSighand = 0x00000800ULL;
constexpr uint64_t kClonePidfd = 0x00001000ULL;
constexpr uint64_t kCloneVfork = 0x00004000ULL;
constexpr uint64_t kCloneParent = 0x00008000ULL;
constexpr uint64_t kCloneThread = 0x00010000ULL;
constexpr uint64_t kCloneNewns = 0x00020000ULL;
constexpr uint64_t kCloneSettls = 0x00080000ULL;
constexpr uint64_t kCloneParentSettid = 0x00100000ULL;
constexpr uint64_t kCloneChildCleartid = 0x00200000ULL;
constexpr uint64_t kCloneChildSettid = 0x01000000ULL;
constexpr uint64_t kCloneNamespaces = 0x7E020000ULL | kCloneNewns;  // NEWCGROUP..NEWNET
constexpr uint64_t kCloneIntoCgroup = 0x200000000ULL;

constexpr uint64_t kRlimInfinity = ~0ULL;
constexpr int kRlimitNofile = 7;
constexpr uint64_t kNrOpen = 1048576;  // fs.nr_open default

constexpr int kCapLastCap = 40;

struct LinuxCloneArgs {
  uint64_t flags;
  uint64_t pidfd;
  uint64_t child_tid;
  uint64_t parent_tid;
  uint64_t exit_signal;
  uint64_t stack;
  uint64_t stack_size;
  uint64_t tls;
  uint64_t set_tid;
  uint64_t set_tid_size;
  uint64_t cgroup;
};

std::mutex g_uts_mtx;
std::string g_hostname = "zircon-embedded";  // configured default nodename
std::string g_domainname = "(none)";          // Linux default

// --- small helpers ---------------------------------------------------------

std::shared_ptr<LinuxThread> FindThreadSp(LinuxProcess* proc, LinuxThread* thrd) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  for (auto& t : proc->threads) {
    if (t.get() == thrd) return t;
  }
  return nullptr;
}

LinuxThread* FindThreadByTidLocked(int tid, LinuxProcess** owner) {
  for (auto& kv : g_processes) {
    for (auto& t : kv.second->threads) {
      if (t->tid == tid) {
        if (owner) *owner = kv.second.get();
        return t.get();
      }
    }
  }
  return nullptr;
}

// Reads a NUL-terminated user string of at most max_len bytes (excluding the
// NUL). Returns 0, -EFAULT, or -E2BIG/-ENAMETOOLONG (caller's choice) if too
// long.
int64_t ReadUserString(LinuxProcess* proc, uintptr_t addr, size_t max_len, int64_t too_long,
                       std::string* out) {
  out->clear();
  if (addr == 0) return kEFAULT;
  while (true) {
    size_t chunk = 4096 - (addr & 4095);
    if (chunk > 256) chunk = 256;
    char buf[256];
    if (!CopyFromUser(proc, buf, addr, chunk)) return kEFAULT;
    for (size_t i = 0; i < chunk; ++i) {
      if (buf[i] == '\0') return 0;
      if (out->size() >= max_len) return too_long;
      out->push_back(buf[i]);
    }
    addr += chunk;
  }
}

// Reads a NULL-terminated array of user string pointers (argv/envp) with the
// Linux limits MAX_ARG_STRLEN (128 KiB per string) and 2 MiB in total.
int64_t ReadUserStringArray(LinuxProcess* proc, uintptr_t arr, std::vector<std::string>* out) {
  out->clear();
  if (arr == 0) return 0;
  size_t total = 0;
  for (size_t i = 0;; ++i) {
    uint64_t ptr = 0;
    if (!CopyFromUser(proc, &ptr, arr + i * 8, sizeof(ptr))) return kEFAULT;
    if (ptr == 0) return 0;
    std::string s;
    int64_t r = ReadUserString(proc, ptr, 131072 - 1, kE2BIG, &s);
    if (r != 0) return r;
    total += s.size() + 1 + sizeof(void*);
    if (total > 2 * 1024 * 1024) return kE2BIG;
    out->push_back(std::move(s));
  }
}

void InitRlimitsIfNeeded(LinuxProcess* p) {
  if (p->rlimits_init) return;
  for (auto& r : p->rlimits) r[0] = r[1] = kRlimInfinity;
  p->rlimits[3][0] = 8ULL * 1024 * 1024;           // RLIMIT_STACK soft 8 MiB
  p->rlimits[4][0] = 0;                            // RLIMIT_CORE soft 0
  p->rlimits[kRlimitNofile][0] = 1024;             // RLIMIT_NOFILE (INR_OPEN_CUR)
  p->rlimits[kRlimitNofile][1] = 4096;             //               (INR_OPEN_MAX)
  p->rlimits[8][0] = p->rlimits[8][1] = 8ULL * 1024 * 1024;  // RLIMIT_MEMLOCK
  p->rlimits[11][0] = p->rlimits[11][1] = 1024;    // RLIMIT_SIGPENDING (per queue)
  p->rlimits[12][0] = p->rlimits[12][1] = 819200;  // RLIMIT_MSGQUEUE
  p->rlimits[13][0] = p->rlimits[13][1] = 0;       // RLIMIT_NICE
  p->rlimits[14][0] = p->rlimits[14][1] = 0;       // RLIMIT_RTPRIO
  p->rlimits_init = true;
}

void WriteRusage(uint8_t* ru, int64_t user_ns, int64_t sys_ns) {
  memset(ru, 0, 144);
  const int64_t tv[4] = {user_ns / 1000000000LL, (user_ns % 1000000000LL) / 1000,
                         sys_ns / 1000000000LL, (sys_ns % 1000000000LL) / 1000};
  memcpy(ru, tv, sizeof(tv));
}

// --- scheduling profiles -----------------------------------------------------

std::mutex g_profile_mtx;
zx_handle_t g_profile_rsrc = ZX_HANDLE_INVALID;
std::map<std::pair<int, uint64_t>, zx_handle_t> g_profiles;

uint64_t OnlineCpuMask() {
  uint32_t n = zx_system_get_num_cpus();
  if (n == 0) n = 1;
  return n >= 64 ? ~0ULL : ((1ULL << n) - 1);
}

int NiceToZirconPriority(int nice) { return ZX_PRIORITY_DEFAULT - (nice * 8) / 20; }

// Applies the process nice value and the thread CPU affinity to the Zircon
// thread through a scheduler profile.
zx_status_t ApplySchedProfile(LinuxProcess* p, LinuxThread* t, uint64_t affinity) {
  const int prio = NiceToZirconPriority(p->nice);
  const uint64_t mask = affinity & OnlineCpuMask();
  zx_handle_t profile = ZX_HANDLE_INVALID;
  {
    std::lock_guard<std::mutex> l(g_profile_mtx);
    if (g_profile_rsrc == ZX_HANDLE_INVALID) {
      if (g_sys_rsrc == ZX_HANDLE_INVALID) return ZX_ERR_ACCESS_DENIED;
      zx_status_t st = zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                                          ZX_RSRC_SYSTEM_PROFILE_BASE, 1, "profile", 7,
                                          &g_profile_rsrc);
      if (st != ZX_OK) return st;
    }
    auto it = g_profiles.find({prio, mask});
    if (it != g_profiles.end()) {
      profile = it->second;
    } else {
      zx_profile_info_t info{};
      info.flags = ZX_PROFILE_INFO_FLAG_PRIORITY | ZX_PROFILE_INFO_FLAG_CPU_MASK;
      info.priority = prio;
      info.cpu_affinity_mask.mask[0] = mask;
      zx_status_t st = zx_profile_create(g_profile_rsrc, 0, &info, &profile);
      if (st != ZX_OK) return st;
      g_profiles[{prio, mask}] = profile;
    }
  }
  std::lock_guard<std::mutex> wl(t->wake_mtx);
  if (t->zx_thread == ZX_HANDLE_INVALID) return ZX_ERR_BAD_STATE;
  return zx_object_set_profile(t->zx_thread, profile, 0);
}

// Per-thread CPU affinity (sched_setaffinity); absent = all CPUs.
std::mutex g_affinity_mtx;
std::map<const LinuxThread*, uint64_t> g_affinity;

uint64_t ThreadAffinity(const LinuxThread* t) {
  std::lock_guard<std::mutex> l(g_affinity_mtx);
  auto it = g_affinity.find(t);
  return it == g_affinity.end() ? OnlineCpuMask() : it->second;
}

// --- fork / clone ------------------------------------------------------------

void NotifyVforkDone(LinuxProcess* proc) {
  std::shared_ptr<LinuxThread> parent;
  {
    std::lock_guard<std::mutex> vlock(proc->vfork_mtx);
    proc->vfork_done = true;
    parent.swap(proc->vfork_parent);
  }
  proc->vfork_cv.notify_all();
  if (parent) WakeThread(parent.get());
}

int64_t DoForkOrClone(LinuxProcess* proc, LinuxThread* thrd, uint64_t flags,
                      uintptr_t child_stack, uintptr_t parent_tidptr, uintptr_t child_tidptr,
                      uintptr_t newtls, int exit_signal) {
  // Validation as in Linux copy_process().
  if ((flags & kCloneThread) && !(flags & kCloneSighand)) return kEINVAL;
  if ((flags & kCloneSighand) && !(flags & kCloneVm)) return kEINVAL;
  if (flags & kCloneNamespaces) return kEINVAL;  // no namespace support
  if (flags & kClonePidfd) return kEINVAL;       // no pidfd support
  if (exit_signal < 0 || exit_signal > kNSIG) return kEINVAL;

  // Case 1: thread in the same process (CLONE_VM | CLONE_THREAD | ...).
  if (flags & kCloneThread) {
    const int new_tid = AllocateNextTid();
    auto nt = std::make_shared<LinuxThread>();
    nt->tid = new_tid;
    nt->proc = proc;
    nt->comm = thrd->comm;
    nt->sigmask.store(thrd->sigmask.load());
    nt->saved_sig_state = *thrd->state;
#if defined(__x86_64__)
    nt->saved_sig_state.rax = 0;
    if (child_stack != 0) nt->saved_sig_state.rsp = child_stack;
    if (flags & kCloneSettls) nt->saved_sig_state.fs_base = newtls;
#elif defined(__aarch64__)
    nt->saved_sig_state.x[0] = 0;
    if (child_stack != 0) nt->saved_sig_state.sp = child_stack;
    if (flags & kCloneSettls) nt->saved_sig_state.tpidr_el0 = newtls;
#endif
    if (flags & kCloneChildCleartid) nt->clear_child_tid = child_tidptr;
    if (flags & kCloneChildSettid) nt->set_child_tid = child_tidptr;
    nt->xstate = XstateAlloc();
    if (nt->xstate == nullptr) return kENOMEM;
    memcpy(nt->xstate, thrd->xstate, XstateSize());  // FPU state is inherited
    // CLONE_VM children start without an alternate signal stack.
    if (flags & kCloneParentSettid) {
      const int32_t tid32 = new_tid;
      if (!CopyToUser(proc, parent_tidptr, &tid32, sizeof(tid32))) {
        XstateFree(nt->xstate);
        nt->xstate = nullptr;
        return kEFAULT;
      }
    }
    std::shared_ptr<LinuxProcess> proc_sp;
    {
      std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
      proc_sp = g_processes[proc->pid];
    }
    {
      std::lock_guard<std::mutex> l(g_affinity_mtx);
      auto it = g_affinity.find(thrd);
      if (it != g_affinity.end()) g_affinity[nt.get()] = it->second;
    }
    SpawnLinuxThreadInProcess(proc_sp, nt);
    return new_tid;
  }

  // Case 2: a new process. Sharing the fd table, fs info or signal handlers
  // with a separate process is not supported, and neither is sharing the
  // address space except for vfork: CLONE_VM|CLONE_VFORK children get a copy
  // of the address space, and the parent stays suspended until the child
  // execs or exits. Memory the child writes before exec is NOT visible to
  // the parent (a deviation from Linux).
  if (flags & (kCloneFiles | kCloneFs | kCloneSighand)) return kEINVAL;
  if ((flags & kCloneVm) && !(flags & kCloneVfork)) return kEINVAL;
  const bool is_vfork = (flags & kCloneVfork) != 0;

  const int child_pid = AllocateNextPid();
  auto child = std::make_shared<LinuxProcess>();
  child->pid = child_pid;
  child->ppid = (flags & kCloneParent) ? proc->ppid : proc->pid;
  child->pgid = proc->pgid;
  child->sid = proc->sid;
  child->umask_val = proc->umask_val;
  child->exe_path = proc->exe_path;
  child->comm = proc->comm;
  child->cmdline = proc->cmdline;
  child->cwd = proc->cwd;
  child->brk_start = proc->brk_start;
  child->brk_current = proc->brk_current;
  child->brk_mapped_end = proc->brk_mapped_end;
  child->mmap_cursor = proc->mmap_cursor;
  child->exit_signal = exit_signal;
  child->nice = proc->nice;
  child->ruid = proc->ruid;
  child->euid = proc->euid;
  child->suid = proc->suid;
  child->fsuid = proc->fsuid;
  child->rgid = proc->rgid;
  child->egid = proc->egid;
  child->sgid = proc->sgid;
  child->fsgid = proc->fsgid;
  child->groups = proc->groups;
  InitRlimitsIfNeeded(proc);
  memcpy(child->rlimits, proc->rlimits, sizeof(child->rlimits));
  child->rlimits_init = true;
  child->dumpable = proc->dumpable;
  child->no_new_privs = proc->no_new_privs;
  child->keepcaps = proc->keepcaps;
  {
    std::lock_guard<std::mutex> sl(proc->sig_mtx);
    memcpy(child->sigactions, proc->sigactions, sizeof(child->sigactions));
  }

  char proc_name[32];
  snprintf(proc_name, sizeof(proc_name), "linux-pid-%d", child_pid);
  zx_status_t st = zx_process_create_shared(zx_process_self(), 0, proc_name, strlen(proc_name),
                                            &child->zx_proc, &child->restricted_vmar);
  if (st != ZX_OK) {
    ShimLogf("[fork] zx_process_create_shared failed: %d\n", st);
    return kENOMEM;
  }
  zx_info_vmar_t vmar_info{};
  zx_object_get_info(child->restricted_vmar, ZX_INFO_VMAR, &vmar_info, sizeof(vmar_info), nullptr,
                     nullptr);
  child->vmar_base = vmar_info.base;
  child->vmar_len = vmar_info.len;

  if (!CloneProcessAddressSpace(proc, child.get())) {
    zx_handle_close(child->restricted_vmar);
    zx_handle_close(child->zx_proc);
    return kENOMEM;
  }

  // Copy the fd table: the child holds new references to the same open file
  // descriptions (shared_ptr), so offsets and status flags are shared.
  {
    std::lock_guard<std::recursive_mutex> flock(proc->fd_mtx);
    child->fds = proc->fds;
  }

  auto ct = std::make_shared<LinuxThread>();
  ct->tid = child_pid;
  ct->proc = child.get();
  ct->comm = thrd->comm;
  ct->sigmask.store(thrd->sigmask.load());
  ct->altstack_sp = thrd->altstack_sp;
  ct->altstack_size = thrd->altstack_size;
  ct->altstack_flags = thrd->altstack_flags;
  ct->saved_sig_state = *thrd->state;
#if defined(__x86_64__)
  ct->saved_sig_state.rax = 0;
  if (child_stack != 0) ct->saved_sig_state.rsp = child_stack;
  if (flags & kCloneSettls) ct->saved_sig_state.fs_base = newtls;
#elif defined(__aarch64__)
  ct->saved_sig_state.x[0] = 0;
  if (child_stack != 0) ct->saved_sig_state.sp = child_stack;
  if (flags & kCloneSettls) ct->saved_sig_state.tpidr_el0 = newtls;
#endif
  if (flags & kCloneChildCleartid) ct->clear_child_tid = child_tidptr;
  // CLONE_CHILD_SETTID targets the child's address space; the child thread
  // writes it before it first runs (RunLinuxThreadLoop).
  if (flags & kCloneChildSettid) ct->set_child_tid = child_tidptr;
  ct->xstate = XstateAlloc();
  if (ct->xstate != nullptr) memcpy(ct->xstate, thrd->xstate, XstateSize());
  if (flags & kCloneParentSettid) {
    const int32_t pid32 = child_pid;
    CopyToUser(proc, parent_tidptr, &pid32, sizeof(pid32));
  }

  if (is_vfork) child->vfork_parent = FindThreadSp(proc, thrd);
  {
    std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
    g_processes[child_pid] = child;
  }
  {
    std::lock_guard<std::mutex> l(g_affinity_mtx);
    auto it = g_affinity.find(thrd);
    if (it != g_affinity.end()) g_affinity[ct.get()] = it->second;
  }
  SpawnLinuxThreadInProcess(child, ct);

  if (is_vfork) {
    // Linux waits killably until the child execs or exits.
    for (;;) {
      ThreadClearWake(thrd);
      {
        std::lock_guard<std::mutex> vlock(child->vfork_mtx);
        if (child->vfork_done) break;
      }
      if (proc->exited.load() || thrd->kill_requested.load()) break;
      ThreadWaitWake(thrd, ZX_TIME_INFINITE);
    }
  }
  return child_pid;
}

// --- execve ---------------------------------------------------------------

// Kills every other thread of the process (Linux de_thread()).
void KillOtherThreads(LinuxProcess* proc, LinuxThread* self) {
  bool set_proc_kill = false;
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    if (proc->threads.size() <= 1) return;
    for (auto& t : proc->threads) {
      if (t.get() == self) continue;
      t->kill_requested.store(true);
      t->pending_signals.fetch_or(SigBit(kSIGKILL));
      WakeThread(t.get());
    }
    // Blocking loops elsewhere in the shim test the process pending set;
    // a transient SIGKILL bit makes them return so the threads can exit.
    if ((proc->pending_signals.load() & SigBit(kSIGKILL)) == 0) {
      proc->pending_signals.fetch_or(SigBit(kSIGKILL));
      set_proc_kill = true;
    }
  }
  NotifyPollWaiters();
  g_wait4_cv.notify_all();
  for (;;) {
    ThreadClearWake(self);
    {
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      if (proc->threads.size() <= 1 || proc->exited.load()) break;
    }
    ThreadWaitWake(self, ZX_TIME_INFINITE);
  }
  if (set_proc_kill && !proc->exited.load()) {
    proc->pending_signals.fetch_and(~SigBit(kSIGKILL));
  }
}

int64_t SysExecve(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc, uint64_t a0, uint64_t a1,
                  uint64_t a2, uint64_t a3, uint64_t a4) {
  const uintptr_t u_path = (sc == 59) ? a0 : a1;
  const uintptr_t u_argv = (sc == 59) ? a1 : a2;
  const uintptr_t u_envp = (sc == 59) ? a2 : a3;
  std::string path;
  int64_t r = ReadUserString(proc, u_path, 4095, -36 /* ENAMETOOLONG */, &path);
  if (r != 0) return r;
  if (sc == 322) {  // execveat(dirfd, path, argv, envp, flags)
    const int dirfd = static_cast<int>(a0);
    const int flags = static_cast<int>(a4);
    if (flags & ~(kLinuxAT_EMPTY_PATH | kLinuxAT_SYMLINK_NOFOLLOW)) return kEINVAL;
    if (flags & kLinuxAT_SYMLINK_NOFOLLOW) {
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* n = Vfs::Lookup(proc->cwd, path, false, 0);
      if (n != nullptr && n->type == VfsNodeType::kSymlink) return kELOOP;
    }
    if (path.empty() || path[0] != '/') {
      if (path.empty() && !(flags & kLinuxAT_EMPTY_PATH)) return kENOENT;
      if (dirfd != kLinuxAT_FDCWD) {
        std::shared_ptr<FileDescription> desc;
        {
          std::lock_guard<std::recursive_mutex> fl(proc->fd_mtx);
          auto it = proc->fds.find(dirfd);
          if (it == proc->fds.end()) return kEBADF;
          desc = it->second.desc;
        }
        if (desc->path.empty()) return kENOENT;
        path = path.empty() ? desc->path : desc->path + "/" + path;
      }
    }
  } else if (path.empty()) {
    return kENOENT;
  }
  std::vector<std::string> args, env;
  r = ReadUserStringArray(proc, u_argv, &args);
  if (r != 0) return r;
  r = ReadUserStringArray(proc, u_envp, &env);
  if (r != 0) return r;

  KillOtherThreads(proc, thrd);
  if (proc->exited.load()) return 0;

  uintptr_t entry = 0, sp = 0;
  const int64_t rc = LoadLinuxExecutable(proc, path, args, env, &entry, &sp);
  if (rc < 0) {
    // LoadLinuxExecutable validates everything before it tears down the old
    // image; a failure after that point leaves nothing to return to. Detect
    // that by checking whether the caller's code is still mapped, and kill
    // the process as Linux does after the point of no return.
    uint8_t probe = 0;
#if defined(__x86_64__)
    const uint64_t rip_pc = thrd->state->ip;
#elif defined(__aarch64__)
    const uint64_t rip_pc = thrd->state->pc;
#endif
    if (!CopyFromUser(proc, &probe, rip_pc, 1)) GroupExit(proc, kSIGKILL);
    return rc;
  }

  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    thrd->tid = proc->pid;  // the exec'ing thread becomes the leader
  }
  // Close O_CLOEXEC descriptors.
  std::vector<OpenFd> closed;
  {
    std::lock_guard<std::recursive_mutex> flock(proc->fd_mtx);
    for (auto it = proc->fds.begin(); it != proc->fds.end();) {
      if (it->second.cloexec) {
        closed.push_back(std::move(it->second));
        it = proc->fds.erase(it);
      } else {
        ++it;
      }
    }
  }
  closed.clear();
  // Caught signals revert to SIG_DFL; ignored ones stay ignored.
  {
    std::lock_guard<std::mutex> sl(proc->sig_mtx);
    for (int s = 1; s <= kNSIG; ++s) {
      const bool ign = proc->sigactions[s].sa_handler == kSIG_IGN;
      proc->sigactions[s] = LinuxSigaction{};
      if (ign) proc->sigactions[s].sa_handler = kSIG_IGN;
    }
  }
  TimeProcessExec(proc);
  proc->membarrier_registered = 0;

  memset(thrd->state, 0, sizeof(zx_restricted_state_t));
#if defined(__x86_64__)
  thrd->state->ip = entry;
  thrd->state->rsp = sp;
  thrd->state->flags = 0;
#elif defined(__aarch64__)
  thrd->state->pc = entry;
  thrd->state->sp = sp;
  thrd->state->cpsr = 0;
#endif
  XstateReset(thrd->xstate);
  thrd->clear_child_tid = 0;
  thrd->robust_list_head = 0;
  thrd->robust_list_len = 0;
  thrd->altstack_sp = 0;
  thrd->altstack_size = 0;
  thrd->altstack_flags = 0;
  thrd->comm.clear();
  thrd->restart_kind = kRestartNone;

  NotifyVforkDone(proc);
  return 0;
}

// --- wait ------------------------------------------------------------------

struct WaitResult {
  int pid = 0;
  int status = 0;  // wait(2) status word
  int code = 0;    // CLD_*
  int si_status = 0;
  int64_t cpu_ns = 0;
};

void ReapLocked(LinuxProcess* parent, std::map<int, std::shared_ptr<LinuxProcess>>::iterator it) {
  LinuxProcess* c = it->second.get();
  c->reaped = true;
  parent->children_cpu_ns += c->cpu_time_ns + c->children_cpu_ns;
  if (c->restricted_vmar != ZX_HANDLE_INVALID) {
    zx_handle_close(c->restricted_vmar);
    c->restricted_vmar = ZX_HANDLE_INVALID;
  }
  if (c->zx_proc != ZX_HANDLE_INVALID) {
    zx_handle_close(c->zx_proc);
    c->zx_proc = ZX_HANDLE_INVALID;
  }
  g_processes.erase(it);
}

int64_t SysWait(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc, uint64_t a0, uint64_t a1,
                uint64_t a2, uint64_t a3, uint64_t a4) {
  constexpr int kWNOHANG = 1, kWUNTRACED = 2, kWEXITED = 4, kWCONTINUED = 8;
  constexpr int kWNOWAIT = 0x01000000, kWNOTHREAD = 0x20000000;
  constexpr int kWALL = 0x40000000;
  constexpr int kWCLONE = static_cast<int>(0x80000000U);
  int options;
  int sel_pid = -1;   // >0 pid, -1 any, 0/neg: process group
  int sel_pgid = 0;
  uintptr_t out_status = 0, out_info = 0, out_rusage = 0;
  if (sc == 61) {  // wait4(pid, wstatus, options, rusage)
    const int pid = static_cast<int>(a0);
    options = static_cast<int>(a2);
    if (options & ~(kWNOHANG | kWUNTRACED | kWCONTINUED | kWNOTHREAD | kWALL | kWCLONE)) {
      return kEINVAL;
    }
    if (pid == INT32_MIN) return kESRCH;
    options |= kWEXITED;
    if (pid > 0) sel_pid = pid;
    else if (pid == 0) { sel_pid = 0; sel_pgid = proc->pgid; }
    else if (pid < -1) { sel_pid = 0; sel_pgid = -pid; }
    out_status = a1;
    out_rusage = a3;
  } else {  // waitid(idtype, id, infop, options, rusage)
    const int idtype = static_cast<int>(a0);
    const int id = static_cast<int>(a1);
    options = static_cast<int>(a3);
    if (options & ~(kWNOHANG | kWUNTRACED | kWEXITED | kWCONTINUED | kWNOWAIT | kWNOTHREAD |
                    kWALL | kWCLONE)) {
      return kEINVAL;
    }
    if (!(options & (kWEXITED | kWUNTRACED | kWCONTINUED))) return kEINVAL;
    if (idtype == 0) {         // P_ALL
      sel_pid = -1;
    } else if (idtype == 1) {  // P_PID
      if (id <= 0) return kEINVAL;
      sel_pid = id;
    } else if (idtype == 2) {  // P_PGID
      if (id < 0) return kEINVAL;
      sel_pid = 0;
      sel_pgid = id == 0 ? proc->pgid : id;
    } else {
      return kEINVAL;  // P_PIDFD: pidfds are not supported
    }
    out_info = a2;
    out_rusage = a4;
  }
  const bool nowait = (options & kWNOWAIT) != 0;

  for (;;) {
    ThreadClearWake(thrd);
    WaitResult res;
    bool found = false;
    bool has_child = false;
    {
      std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
      for (auto it = g_processes.begin(); it != g_processes.end(); ++it) {
        LinuxProcess* c = it->second.get();
        if (c->ppid != proc->pid || c->reaped) continue;
        if (sel_pid > 0 && c->pid != sel_pid) continue;
        if (sel_pid == 0 && c->pgid != sel_pgid) continue;
        if (!(options & kWALL)) {
          const bool is_clone = c->exit_signal != kSIGCHLD;
          if (is_clone != ((options & kWCLONE) != 0)) continue;
        }
        has_child = true;
        if ((options & kWEXITED) && c->zombie.load()) {
          res.pid = c->pid;
          res.status = c->exit_status;
          if ((res.status & 0x7f) == 0) {
            res.code = kCLD_EXITED;
            res.si_status = (res.status >> 8) & 0xff;
          } else {
            res.code = (res.status & 0x80) ? kCLD_DUMPED : kCLD_KILLED;
            res.si_status = res.status & 0x7f;
          }
          res.cpu_ns = c->cpu_time_ns + c->children_cpu_ns;
          if (!nowait) ReapLocked(proc, it);
          found = true;
          break;
        }
        std::lock_guard<std::mutex> sl(c->sig_mtx);
        if ((options & kWUNTRACED) && c->wait_report == kWaitReportStopped) {
          res.pid = c->pid;
          res.status = (c->stop_sig << 8) | 0x7f;
          res.code = kCLD_STOPPED;
          res.si_status = c->stop_sig;
          if (!nowait) c->wait_report = kWaitReportNone;
          found = true;
        } else if ((options & kWCONTINUED) && c->wait_report == kWaitReportContinued) {
          res.pid = c->pid;
          res.status = 0xffff;
          res.code = kCLD_CONTINUED;
          res.si_status = kSIGCONT;
          if (!nowait) c->wait_report = kWaitReportNone;
          found = true;
        }
        if (found) {
          res.cpu_ns = ProcessCpuTimeNs(c);
          break;
        }
      }
    }
    if (found) {
      if (out_status != 0 && !CopyToUser(proc, out_status, &res.status, sizeof(int32_t))) {
        return kEFAULT;
      }
      if (out_info != 0) {
        LinuxSiginfo si = MakeSiginfoChld(res.code, res.pid, res.si_status, res.cpu_ns);
        if (!CopyToUser(proc, out_info, &si, sizeof(si))) return kEFAULT;
      }
      if (out_rusage != 0) {
        uint8_t ru[144];
        // Zircon reports total CPU time only; it is reported as user time.
        WriteRusage(ru, res.cpu_ns, 0);
        if (!CopyToUser(proc, out_rusage, ru, sizeof(ru))) return kEFAULT;
      }
      return sc == 61 ? res.pid : 0;
    }
    if (!has_child) return kECHILD;
    if (options & kWNOHANG) {
      if (out_info != 0) {
        LinuxSiginfo zero;
        if (!CopyToUser(proc, out_info, &zero, sizeof(zero))) return kEFAULT;
      }
      return 0;
    }
    if (ShimSignalPending(proc, thrd)) return kERESTARTSYS;
    ThreadWaitWake(thrd, ZX_TIME_INFINITE);
  }
}

// --- credentials -------------------------------------------------------------
// Credentials are tracked per process and reported consistently. The shim
// performs no permission checks based on them (every process can access every
// file), which callers dropping privileges should be aware of.

bool Privileged(const LinuxProcess* p) { return p->euid == 0; }

int64_t SysSetresid(uint32_t* r, uint32_t* e, uint32_t* s, uint32_t* fs, uint32_t nr,
                    uint32_t ne, uint32_t ns, bool priv) {
  const uint32_t keep = 0xffffffffU;
  auto allowed = [&](uint32_t v) { return v == keep || priv || v == *r || v == *e || v == *s; };
  if (!allowed(nr) || !allowed(ne) || !allowed(ns)) return kEPERM;
  if (nr != keep) *r = nr;
  if (ne != keep) *e = ne;
  if (ns != keep) *s = ns;
  *fs = *e;
  return 0;
}

int64_t SysSetreid(uint32_t* r, uint32_t* e, uint32_t* s, uint32_t* fs, uint32_t nr,
                   uint32_t ne, bool priv) {
  const uint32_t keep = 0xffffffffU;
  const uint32_t old_r = *r;
  if (nr != keep && !priv && nr != *r && nr != *e) return kEPERM;
  if (ne != keep && !priv && ne != *r && ne != *e && ne != *s) return kEPERM;
  if (nr != keep) *r = nr;
  if (ne != keep) *e = ne;
  if (nr != keep || (ne != keep && ne != old_r)) *s = *e;
  *fs = *e;
  return 0;
}

int64_t SysSetid(uint32_t* r, uint32_t* e, uint32_t* s, uint32_t* fs, uint32_t id, bool priv) {
  if (id == 0xffffffffU) return kEINVAL;
  if (priv) {
    *r = *e = *s = *fs = id;
    return 0;
  }
  if (id != *r && id != *s) return kEPERM;
  *e = *fs = id;
  return 0;
}

int64_t HandleCredSyscall(LinuxProcess* p, uint64_t sc, uint64_t a0, uint64_t a1, uint64_t a2,
                          bool* handled) {
  *handled = true;
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  const bool priv = Privileged(p);
  const auto u = [](uint64_t v) { return static_cast<uint32_t>(v); };
  switch (sc) {
    case 102: return p->ruid;  // getuid
    case 104: return p->rgid;  // getgid
    case 107: return p->euid;  // geteuid
    case 108: return p->egid;  // getegid
    case 105: return SysSetid(&p->ruid, &p->euid, &p->suid, &p->fsuid, u(a0), priv);
    case 106: return SysSetid(&p->rgid, &p->egid, &p->sgid, &p->fsgid, u(a0), priv);
    case 113: return SysSetreid(&p->ruid, &p->euid, &p->suid, &p->fsuid, u(a0), u(a1), priv);
    case 114: return SysSetreid(&p->rgid, &p->egid, &p->sgid, &p->fsgid, u(a0), u(a1), priv);
    case 117:
      return SysSetresid(&p->ruid, &p->euid, &p->suid, &p->fsuid, u(a0), u(a1), u(a2), priv);
    case 119:
      return SysSetresid(&p->rgid, &p->egid, &p->sgid, &p->fsgid, u(a0), u(a1), u(a2), priv);
    case 118:    // getresuid
    case 120: {  // getresgid
      const uint32_t v[3] = {sc == 118 ? p->ruid : p->rgid, sc == 118 ? p->euid : p->egid,
                             sc == 118 ? p->suid : p->sgid};
      for (int i = 0; i < 3; ++i) {
        const uint64_t ptr = i == 0 ? a0 : (i == 1 ? a1 : a2);
        if (!CopyToUser(p, ptr, &v[i], sizeof(uint32_t))) return kEFAULT;
      }
      return 0;
    }
    case 122: {  // setfsuid: always returns the previous value
      const uint32_t old = p->fsuid;
      const uint32_t id = u(a0);
      if (priv || id == p->ruid || id == p->euid || id == p->suid || id == p->fsuid) p->fsuid = id;
      return old;
    }
    case 123: {  // setfsgid
      const uint32_t old = p->fsgid;
      const uint32_t id = u(a0);
      if (priv || id == p->rgid || id == p->egid || id == p->sgid || id == p->fsgid) p->fsgid = id;
      return old;
    }
    case 115: {  // getgroups(size, list)
      const int size = static_cast<int>(a0);
      if (size < 0) return kEINVAL;
      const int n = static_cast<int>(p->groups.size());
      if (size == 0) return n;
      if (size < n) return kEINVAL;
      if (n > 0 && !CopyToUser(p, a1, p->groups.data(), n * sizeof(uint32_t))) return kEFAULT;
      return n;
    }
    case 116: {  // setgroups(size, list)
      if (!priv) return kEPERM;
      const int64_t size = static_cast<int64_t>(a0);
      if (size < 0 || size > 65536) return kEINVAL;
      std::vector<uint32_t> g(static_cast<size_t>(size));
      if (size > 0 && !CopyFromUser(p, g.data(), a1, g.size() * sizeof(uint32_t))) return kEFAULT;
      p->groups = std::move(g);
      return 0;
    }
    default:
      *handled = false;
      return kENOSYS;
  }
}

// --- misc syscalls -------------------------------------------------------------

std::shared_ptr<LinuxProcess> LookupPidOrSelf(LinuxProcess* proc, int pid) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  auto it = g_processes.find(pid == 0 ? proc->pid : pid);
  if (it == g_processes.end() || it->second->reaped) return nullptr;
  return it->second;
}

int64_t SysPrlimit(LinuxProcess* proc, int pid, int resource, uintptr_t newp, uintptr_t oldp) {
  if (resource < 0 || resource >= 16) return kEINVAL;
  auto target = LookupPidOrSelf(proc, pid);
  if (!target) return kESRCH;
  uint64_t nl[2] = {0, 0};
  if (newp != 0) {
    if (!CopyFromUser(proc, nl, newp, sizeof(nl))) return kEFAULT;
    if (nl[0] > nl[1]) return kEINVAL;
    if (resource == kRlimitNofile && nl[1] > kNrOpen) return kEPERM;
  }
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  InitRlimitsIfNeeded(target.get());
  const uint64_t old[2] = {target->rlimits[resource][0], target->rlimits[resource][1]};
  if (newp != 0) {
    if (nl[1] > old[1] && !Privileged(proc)) return kEPERM;
    // Limits are recorded and reported; the shim does not enforce them.
    target->rlimits[resource][0] = nl[0];
    target->rlimits[resource][1] = nl[1];
  }
  if (oldp != 0 && !CopyToUser(proc, oldp, old, sizeof(old))) return kEFAULT;
  return 0;
}

int64_t SysGetrusage(LinuxProcess* proc, LinuxThread* thrd, int who, uintptr_t out) {
  int64_t ns;
  if (who == 0) {          // RUSAGE_SELF
    ns = ProcessCpuTimeNs(proc);
  } else if (who == -1) {  // RUSAGE_CHILDREN
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    ns = proc->children_cpu_ns;
  } else if (who == 1) {   // RUSAGE_THREAD
    ns = ThreadCpuTimeNs(thrd);
  } else {
    return kEINVAL;
  }
  if (ns < 0) ns = 0;
  uint8_t ru[144];
  // ZX_INFO_TASK_RUNTIME reports total CPU time without a user/system split;
  // it is reported as ru_utime. Memory and fault counters are not tracked and
  // are reported as 0.
  WriteRusage(ru, ns, 0);
  if (!CopyToUser(proc, out, ru, sizeof(ru))) return kEFAULT;
  return 0;
}

int64_t SysSysinfo(LinuxProcess* proc, uintptr_t out) {
  static zx_handle_t info_rsrc = [] {
    zx_handle_t h = ZX_HANDLE_INVALID;
    if (g_sys_rsrc != ZX_HANDLE_INVALID) {
      zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM, ZX_RSRC_SYSTEM_INFO_BASE, 1, nullptr, 0,
                         &h);
    }
    return h;
  }();
  uint8_t info[112];
  memset(info, 0, sizeof(info));
  const int64_t uptime = zx_clock_get_boot() / 1000000000LL;
  memcpy(info + 0, &uptime, 8);
  // loads[3] (offset 8): load averages are not computed by the shim; 0.
  zx_info_kmem_stats_t kmem{};
  uint64_t total = zx_system_get_physmem();
  uint64_t free_bytes = 0;
  if (info_rsrc != ZX_HANDLE_INVALID &&
      zx_object_get_info(info_rsrc, ZX_INFO_KMEM_STATS, &kmem, sizeof(kmem), nullptr, nullptr) ==
          ZX_OK) {
    total = kmem.total_bytes;
    free_bytes = kmem.free_bytes;
  }
  memcpy(info + 32, &total, 8);       // totalram
  memcpy(info + 40, &free_bytes, 8);  // freeram
  // sharedram, bufferram, totalswap, freeswap: no page cache or swap -> 0.
  uint16_t procs = 0;
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    size_t n = 0;
    for (auto& kv : g_processes) n += kv.second->threads.size();
    procs = static_cast<uint16_t>(std::min<size_t>(n, 0xffff));
  }
  memcpy(info + 80, &procs, 2);
  const uint32_t mem_unit = 1;
  memcpy(info + 104, &mem_unit, 4);
  if (!CopyToUser(proc, out, info, sizeof(info))) return kEFAULT;
  return 0;
}

int64_t SysPrctl(LinuxProcess* proc, LinuxThread* thrd, uint64_t a0, uint64_t a1, uint64_t a2,
                 uint64_t a3, uint64_t a4) {
  const int option = static_cast<int>(a0);
  switch (option) {
    case 1:  // PR_SET_PDEATHSIG
      if (a1 > static_cast<uint64_t>(kNSIG)) return kEINVAL;
      proc->pdeath_signal = static_cast<int>(a1);
      return 0;
    case 2: {  // PR_GET_PDEATHSIG
      const int32_t v = proc->pdeath_signal;
      return CopyToUser(proc, a1, &v, sizeof(v)) ? 0 : kEFAULT;
    }
    case 3:  // PR_GET_DUMPABLE
      return proc->dumpable ? 1 : 0;
    case 4:  // PR_SET_DUMPABLE
      if (a1 > 1) return kEINVAL;
      proc->dumpable = a1 == 1;
      return 0;
    case 7:  // PR_GET_KEEPCAPS
      return proc->keepcaps ? 1 : 0;
    case 8:  // PR_SET_KEEPCAPS
      if (a1 > 1) return kEINVAL;
      proc->keepcaps = a1 == 1;
      return 0;
    case 15: {  // PR_SET_NAME: at most 15 bytes plus NUL (TASK_COMM_LEN)
      std::string name;
      if (a1 == 0) return kEFAULT;
      for (size_t i = 0; i < 15; ++i) {
        char c;
        if (!CopyFromUser(proc, &c, a1 + i, 1)) return kEFAULT;
        if (c == '\0') break;
        name.push_back(c);
      }
      thrd->comm = name;
      if (thrd->tid == proc->pid) proc->comm = name;
      return 0;
    }
    case 16: {  // PR_GET_NAME
      char buf[16] = {};
      const std::string& n = thrd->comm.empty() ? proc->comm : thrd->comm;
      memcpy(buf, n.data(), std::min<size_t>(n.size(), 15));
      return CopyToUser(proc, a1, buf, sizeof(buf)) ? 0 : kEFAULT;
    }
    case 23:  // PR_CAPBSET_READ
      return (a1 <= static_cast<uint64_t>(kCapLastCap)) ? 1 : kEINVAL;
    case 36:  // PR_SET_CHILD_SUBREAPER
      proc->child_subreaper = a1 != 0;
      return 0;
    case 37: {  // PR_GET_CHILD_SUBREAPER
      const int32_t v = proc->child_subreaper ? 1 : 0;
      return CopyToUser(proc, a1, &v, sizeof(v)) ? 0 : kEFAULT;
    }
    case 38:  // PR_SET_NO_NEW_PRIVS (exec never grants privileges here)
      if (a1 != 1 || a2 != 0 || a3 != 0 || a4 != 0) return kEINVAL;
      proc->no_new_privs = true;
      return 0;
    case 39:  // PR_GET_NO_NEW_PRIVS
      if (a1 != 0 || a2 != 0 || a3 != 0 || a4 != 0) return kEINVAL;
      return proc->no_new_privs ? 1 : 0;
    default:
      return kEINVAL;
  }
}

int64_t SysCapget(LinuxProcess* proc, uintptr_t hdrp, uintptr_t datap, bool set) {
  constexpr uint32_t kV1 = 0x19980330, kV2 = 0x20071026, kV3 = 0x20080522;
  uint32_t hdr[2];
  if (!CopyFromUser(proc, hdr, hdrp, sizeof(hdr))) return kEFAULT;
  if (hdr[0] != kV1 && hdr[0] != kV2 && hdr[0] != kV3) {
    hdr[0] = kV3;
    if (!CopyToUser(proc, hdrp, hdr, sizeof(hdr))) return kEFAULT;
    return datap == 0 ? 0 : kEINVAL;
  }
  const int pid = static_cast<int32_t>(hdr[1]);
  if (pid < 0) return kEINVAL;
  auto target = LookupPidOrSelf(proc, pid);
  if (!target) return kESRCH;
  const int words = hdr[0] == kV1 ? 1 : 2;
  // Capabilities follow the effective UID (no file capabilities): euid 0
  // has the full set, other UIDs have none.
  const uint64_t full = (1ULL << (kCapLastCap + 1)) - 1;
  const uint64_t caps = target->euid == 0 ? full : 0;
  if (!set) {
    if (datap == 0) return 0;
    uint32_t data[6] = {};
    for (int w = 0; w < words; ++w) {
      data[w * 3 + 0] = static_cast<uint32_t>(caps >> (32 * w));  // effective
      data[w * 3 + 1] = static_cast<uint32_t>(caps >> (32 * w));  // permitted
      data[w * 3 + 2] = 0;                                        // inheritable
    }
    return CopyToUser(proc, datap, data, words * 3 * sizeof(uint32_t)) ? 0 : kEFAULT;
  }
  if (pid != 0 && pid != proc->pid) return kEPERM;
  uint32_t data[6] = {};
  if (!CopyFromUser(proc, data, datap, words * 3 * sizeof(uint32_t))) return kEFAULT;
  uint64_t eff = 0, perm = 0, inh = 0;
  for (int w = 0; w < words; ++w) {
    eff |= static_cast<uint64_t>(data[w * 3 + 0]) << (32 * w);
    perm |= static_cast<uint64_t>(data[w * 3 + 1]) << (32 * w);
    inh |= static_cast<uint64_t>(data[w * 3 + 2]) << (32 * w);
  }
  const uint64_t cur = (words == 1) ? (caps & 0xffffffffULL) : caps;
  // The shim does not enforce capabilities, so it cannot honor a request to
  // drop or change them; only re-setting the current sets succeeds.
  if (eff != cur || perm != cur || inh != 0) return kEPERM;
  return 0;
}

int64_t SysPriority(LinuxProcess* proc, uint64_t sc, int which, int who, int niceval) {
  std::vector<std::shared_ptr<LinuxProcess>> targets;
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    if (which < 0 || which > 2) return kEINVAL;
    for (auto& kv : g_processes) {
      LinuxProcess* q = kv.second.get();
      if (q->reaped || q->zombie.load()) continue;
      if (which == 0 && q->pid == (who == 0 ? proc->pid : who)) targets.push_back(kv.second);
      if (which == 1 && q->pgid == (who == 0 ? proc->pgid : who)) targets.push_back(kv.second);
      if (which == 2 && q->ruid == static_cast<uint32_t>(who == 0 ? proc->ruid : who)) {
        targets.push_back(kv.second);
      }
    }
    if (which == 0 && targets.empty() && who != 0) {
      LinuxProcess* owner = nullptr;
      if (FindThreadByTidLocked(who, &owner) != nullptr) targets.push_back(g_processes[owner->pid]);
    }
  }
  if (targets.empty()) return kESRCH;
  if (sc == 140) {  // getpriority: kernel ABI returns 20 - nice
    int best = 19;
    for (auto& q : targets) best = std::min(best, q->nice);
    return 20 - best;
  }
  const int nice = std::max(-20, std::min(19, niceval));
  for (auto& q : targets) {
    if (nice < q->nice && !Privileged(proc)) return -13;  // EACCES
  }
  for (auto& q : targets) {
    q->nice = nice;
    std::vector<std::shared_ptr<LinuxThread>> ths;
    {
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      ths = q->threads;
    }
    for (auto& t : ths) {
      if (ApplySchedProfile(q.get(), t.get(), ThreadAffinity(t.get())) != ZX_OK) return kEPERM;
    }
  }
  return 0;
}

LinuxThread* LookupSchedTarget(LinuxProcess* proc, LinuxThread* thrd, int pid,
                               LinuxProcess** owner) {
  if (pid == 0) {
    *owner = proc;
    return thrd;
  }
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  return FindThreadByTidLocked(pid, owner);
}

int64_t SysSched(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc, uint64_t a0, uint64_t a1,
                 uint64_t a2) {
  constexpr int kSchedOther = 0, kSchedFifo = 1, kSchedRr = 2, kSchedBatch = 3, kSchedIdle = 5,
                kSchedDeadline = 6;
  switch (sc) {
    case 24:  // sched_yield
      zx_thread_legacy_yield(0);
      return 0;
    case 146:    // sched_get_priority_max
    case 147: {  // sched_get_priority_min
      const int policy = static_cast<int>(a0);
      if (policy == kSchedFifo || policy == kSchedRr) return sc == 146 ? 99 : 1;
      if (policy == kSchedOther || policy == kSchedBatch || policy == kSchedIdle ||
          policy == kSchedDeadline) {
        return 0;
      }
      return kEINVAL;
    }
    default:
      break;
  }
  if (static_cast<int>(a0) < 0) return kEINVAL;
  LinuxProcess* owner = nullptr;
  LinuxThread* target = LookupSchedTarget(proc, thrd, static_cast<int>(a0), &owner);
  if (target == nullptr) return kESRCH;
  switch (sc) {
    case 145:  // sched_getscheduler: every thread runs under SCHED_OTHER
      return kSchedOther;
    case 143: {  // sched_getparam
      const int32_t prio = 0;
      return CopyToUser(proc, a1, &prio, sizeof(prio)) ? 0 : kEFAULT;
    }
    case 142:    // sched_setparam
    case 144: {  // sched_setscheduler
      const int policy = sc == 144 ? (static_cast<int>(a1) & ~0x40000000) : kSchedOther;
      const uintptr_t pp = sc == 144 ? a2 : a1;
      int32_t prio = 0;
      if (!CopyFromUser(proc, &prio, pp, sizeof(prio))) return kEFAULT;
      if (policy == kSchedOther) return prio == 0 ? 0 : kEINVAL;  // already SCHED_OTHER
      if (policy == kSchedFifo || policy == kSchedRr) {
        if (prio < 1 || prio > 99) return kEINVAL;
        return kEPERM;  // real-time policies are not implemented
      }
      return kEINVAL;  // SCHED_BATCH / SCHED_IDLE / SCHED_DEADLINE not implemented
    }
    case 203: {  // sched_setaffinity(pid, len, mask)
      uint64_t mask = 0;
      const size_t len = std::min<size_t>(static_cast<size_t>(a1), sizeof(mask));
      if (len == 0) return kEINVAL;
      if (!CopyFromUser(proc, &mask, a2, len)) return kEFAULT;
      mask &= OnlineCpuMask();
      if (mask == 0) return kEINVAL;
      if (ApplySchedProfile(owner, target, mask) != ZX_OK) return kEPERM;
      std::lock_guard<std::mutex> l(g_affinity_mtx);
      g_affinity[target] = mask;
      return 0;
    }
    case 204: {  // sched_getaffinity(pid, len, mask)
      const size_t len = static_cast<size_t>(a1);
      const uint32_t ncpu = std::max<uint32_t>(1, zx_system_get_num_cpus());
      if (len * 8 < ncpu || (len & 7) != 0) return kEINVAL;
      const uint64_t mask = ThreadAffinity(target) & OnlineCpuMask();
      std::vector<uint8_t> buf(std::min<size_t>(len, 64), 0);
      memcpy(buf.data(), &mask, std::min<size_t>(buf.size(), sizeof(mask)));
      if (!CopyToUser(proc, a2, buf.data(), buf.size())) return kEFAULT;
      return 8;  // sizeof(cpumask) for <= 64 CPUs
    }
    default:
      return kENOSYS;
  }
}

int64_t SysMembarrier(LinuxProcess* proc, int cmd, unsigned flags) {
  constexpr int kQuery = 0, kPrivExp = 8, kRegPrivExp = 16, kPrivExpSync = 32,
                kRegPrivExpSync = 64, kGetRegistrations = 512;
  constexpr int kSupported = kPrivExp | kRegPrivExp | kPrivExpSync | kRegPrivExpSync |
                             kGetRegistrations;
  if (flags != 0) return kEINVAL;
  switch (cmd) {
    case kQuery:
      return kSupported;
    case kRegPrivExp:
      proc->membarrier_registered |= 1;
      return 0;
    case kRegPrivExpSync:
      proc->membarrier_registered |= 2;
      return 0;
    case kPrivExp:
      if (!(proc->membarrier_registered & 1)) return kEPERM;
      // The calling thread runs in the Zircon process that holds all threads
      // of this Linux process.
      zx_membarrier_sync_process_data();
      return 0;
    case kPrivExpSync:
      if (!(proc->membarrier_registered & 2)) return kEPERM;
      zx_membarrier_sync_process_insn();
      return 0;
    case kGetRegistrations:
      return ((proc->membarrier_registered & 1) ? kRegPrivExp : 0) |
             ((proc->membarrier_registered & 2) ? kRegPrivExpSync : 0);
    default:
      return kEINVAL;  // GLOBAL*, *_RSEQ: not supported (not in QUERY)
  }
}

// Picks the reaper for orphans of `p`: the nearest live ancestor marked as a
// child subreaper, otherwise init.
int FindReaperLocked(LinuxProcess* p) {
  int pid = p->ppid;
  for (int depth = 0; depth < 1024 && pid > 1; ++depth) {
    auto it = g_processes.find(pid);
    if (it == g_processes.end()) break;
    if (it->second->child_subreaper && !it->second->exited.load()) return pid;
    pid = it->second->ppid;
  }
  return 1;
}

int64_t HandleProcSyscallImpl(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc, uint64_t a0,
                              uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                              bool* handled) {
  *handled = true;
  bool sub = false;
  int64_t r = HandleSignalSyscall(proc, thrd, sc, a0, a1, a2, a3, &sub);
  if (sub) return r;
  r = HandleTimeSyscall(proc, thrd, sc, a0, a1, a2, a3, &sub);
  if (sub) return r;
  r = HandleCredSyscall(proc, sc, a0, a1, a2, &sub);
  if (sub) return r;

  switch (sc) {
    case 57:  // fork
      return DoForkOrClone(proc, thrd, 0, 0, 0, 0, 0, kSIGCHLD);
    case 58:  // vfork
      return DoForkOrClone(proc, thrd, kCloneVm | kCloneVfork, 0, 0, 0, 0, kSIGCHLD);
    case 56:  // clone(flags, stack, parent_tid, child_tid, tls)
      return DoForkOrClone(proc, thrd, a0 & ~kCsignal, a1, a2, a3, a4,
                           static_cast<int>(a0 & kCsignal));
    case 435: {  // clone3(args, size)
      const size_t size = static_cast<size_t>(a1);
      if (size < 64) return kEINVAL;
      if (size > 4096) return kE2BIG;
      LinuxCloneArgs ca{};
      if (!CopyFromUser(proc, &ca, a0, std::min(size, sizeof(ca)))) return kEFAULT;
      for (size_t off = sizeof(ca); off < size; ++off) {  // unknown fields must be 0
        uint8_t b = 0;
        if (!CopyFromUser(proc, &b, a0 + off, 1)) return kEFAULT;
        if (b != 0) return kE2BIG;
      }
      if (ca.flags & kCsignal) return kEINVAL;
      if (ca.exit_signal > static_cast<uint64_t>(kNSIG)) return kEINVAL;
      if (ca.set_tid != 0 || ca.set_tid_size != 0) return kEINVAL;  // choosing PIDs
      if (ca.flags & kCloneIntoCgroup) return kEINVAL;              // no cgroups
      if ((ca.stack == 0) != (ca.stack_size == 0)) return kEINVAL;
      const uintptr_t child_sp = ca.stack != 0 ? ca.stack + ca.stack_size : 0;
      return DoForkOrClone(proc, thrd, ca.flags, child_sp, ca.parent_tid, ca.child_tid, ca.tls,
                           static_cast<int>(ca.exit_signal));
    }
    case 59:    // execve
    case 322:   // execveat
      return SysExecve(proc, thrd, sc, a0, a1, a2, a3, a4);
    case 60:  // exit: ends only the calling thread
      thrd->exit_code = static_cast<int>(a0) & 0xff;
      thrd->kill_requested.store(true);
      return 0;
    case 231:  // exit_group
      thrd->exit_code = static_cast<int>(a0) & 0xff;
      GroupExit(proc, (static_cast<int>(a0) & 0xff) << 8);
      return 0;
    case 61:    // wait4
    case 247:   // waitid
      return SysWait(proc, thrd, sc, a0, a1, a2, a3, a4);
    case 158: {  // arch_prctl
#if defined(__x86_64__)
      const int code = static_cast<int>(a0);
      const uintptr_t addr = static_cast<uintptr_t>(a1);
      constexpr uintptr_t kTaskSizeMax = 0x00007ffffffff000ULL;
      if (code == 0x1002 || code == 0x1001) {  // ARCH_SET_FS / ARCH_SET_GS
        if (addr >= kTaskSizeMax) return kEPERM;
        if (code == 0x1002) thrd->state->fs_base = addr; else thrd->state->gs_base = addr;
        return 0;
      }
      if (code == 0x1003 || code == 0x1004) {  // ARCH_GET_FS / ARCH_GET_GS
        const uint64_t v = code == 0x1003 ? thrd->state->fs_base : thrd->state->gs_base;
        return CopyToUser(proc, addr, &v, sizeof(v)) ? 0 : kEFAULT;
      }
      return kEINVAL;
#else
      return kEINVAL;
#endif
    }
    case 218:  // set_tid_address
      thrd->clear_child_tid = static_cast<uintptr_t>(a0);
      return thrd->tid;
    case 202:  // futex
      return SysFutex(proc, thrd, static_cast<uintptr_t>(a0), static_cast<int>(a1),
                      static_cast<uint32_t>(a2), static_cast<uintptr_t>(a3),
                      static_cast<uintptr_t>(a4), static_cast<uint32_t>(a5));
    case 273:  // set_robust_list(head, len)
      if (a1 != 24) return kEINVAL;
      thrd->robust_list_head = static_cast<uintptr_t>(a0);
      thrd->robust_list_len = static_cast<size_t>(a1);
      return 0;
    case 274: {  // get_robust_list(pid, head_ptr, len_ptr)
      uint64_t head = 0, len = 0;
      if (static_cast<int>(a0) == 0) {
        head = thrd->robust_list_head;
        len = thrd->robust_list_len;
      } else {
        std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
        LinuxThread* t = FindThreadByTidLocked(static_cast<int>(a0), nullptr);
        if (t == nullptr) return kESRCH;
        head = t->robust_list_head;
        len = t->robust_list_len;
      }
      if (!CopyToUser(proc, a1, &head, sizeof(head))) return kEFAULT;
      if (!CopyToUser(proc, a2, &len, sizeof(len))) return kEFAULT;
      return 0;
    }
    case 334:  // rseq: restartable sequences are not implemented
      return kENOSYS;
    case 63: {  // uname
      char buf[65 * 6];
      memset(buf, 0, sizeof(buf));
      std::lock_guard<std::mutex> l(g_uts_mtx);
      strncpy(buf + 0 * 65, "Linux", 64);
      strncpy(buf + 1 * 65, g_hostname.c_str(), 64);
      // The release string is the Linux ABI level the shim targets.
      strncpy(buf + 2 * 65, "6.6.0-zircon-shim", 64);
      strncpy(buf + 3 * 65, "#1 SMP Fuchsia Zircon restricted mode", 64);
#if defined(__x86_64__)
      strncpy(buf + 4 * 65, "x86_64", 64);
#elif defined(__aarch64__)
      strncpy(buf + 4 * 65, "aarch64", 64);
#endif
      strncpy(buf + 5 * 65, g_domainname.c_str(), 64);
      return CopyToUser(proc, a0, buf, sizeof(buf)) ? 0 : kEFAULT;
    }
    case 170:    // sethostname(name, len)
    case 171: {  // setdomainname(name, len)
      if (!Privileged(proc)) return kEPERM;
      const size_t len = static_cast<size_t>(a1);
      if (len > 64) return kEINVAL;
      std::string name(len, '\0');
      if (len > 0 && !CopyFromUser(proc, name.data(), a0, len)) return kEFAULT;
      name = std::string(name.c_str());  // stop at an embedded NUL like Linux's utsname
      std::lock_guard<std::mutex> l(g_uts_mtx);
      (sc == 170 ? g_hostname : g_domainname) = name;
      return 0;
    }
    case 99:  // sysinfo
      return SysSysinfo(proc, a0);
    case 318: {  // getrandom(buf, len, flags)
      if (a2 & ~0x7ULL) return kEINVAL;  // GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE
      const size_t len = std::min<size_t>(static_cast<size_t>(a1), 33554431);
      std::vector<uint8_t> tmp(std::min<size_t>(len, 65536));
      size_t done = 0;
      while (done < len) {
        const size_t n = std::min(tmp.size(), len - done);
        zx_cprng_draw(tmp.data(), n);
        if (!CopyToUser(proc, a0 + done, tmp.data(), n)) return done ? done : kEFAULT;
        done += n;
      }
      return static_cast<int64_t>(len);
    }
    case 309: {  // getcpu(cpu, node, tcache)
      zx_info_thread_stats_t stats{};
      zx_status_t st;
      {
        std::lock_guard<std::mutex> wl(thrd->wake_mtx);
        st = zx_object_get_info(thrd->zx_thread, ZX_INFO_THREAD_STATS, &stats, sizeof(stats),
                                nullptr, nullptr);
      }
      if (st != ZX_OK) return kEINVAL;
      const uint32_t cpu = stats.last_scheduled_cpu, node = 0;
      if (a0 != 0 && !CopyToUser(proc, a0, &cpu, sizeof(cpu))) return kEFAULT;
      if (a1 != 0 && !CopyToUser(proc, a1, &node, sizeof(node))) return kEFAULT;
      return 0;
    }
    case 39:  // getpid
      return proc->pid;
    case 186:  // gettid
      return thrd->tid;
    case 110: {  // getppid
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      return proc->ppid;
    }
    case 111:  // getpgrp
      return proc->pgid;
    case 121:    // getpgid(pid)
    case 124: {  // getsid(pid)
      auto target = LookupPidOrSelf(proc, static_cast<int>(a0));
      if (!target) return kESRCH;
      return sc == 121 ? target->pgid : target->sid;
    }
    case 109: {  // setpgid(pid, pgid)
      int pid = static_cast<int>(a0);
      int pgid = static_cast<int>(a1);
      if (pgid < 0) return kEINVAL;
      if (pid == 0) pid = proc->pid;
      if (pgid == 0) pgid = pid;
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      auto it = g_processes.find(pid);
      if (it == g_processes.end() || it->second->reaped) return kESRCH;
      LinuxProcess* t = it->second.get();
      if (t != proc && t->ppid != proc->pid) return kESRCH;
      if (t->sid != proc->sid) return kEPERM;
      if (t->sid == t->pid) return kEPERM;  // session leader
      if (pgid != pid) {
        bool exists = false;
        for (auto& kv : g_processes) {
          if (kv.second->pgid == pgid && kv.second->sid == proc->sid) exists = true;
        }
        if (!exists) return kEPERM;
      }
      t->pgid = pgid;
      return 0;
    }
    case 112: {  // setsid
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      for (auto& kv : g_processes) {
        if (kv.second->pgid == proc->pid) return kEPERM;  // already a group leader
      }
      proc->sid = proc->pid;
      proc->pgid = proc->pid;
      return proc->sid;
    }
    case 97:  // getrlimit(resource, rlim)
      return SysPrlimit(proc, 0, static_cast<int>(a0), 0, static_cast<uintptr_t>(a1));
    case 160:  // setrlimit(resource, rlim)
      return SysPrlimit(proc, 0, static_cast<int>(a0), static_cast<uintptr_t>(a1), 0);
    case 302:  // prlimit64(pid, resource, new, old)
      return SysPrlimit(proc, static_cast<int>(a0), static_cast<int>(a1),
                        static_cast<uintptr_t>(a2), static_cast<uintptr_t>(a3));
    case 98:  // getrusage(who, usage)
      return SysGetrusage(proc, thrd, static_cast<int>(a0), static_cast<uintptr_t>(a1));
    case 157:  // prctl
      return SysPrctl(proc, thrd, a0, a1, a2, a3, a4);
    case 125:  // capget
      return SysCapget(proc, a0, a1, false);
    case 126:  // capset
      return SysCapget(proc, a0, a1, true);
    case 140:  // getpriority(which, who)
    case 141:  // setpriority(which, who, prio)
      return SysPriority(proc, sc, static_cast<int>(a0), static_cast<int>(a1),
                         static_cast<int>(a2));
    case 24:
    case 142:
    case 143:
    case 144:
    case 145:
    case 146:
    case 147:
    case 203:
    case 204:
      return SysSched(proc, thrd, sc, a0, a1, a2);
    case 324:  // membarrier(cmd, flags, cpu_id)
      return SysMembarrier(proc, static_cast<int>(a0), static_cast<unsigned>(a1));
    case 237:  // mbind
    case 238:  // set_mempolicy
    case 239:  // get_mempolicy
    case 256:  // migrate_pages
    case 279:  // move_pages
      // NUMA memory policy: the shim behaves like a kernel built without
      // CONFIG_NUMA, where these syscalls return -ENOSYS.
      return kENOSYS;
    case 251:  // ioprio_set
    case 252:  // ioprio_get
      // There is no block I/O scheduler (as with CONFIG_BLOCK=n): -ENOSYS.
      return kENOSYS;
    default:
      *handled = false;
      return kENOSYS;
  }
}

}  // namespace

void ApplyThreadSchedProfile(LinuxProcess* proc, LinuxThread* thrd) {
  const uint64_t aff = ThreadAffinity(thrd);
  if (proc->nice == 0 && aff == OnlineCpuMask()) return;  // Zircon defaults
  ApplySchedProfile(proc, thrd, aff);
}

void ForgetThreadSchedState(LinuxThread* thrd) {
  std::lock_guard<std::mutex> l(g_affinity_mtx);
  g_affinity.erase(thrd);
}

void OnLastThreadExit(LinuxProcess* p) {
  {
    std::lock_guard<std::mutex> sl(p->sig_mtx);
    if (!p->exited.load()) {
      // Every thread called SYS_exit: the status is the leader's exit code.
      p->exit_status = (p->leader_exit_code & 0xff) << 8;
      p->exited.store(true);
    }
    p->stopped = false;
  }
  NotifyVforkDone(p);

  // Close all descriptors so pipe/socket peers see EOF.
  std::map<int, OpenFd> closed_fds;
  {
    std::lock_guard<std::recursive_mutex> flock(p->fd_mtx);
    closed_fds.swap(p->fds);
  }
  closed_fds.clear();
  TimeProcessExit(p);
  {
    const int64_t cpu = ProcessCpuTimeNs(p);
    p->cpu_time_ns = cpu > 0 ? cpu : 0;
  }
  ReleaseProcessAddressSpace(p);

  {
    std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
    // Reparent children; deliver PR_SET_PDEATHSIG.
    const int reaper_pid = FindReaperLocked(p);
    auto reaper_it = g_processes.find(reaper_pid);
    bool reaper_has_zombie = false;
    for (auto& kv : g_processes) {
      LinuxProcess* c = kv.second.get();
      if (c->ppid != p->pid || c == p) continue;
      c->ppid = reaper_pid;
      if (c->pdeath_signal != 0) {
        SendSignalToProcess(c, MakeSiginfo(c->pdeath_signal, kSI_USER));
      }
      if (c->zombie.load()) reaper_has_zombie = true;
    }
    if (reaper_has_zombie && reaper_it != g_processes.end()) {
      SendSignalToProcess(reaper_it->second.get(), MakeSiginfo(kSIGCHLD, kSI_KERNEL));
      WakeProcessThreads(reaper_it->second.get());
    }

    auto parent_it = g_processes.find(p->ppid);
    LinuxProcess* parent = parent_it == g_processes.end() ? nullptr : parent_it->second.get();
    bool autoreap = false;
    bool send = p->exit_signal != 0;
    if (parent != nullptr && p->exit_signal == kSIGCHLD) {
      std::lock_guard<std::mutex> sl(parent->sig_mtx);
      const LinuxSigaction& a = parent->sigactions[kSIGCHLD];
      if (a.sa_handler == kSIG_IGN || (a.sa_flags & kSA_NOCLDWAIT) != 0) autoreap = true;
      if (a.sa_handler == kSIG_IGN) send = false;
    }
    if (parent != nullptr && send) {
      const int st = p->exit_status;
      const int code = (st & 0x7f) == 0 ? kCLD_EXITED : kCLD_KILLED;
      const int si_status = (st & 0x7f) == 0 ? ((st >> 8) & 0xff) : (st & 0x7f);
      LinuxSiginfo info = MakeSiginfoChld(code, p->pid, si_status, p->cpu_time_ns);
      info.si_signo = p->exit_signal;
      SendSignalToProcess(parent, info);
    }
    p->zombie.store(true);
    if (autoreap) {
      auto self_it = g_processes.find(p->pid);
      if (self_it != g_processes.end()) ReapLocked(parent, self_it);
    }
    if (parent != nullptr) WakeProcessThreads(parent);
  }
  g_wait4_cv.notify_all();
  NotifyPollWaiters();
}

int64_t HandleProcSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc, uint64_t a0,
                          uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
  bool handled = false;
  return HandleProcSyscallImpl(proc, thrd, sc, a0, a1, a2, a3, a4, a5, &handled);
}

int64_t DispatchLinuxSyscall(LinuxProcess* proc, LinuxThread* thrd) {
#if defined(__x86_64__)
  const uint64_t sc = thrd->state->rax;
  const uint64_t a0 = thrd->state->rdi;
  const uint64_t a1 = thrd->state->rsi;
  const uint64_t a2 = thrd->state->rdx;
  const uint64_t a3 = thrd->state->r10;
  const uint64_t a4 = thrd->state->r8;
  const uint64_t a5 = thrd->state->r9;
#elif defined(__aarch64__)
  uint64_t arm_sc = thrd->state->x[8];
  uint64_t a0 = thrd->state->x[0];
  uint64_t a1 = thrd->state->x[1];
  uint64_t a2 = thrd->state->x[2];
  uint64_t a3 = thrd->state->x[3];
  uint64_t a4 = thrd->state->x[4];
  uint64_t a5 = thrd->state->x[5];
  if (arm_sc == 220) {  // clone on ARM64: (flags, stack, ptid, tls, ctid)
    std::swap(a3, a4);  // swap tls and ctid to match canonical clone order (flags, stack, ptid, ctid, tls)
  }
  const uint64_t sc = MapArm64SyscallToCanonical(arm_sc);
#endif

  thrd->last_sc.store(sc);
  thrd->in_syscall.store(true);
  struct ScGuard {
    LinuxThread* t;
    ~ScGuard() { t->in_syscall.store(false); }
  } guard{thrd};

  int64_t ret = HandleFsSyscall(proc, thrd, sc, a0, a1, a2, a3, a4, a5);
  if (ret != kENOSYS) return ret;
  ret = HandleMemSyscall(proc, thrd, sc, a0, a1, a2, a3, a4, a5);
  if (ret != kENOSYS) return ret;
  ret = HandleIpcSyscall(proc, thrd, sc, a0, a1, a2, a3, a4, a5);
  if (ret != kENOSYS) return ret;

  bool handled = false;
  ret = HandleProcSyscallImpl(proc, thrd, sc, a0, a1, a2, a3, a4, a5, &handled);
  if (handled) return ret;

  // No handler claimed this syscall number (an FS/MEM/IPC handler that
  // returned -ENOSYS for a number it handles also ends up here).
  static std::atomic<bool> logged_enosys[512]{};
  if (sc < 512 && !logged_enosys[sc].exchange(true)) {
    ShimLogf("[shim] syscall %llu not implemented (first seen in PID %d (%s)); -ENOSYS\n",
             static_cast<unsigned long long>(sc), proc->pid, proc->comm.c_str());
  }
  return kENOSYS;
}
