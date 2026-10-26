// Linux memory-management syscalls: brk, mmap, munmap, mprotect, mremap,
// madvise, mincore, msync, mlock*, memfd_create and SysV shared memory.
// Address-space bookkeeping lives in mem_space.{h,cc}.

#include <stddef.h>

#include <algorithm>
#include <atomic>

#include "src/shim/mem_space.h"
#include "src/shim/shim_common.h"
#include "src/shim/vfs.h"

namespace {

// Linux mmap flags (x86_64).
constexpr uint32_t kMapShared = 0x01;
constexpr uint32_t kMapPrivate = 0x02;
constexpr uint32_t kMapSharedValidate = 0x03;
constexpr uint32_t kMapTypeMask = 0x0f;
constexpr uint32_t kMapFixed = 0x10;
constexpr uint32_t kMapAnonymous = 0x20;
constexpr uint32_t kMap32Bit = 0x40;
constexpr uint32_t kMapGrowsDown = 0x100;
constexpr uint32_t kMapDenyWrite = 0x800;
constexpr uint32_t kMapExecutable = 0x1000;
constexpr uint32_t kMapLocked = 0x2000;
constexpr uint32_t kMapNoReserve = 0x4000;
constexpr uint32_t kMapPopulate = 0x8000;
constexpr uint32_t kMapNonBlock = 0x10000;
constexpr uint32_t kMapStack = 0x20000;
constexpr uint32_t kMapHugeTlb = 0x40000;
constexpr uint32_t kMapSync = 0x80000;
constexpr uint32_t kMapFixedNoReplace = 0x100000;
constexpr uint32_t kMapUninitialized = 0x4000000;
// Flags MAP_SHARED_VALIDATE accepts (MAP_SYNC needs DAX, so it is rejected
// with -EOPNOTSUPP below).
constexpr uint32_t kMapKnownFlags =
    kMapTypeMask | kMapFixed | kMapAnonymous | kMap32Bit | kMapGrowsDown |
    kMapDenyWrite | kMapExecutable | kMapLocked | kMapNoReserve |
    kMapPopulate | kMapNonBlock | kMapStack | kMapHugeTlb | kMapSync |
    kMapFixedNoReplace | kMapUninitialized | (0x3fU << 26);

constexpr uint32_t kProtSem = 0x8;
constexpr uint32_t kProtGrowsDown = 0x01000000;
constexpr uint32_t kProtGrowsUp = 0x02000000;

// MAP_32BIT places mappings in the low 2 GiB (Linux uses [1 GiB, 2 GiB)).
constexpr uintptr_t kMap32BitLo = 0x40000000ULL;
constexpr uintptr_t kMap32BitHi = 0x80000000ULL;

int64_t ZxToLinuxMapError(zx_status_t st) {
  switch (st) {
    case ZX_ERR_ACCESS_DENIED:
      return kEACCES;
    case ZX_ERR_ALREADY_EXISTS:
      return kEEXIST;
    default:
      return kENOMEM;
  }
}

std::shared_ptr<FileDescription> GetFdDescMem(LinuxProcess* proc, int fd) {
  std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
  auto it = proc->fds.find(fd);
  if (it == proc->fds.end()) return nullptr;
  return it->second.desc;
}

// ---------------------------------------------------------------------------
// SysV shared memory state (global IPC namespace).
// ---------------------------------------------------------------------------

constexpr int kIpcPrivate = 0;
constexpr int kIpcCreat = 01000;
constexpr int kIpcExcl = 02000;
constexpr int kIpcRmid = 0;
constexpr int kIpcSet = 1;
constexpr int kIpcStat = 2;
constexpr int kIpcInfo = 3;
constexpr int kShmLock = 11;
constexpr int kShmUnlock = 12;
constexpr int kShmStat = 13;
constexpr int kShmInfo = 14;
constexpr int kShmStatAny = 15;
constexpr int kShmRdonly = 010000;
constexpr int kShmRnd = 020000;
constexpr int kShmRemap = 040000;
constexpr int kShmExec = 0100000;
constexpr uint32_t kShmDestMode = 01000;    // SHM_DEST in shm_perm.mode
constexpr uint32_t kShmLockedMode = 02000;  // SHM_LOCKED in shm_perm.mode
constexpr int kShmMni = 4096;               // Linux SHMMNI
constexpr uint64_t kShmMax = UINT64_MAX - (1ULL << 24);  // Linux SHMMAX
constexpr uint64_t kShmAll = UINT64_MAX - (1ULL << 24);  // Linux SHMALL

struct ShmSegment {
  int id = 0;
  int32_t key = 0;
  size_t segsz = 0;     // size requested by shmget
  size_t map_size = 0;  // page-rounded VMO size
  std::shared_ptr<VmoRef> vmo;
  zx_koid_t koid = ZX_KOID_INVALID;
  uint32_t mode = 0;  // permission bits | SHM_DEST | SHM_LOCKED
  uint32_t uid = 0, gid = 0, cuid = 0, cgid = 0;
  uint16_t seq = 0;
  int cpid = 0;
  int lpid = 0;
  int64_t atime = 0, dtime = 0, ctime = 0;
  uint64_t nattch = 0;
};

struct ShmState {
  std::mutex mtx;
  int next_id = 0;
  uint16_t next_seq = 0;
  std::map<int, ShmSegment> segs;
};

// Intentionally leaked so that process teardown during static destruction
// never touches a destroyed mutex.
ShmState& Shm() {
  static ShmState* s = new ShmState();
  return *s;
}

// SysV shm timestamps use the shim's CMOS-RTC-backed wall clock.
int64_t ShmWallSeconds() {
  return static_cast<int64_t>(ShimRealtimeNs() / 1000000000ULL);
}

// Linux struct shmid64_ds (x86_64), 112 bytes.
struct LinuxShmidDs {
  int32_t key;
  uint32_t uid, gid, cuid, cgid;
  uint32_t mode;
  uint16_t seq;
  uint16_t pad2;
  uint32_t pad3;
  uint64_t unused1, unused2;
  uint64_t shm_segsz;
  int64_t shm_atime, shm_dtime, shm_ctime;
  int32_t shm_cpid, shm_lpid;
  uint64_t shm_nattch;
  uint64_t unused4, unused5;
};
static_assert(sizeof(LinuxShmidDs) == 112, "shmid64_ds layout");
static_assert(offsetof(LinuxShmidDs, shm_segsz) == 48, "shmid64_ds layout");
static_assert(offsetof(LinuxShmidDs, shm_nattch) == 88, "shmid64_ds layout");

struct LinuxShmInfo64 {  // IPC_INFO
  uint64_t shmmax, shmmin, shmmni, shmseg, shmall;
  uint64_t unused[4];
};
struct LinuxShmInfoStats {  // SHM_INFO
  int32_t used_ids;
  int32_t pad;
  uint64_t shm_tot, shm_rss, shm_swp, swap_attempts, swap_successes;
};

void ShmMaybeFreeLocked(ShmState& s, std::map<int, ShmSegment>::iterator it) {
  if ((it->second.mode & kShmDestMode) != 0 && it->second.nattch == 0) {
    s.segs.erase(it);  // mapped regions keep their own VmoRef
  }
}

// ---------------------------------------------------------------------------
// memfd: anonymous, unlinked VFS nodes.
// ---------------------------------------------------------------------------

std::atomic<uint64_t> g_memfd_ino{0x7f00000000000000ULL};

void DestroyAnonNode(VfsNode* node) {
  std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
  if (node->shared_host_ptr != 0) {
    zx_vmar_unmap(zx_vmar_root_self(), node->shared_host_ptr,
                  node->shared_vmo_size);
  }
  if (node->shared_vmo != ZX_HANDLE_INVALID) zx_handle_close(node->shared_vmo);
  delete node;
}

int64_t SysMemfdCreate(LinuxProcess* proc, uintptr_t uname, uint32_t flags) {
  constexpr uint32_t kMfdCloexec = 0x1;
  constexpr uint32_t kMfdAllowSealing = 0x2;
  constexpr uint32_t kMfdHugetlb = 0x4;
  constexpr uint32_t kMfdNoexecSeal = 0x8;
  constexpr uint32_t kMfdExec = 0x10;
  if ((flags & ~(kMfdCloexec | kMfdAllowSealing | kMfdHugetlb |
                 kMfdNoexecSeal | kMfdExec)) != 0) {
    return kEINVAL;
  }
  // No hugetlbfs, and file seals are not implemented (F_ADD_SEALS is not
  // supported), so refuse requests that depend on them.
  // MFD_ALLOW_SEALING is accepted (it only permits F_ADD_SEALS later);
  // MFD_NOEXEC_SEAL would require applying F_SEAL_EXEC at creation.
  if ((flags & kMfdHugetlb) != 0) return kEINVAL;
  if ((flags & kMfdNoexecSeal) != 0) return kEINVAL;
  if (uname == 0) return kEFAULT;
  const char* uptr = reinterpret_cast<const char*>(uname);
  constexpr size_t kMfdNameMax = 249;  // NAME_MAX - strlen("memfd:")
  size_t len = strnlen(uptr, kMfdNameMax + 1);
  if (len > kMfdNameMax) return kEINVAL;
  std::string name(uptr, len);

  VfsNode* raw = new VfsNode();
  raw->name = "memfd:" + name;
  raw->type = VfsNodeType::kRegular;
  raw->mode = 0100777;
  raw->ino = g_memfd_ino.fetch_add(1);
  raw->parent = nullptr;  // not linked into the namespace
  std::shared_ptr<VfsNode> node(raw, DestroyAnonNode);

  auto desc = std::make_shared<FileDescription>();
  desc->kind = FdKind::kVfsFile;
  desc->flags = kLinuxO_RDWR;
  desc->node = raw;
  desc->owned_node = node;
  desc->path = "/memfd:" + name + " (deleted)";
  std::lock_guard<std::recursive_mutex> flock(proc->fd_mtx);
  int fd = 0;
  while (proc->fds.find(fd) != proc->fds.end()) fd++;
  proc->fds[fd] = OpenFd{std::move(desc), (flags & kMfdCloexec) != 0};
  return fd;
}

// ---------------------------------------------------------------------------
// brk
// ---------------------------------------------------------------------------

int64_t SysBrk(LinuxProcess* proc, uintptr_t req) {
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (req == 0 || req < proc->brk_start) {
    return static_cast<int64_t>(proc->brk_current);
  }
  uint64_t new_end = 0;
  if (!PageRoundUp(req, &new_end)) return static_cast<int64_t>(proc->brk_current);
  if (new_end > proc->brk_mapped_end) {
    size_t delta = new_end - proc->brk_mapped_end;
    // Growing into another mapping fails (brk returns the old break), just as
    // on Linux.
    if (!RangeInVmar(proc, proc->brk_mapped_end, delta) ||
        !RangeIsFreeLocked(proc, proc->brk_mapped_end, delta)) {
      return static_cast<int64_t>(proc->brk_current);
    }
    MemRegion r;
    r.kind = RegionKind::kAnonPrivate;
    r.prot = kLinuxProtRead | kLinuxProtWrite;
    if (CreateAnonVmo(delta, &r.vmo) != ZX_OK) {
      return static_cast<int64_t>(proc->brk_current);
    }
    if (MapRegionLocked(proc, proc->brk_mapped_end, delta, std::move(r),
                        /*overwrite=*/false) != ZX_OK) {
      return static_cast<int64_t>(proc->brk_current);
    }
    proc->brk_mapped_end = new_end;
  } else if (new_end < proc->brk_mapped_end) {
    // Shrinking releases the pages; growing again yields zero-filled memory.
    UnmapRangeLocked(proc, new_end, proc->brk_mapped_end - new_end);
    proc->brk_mapped_end = new_end;
  }
  proc->brk_current = req;
  return static_cast<int64_t>(proc->brk_current);
}

// ---------------------------------------------------------------------------
// mmap
// ---------------------------------------------------------------------------

int64_t SysMmap(LinuxProcess* proc, uintptr_t hint, size_t length,
                uint32_t prot, uint32_t flags, int fd, uint64_t offset) {
  if (length == 0) return kEINVAL;
  if ((offset & (kPageSize - 1)) != 0) return kEINVAL;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kENOMEM;
  if (offset > UINT64_MAX - len) return -75;  // EOVERFLOW

  const uint32_t type = flags & kMapTypeMask;
  bool shared = false;
  if (type == kMapShared) {
    shared = true;
  } else if (type == kMapSharedValidate) {
    shared = true;
    if ((flags & ~kMapKnownFlags) != 0) return kEOPNOTSUPP;
    if ((flags & kMapSync) != 0) return kEOPNOTSUPP;  // needs DAX
  } else if (type != kMapPrivate) {
    return kEINVAL;
  }
  const bool anon = (flags & kMapAnonymous) != 0;
  const bool noreplace = (flags & kMapFixedNoReplace) != 0;
  const bool fixed = noreplace || (flags & kMapFixed) != 0;
  // No huge pages are reserved (Linux behaves the same with nr_hugepages=0).
  if ((flags & kMapHugeTlb) != 0) return kENOMEM;
  // Auto-growing VM_GROWSDOWN mappings are not implemented.
  if ((flags & kMapGrowsDown) != 0) return kEINVAL;
  prot &= kLinuxProtRWX;  // Linux ignores unknown PROT bits in mmap

  MemRegion region;
  region.prot = prot;

  std::shared_ptr<FileDescription> desc;
  if (!anon) {
    desc = GetFdDescMem(proc, fd);
    if (!desc) return kEBADF;
  }

  if (anon || desc->kind == FdKind::kDevZero) {
    // MAP_ANONYMOUS, or /dev/zero which Linux treats as anonymous memory.
    region.kind = shared ? RegionKind::kAnonShared : RegionKind::kAnonPrivate;
    if (CreateAnonVmo(len, &region.vmo) != ZX_OK) return kENOMEM;
  } else if (desc->kind == FdKind::kDevFb0) {
    const auto& fb = GetFbHardwareInfo();
    if (fb.phys_vmo == ZX_HANDLE_INVALID) return kENODEV;
    uint64_t fb_size = 0;
    PageRoundUp(fb.size_bytes, &fb_size);
    if (offset > fb_size || len > fb_size - offset) return kEINVAL;
    uint32_t acc = desc->flags & kLinuxO_ACCMODE;
    if (acc == kLinuxO_WRONLY) return kEACCES;
    if ((prot & kLinuxProtWrite) != 0 && acc != kLinuxO_RDWR) return kEACCES;
    zx_handle_t dup = ZX_HANDLE_INVALID;
    if (zx_handle_duplicate(fb.phys_vmo, ZX_RIGHT_SAME_RIGHTS, &dup) != ZX_OK) {
      return kENOMEM;
    }
    region.kind = RegionKind::kDevice;
    region.vmo = std::make_shared<VmoRef>(dup);
    region.vmo_offset = offset;
    region.max_prot = kLinuxProtRead | kLinuxProtWrite;  // no exec right
    if (acc != kLinuxO_RDWR) region.max_prot &= ~kLinuxProtWrite;
    if ((prot & kLinuxProtExec) != 0) return kEACCES;
  } else if (desc->kind == FdKind::kVfsFile && desc->node) {
    VfsNode* node = desc->node;
    if (node->type != VfsNodeType::kRegular) return kENODEV;
    uint32_t acc = desc->flags & kLinuxO_ACCMODE;
    if (acc == kLinuxO_WRONLY) return kEACCES;
    if (shared) {
      if ((prot & kLinuxProtWrite) != 0 && acc != kLinuxO_RDWR) return kEACCES;
      // The mapping uses the node's shared VMO, which is the file's backing
      // store for read()/write() as well (they sync node->data with it).
      Vfs::EnsureSharedVmo(node, offset + len);
      zx_handle_t dup = ZX_HANDLE_INVALID;
      {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        if (node->shared_vmo == ZX_HANDLE_INVALID ||
            node->shared_vmo_size < offset + len) {
          return kENOMEM;
        }
        if (zx_handle_duplicate(node->shared_vmo, ZX_RIGHT_SAME_RIGHTS, &dup) !=
            ZX_OK) {
          return kENOMEM;
        }
      }
      dup = VmoMakeExecutable(dup);
      if (dup == ZX_HANDLE_INVALID) return kENOMEM;
      region.kind = RegionKind::kFileShared;
      region.vmo = std::make_shared<VmoRef>(dup);
      region.vmo_offset = offset;
      region.node = node;
      region.node_owner = desc->owned_node;
      if (acc != kLinuxO_RDWR) region.max_prot &= ~kLinuxProtWrite;
    } else {
      // MAP_PRIVATE: a private copy of the file contents at mmap time.
      if (CreateAnonVmo(len, &region.vmo) != ZX_OK) return kENOMEM;
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      if (node->shared_vmo != ZX_HANDLE_INVALID) {
        Vfs::SyncNodeFromSharedVmo(node);
      }
      uint64_t copied = 0;
      if (offset < node->data.size()) {
        copied = std::min<uint64_t>(len, node->data.size() - offset);
        if (zx_vmo_write(region.vmo->handle, node->data.data() + offset, 0,
                         copied) != ZX_OK) {
          return kENOMEM;
        }
      }
      region.kind = RegionKind::kFilePrivate;
      region.node = node;
      region.node_owner = desc->owned_node;
      region.file_offset = offset;
      region.file_len = copied;
    }
  } else {
    // Pipes, sockets, character devices without mmap support, procfs files.
    return kENODEV;
  }

  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);

