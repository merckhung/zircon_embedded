// Restricted-VMAR mapping table, free-range allocator and fork support.
// See mem_space.h for the design.

#include "src/shim/mem_space.h"

#include <algorithm>
#include <unordered_map>

#include "src/shim/vfs.h"

namespace {

// Non-MAP_FIXED mmaps are never placed in the gap just below the initial
// stack, so that overflowing the (fixed-size) stack faults instead of
// silently running into an unrelated mapping. Must match elf_loader.cc.
constexpr uintptr_t kStackGuardTop = 0x0000003ff0000000ULL;  // kStackBottom
constexpr uintptr_t kStackGuardGap = 256 * kPageSize;        // Linux default

zx_koid_t KoidOf(zx_handle_t h) {
  zx_info_handle_basic_t info{};
  if (zx_object_get_info(h, ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr,
                         nullptr) != ZX_OK) {
    return ZX_KOID_INVALID;
  }
  return info.koid;
}

uint32_t VmFlagsToProt(zx_vm_option_t f) {
  uint32_t p = 0;
  if (f & ZX_VM_PERM_READ) p |= kLinuxProtRead;
  if (f & ZX_VM_PERM_WRITE) p |= kLinuxProtWrite;
  if (f & ZX_VM_PERM_EXECUTE) p |= kLinuxProtExec;
  return p;
}

// Produces a copy of `r` describing only [r_start + delta, r_start + delta +
// len).
MemRegion SubRegion(const MemRegion& r, uint64_t delta, size_t len) {
  MemRegion out = r;
  out.len = len;
  out.vmo_offset = r.vmo_offset + delta;
  if (r.kind == RegionKind::kFilePrivate) {
    out.file_offset = r.file_offset + delta;
    out.file_len = (r.file_len > delta) ? std::min<uint64_t>(r.file_len - delta, len)
                                        : 0;
  }
  return out;
}

std::vector<zx_info_maps_t> QueryKernelMaps(zx_handle_t zx_proc) {
  std::vector<zx_info_maps_t> maps(256);
  for (int attempt = 0; attempt < 4; ++attempt) {
    size_t actual = 0, avail = 0;
    zx_status_t st = zx_object_get_info(zx_proc, ZX_INFO_PROCESS_MAPS,
                                        maps.data(),
                                        maps.size() * sizeof(zx_info_maps_t),
                                        &actual, &avail);
    if (st != ZX_OK) return {};
    if (actual == avail) {
      maps.resize(actual);
      return maps;
    }
    maps.resize(avail + 64);
  }
  return {};
}

// Copies `len` bytes of VMO `src` at `src_off` into VMO `dst` at 0.
zx_status_t CopyVmoRange(zx_handle_t src, uint64_t src_off, zx_handle_t dst,
                         size_t len) {
  constexpr size_t kChunk = 256 * 1024;
  std::vector<uint8_t> buf(std::min(len, kChunk));
  for (size_t done = 0; done < len;) {
    size_t n = std::min(kChunk, len - done);
    zx_status_t st = zx_vmo_read(src, buf.data(), src_off + done, n);
    if (st != ZX_OK) return st;
    st = zx_vmo_write(dst, buf.data(), done, n);
    if (st != ZX_OK) return st;
    done += n;
  }
  return ZX_OK;
}

}  // namespace

zx_status_t CopyRegionThroughMappingLocked(LinuxProcess* proc, uintptr_t start,
                                           const MemRegion& r,
                                           std::shared_ptr<VmoRef>* out) {
  zx_status_t st = CreateAnonVmo(r.len, out);
  if (st != ZX_OK) return st;
  bool unreadable = (r.prot & kLinuxProtRead) == 0;
  if (unreadable) {
    st = zx_vmar_protect(proc->restricted_vmar, ZX_VM_PERM_READ, start, r.len);
    if (st != ZX_OK) return st;
  }
  st = zx_vmo_write((*out)->handle, reinterpret_cast<const void*>(start), 0,
                    r.len);
  if (unreadable) {
    zx_vmar_protect(proc->restricted_vmar, ProtToVmPerms(r.prot), start, r.len);
  }
  return st;
}

