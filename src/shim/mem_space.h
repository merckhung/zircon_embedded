#ifndef ZIRCON_EMBEDDED_SRC_SHIM_MEM_SPACE_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_MEM_SPACE_H_

// Per-process bookkeeping for the Linux address space that lives in a
// process's restricted VMAR (LinuxProcess::restricted_vmar).
//
// Every mapping the shim creates in the restricted VMAR (ELF segments, the
// initial stack, brk, mmap, shmat) is recorded here as a MemRegion together
// with the VMO that backs it. The table is the source of truth for:
//   * free-range allocation (mmap without MAP_FIXED, mremap moves),
//   * MAP_FIXED_NOREPLACE / shmat overlap checks,
//   * mprotect/madvise/mincore/msync range validation (-ENOMEM on holes),
//   * fork (CloneProcessAddressSpace) and SysV shm attach counts.
//
// Locking: all *Locked functions require proc->map_mtx to be held. SysV shm
// state (syscalls_mem.cc) has its own mutex which may be acquired while
// map_mtx is held, never the other way round.

#include "src/shim/shim_common.h"

// Linux PROT_* bits.
constexpr uint32_t kLinuxProtRead = 0x1;
constexpr uint32_t kLinuxProtWrite = 0x2;
constexpr uint32_t kLinuxProtExec = 0x4;
constexpr uint32_t kLinuxProtRWX =
    kLinuxProtRead | kLinuxProtWrite | kLinuxProtExec;

constexpr uintptr_t kPageSize = 4096;

inline uintptr_t PageRoundDown(uintptr_t v) { return v & ~(kPageSize - 1); }
// Returns false on overflow.
inline bool PageRoundUp(uint64_t v, uint64_t* out) {
  if (v > UINT64_MAX - (kPageSize - 1)) return false;
  *out = (v + kPageSize - 1) & ~(kPageSize - 1);
  return true;
}

// Owns one Zircon VMO handle. Shared by every region (possibly in several
// processes) that maps part of the same VMO; the handle is closed when the
// last reference goes away.
struct VmoRef {
  explicit VmoRef(zx_handle_t h) : handle(h) {}
  ~VmoRef() {
    if (handle != ZX_HANDLE_INVALID) zx_handle_close(handle);
  }
  VmoRef(const VmoRef&) = delete;
  VmoRef& operator=(const VmoRef&) = delete;
  zx_handle_t handle;
};

enum class RegionKind : uint8_t {
  kAnonPrivate,  // MAP_PRIVATE|MAP_ANONYMOUS, brk, stack
  kFilePrivate,  // MAP_PRIVATE file mapping or ELF PT_LOAD (private copy)
  kAnonShared,   // MAP_SHARED|MAP_ANONYMOUS or MAP_SHARED of /dev/zero
  kFileShared,   // MAP_SHARED of a VFS file (maps VfsNode::shared_vmo)
  kSysvShm,      // shmat() attachment
  kDevice,       // /dev/fb0 framebuffer
};

inline bool IsSharedKind(RegionKind k) {
  return k == RegionKind::kAnonShared || k == RegionKind::kFileShared ||
         k == RegionKind::kSysvShm || k == RegionKind::kDevice;
}

struct MemRegion {
  size_t len = 0;
  uint32_t prot = 0;                 // current Linux PROT_* bits
  uint32_t max_prot = kLinuxProtRWX;  // mprotect beyond this -> -EACCES
  RegionKind kind = RegionKind::kAnonPrivate;
  // VMO mapped at this region. Null only for regions adopted from the kernel
  // mapping list (see AdoptUntrackedKernelMappingsLocked), whose VMO handle
  // the shim does not hold.
  std::shared_ptr<VmoRef> vmo;
  uint64_t vmo_offset = 0;  // VMO offset that corresponds to the region start
  // File backing (kFilePrivate, kFileShared). VfsNodes are owned by the VFS;
  // node_owner additionally keeps anonymous (memfd) nodes alive.
  VfsNode* node = nullptr;
  std::shared_ptr<VfsNode> node_owner;
  // kFilePrivate: file offset of the region start and how many bytes from the
  // region start are file-backed (the rest is anonymous zero fill, e.g. bss).
  uint64_t file_offset = 0;
  uint64_t file_len = 0;
  // kSysvShm
  int shmid = -1;
  uintptr_t shm_attach_base = 0;
  // madvise(MADV_DONTFORK / MADV_WIPEONFORK)
  bool dont_fork = false;
  bool wipe_on_fork = false;
};