  auto choose = [&]() -> uintptr_t {
    uint64_t h = 0;
    if (hint != 0 && PageRoundUp(hint, &h) && RangeInVmar(proc, h, len) &&
        RangeIsFreeLocked(proc, h, len)) {
      return h;
    }
    if ((flags & kMap32Bit) != 0) {
      return FindFreeRangeLocked(proc, len, kMap32BitLo, kMap32BitHi);
    }
    return FindFreeRangeLocked(proc, len, proc->vmar_base, VmarEnd(proc));
  };

  uintptr_t addr = 0;
  if (fixed) {
    if ((hint & (kPageSize - 1)) != 0) return kEINVAL;
    if (!RangeInVmar(proc, hint, len)) return kENOMEM;
    addr = hint;
    if (noreplace && !RangeIsFreeLocked(proc, addr, len)) return kEEXIST;
  } else {
    addr = choose();
    if (addr == 0) return kENOMEM;
  }

  zx_status_t st = MapRegionLocked(proc, addr, len, region,
                                   /*overwrite=*/fixed && !noreplace);
  if (st == ZX_ERR_ALREADY_EXISTS) {
    // The kernel has a mapping the table did not know about: record it and,
    // unless the caller demanded this exact range, pick another one.
    AdoptUntrackedKernelMappingsLocked(proc);
    if (noreplace) return kEEXIST;
    if (!fixed) {
      addr = choose();
      if (addr == 0) return kENOMEM;
      st = MapRegionLocked(proc, addr, len, region, /*overwrite=*/false);
    }
  }
  if (st != ZX_OK) return ZxToLinuxMapError(st);
  if ((flags & kMapLocked) != 0 && region.kind != RegionKind::kDevice) {
    zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_COMMIT, addr, len,
                     nullptr, 0);
  }
  FlushSharedMappingsLocked(proc);
  return static_cast<int64_t>(addr);
}