AddressSpaceState::~AddressSpaceState() {
  for (const auto& kv : regions) {
    if (kv.second.kind == RegionKind::kSysvShm) {
      ShmAdjustAttach(kv.second.shmid, -1, pid);
    }
  }
}

uintptr_t VmarEnd(const LinuxProcess* proc) {
  return proc->vmar_base + proc->vmar_len;
}

bool RangeInVmar(const LinuxProcess* proc, uintptr_t addr, size_t len) {
  if (len == 0) return false;
  if (addr < proc->vmar_base) return false;
  if (addr > VmarEnd(proc) || len > VmarEnd(proc) - addr) return false;
  return true;
}

AddressSpaceState& MmLocked(LinuxProcess* proc) {
  if (!proc->mm) {
    proc->mm = std::make_shared<AddressSpaceState>();
    proc->mm->pid = proc->pid;
    // A process whose table was not created by LoadLinuxExecutable or
    // CloneProcessAddressSpace (e.g. a child created by a fork path that only
    // copied pages) still has kernel mappings; record them so the allocator
    // never hands out occupied addresses.
    AdoptUntrackedKernelMappingsLocked(proc);
  }
  proc->mm->pid = proc->pid;
  return *proc->mm;
}

bool RangeIsFreeLocked(LinuxProcess* proc, uintptr_t addr, size_t len) {
  auto& regions = MmLocked(proc).regions;
  uintptr_t end = addr + len;
  auto it = regions.lower_bound(addr);
  if (it != regions.end() && it->first < end) return false;
  if (it != regions.begin()) {
    auto prev = std::prev(it);
    if (prev->first + prev->second.len > addr) return false;
  }
  return true;
}

bool RangeFullyMappedLocked(LinuxProcess* proc, uintptr_t addr, size_t len) {
  auto& regions = MmLocked(proc).regions;
  uintptr_t cur = addr;
  uintptr_t end = addr + len;
  auto it = regions.upper_bound(addr);
  if (it != regions.begin()) --it;
  for (; it != regions.end() && cur < end; ++it) {
    uintptr_t rs = it->first;
    uintptr_t re = rs + it->second.len;
    if (re <= cur) continue;
    if (rs > cur) return false;
    cur = re;
  }
  return cur >= end;
}

uintptr_t FindFreeRangeLocked(LinuxProcess* proc, size_t len, uintptr_t lo,
                              uintptr_t hi) {
  auto& regions = MmLocked(proc).regions;
  lo = std::max<uintptr_t>(lo, proc->vmar_base);
  hi = std::min<uintptr_t>(hi, VmarEnd(proc));
  if (len == 0 || hi <= lo || hi - lo < len) return 0;

  const uintptr_t guard_lo = kStackGuardTop - kStackGuardGap;
  const uintptr_t guard_hi = kStackGuardTop;
  // Highest page-aligned placement of `len` bytes in [gap_lo, gap_hi) that
  // avoids the stack guard window, or 0.
  auto place = [&](uintptr_t gap_lo, uintptr_t gap_hi) -> uintptr_t {
    gap_lo = std::max(gap_lo, lo);
    gap_hi = std::min(gap_hi, hi);
    if (gap_hi <= gap_lo || gap_hi - gap_lo < len) return 0;
    uintptr_t cand = PageRoundDown(gap_hi - len);
    if (cand < gap_lo) return 0;
    if (cand < guard_hi && cand + len > guard_lo) {
      if (guard_lo < gap_lo || guard_lo - gap_lo < len) return 0;
      cand = PageRoundDown(guard_lo - len);
      if (cand < gap_lo) return 0;
    }
    return cand;
  };

  uintptr_t cursor = hi;
  auto it = regions.lower_bound(hi);
  // A region starting below hi may still extend past it.
  while (it != regions.begin()) {
    --it;
    uintptr_t rs = it->first;
    uintptr_t re = rs + it->second.len;
    if (re < cursor) {
      uintptr_t cand = place(re, cursor);
      if (cand != 0) return cand;
    }
    cursor = std::min(cursor, rs);
    if (cursor <= lo) return 0;
  }
  return place(lo, cursor);
}