struct AddressSpaceState {
  int pid = 0;
  std::map<uintptr_t, MemRegion> regions;  // keyed by start address
  bool shared_dirty = false;  // LinuxProcess::shared_mappings needs rebuild
  bool lock_future = false;   // mlockall(MCL_FUTURE)
  // Drops SysV shm attach counts for any regions still recorded.
  ~AddressSpaceState();
};

// Returns the process's table, creating it (and adopting whatever the kernel
// already has mapped in the restricted VMAR) if it does not exist yet.
AddressSpaceState& MmLocked(LinuxProcess* proc);

uintptr_t VmarEnd(const LinuxProcess* proc);
bool RangeInVmar(const LinuxProcess* proc, uintptr_t addr, size_t len);
bool RangeIsFreeLocked(LinuxProcess* proc, uintptr_t addr, size_t len);
bool RangeFullyMappedLocked(LinuxProcess* proc, uintptr_t addr, size_t len);

// Finds a free, page-aligned range of `len` bytes inside [lo, hi), preferring
// the highest addresses (top-down, like Linux). Returns 0 if none.
uintptr_t FindFreeRangeLocked(LinuxProcess* proc, size_t len, uintptr_t lo,
                              uintptr_t hi);

// Linux PROT_* -> ZX_VM_PERM_*. x86 cannot express write-only or execute-only
// pages, so PROT_WRITE / PROT_EXEC imply PROT_READ (as on Linux/x86).
zx_vm_option_t ProtToVmPerms(uint32_t prot);

// Gives `vmo` ZX_RIGHT_EXECUTE so that mappings of it can be made executable
// now or later via mprotect. Consumes `vmo`; returns the new handle or
// ZX_HANDLE_INVALID on failure.
zx_handle_t VmoMakeExecutable(zx_handle_t vmo);

// Creates a zero-filled anonymous VMO (with ZX_RIGHT_EXECUTE).
zx_status_t CreateAnonVmo(size_t len, std::shared_ptr<VmoRef>* out);

// Maps region.vmo at [addr, addr + len) with region.prot and records it.
// overwrite=false uses ZX_VM_SPECIFIC (fails if anything is mapped there);
// overwrite=true atomically replaces existing mappings (MAP_FIXED).
zx_status_t MapRegionLocked(LinuxProcess* proc, uintptr_t addr, size_t len,
                            MemRegion region, bool overwrite);

// Unmaps every mapped piece of [addr, addr + len) and drops it from the table.
void UnmapRangeLocked(LinuxProcess* proc, uintptr_t addr, size_t len);

// mprotect core. Returns 0 or a negative Linux errno.
int64_t ProtectRangeLocked(LinuxProcess* proc, uintptr_t addr, size_t len,
                           uint32_t prot);

// Splits regions so that `addr` is a region boundary (if it is inside one).
void SplitRegionAtLocked(LinuxProcess* proc, uintptr_t addr);

// Records kernel mappings in the restricted VMAR that the table does not
// know about (e.g. inherited through a fork path that did not copy the
// table). Their VMO handle is unknown, so they are recorded with vmo=null.
void AdoptUntrackedKernelMappingsLocked(LinuxProcess* proc);

// Rebuilds LinuxProcess::shared_mappings from the table if it changed.
void FlushSharedMappingsLocked(LinuxProcess* proc);

// Unmaps the whole restricted VMAR and clears all bookkeeping (execve, exit).
void TearDownAddressSpaceLocked(LinuxProcess* proc);

// Erases one table entry (adjusting SysV shm attach counts). No kernel op.
void EraseRegionLocked(LinuxProcess* proc,
                       std::map<uintptr_t, MemRegion>::iterator it);
// Inserts one table entry (adjusting SysV shm attach counts). No kernel op.
void InsertRegionLocked(LinuxProcess* proc, uintptr_t addr, MemRegion region);

// SysV shm hooks implemented in syscalls_mem.cc. ShmAdjustAttach changes a
// segment's attach count by `delta` (one per mapped region, like Linux's
// per-VMA shm_nattch) and returns false if the segment no longer exists.
bool ShmAdjustAttach(int shmid, int delta, int pid);
// Finds the SysV shm segment whose VMO has koid `koid`.
bool ShmLookupByKoid(zx_koid_t koid, int* shmid, std::shared_ptr<VmoRef>* vmo);

// Builds a private copy of a region that has no VMO handle (vmo == null) by
// reading its pages through the mapping at `start`. Must run on a thread of
// `proc`'s Zircon process. Non-readable pages are made readable for the copy
// and then restored.
zx_status_t CopyRegionThroughMappingLocked(LinuxProcess* proc, uintptr_t start,
                                           const MemRegion& r,
                                           std::shared_ptr<VmoRef>* out);

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_MEM_SPACE_H_