// ---------------------------------------------------------------------------
// munmap / mprotect
// ---------------------------------------------------------------------------

int64_t SysMunmap(LinuxProcess* proc, uintptr_t addr, size_t length) {
  if ((addr & (kPageSize - 1)) != 0 || length == 0) return kEINVAL;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kEINVAL;
  if (addr > UINT64_MAX - len) return kEINVAL;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  // Ranges entirely outside the restricted VMAR can never be mapped; Linux
  // rejects ranges beyond TASK_SIZE with -EINVAL.
  if (addr + len > VmarEnd(proc)) return kEINVAL;
  uintptr_t lo = std::max<uintptr_t>(addr, proc->vmar_base);
  if (addr + len > lo) UnmapRangeLocked(proc, lo, addr + len - lo);
  FlushSharedMappingsLocked(proc);
  return 0;
}

int64_t SysMprotect(LinuxProcess* proc, uintptr_t addr, size_t length,
                    uint32_t prot) {
  if ((addr & (kPageSize - 1)) != 0) return kEINVAL;
  // PROT_GROWSDOWN/UP require a VM_GROWS* mapping, which the shim never
  // creates; Linux returns -EINVAL in that case.
  if ((prot & ~(kLinuxProtRWX | kProtSem)) != 0) {
    (void)kProtGrowsDown;
    (void)kProtGrowsUp;
    return kEINVAL;
  }
  prot &= kLinuxProtRWX;
  if (length == 0) return 0;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kENOMEM;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, addr, len)) return kENOMEM;
  int64_t rc = ProtectRangeLocked(proc, addr, len, prot);
  FlushSharedMappingsLocked(proc);
  return rc;
}

// ---------------------------------------------------------------------------
// mremap
// ---------------------------------------------------------------------------