zx_vm_option_t ProtToVmPerms(uint32_t prot) {
  zx_vm_option_t f = 0;
  if (prot & (kLinuxProtRead | kLinuxProtWrite | kLinuxProtExec)) {
    f |= ZX_VM_PERM_READ;
  }
  if (prot & kLinuxProtWrite) f |= ZX_VM_PERM_WRITE;
  if (prot & kLinuxProtExec) f |= ZX_VM_PERM_EXECUTE;
  return f;
}

zx_handle_t VmoMakeExecutable(zx_handle_t vmo) {
  zx_handle_t out = ZX_HANDLE_INVALID;
  if (zx_vmo_replace_as_executable(vmo, g_vmex_rsrc, &out) != ZX_OK) {
    return ZX_HANDLE_INVALID;
  }
  return out;
}

zx_status_t CreateAnonVmo(size_t len, std::shared_ptr<VmoRef>* out) {
  zx_handle_t vmo = ZX_HANDLE_INVALID;
  zx_status_t st = zx_vmo_create(len, 0, &vmo);
  if (st != ZX_OK) return st;
  vmo = VmoMakeExecutable(vmo);
  if (vmo == ZX_HANDLE_INVALID) return ZX_ERR_ACCESS_DENIED;
  *out = std::make_shared<VmoRef>(vmo);
  return ZX_OK;
}

void EraseRegionLocked(LinuxProcess* proc,
                       std::map<uintptr_t, MemRegion>::iterator it) {
  auto& mm = MmLocked(proc);
  if (IsSharedKind(it->second.kind)) mm.shared_dirty = true;
  if (it->second.kind == RegionKind::kSysvShm) {
    ShmAdjustAttach(it->second.shmid, -1, proc->pid);
  }
  mm.regions.erase(it);
}

void InsertRegionLocked(LinuxProcess* proc, uintptr_t addr, MemRegion region) {
  auto& mm = MmLocked(proc);
  if (IsSharedKind(region.kind)) mm.shared_dirty = true;
  if (region.kind == RegionKind::kSysvShm) {
    ShmAdjustAttach(region.shmid, +1, proc->pid);
  }
  mm.regions[addr] = std::move(region);
}

void SplitRegionAtLocked(LinuxProcess* proc, uintptr_t addr) {
  auto& regions = MmLocked(proc).regions;
  auto it = regions.upper_bound(addr);
  if (it == regions.begin()) return;
  --it;
  uintptr_t rs = it->first;
  MemRegion& r = it->second;
  if (addr <= rs || addr >= rs + r.len) return;
  uint64_t delta = addr - rs;
  MemRegion upper = SubRegion(r, delta, r.len - delta);
  r = SubRegion(r, 0, delta);
  // A split creates one more region (VMA); for SysV shm that is one more
  // attachment, mirroring Linux's per-VMA shm_nattch accounting.
  InsertRegionLocked(proc, addr, std::move(upper));
}

// Removes [addr, addr + len) from the table only.
static void RemoveRangeFromTableLocked(LinuxProcess* proc, uintptr_t addr,
                                       size_t len) {
  SplitRegionAtLocked(proc, addr);
  SplitRegionAtLocked(proc, addr + len);
  auto& regions = MmLocked(proc).regions;
  auto it = regions.lower_bound(addr);
  while (it != regions.end() && it->first < addr + len) {
    auto next = std::next(it);
    EraseRegionLocked(proc, it);
    it = next;
  }
}