// Maps `ext` more bytes at `at`, continuing the mapping described by `last`
// (the region that ends at `at`).
int64_t MapExtensionLocked(LinuxProcess* proc, const MemRegion& last,
                           uintptr_t at, size_t ext) {
  MemRegion e = last;
  e.len = ext;
  if (!IsSharedKind(last.kind)) {
    // Private memory: fresh pages, filled from the file for file mappings.
    if (CreateAnonVmo(ext, &e.vmo) != ZX_OK) return kENOMEM;
    e.vmo_offset = 0;
    if (last.kind == RegionKind::kFilePrivate && last.node) {
      uint64_t pos = last.file_offset + last.len;
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      if (last.node->shared_vmo != ZX_HANDLE_INVALID) {
        Vfs::SyncNodeFromSharedVmo(last.node);
      }
      uint64_t copied = 0;
      if (pos < last.node->data.size()) {
        copied = std::min<uint64_t>(ext, last.node->data.size() - pos);
        zx_vmo_write(e.vmo->handle, last.node->data.data() + pos, 0, copied);
      }
      e.file_offset = pos;
      e.file_len = copied;
    } else {
      e.kind = RegionKind::kAnonPrivate;
      e.node = nullptr;
      e.node_owner.reset();
      e.file_offset = e.file_len = 0;
    }
  } else {
    // Shared memory keeps sharing: map the following pages of the same VMO.
    if (!last.vmo) return kEFAULT;
    e.vmo_offset = last.vmo_offset + last.len;
    uint64_t needed = e.vmo_offset + ext;
    if (last.kind == RegionKind::kFileShared && last.node) {
      Vfs::EnsureSharedVmo(last.node, needed);
    }
    uint64_t vmo_size = 0;
    if (zx_vmo_get_size(last.vmo->handle, &vmo_size) != ZX_OK ||
        vmo_size < needed) {
      // SysV segments, shared anonymous memory and the framebuffer have a
      // fixed size; the shim cannot extend them.
      return kENOMEM;
    }
  }
  zx_status_t st = MapRegionLocked(proc, at, ext, std::move(e), false);
  return st == ZX_OK ? 0 : ZxToLinuxMapError(st);
}

// Moves the mapped run [old_addr, old_addr + old_size) to new_addr (which
// must be free) and grows it to new_size. Pages move by remapping the same
// VMOs, so private contents are preserved and shared mappings stay shared.
int64_t MoveRunLocked(LinuxProcess* proc, uintptr_t old_addr, size_t old_size,
                      uintptr_t new_addr, size_t new_size) {
  SplitRegionAtLocked(proc, old_addr);
  SplitRegionAtLocked(proc, old_addr + old_size);
  auto& regions = MmLocked(proc).regions;
  std::vector<std::pair<uintptr_t, MemRegion>> pieces;
  for (auto it = regions.lower_bound(old_addr);
       it != regions.end() && it->first < old_addr + old_size; ++it) {
    pieces.emplace_back(it->first, it->second);
  }
  if (pieces.empty()) return kEFAULT;
  for (auto& p : pieces) {
    MemRegion m = p.second;
    if (!m.vmo) {
      if (CopyRegionThroughMappingLocked(proc, p.first, p.second, &m.vmo) !=
          ZX_OK) {
        UnmapRangeLocked(proc, new_addr, p.first - old_addr);
        return kENOMEM;
      }
      m.vmo_offset = 0;
    }
    uintptr_t dest = new_addr + (p.first - old_addr);
    zx_status_t st = MapRegionLocked(proc, dest, m.len, std::move(m), false);
    if (st != ZX_OK) {
      UnmapRangeLocked(proc, new_addr, p.first - old_addr);
      return ZxToLinuxMapError(st);
    }
  }
  if (new_size > old_size) {
    MemRegion last = pieces.back().second;
    if (!last.vmo) {
      // The moved copy at the destination now has a handle; use it.
      auto it = regions.find(new_addr + (pieces.back().first - old_addr));
      if (it != regions.end()) last = it->second;
    }
    int64_t rc = MapExtensionLocked(proc, last, new_addr + old_size,
                                    new_size - old_size);
    if (rc < 0) {
      UnmapRangeLocked(proc, new_addr, old_size);
      return rc;
    }
  }
  UnmapRangeLocked(proc, old_addr, old_size);
  return 0;
}

int64_t SysMremap(LinuxProcess* proc, uintptr_t old_addr, size_t old_size_in,
                  size_t new_size_in, uint32_t flags, uintptr_t new_addr) {
  constexpr uint32_t kMayMove = 1, kFixed = 2, kDontUnmap = 4;
  if ((flags & ~(kMayMove | kFixed | kDontUnmap)) != 0) return kEINVAL;
  if ((flags & kFixed) != 0 && (flags & kMayMove) == 0) return kEINVAL;
  // MREMAP_DONTUNMAP is not implemented (Linux < 5.7 also rejects it).
  if ((flags & kDontUnmap) != 0) return kEINVAL;
  if ((old_addr & (kPageSize - 1)) != 0) return kEINVAL;
  uint64_t old_size = 0, new_size = 0;
  if (!PageRoundUp(old_size_in, &old_size) || !PageRoundUp(new_size_in, &new_size)) {
    return kEINVAL;
  }
  if (new_size == 0) return kEINVAL;

  std::lock_guard<std::mutex> lock(proc->map_mtx);
  auto& regions = MmLocked(proc).regions;

  if ((flags & kFixed) != 0) {
    if ((new_addr & (kPageSize - 1)) != 0) return kEINVAL;
    if (!RangeInVmar(proc, new_addr, new_size)) return kEINVAL;
    uint64_t span = std::max<uint64_t>(old_size, 1);
    if (new_addr < old_addr + span && old_addr < new_addr + new_size) {
      return kEINVAL;
    }
  }

  if (old_size == 0) {
    // Linux: old_size == 0 creates a second mapping of the same shared pages.
    if ((flags & kMayMove) == 0) return kEINVAL;
    auto it = regions.upper_bound(old_addr);
    if (it == regions.begin()) return kEFAULT;
    --it;
    if (old_addr >= it->first + it->second.len) return kEFAULT;
    const MemRegion& r = it->second;
    if (!IsSharedKind(r.kind) || !r.vmo) return kEINVAL;
    MemRegion d = r;
    d.vmo_offset = r.vmo_offset + (old_addr - it->first);
    uint64_t vmo_size = 0;
    if (r.kind == RegionKind::kFileShared && r.node) {
      Vfs::EnsureSharedVmo(r.node, d.vmo_offset + new_size);
    }
    if (zx_vmo_get_size(r.vmo->handle, &vmo_size) != ZX_OK ||
        vmo_size < d.vmo_offset + new_size) {
      return kENOMEM;
    }
    uintptr_t dest = 0;
    bool overwrite = false;
    if ((flags & kFixed) != 0) {
      dest = new_addr;
      overwrite = true;
    } else {
      dest = FindFreeRangeLocked(proc, new_size, proc->vmar_base, VmarEnd(proc));
      if (dest == 0) return kENOMEM;
    }
    zx_status_t st = MapRegionLocked(proc, dest, new_size, std::move(d), overwrite);
    if (st != ZX_OK) return ZxToLinuxMapError(st);
    FlushSharedMappingsLocked(proc);
    return static_cast<int64_t>(dest);
  }

  if (!RangeInVmar(proc, old_addr, old_size) ||
      !RangeFullyMappedLocked(proc, old_addr, old_size)) {
    return kEFAULT;
  }
  // The old range must be one kind of mapping (Linux requires one VMA).
  {
    auto it = regions.upper_bound(old_addr);
    --it;
    bool first_shared = IsSharedKind(it->second.kind);
    for (; it != regions.end() && it->first < old_addr + old_size; ++it) {
      if (IsSharedKind(it->second.kind) != first_shared) return kEFAULT;
    }
  }

  int64_t result = 0;
  if ((flags & kFixed) != 0) {
    if (new_size < old_size) {
      UnmapRangeLocked(proc, old_addr + new_size, old_size - new_size);
      old_size = new_size;
    }
    UnmapRangeLocked(proc, new_addr, new_size);
    int64_t rc = MoveRunLocked(proc, old_addr, old_size, new_addr, new_size);
    result = rc < 0 ? rc : static_cast<int64_t>(new_addr);
  } else if (new_size <= old_size) {
    if (new_size < old_size) {
      UnmapRangeLocked(proc, old_addr + new_size, old_size - new_size);
    }
    result = static_cast<int64_t>(old_addr);
  } else {
    const uintptr_t old_end = old_addr + old_size;
    const size_t ext = new_size - old_size;
    bool grown = false;
    if (RangeInVmar(proc, old_end, ext) && RangeIsFreeLocked(proc, old_end, ext)) {
      // The region containing old_end - 1 ends exactly at old_end (the range
      // after it is free); the extension continues it.
      auto it = regions.upper_bound(old_end - 1);
      --it;
      MemRegion last = it->second;
      int64_t rc = MapExtensionLocked(proc, last, old_end, ext);
      if (rc == 0) {
        grown = true;
        result = static_cast<int64_t>(old_addr);
      } else if (rc != kENOMEM || (flags & kMayMove) == 0) {
        result = rc;
        grown = true;  // report this error
      }
    }
    if (!grown) {
      if ((flags & kMayMove) == 0) {
        result = kENOMEM;
      } else {
        uintptr_t dest =
            FindFreeRangeLocked(proc, new_size, proc->vmar_base, VmarEnd(proc));
        if (dest == 0) {
          result = kENOMEM;
        } else {
          int64_t rc = MoveRunLocked(proc, old_addr, old_size, dest, new_size);
          result = rc < 0 ? rc : static_cast<int64_t>(dest);
        }
      }
    }
  }
  FlushSharedMappingsLocked(proc);
  return result;
}

// ---------------------------------------------------------------------------
// madvise / mincore / msync / mlock
// ---------------------------------------------------------------------------

// Calls fn(start, len, region) for each mapped piece of [addr, addr + len).
template <typename Fn>
void ForEachMappedPieceLocked(LinuxProcess* proc, uintptr_t addr, size_t len,
                              Fn fn) {
  auto& regions = MmLocked(proc).regions;
  uintptr_t end = addr + len;
  auto it = regions.upper_bound(addr);
  if (it != regions.begin()) --it;
  for (; it != regions.end() && it->first < end; ++it) {
    uintptr_t s = std::max<uintptr_t>(it->first, addr);
    uintptr_t e = std::min<uintptr_t>(it->first + it->second.len, end);
    if (e <= s) continue;
    fn(s, e - s, it->first, it->second);
  }
}

// Zeroes private pages (MADV_DONTNEED semantics); file-backed private pages
// are restored from the file, which is what re-faulting them on Linux yields.
int64_t DiscardPrivateLocked(LinuxProcess* proc, uintptr_t s, size_t l,
                             uintptr_t rstart, const MemRegion& r) {
  // ZX_VMAR_OP_ZERO decommits whole pages where it can (freeing memory) and
  // otherwise zeroes them (e.g. copy-on-write children after fork).
  zx_status_t st = zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_ZERO, s,
                                    l, nullptr, 0);
  if (st != ZX_OK) {
    ShimLogf("[mem] madvise discard 0x%llx+0x%llx failed: %d\n",
             static_cast<unsigned long long>(s),
             static_cast<unsigned long long>(l), st);
    return kEINVAL;
  }
  if (r.kind == RegionKind::kFilePrivate && r.node && r.vmo) {
    uint64_t delta = s - rstart;
    if (delta < r.file_len) {
      uint64_t n = std::min<uint64_t>(l, r.file_len - delta);
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      if (r.node->shared_vmo != ZX_HANDLE_INVALID) {
        Vfs::SyncNodeFromSharedVmo(r.node);
      }
      uint64_t fpos = r.file_offset + delta;
      if (fpos < r.node->data.size()) {
        n = std::min<uint64_t>(n, r.node->data.size() - fpos);
        zx_vmo_write(r.vmo->handle, r.node->data.data() + fpos,
                     r.vmo_offset + delta, n);
      }
    }
  }
  return 0;
}

int64_t SysMadvise(LinuxProcess* proc, uintptr_t addr, size_t length,
                   int advice) {
  enum : int {
    kNormal = 0, kRandom = 1, kSequential = 2, kWillNeed = 3, kDontNeed = 4,
    kFree = 8, kRemove = 9, kDontFork = 10, kDoFork = 11, kMergeable = 12,
    kUnmergeable = 13, kHugePage = 14, kNoHugePage = 15, kDontDump = 16,
    kDoDump = 17, kWipeOnFork = 18, kKeepOnFork = 19, kCold = 20,
    kPageOut = 21, kPopulateRead = 22, kPopulateWrite = 23,
    kDontNeedLocked = 24, kCollapse = 25,
  };
  switch (advice) {
    case kNormal: case kRandom: case kSequential: case kWillNeed:
    case kDontNeed: case kFree: case kRemove: case kDontFork: case kDoFork:
    case kDontDump: case kDoDump: case kWipeOnFork: case kKeepOnFork:
    case kCold: case kPageOut: case kPopulateRead: case kPopulateWrite:
    case kDontNeedLocked:
      break;
    case kMergeable: case kUnmergeable:  // no KSM
    case kHugePage: case kNoHugePage: case kCollapse:  // no THP
    default:  // incl. MADV_HWPOISON / MADV_SOFT_OFFLINE
      return kEINVAL;
  }
  if ((addr & (kPageSize - 1)) != 0) return kEINVAL;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kEINVAL;
  if (len == 0) return 0;
  if (addr > UINT64_MAX - len) return kEINVAL;

  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, addr, len)) return kENOMEM;
  const bool holes = !RangeFullyMappedLocked(proc, addr, len);
  int64_t rc = 0;

  switch (advice) {
    case kDontNeed:
    case kDontNeedLocked:
      ForEachMappedPieceLocked(
          proc, addr, len,
          [&](uintptr_t s, size_t l, uintptr_t rs, const MemRegion& r) {
            // Shared pages keep their contents (Linux only drops the PTEs).
            if (IsSharedKind(r.kind) || rc < 0) return;
            rc = DiscardPrivateLocked(proc, s, l, rs, r);
          });
      break;
    case kFree:
      // Only valid for private anonymous memory; contents may be discarded.
      ForEachMappedPieceLocked(proc, addr, len,
                               [&](uintptr_t, size_t, uintptr_t, const MemRegion& r) {
                                 if (r.kind != RegionKind::kAnonPrivate) rc = kEINVAL;
                               });
      if (rc == 0) {
        ForEachMappedPieceLocked(
            proc, addr, len,
            [&](uintptr_t s, size_t l, uintptr_t rs, const MemRegion& r) {
              if (rc == 0) rc = DiscardPrivateLocked(proc, s, l, rs, r);
            });
      }
      break;
    case kRemove:
      // Frees the backing store of shared mappings (punches a hole).
      ForEachMappedPieceLocked(proc, addr, len,
                               [&](uintptr_t, size_t, uintptr_t, const MemRegion& r) {
                                 if (!IsSharedKind(r.kind) ||
                                     r.kind == RegionKind::kDevice) {
                                   rc = kEINVAL;
                                 }
                               });
      if (rc == 0) {
        ForEachMappedPieceLocked(
            proc, addr, len, [&](uintptr_t s, size_t l, uintptr_t, const MemRegion&) {
              if (rc != 0) return;
              if (zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_ZERO, s, l,
                                   nullptr, 0) != ZX_OK) {
                rc = kEINVAL;
              }
            });
      }
      break;
    case kDontFork:
    case kDoFork:
    case kWipeOnFork:
    case kKeepOnFork: {
      if (advice == kWipeOnFork) {
        ForEachMappedPieceLocked(proc, addr, len,
                                 [&](uintptr_t, size_t, uintptr_t, const MemRegion& r) {
                                   if (r.kind != RegionKind::kAnonPrivate) rc = kEINVAL;
                                 });
        if (rc != 0) break;
      }
      // Honored by CloneProcessAddressSpace().
      SplitRegionAtLocked(proc, addr);
      SplitRegionAtLocked(proc, addr + len);
      auto& regions = MmLocked(proc).regions;
      for (auto it = regions.lower_bound(addr);
           it != regions.end() && it->first < addr + len; ++it) {
        if (advice == kDontFork) it->second.dont_fork = true;
        if (advice == kDoFork) it->second.dont_fork = false;
        if (advice == kWipeOnFork) it->second.wipe_on_fork = true;
        if (advice == kKeepOnFork) it->second.wipe_on_fork = false;
      }
      break;
    }
    default:
      // NORMAL/RANDOM/SEQUENTIAL/WILLNEED/COLD/PAGEOUT/POPULATE_*: access
      // pattern and reclaim hints that do not change memory contents; the
      // shim has no readahead or swap, so ignoring them is correct.
      // DONTDUMP/DODUMP only affect core dumps, which the shim never writes.
      break;
  }
  FlushSharedMappingsLocked(proc);
  if (rc != 0) return rc;
  return holes ? kENOMEM : 0;
}