zx_status_t MapRegionLocked(LinuxProcess* proc, uintptr_t addr, size_t len,
                            MemRegion region, bool overwrite) {
  if (!region.vmo) return ZX_ERR_INVALID_ARGS;
  if (!RangeInVmar(proc, addr, len)) return ZX_ERR_OUT_OF_RANGE;
  auto& mm = MmLocked(proc);
  zx_vm_option_t opts = ProtToVmPerms(region.prot) |
                        (overwrite ? ZX_VM_SPECIFIC_OVERWRITE : ZX_VM_SPECIFIC);
  uintptr_t mapped = 0;
  zx_status_t st =
      zx_vmar_map(proc->restricted_vmar, opts, addr - proc->vmar_base,
                  region.vmo->handle, region.vmo_offset, len, &mapped);
  if (st != ZX_OK) return st;
  if (overwrite) {
    RemoveRangeFromTableLocked(proc, addr, len);
  }
  if (mm.lock_future && region.kind != RegionKind::kDevice) {
    // mlockall(MCL_FUTURE): populate now; Zircon never pages anonymous
    // memory out to a backing store.
    zx_vmar_op_range(proc->restricted_vmar, ZX_VMAR_OP_COMMIT, addr, len,
                     nullptr, 0);
  }
  region.len = len;
  InsertRegionLocked(proc, addr, std::move(region));
  return ZX_OK;
}

void UnmapRangeLocked(LinuxProcess* proc, uintptr_t addr, size_t len) {
  auto& regions = MmLocked(proc).regions;
  uintptr_t end = addr + len;
  auto it = regions.upper_bound(addr);
  if (it != regions.begin()) --it;
  for (; it != regions.end() && it->first < end; ++it) {
    uintptr_t rs = std::max<uintptr_t>(it->first, addr);
    uintptr_t re = std::min<uintptr_t>(it->first + it->second.len, end);
    if (re <= rs) continue;
    zx_status_t st = zx_vmar_unmap(proc->restricted_vmar, rs, re - rs);
    if (st != ZX_OK) {
      ShimLogf("[mem] zx_vmar_unmap(0x%llx, 0x%llx) failed: %d\n",
               static_cast<unsigned long long>(rs),
               static_cast<unsigned long long>(re - rs), st);
    }
  }
  RemoveRangeFromTableLocked(proc, addr, len);
}

int64_t ProtectRangeLocked(LinuxProcess* proc, uintptr_t addr, size_t len,
                           uint32_t prot) {
  auto& regions = MmLocked(proc).regions;
  if (!RangeFullyMappedLocked(proc, addr, len)) return kENOMEM;
  uintptr_t end = addr + len;
  // Linux checks VM_MAY* for every VMA before changing anything.
  {
    auto it = regions.upper_bound(addr);
    if (it != regions.begin()) --it;
    for (; it != regions.end() && it->first < end; ++it) {
      if (it->first + it->second.len <= addr) continue;
      if ((prot & ~it->second.max_prot) != 0) return kEACCES;
    }
  }
  zx_status_t st = zx_vmar_protect(proc->restricted_vmar, ProtToVmPerms(prot),
                                   addr, len);
  if (st == ZX_ERR_NOT_FOUND) return kENOMEM;
  if (st == ZX_ERR_ACCESS_DENIED) return kEACCES;
  if (st != ZX_OK) return kEINVAL;
  SplitRegionAtLocked(proc, addr);
  SplitRegionAtLocked(proc, end);
  for (auto it = regions.lower_bound(addr);
       it != regions.end() && it->first < end; ++it) {
    it->second.prot = prot;
    if (IsSharedKind(it->second.kind)) MmLocked(proc).shared_dirty = true;
  }
  return 0;
}