int64_t SysMincore(LinuxProcess* proc, uintptr_t addr, size_t length,
                   uintptr_t uvec) {
  if ((addr & (kPageSize - 1)) != 0) return kEINVAL;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kENOMEM;
  if (len == 0) return 0;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, addr, len) || !RangeFullyMappedLocked(proc, addr, len)) {
    return kENOMEM;
  }
  if (uvec == 0) return kEFAULT;
  uint8_t* vec = reinterpret_cast<uint8_t*>(uvec);
  // Zircon exposes residency per VMO, not per page. A page is reported
  // non-resident only when its VMO has no committed pages at all; otherwise
  // it is reported resident. There is no swap or page cache eviction for
  // these VMOs, so committed data is always in RAM and uncommitted pages are
  // zero-fill-on-demand that never require I/O.
  ForEachMappedPieceLocked(
      proc, addr, len, [&](uintptr_t s, size_t l, uintptr_t, const MemRegion& r) {
        uint8_t resident = 1;
        if (r.vmo && r.kind != RegionKind::kDevice) {
          zx_info_vmo_t info{};
          if (zx_object_get_info(r.vmo->handle, ZX_INFO_VMO, &info, sizeof(info),
                                 nullptr, nullptr) == ZX_OK &&
              info.committed_bytes == 0 && info.populated_bytes == 0) {
            resident = 0;
          }
        }
        memset(vec + (s - addr) / kPageSize, resident, l / kPageSize);
      });
  return 0;
}

int64_t SysMsync(LinuxProcess* proc, uintptr_t addr, size_t length, int flags) {
  constexpr int kMsAsync = 1, kMsInvalidate = 2, kMsSync = 4;
  if ((flags & ~(kMsAsync | kMsInvalidate | kMsSync)) != 0) return kEINVAL;
  if ((flags & kMsAsync) && (flags & kMsSync)) return kEINVAL;
  if ((addr & (kPageSize - 1)) != 0) return kEINVAL;
  uint64_t len = 0;
  if (!PageRoundUp(length, &len)) return kENOMEM;
  if (len == 0) return 0;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, addr, len) || !RangeFullyMappedLocked(proc, addr, len)) {
    return kENOMEM;
  }
  // Nothing to flush: MAP_SHARED file mappings map VfsNode::shared_vmo, which
  // is the file's only backing store once it exists (read/write/pread/pwrite/
  // ftruncate sync node->data from/to it), so mapped stores are immediately
  // visible to file I/O and vice versa. The VFS is RAM-only, so there is no
  // stable storage to write back to.
  return 0;
}

int64_t SysMlock(LinuxProcess* proc, uintptr_t addr, size_t length) {
  uintptr_t start = PageRoundDown(addr);
  uint64_t len = 0;
  if (!PageRoundUp(length + (addr - start), &len)) return kENOMEM;
  if (len == 0) return 0;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, start, len) || !RangeFullyMappedLocked(proc, start, len)) {
    return kENOMEM;
  }
  // Populate the pages now (as mlock does). Anonymous and VFS VMOs are never
  // paged out to a backing store, so committed pages stay resident.
  int64_t rc = 0;
  ForEachMappedPieceLocked(
      proc, start, len, [&](uintptr_t s, size_t l, uintptr_t, const MemRegion& r) {
        if (rc != 0 || r.kind == RegionKind::kDevice) return;
        zx_status_t st = zx_vmar_op_range(proc->restricted_vmar,
                                          ZX_VMAR_OP_COMMIT, s, l, nullptr, 0);
        if (st == ZX_ERR_NO_MEMORY) rc = kEAGAIN;
        zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_ALWAYS_NEED, s, l,
                         nullptr, 0);
      });
  return rc;
}

int64_t SysMlockall(LinuxProcess* proc, int flags) {
  constexpr int kMclCurrent = 1, kMclFuture = 2, kMclOnFault = 4;
  if (flags == 0 || (flags & ~(kMclCurrent | kMclFuture | kMclOnFault)) != 0 ||
      flags == kMclOnFault) {
    return kEINVAL;
  }
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  auto& mm = MmLocked(proc);
  if ((flags & kMclCurrent) != 0 && (flags & kMclOnFault) == 0) {
    for (const auto& kv : mm.regions) {
      if (kv.second.kind == RegionKind::kDevice) continue;
      if (zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_COMMIT, kv.first,
                           kv.second.len, nullptr, 0) == ZX_ERR_NO_MEMORY) {
        return kENOMEM;
      }
    }
  }
  mm.lock_future = (flags & kMclFuture) != 0 && (flags & kMclOnFault) == 0;
  return 0;
}

int64_t SysMunlock(LinuxProcess* proc, uintptr_t addr, size_t length) {
  uintptr_t start = PageRoundDown(addr);
  uint64_t len = 0;
  if (!PageRoundUp(length + (addr - start), &len)) return kENOMEM;
  if (len == 0) return 0;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  if (!RangeInVmar(proc, start, len) || !RangeFullyMappedLocked(proc, start, len)) {
    return kENOMEM;
  }
  // Unlocking only removes the residency guarantee; pages stay as they are.
  return 0;
}

// ---------------------------------------------------------------------------
// SysV shm syscalls
// ---------------------------------------------------------------------------

int64_t SysShmget(LinuxProcess* proc, int32_t key, size_t size, int shmflg) {
  ShmState& s = Shm();
  std::lock_guard<std::mutex> lock(s.mtx);
  if (key != kIpcPrivate) {
    for (auto& kv : s.segs) {
      ShmSegment& seg = kv.second;
      if ((seg.mode & kShmDestMode) != 0 || seg.key != key) continue;
      if ((shmflg & kIpcCreat) && (shmflg & kIpcExcl)) return kEEXIST;
      if (size > seg.segsz) return kEINVAL;
      return seg.id;
    }
    if ((shmflg & kIpcCreat) == 0) return kENOENT;
  }
  if (size < 1 || size > kShmMax) return kEINVAL;
  if (static_cast<int>(s.segs.size()) >= kShmMni) return kENOSPC;
  uint64_t map_size = 0;
  if (!PageRoundUp(size, &map_size)) return kEINVAL;
  uint64_t total = map_size;
  for (const auto& kv : s.segs) total += kv.second.map_size;
  if (total / kPageSize > kShmAll) return kENOSPC;

  std::shared_ptr<VmoRef> vmo;
  if (CreateAnonVmo(map_size, &vmo) != ZX_OK) return kENOMEM;

  // Find an unused id (ids are reused only after the segment is gone).
  int id = s.next_id;
  for (int tries = 0; tries <= kShmMni; ++tries) {
    if (s.segs.find(id) == s.segs.end()) break;
    id = (id + 1) % (1 << 30);
  }
  s.next_id = (id + 1) % (1 << 30);

  ShmSegment seg;
  seg.id = id;
  seg.key = key;
  seg.segsz = size;
  seg.map_size = map_size;
  seg.vmo = vmo;
  seg.koid = 0;
  {
    zx_info_handle_basic_t info{};
    if (zx_object_get_info(vmo->handle, ZX_INFO_HANDLE_BASIC, &info,
                           sizeof(info), nullptr, nullptr) == ZX_OK) {
      seg.koid = info.koid;
    }
  }
  seg.mode = static_cast<uint32_t>(shmflg) & 0777;
  seg.seq = s.next_seq++;
  seg.cpid = proc->pid;
  seg.ctime = ShmWallSeconds();
  s.segs[id] = std::move(seg);
  return id;
}

int64_t SysShmat(LinuxProcess* proc, int shmid, uintptr_t shmaddr, int shmflg) {
  uint32_t prot = (shmflg & kShmRdonly) ? kLinuxProtRead
                                        : (kLinuxProtRead | kLinuxProtWrite);
  if (shmflg & kShmExec) prot |= kLinuxProtExec;

  std::lock_guard<std::mutex> lock(proc->map_mtx);
  MmLocked(proc);
  MemRegion r;
  size_t size = 0;
  {
    ShmState& s = Shm();
    std::lock_guard<std::mutex> slock(s.mtx);
    auto it = s.segs.find(shmid);
    if (it == s.segs.end()) return kEINVAL;
    size = it->second.map_size;
    r.vmo = it->second.vmo;
  }
  r.kind = RegionKind::kSysvShm;
  r.prot = prot;
  r.max_prot = (shmflg & kShmRdonly) ? (kLinuxProtRead | kLinuxProtExec)
                                     : kLinuxProtRWX;
  r.shmid = shmid;
  r.vmo_offset = 0;

  uintptr_t addr = 0;
  bool overwrite = false;
  if (shmaddr != 0) {
    addr = shmaddr;
    if ((addr & (kPageSize - 1)) != 0) {
      if ((shmflg & kShmRnd) == 0) return kEINVAL;
      addr = PageRoundDown(addr);  // SHMLBA == PAGE_SIZE on x86_64
    }
    if (!RangeInVmar(proc, addr, size)) return kEINVAL;
    if (shmflg & kShmRemap) {
      overwrite = true;
    } else if (!RangeIsFreeLocked(proc, addr, size)) {
      return kEINVAL;
    }
  } else {
    if (shmflg & kShmRemap) return kEINVAL;
    addr = FindFreeRangeLocked(proc, size, proc->vmar_base, VmarEnd(proc));
    if (addr == 0) return kENOMEM;
  }
  r.shm_attach_base = addr;
  zx_status_t st = MapRegionLocked(proc, addr, size, std::move(r), overwrite);
  if (st != ZX_OK) return st == ZX_ERR_ALREADY_EXISTS ? kEINVAL : kENOMEM;
  // MapRegionLocked counted the attachment; if the segment vanished
  // meanwhile (IPC_RMID with no attachments), undo.
  {
    ShmState& s = Shm();
    std::lock_guard<std::mutex> slock(s.mtx);
    if (s.segs.find(shmid) == s.segs.end()) {
      // fallthrough to unmap below
    } else {
      FlushSharedMappingsLocked(proc);
      return static_cast<int64_t>(addr);
    }
  }
  UnmapRangeLocked(proc, addr, size);
  FlushSharedMappingsLocked(proc);
  return kEINVAL;
}

int64_t SysShmdt(LinuxProcess* proc, uintptr_t shmaddr) {
  if ((shmaddr & (kPageSize - 1)) != 0) return kEINVAL;
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  auto& regions = MmLocked(proc).regions;
  auto it = regions.find(shmaddr);
  if (it == regions.end() || it->second.kind != RegionKind::kSysvShm ||
      it->second.vmo_offset != 0) {
    return kEINVAL;
  }
  const int shmid = it->second.shmid;
  const uintptr_t base = it->second.shm_attach_base;
  size_t seg_size = 0;
  {
    ShmState& s = Shm();
    std::lock_guard<std::mutex> slock(s.mtx);
    auto sit = s.segs.find(shmid);
    seg_size = (sit != s.segs.end()) ? sit->second.map_size : 0;
  }
  // Detach every piece of this attachment (it may have been split by
  // mprotect or partially unmapped).
  std::vector<std::pair<uintptr_t, size_t>> pieces;
  for (auto pit = regions.lower_bound(shmaddr); pit != regions.end(); ++pit) {
    if (seg_size != 0 && pit->first >= shmaddr + seg_size) break;
    if (pit->second.kind != RegionKind::kSysvShm || pit->second.shmid != shmid ||
        pit->second.shm_attach_base != base) {
      if (seg_size == 0) break;
      continue;
    }
    pieces.emplace_back(pit->first, pit->second.len);
  }
  for (const auto& p : pieces) UnmapRangeLocked(proc, p.first, p.second);
  FlushSharedMappingsLocked(proc);
  return 0;
}

void FillShmidDs(const ShmSegment& seg, LinuxShmidDs* out) {
  memset(out, 0, sizeof(*out));
  out->key = (seg.mode & kShmDestMode) ? kIpcPrivate : seg.key;
  out->uid = seg.uid;
  out->gid = seg.gid;
  out->cuid = seg.cuid;
  out->cgid = seg.cgid;
  out->mode = seg.mode;
  out->seq = seg.seq;
  out->shm_segsz = seg.segsz;
  out->shm_atime = seg.atime;
  out->shm_dtime = seg.dtime;
  out->shm_ctime = seg.ctime;
  out->shm_cpid = seg.cpid;
  out->shm_lpid = seg.lpid;
  out->shm_nattch = seg.nattch;
}