void AdoptUntrackedKernelMappingsLocked(LinuxProcess* proc) {
  auto& mm = *proc->mm;  // created by the caller (MmLocked)
  std::vector<zx_info_maps_t> maps = QueryKernelMaps(proc->zx_proc);
  if (maps.empty()) return;

  // Identify shared VMOs by koid so adopted shared regions keep sharing.
  struct SharedSrc {
    zx_handle_t vmo;
    VfsNode* node;
  };
  std::unordered_map<zx_koid_t, SharedSrc> shared_by_koid;
  for (const auto& sm : proc->shared_mappings) {
    if (sm.vmo == ZX_HANDLE_INVALID) continue;
    zx_koid_t k = KoidOf(sm.vmo);
    if (k != ZX_KOID_INVALID) shared_by_koid[k] = SharedSrc{sm.vmo, sm.backing_node};
  }
  zx_koid_t fb_koid = ZX_KOID_INVALID;
  if (GetFbHardwareInfo().phys_vmo != ZX_HANDLE_INVALID) {
    fb_koid = KoidOf(GetFbHardwareInfo().phys_vmo);
  }

  for (const auto& m : maps) {
    if (m.type != ZX_INFO_MAPS_TYPE_MAPPING) continue;
    if (m.base < proc->vmar_base || m.base + m.size > VmarEnd(proc)) continue;
    // Walk the parts of [m.base, m.base + m.size) not covered by the table.
    uintptr_t cur = m.base;
    const uintptr_t mend = m.base + m.size;
    while (cur < mend) {
      auto it = mm.regions.upper_bound(cur);
      uintptr_t gap_end = (it == mm.regions.end()) ? mend : std::min(mend, it->first);
      if (it != mm.regions.begin()) {
        auto prev = std::prev(it);
        uintptr_t pend = prev->first + prev->second.len;
        if (pend > cur) {
          cur = pend;
          continue;
        }
      }
      if (gap_end <= cur) {
        cur = (it == mm.regions.end()) ? mend : it->first + it->second.len;
        continue;
      }
      MemRegion r;
      r.len = gap_end - cur;
      r.prot = VmFlagsToProt(m.u.mapping.mmu_flags);
      r.vmo_offset = m.u.mapping.vmo_offset + (cur - m.base);
      auto sit = shared_by_koid.find(m.u.mapping.vmo_koid);
      int shmid = -1;
      std::shared_ptr<VmoRef> shm_vmo;
      if (sit != shared_by_koid.end()) {
        zx_handle_t dup = ZX_HANDLE_INVALID;
        zx_handle_duplicate(sit->second.vmo, ZX_RIGHT_SAME_RIGHTS, &dup);
        r.vmo = std::make_shared<VmoRef>(dup);
        r.node = sit->second.node;
        if (m.u.mapping.vmo_koid == fb_koid) {
          r.kind = RegionKind::kDevice;
        } else {
          r.kind = sit->second.node ? RegionKind::kFileShared
                                    : RegionKind::kAnonShared;
        }
      } else if (ShmLookupByKoid(m.u.mapping.vmo_koid, &shmid, &shm_vmo)) {
        r.kind = RegionKind::kSysvShm;
        r.shmid = shmid;
        r.vmo = shm_vmo;
        r.shm_attach_base = cur - r.vmo_offset;
      } else {
        // Private memory whose VMO handle the shim does not hold.
        r.kind = RegionKind::kAnonPrivate;
      }
      InsertRegionLocked(proc, cur, std::move(r));
      cur = gap_end;
    }
  }
}

void FlushSharedMappingsLocked(LinuxProcess* proc) {
  auto& mm = MmLocked(proc);
  if (!mm.shared_dirty) return;
  mm.shared_dirty = false;
  for (auto& sm : proc->shared_mappings) {
    if (sm.vmo != ZX_HANDLE_INVALID) zx_handle_close(sm.vmo);
  }
  proc->shared_mappings.clear();
  // Adjacent pieces of one VMO are coalesced so that a kernel mapping that
  // spans several table entries is still contained in one SharedMapping.
  const VmoRef* last_vmo = nullptr;
  for (const auto& kv : mm.regions) {
    const MemRegion& r = kv.second;
    if (!IsSharedKind(r.kind) || !r.vmo) {
      last_vmo = nullptr;
      continue;
    }
    if (last_vmo == r.vmo.get() && !proc->shared_mappings.empty()) {
      SharedMapping& prev = proc->shared_mappings.back();
      if (prev.vaddr + prev.size == kv.first &&
          prev.vmo_offset + prev.size == r.vmo_offset) {
        prev.size += r.len;
        prev.prot |= r.prot;
        continue;
      }
    }
    zx_handle_t dup = ZX_HANDLE_INVALID;
    if (zx_handle_duplicate(r.vmo->handle, ZX_RIGHT_SAME_RIGHTS, &dup) != ZX_OK) {
      last_vmo = nullptr;
      continue;
    }
    proc->shared_mappings.push_back(
        SharedMapping{kv.first, r.len, r.prot, dup, r.vmo_offset, r.node,
                      KoidOf(dup)});
    last_vmo = r.vmo.get();
  }
}