int64_t SysShmctl(LinuxProcess* proc, int shmid, int cmd_in, uintptr_t ubuf) {
  (void)proc;
  const int cmd = cmd_in & 0xff;  // strip IPC_64
  ShmState& s = Shm();
  std::lock_guard<std::mutex> lock(s.mtx);
  switch (cmd) {
    case kIpcInfo: {
      if (ubuf == 0) return kEFAULT;
      LinuxShmInfo64 info{};
      info.shmmax = kShmMax;
      info.shmmin = 1;
      info.shmmni = kShmMni;
      info.shmseg = kShmMni;
      info.shmall = kShmAll;
      memcpy(reinterpret_cast<void*>(ubuf), &info, sizeof(info));
      return s.segs.empty() ? 0 : s.segs.rbegin()->first;
    }
    case kShmInfo: {
      if (ubuf == 0) return kEFAULT;
      LinuxShmInfoStats info{};
      info.used_ids = static_cast<int32_t>(s.segs.size());
      for (const auto& kv : s.segs) {
        info.shm_tot += kv.second.map_size / kPageSize;
        zx_info_vmo_t vi{};
        if (zx_object_get_info(kv.second.vmo->handle, ZX_INFO_VMO, &vi,
                               sizeof(vi), nullptr, nullptr) == ZX_OK) {
          info.shm_rss += vi.committed_bytes / kPageSize;
        }
      }
      memcpy(reinterpret_cast<void*>(ubuf), &info, sizeof(info));
      return s.segs.empty() ? 0 : s.segs.rbegin()->first;
    }
    case kShmStat:
    case kShmStatAny:
    case kIpcStat: {
      // Segment ids double as SHM_STAT indices.
      auto it = s.segs.find(shmid);
      if (it == s.segs.end()) return kEINVAL;
      if (ubuf == 0) return kEFAULT;
      LinuxShmidDs ds;
      FillShmidDs(it->second, &ds);
      memcpy(reinterpret_cast<void*>(ubuf), &ds, sizeof(ds));
      return cmd == kIpcStat ? 0 : it->first;
    }
    case kIpcSet: {
      auto it = s.segs.find(shmid);
      if (it == s.segs.end()) return kEINVAL;
      if (ubuf == 0) return kEFAULT;
      LinuxShmidDs ds;
      memcpy(&ds, reinterpret_cast<const void*>(ubuf), sizeof(ds));
      it->second.uid = ds.uid;
      it->second.gid = ds.gid;
      it->second.mode = (it->second.mode & ~0777u) | (ds.mode & 0777u);
      it->second.ctime = ShmWallSeconds();
      return 0;
    }
    case kIpcRmid: {
      auto it = s.segs.find(shmid);
      if (it == s.segs.end()) return kEINVAL;
      it->second.mode |= kShmDestMode;
      it->second.ctime = ShmWallSeconds();
      ShmMaybeFreeLocked(s, it);
      return 0;
    }
    case kShmLock:
    case kShmUnlock: {
      auto it = s.segs.find(shmid);
      if (it == s.segs.end()) return kEINVAL;
      if (cmd == kShmLock) {
        // Segment VMOs are never paged out, so they are resident once
        // committed; commit them now.
        zx_vmo_op_range(it->second.vmo->handle, ZX_VMO_OP_COMMIT, 0,
                        it->second.map_size, nullptr, 0);
        it->second.mode |= kShmLockedMode;
      } else {
        it->second.mode &= ~kShmLockedMode;
      }
      return 0;
    }
    default:
      return kEINVAL;
  }
}

}  // namespace

bool ShmAdjustAttach(int shmid, int delta, int pid) {
  ShmState& s = Shm();
  std::lock_guard<std::mutex> lock(s.mtx);
  auto it = s.segs.find(shmid);
  if (it == s.segs.end()) return false;
  ShmSegment& seg = it->second;
  if (delta > 0) {
    seg.nattch += static_cast<uint64_t>(delta);
    seg.atime = ShmWallSeconds();
  } else {
    uint64_t d = static_cast<uint64_t>(-delta);
    seg.nattch = (seg.nattch > d) ? seg.nattch - d : 0;
    seg.dtime = ShmWallSeconds();
  }
  seg.lpid = pid;
  ShmMaybeFreeLocked(s, it);
  return true;
}

bool ShmLookupByKoid(zx_koid_t koid, int* shmid, std::shared_ptr<VmoRef>* vmo) {
  if (koid == ZX_KOID_INVALID) return false;
  ShmState& s = Shm();
  std::lock_guard<std::mutex> lock(s.mtx);
  for (const auto& kv : s.segs) {
    if (kv.second.koid == koid) {
      *shmid = kv.first;
      *vmo = kv.second.vmo;
      return true;
    }
  }
  return false;
}

int64_t HandleMemSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                         uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5) {
  (void)thrd;
  switch (sc) {
    case 12:  // SYS_brk
      return SysBrk(proc, static_cast<uintptr_t>(a0));
    case 9:  // SYS_mmap
      return SysMmap(proc, static_cast<uintptr_t>(a0), static_cast<size_t>(a1),
                     static_cast<uint32_t>(a2), static_cast<uint32_t>(a3),
                     static_cast<int>(a4), a5);
    case 10:  // SYS_mprotect
      return SysMprotect(proc, static_cast<uintptr_t>(a0),
                         static_cast<size_t>(a1), static_cast<uint32_t>(a2));
    case 11:  // SYS_munmap
      return SysMunmap(proc, static_cast<uintptr_t>(a0), static_cast<size_t>(a1));
    case 25:  // SYS_mremap
      return SysMremap(proc, static_cast<uintptr_t>(a0), static_cast<size_t>(a1),
                       static_cast<size_t>(a2), static_cast<uint32_t>(a3),
                       static_cast<uintptr_t>(a4));
    case 26:  // SYS_msync
      return SysMsync(proc, static_cast<uintptr_t>(a0), static_cast<size_t>(a1),
                      static_cast<int>(a2));
    case 27:  // SYS_mincore
      return SysMincore(proc, static_cast<uintptr_t>(a0),
                        static_cast<size_t>(a1), static_cast<uintptr_t>(a2));
    case 28:  // SYS_madvise
      return SysMadvise(proc, static_cast<uintptr_t>(a0),
                        static_cast<size_t>(a1), static_cast<int>(a2));
    case 149:  // SYS_mlock
      return SysMlock(proc, static_cast<uintptr_t>(a0), static_cast<size_t>(a1));
    case 150:  // SYS_munlock
      return SysMunlock(proc, static_cast<uintptr_t>(a0),
                        static_cast<size_t>(a1));
    case 151:  // SYS_mlockall
      return SysMlockall(proc, static_cast<int>(a0));
    case 152: {  // SYS_munlockall
      std::lock_guard<std::mutex> lock(proc->map_mtx);
      MmLocked(proc).lock_future = false;
      return 0;
    }
    case 319:  // SYS_memfd_create
      return SysMemfdCreate(proc, static_cast<uintptr_t>(a0),
                            static_cast<uint32_t>(a1));
    case 29:  // SYS_shmget
      return SysShmget(proc, static_cast<int32_t>(a0), static_cast<size_t>(a1),
                       static_cast<int>(a2));
    case 30:  // SYS_shmat
      return SysShmat(proc, static_cast<int>(a0), static_cast<uintptr_t>(a1),
                      static_cast<int>(a2));
    case 31:  // SYS_shmctl
      return SysShmctl(proc, static_cast<int>(a0), static_cast<int>(a1),
                       static_cast<uintptr_t>(a2));
    case 67:  // SYS_shmdt
      return SysShmdt(proc, static_cast<uintptr_t>(a0));
    default:
      return kENOSYS;
  }
}