void TearDownAddressSpaceLocked(LinuxProcess* proc) {
  if (proc->restricted_vmar != ZX_HANDLE_INVALID && proc->vmar_len != 0) {
    zx_vmar_unmap(proc->restricted_vmar, proc->vmar_base, proc->vmar_len);
  }
  if (proc->mm) {
    auto& regions = proc->mm->regions;
    while (!regions.empty()) EraseRegionLocked(proc, regions.begin());
    proc->mm->shared_dirty = false;
    proc->mm->lock_future = false;
  } else {
    proc->mm = std::make_shared<AddressSpaceState>();
    proc->mm->pid = proc->pid;
  }
  for (auto& sm : proc->shared_mappings) {
    if (sm.vmo != ZX_HANDLE_INVALID) zx_handle_close(sm.vmo);
  }
  proc->shared_mappings.clear();
}

void ReleaseProcessAddressSpace(LinuxProcess* proc) {
  std::lock_guard<std::mutex> lock(proc->map_mtx);
  TearDownAddressSpaceLocked(proc);
}

bool CloneProcessAddressSpace(LinuxProcess* parent, LinuxProcess* child) {
  std::lock_guard<std::mutex> plock(parent->map_mtx);
  auto& pmm = MmLocked(parent);
  std::lock_guard<std::mutex> clock(child->map_mtx);
  child->mm = std::make_shared<AddressSpaceState>();
  child->mm->pid = child->pid;
  // mlockall(MCL_FUTURE) is not inherited across fork (Linux semantics).

  for (const auto& kv : pmm.regions) {
    const uintptr_t start = kv.first;
    const MemRegion& r = kv.second;
    if (r.dont_fork) continue;

    MemRegion c = r;
    if (IsSharedKind(r.kind)) {
      // Same VMO: parent and child keep sharing the pages.
      if (!c.vmo) continue;  // cannot happen: shared regions carry a handle
    } else if (r.wipe_on_fork) {
      if (CreateAnonVmo(r.len, &c.vmo) != ZX_OK) return false;
      c.vmo_offset = 0;
    } else if (r.vmo) {
      // Copy-on-write snapshot of exactly this region's pages.
      zx_handle_t snap = ZX_HANDLE_INVALID;
      zx_status_t st = zx_vmo_create_child(r.vmo->handle, ZX_VMO_CHILD_SNAPSHOT,
                                           r.vmo_offset, r.len, &snap);
      if (st == ZX_OK) {
        snap = VmoMakeExecutable(snap);
        if (snap == ZX_HANDLE_INVALID) return false;
        c.vmo = std::make_shared<VmoRef>(snap);
      } else {
        // Snapshots are unsupported for some VMOs; fall back to an eager copy.
        std::shared_ptr<VmoRef> copy;
        if (CreateAnonVmo(r.len, &copy) != ZX_OK) return false;
        if (CopyVmoRange(r.vmo->handle, r.vmo_offset, copy->handle, r.len) != ZX_OK) {
          return false;
        }
        c.vmo = copy;
      }
      c.vmo_offset = 0;
    } else {
      // Adopted region without a VMO handle: copy through the mapping.
      std::shared_ptr<VmoRef> copy;
      if (CopyRegionThroughMappingLocked(parent, start, r, &copy) != ZX_OK) {
        ShimLogf("[fork] copy of 0x%llx (+0x%llx) failed\n",
                 static_cast<unsigned long long>(start),
                 static_cast<unsigned long long>(r.len));
        return false;
      }
      c.vmo = copy;
      c.vmo_offset = 0;
    }
    c.wipe_on_fork = r.wipe_on_fork;
    zx_status_t st = MapRegionLocked(child, start, r.len, std::move(c),
                                     /*overwrite=*/false);
    if (st != ZX_OK) {
      ShimLogf("[fork] map of 0x%llx (+0x%llx) failed: %d\n",
               static_cast<unsigned long long>(start),
               static_cast<unsigned long long>(r.len), st);
      return false;
    }
  }
  FlushSharedMappingsLocked(child);
  return true;
}
