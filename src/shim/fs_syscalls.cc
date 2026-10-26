// HandleFsSyscall: Linux file-system / fd syscalls.

#include <algorithm>

#include "src/shim/fs_internal.h"
#include "src/shim/net_inet.h"
#include "src/shim/tty.h"

namespace {

constexpr int64_t kUtimeNow = (1LL << 30) - 1;
constexpr int64_t kUtimeOmit = (1LL << 30) - 2;

std::string UserPath(const char* p) { return p ? std::string(p) : std::string(); }

int64_t ResolveUser(LinuxProcess* proc, int dirfd, const char* p,
                    std::string* out) {
  if (!p) return kEFAULT;
  if (p[0] == '\0') return kENOENT;
  return ResolveAt(proc, dirfd, p, out);
}

// Gather/scatter helpers for the vector I/O syscalls.
int64_t IovTotal(const LinuxIovec* iov, int iovcnt, size_t* total) {
  if (iovcnt < 0 || iovcnt > 1024) return kEINVAL;
  if (iovcnt > 0 && !iov) return kEFAULT;
  size_t t = 0;
  for (int i = 0; i < iovcnt; ++i) {
    if (iov[i].iov_len > static_cast<size_t>(INT64_MAX) - t) return kEINVAL;
    t += iov[i].iov_len;
  }
  *total = std::min<size_t>(t, 0x7ffff000);  // Linux MAX_RW_COUNT
  return 0;
}

void Scatter(const LinuxIovec* iov, int iovcnt, const uint8_t* src, size_t n) {
  for (int i = 0; i < iovcnt && n > 0; ++i) {
    size_t c = std::min(n, iov[i].iov_len);
    memcpy(iov[i].iov_base, src, c);
    src += c;
    n -= c;
  }
}

std::vector<uint8_t> Gather(const LinuxIovec* iov, int iovcnt, size_t total) {
  std::vector<uint8_t> buf(total);
  size_t off = 0;
  for (int i = 0; i < iovcnt && off < total; ++i) {
    size_t c = std::min(total - off, iov[i].iov_len);
    memcpy(buf.data() + off, iov[i].iov_base, c);
    off += c;
  }
  return buf;
}

int64_t DoStatx(LinuxProcess* proc, int dirfd, const char* path, int flags,
                uint32_t mask, LinuxStatx* stx) {
  (void)mask;
  if (!stx) return kEFAULT;
  constexpr int kAtStatxSyncType = 0x6000;
  if (flags & ~(kLinuxAT_SYMLINK_NOFOLLOW | kLinuxAT_EMPTY_PATH | 0x800 |
                kAtStatxSyncType)) {
    return kEINVAL;
  }
  if ((flags & kAtStatxSyncType) == kAtStatxSyncType) return kEINVAL;
  LinuxStat64 st{};
  int64_t rc;
  if ((flags & kLinuxAT_EMPTY_PATH) && path && path[0] == '\0') {
    if (dirfd == kLinuxAT_FDCWD) {
      rc = StatPath(proc, proc->cwd, true, &st);
    } else {
      auto d = LookupFd(proc, dirfd);
      if (!d) return kEBADF;
      FillStatFromDesc(proc, d.get(), &st);
      rc = 0;
    }
  } else {
    std::string norm;
    rc = ResolveUser(proc, dirfd, path, &norm);
    if (rc < 0) return rc;
    rc = StatPath(proc, norm, (flags & kLinuxAT_SYMLINK_NOFOLLOW) == 0, &st);
  }
  if (rc < 0) return rc;
  memset(stx, 0, sizeof(*stx));
  // STATX_BASIC_STATS (type, mode, nlink, uid, gid, atime, mtime, ctime, ino,
  // size, blocks) are all filled; birth time is not tracked.
  stx->stx_mask = 0x7ff;
  stx->stx_blksize = static_cast<uint32_t>(st.st_blksize);
  stx->stx_nlink = static_cast<uint32_t>(st.st_nlink);
  stx->stx_uid = st.st_uid;
  stx->stx_gid = st.st_gid;
  stx->stx_mode = static_cast<uint16_t>(st.st_mode);
  stx->stx_ino = st.st_ino;
  stx->stx_size = static_cast<uint64_t>(st.st_size);
  stx->stx_blocks = static_cast<uint64_t>(st.st_blocks);
  stx->stx_atime = {static_cast<int64_t>(st.st_atime_sec),
                    static_cast<uint32_t>(st.st_atime_nsec), 0};
  stx->stx_mtime = {static_cast<int64_t>(st.st_mtime_sec),
                    static_cast<uint32_t>(st.st_mtime_nsec), 0};
  stx->stx_ctime = {static_cast<int64_t>(st.st_ctime_sec),
                    static_cast<uint32_t>(st.st_ctime_nsec), 0};
  stx->stx_rdev_major = static_cast<uint32_t>((st.st_rdev >> 8) & 0xfff);
  stx->stx_rdev_minor =
      static_cast<uint32_t>((st.st_rdev & 0xff) | ((st.st_rdev >> 12) & 0xfff00));
  stx->stx_dev_major = static_cast<uint32_t>((st.st_dev >> 8) & 0xfff);
  stx->stx_dev_minor =
      static_cast<uint32_t>((st.st_dev & 0xff) | ((st.st_dev >> 12) & 0xfff00));
  return 0;
}

// Fill struct statfs (x86_64 layout, 120 bytes).
void FillStatfs(uint64_t magic, bool memfs, uint64_t extra_flags, uint64_t* b) {
  memset(b, 0, 120);
  b[0] = magic;
  b[1] = 4096;  // f_bsize
  if (memfs) {
    // In-memory filesystem: used = bytes of file data held by the VFS,
    // available = memory the kernel reports as free.
    uint64_t used = 0, nodes = 0;
    Vfs::UsageStats(&used, &nodes);
    zx_info_kmem_stats_t kmem{};
    uint64_t free_bytes = 0;
    zx_handle_t info = ZX_HANDLE_INVALID;
    if (g_sys_rsrc != ZX_HANDLE_INVALID &&
        zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                           ZX_RSRC_SYSTEM_INFO_BASE, 1, "info", 4,
                           &info) == ZX_OK) {
      if (zx_object_get_info(info, ZX_INFO_KMEM_STATS, &kmem, sizeof(kmem),
                             nullptr, nullptr) == ZX_OK) {
        free_bytes = kmem.free_bytes;
      }
      zx_handle_close(info);
    }
    uint64_t used_blocks = (used + 4095) / 4096;
    b[2] = used_blocks + free_bytes / 4096;  // f_blocks
    b[3] = free_bytes / 4096;                // f_bfree
    b[4] = free_bytes / 4096;                // f_bavail
  }
  b[8] = 255;   // f_namelen
  b[9] = 4096;  // f_frsize
  b[10] = 0x20 /*ST_VALID*/ | 0x400 /*ST_NOATIME*/ | extra_flags;
}

int64_t StatfsForDesc(LinuxProcess* proc, FileDescription* d, uint64_t* b) {
  switch (d->kind) {
    case FdKind::kPipeRead:
    case FdKind::kPipeWrite:
      FillStatfs(0x50495045, false, 0, b);
      return 0;
    case FdKind::kUnixSocket:
      FillStatfs(0x534F434B, false, 0, b);
      return 0;
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
    case FdKind::kEpoll:
      FillStatfs(0x09041934, false, 0, b);
      return 0;
    case FdKind::kProcDynamic:
      FillStatfs(0x9fa0, false, 2 | 4 | 8, b);
      return 0;
    default:
      FillStatfs(0x01021994, true, 0, b);  // TMPFS_MAGIC
      return 0;
  }
  (void)proc;
}

int64_t SetTimes(LinuxProcess* proc, VfsNode* node, const int64_t* ts) {
  // ts: NULL or {atime_sec, atime_nsec, mtime_sec, mtime_nsec}
  int64_t now = Vfs::NowNs();
  bool both_now = !ts || (ts[1] == kUtimeNow && ts[3] == kUtimeNow);
  if (both_now) {
    if (!IsOwnerOrRoot(proc, node) && !MayAccess(proc, node, 2)) return kEACCES;
  } else {
    for (int i : {1, 3}) {
      if (ts[i] != kUtimeNow && ts[i] != kUtimeOmit &&
          (ts[i] < 0 || ts[i] >= 1000000000LL)) {
        return kEINVAL;
      }
    }
    if (ts[1] == kUtimeOmit && ts[3] == kUtimeOmit) return 0;
    if (!IsOwnerOrRoot(proc, node)) return kEPERM;
  }
  auto pick = [&](int i, int64_t cur) -> int64_t {
    if (!ts || ts[i + 1] == kUtimeNow) return now;
    if (ts[i + 1] == kUtimeOmit) return cur;
    return ts[i] * 1000000000LL + ts[i + 1];
  };
  node->atime_ns = pick(0, node->atime_ns);
  node->mtime_ns = pick(2, node->mtime_ns);
  Vfs::TouchChange(node);
  return 0;
}

int64_t ChmodNode(LinuxProcess* proc, VfsNode* node, uint32_t mode) {
  if (!IsOwnerOrRoot(proc, node)) return kEPERM;
  uint32_t m = mode & 07777;
  // Non-root users not in the file's group cannot set S_ISGID.
  if (proc->fsuid != 0 && node->gid != proc->fsgid) {
    bool in_group = false;
    for (uint32_t g : proc->groups) in_group |= (g == node->gid);
    if (!in_group) m &= ~02000U;
  }
  node->mode = (node->mode & 0170000) | m;
  Vfs::TouchChange(node);
  return 0;
}

int64_t ChownNode(LinuxProcess* proc, VfsNode* node, uint32_t uid,
                  uint32_t gid) {
  bool change_uid = uid != 0xffffffffU && uid != node->uid;
  bool change_gid = gid != 0xffffffffU && gid != node->gid;
  if (proc->fsuid != 0) {
    if (change_uid) return kEPERM;
    if (change_gid) {
      if (proc->fsuid != node->uid) return kEPERM;
      bool member = gid == proc->fsgid;
      for (uint32_t g : proc->groups) member |= (g == gid);
      if (!member) return kEPERM;
    }
  }
  if (uid != 0xffffffffU) node->uid = uid;
  if (gid != 0xffffffffU) node->gid = gid;
  if ((uid != 0xffffffffU || gid != 0xffffffffU) &&
      node->type == VfsNodeType::kRegular) {
    node->mode &= ~04000U;
    if (node->mode & 010) node->mode &= ~02000U;
  }
  Vfs::TouchChange(node);
  return 0;
}

// Resolve a path to a VfsNode for attribute changes. /proc entries -> EPERM.
int64_t AttrTarget(LinuxProcess* proc, int dirfd, const char* path, int flags,
                   VfsNode** out) {
  if ((flags & kLinuxAT_EMPTY_PATH) && path && path[0] == '\0') {
    if (dirfd == kLinuxAT_FDCWD) {
      *out = Vfs::Lookup("/", proc->cwd, true, 0);
      return *out ? 0 : kENOENT;
    }
    auto d = LookupFd(proc, dirfd);
    if (!d) return kEBADF;
    if (!d->node) return kEPERM;
    *out = d->node;
    return 0;
  }
  std::string norm;
  int64_t rc = ResolveUser(proc, dirfd, path, &norm);
  if (rc < 0) return rc;
  bool follow = (flags & kLinuxAT_SYMLINK_NOFOLLOW) == 0;
  norm = ExpandProcLinks(proc, norm, follow);
  if (Vfs::ProcLookup(proc, norm) != Vfs::ProcEntryType::kNone) return kEPERM;
  return Vfs::LookupErr(norm, follow, out);
}

int64_t Getdents(LinuxProcess* proc, FileDescription* desc, uint8_t* dirp,
                 size_t count, bool legacy) {
  if (!dirp) return kEFAULT;
  struct DirItem {
    uint64_t ino;
    uint8_t dtype;
    std::string name;
  };
  std::vector<DirItem> items;
  std::vector<Vfs::ProcEntry> pentries;
  if (desc->kind == FdKind::kProcDynamic &&
      Vfs::IsDynamicProcDir(proc, desc->path, &pentries)) {
    std::sort(pentries.begin(), pentries.end(),
              [](const Vfs::ProcEntry& a, const Vfs::ProcEntry& b) {
                return a.name < b.name;
              });
    std::string parent = desc->path.substr(0, desc->path.rfind('/'));
    items.push_back({Vfs::ProcInode(desc->path), 4, "."});
    items.push_back({parent == "" ? 1 : Vfs::ProcInode(parent), 4, ".."});
    for (const auto& e : pentries) {
      uint8_t dt = e.type == Vfs::ProcEntryType::kDir       ? 4
                   : e.type == Vfs::ProcEntryType::kSymlink ? 10
                                                            : 8;
      items.push_back({Vfs::ProcInode(desc->path + "/" + e.name), dt, e.name});
    }
  } else {
    std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
    VfsNode* node = desc->node;
    if (!node || node->type != VfsNodeType::kDirectory) return kENOTDIR;
    if (node->nlink == 0) return kENOENT;  // directory was removed
    items.push_back({node->ino, 4, "."});
    items.push_back({node->parent ? node->parent->ino : node->ino, 4, ".."});
    for (const auto& kv : node->children) {
      uint8_t dt = 8;
      switch (kv.second->type) {
        case VfsNodeType::kDirectory: dt = 4; break;
        case VfsNodeType::kSymlink: dt = 10; break;
        case VfsNodeType::kCharDevice: dt = 2; break;
        case VfsNodeType::kSocket: dt = 12; break;
        default: dt = 8; break;
      }
      items.push_back({kv.second->ino, dt, kv.first});
    }
  }

  std::lock_guard<std::mutex> dlock(desc->mtx);
  // Resume after the last returned name when the position is unchanged, so
  // entries removed/added between calls do not shift the cursor.
  size_t idx = desc->offset;
  if (desc->offset > 2 && desc->offset == desc->dir_last_off &&
      !desc->dir_last_name.empty()) {
    idx = items.size();
    for (size_t i = 2; i < items.size(); ++i) {
      if (items[i].name > desc->dir_last_name) {
        idx = i;
        break;
      }
    }
  }
  size_t written = 0;
  uint64_t off = desc->offset;
  while (idx < items.size()) {
    const auto& item = items[idx];
    size_t reclen = legacy ? (8 + 8 + 2 + item.name.size() + 2 + 7) & ~7ULL
                           : (19 + item.name.size() + 1 + 7) & ~7ULL;
    if (written + reclen > count) {
      if (written == 0) return kEINVAL;
      break;
    }
    uint8_t* ent = dirp + written;
    memset(ent, 0, reclen);
    off++;
    *reinterpret_cast<uint64_t*>(ent + 0) = item.ino;
    *reinterpret_cast<int64_t*>(ent + 8) = static_cast<int64_t>(off);
    *reinterpret_cast<uint16_t*>(ent + 16) = static_cast<uint16_t>(reclen);
    if (legacy) {
      memcpy(ent + 18, item.name.c_str(), item.name.size() + 1);
      ent[reclen - 1] = item.dtype;  // d_type is the last byte
    } else {
      ent[18] = item.dtype;
      memcpy(ent + 19, item.name.c_str(), item.name.size() + 1);
    }
    written += reclen;
    desc->dir_last_name = item.name;
    idx++;
  }
  desc->offset = off;
  desc->dir_last_off = off;
  return static_cast<int64_t>(written);
}

int64_t FbIoctl(FileDescription* desc, uint32_t req, uintptr_t arg) {
  (void)desc;
  const auto& fb = GetFbHardwareInfo();
  if (!fb.active) return kENODEV;
  switch (req) {
    case 0x4600: {  // FBIOGET_VSCREENINFO (160 bytes)
      if (!arg) return kEFAULT;
      uint32_t* v = reinterpret_cast<uint32_t*>(arg);
      memset(v, 0, 160);
      v[0] = fb.width;
      v[1] = fb.height;
      v[2] = fb.stride_bytes / 4;  // xres_virtual
      v[3] = fb.height;            // yres_virtual
      v[6] = 32;                   // bits_per_pixel (XRGB8888)
      v[8] = 16;  v[9] = 8;        // red   offset/length
      v[11] = 8;  v[12] = 8;       // green
      v[14] = 0;  v[15] = 8;       // blue
      v[17] = 0;  v[18] = 0;       // transp: none (X channel)
      v[22] = 0xffffffffU;         // height in mm: unknown
      v[23] = 0xffffffffU;         // width in mm: unknown
      return 0;
    }
    case 0x4601: {  // FBIOPUT_VSCREENINFO: only the current mode is supported
      if (!arg) return kEFAULT;
      const uint32_t* v = reinterpret_cast<const uint32_t*>(arg);
      bool same = v[0] == fb.width && v[1] == fb.height &&
                  (v[2] == 0 || v[2] == fb.stride_bytes / 4) &&
                  (v[3] == 0 || v[3] == fb.height) && v[4] == 0 && v[5] == 0 &&
                  (v[6] == 0 || v[6] == 32);
      return same ? 0 : kEINVAL;
    }
    case 0x4602: {  // FBIOGET_FSCREENINFO (80 bytes)
      if (!arg) return kEFAULT;
      uint8_t* f = reinterpret_cast<uint8_t*>(arg);
      memset(f, 0, 80);
      memcpy(f, "bochs-vbe", 10);
      *reinterpret_cast<uint64_t*>(f + 16) = fb.phys_addr;  // smem_start
      *reinterpret_cast<uint32_t*>(f + 24) =
          static_cast<uint32_t>(fb.vram_bytes ? fb.vram_bytes : fb.size_bytes);
      *reinterpret_cast<uint32_t*>(f + 28) = 0;  // FB_TYPE_PACKED_PIXELS
      *reinterpret_cast<uint32_t*>(f + 36) = 2;  // FB_VISUAL_TRUECOLOR
      *reinterpret_cast<uint32_t*>(f + 48) = fb.stride_bytes;  // line_length
      return 0;
    }
    case 0x4606: {  // FBIOPAN_DISPLAY: no panning beyond the visible area
      if (!arg) return kEFAULT;
      const uint32_t* v = reinterpret_cast<const uint32_t*>(arg);
      return (v[4] == 0 && v[5] == 0) ? 0 : kEINVAL;
    }
    default:
      return kENOTTY;
  }
}

}  // namespace

int64_t HandleFsSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                        uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                        uint64_t a4, uint64_t a5) {
  t_current_thread = thrd;

  switch (sc) {
    case 0: {  // SYS_read
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      return ReadFileDescription(proc, d.get(), reinterpret_cast<void*>(a1),
                                 static_cast<size_t>(a2));
    }
    case 1: {  // SYS_write
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      return WriteFileDescription(proc, d.get(),
                                  reinterpret_cast<const void*>(a1),
                                  static_cast<size_t>(a2));
    }
    case 17:    // SYS_pread64
    case 18: {  // SYS_pwrite64
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      int64_t pos = static_cast<int64_t>(a3);
      if (pos < 0) return kEINVAL;
      if (sc == 17) {
        return PreadDesc(proc, d.get(), reinterpret_cast<void*>(a1),
                         static_cast<size_t>(a2), static_cast<uint64_t>(pos));
      }
      return PwriteDesc(proc, d.get(), reinterpret_cast<const void*>(a1),
                        static_cast<size_t>(a2), static_cast<uint64_t>(pos));
    }
    case 19:     // SYS_readv
    case 295:    // SYS_preadv
    case 327: {  // SYS_preadv2
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      const auto* iov = reinterpret_cast<const LinuxIovec*>(a1);
      int iovcnt = static_cast<int>(a2);
      size_t total = 0;
      int64_t rc = IovTotal(iov, iovcnt, &total);
      if (rc < 0) return rc;
      int64_t pos = (sc == 19) ? -1 : static_cast<int64_t>(a3);
      if (sc == 327 && pos == -1) {
        // preadv2 with offset -1 uses the file position.
      } else if (sc != 19 && pos < 0) {
        return kEINVAL;
      }
      if (sc == 327 && (a5 & ~0x1fULL)) return kEOPNOTSUPP;
      // One read into a bounce buffer: never blocks again after a partial
      // fill, and keeps datagram/line boundaries intact.
      std::vector<uint8_t> buf(total);
      ssize_t n = pos < 0 ? ReadFileDescription(proc, d.get(), buf.data(), total)
                          : PreadDesc(proc, d.get(), buf.data(), total,
                                      static_cast<uint64_t>(pos));
      if (n > 0) Scatter(iov, iovcnt, buf.data(), static_cast<size_t>(n));
      return n;
    }
    case 20:     // SYS_writev
    case 296:    // SYS_pwritev
    case 328: {  // SYS_pwritev2
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      const auto* iov = reinterpret_cast<const LinuxIovec*>(a1);
      int iovcnt = static_cast<int>(a2);
      size_t total = 0;
      int64_t rc = IovTotal(iov, iovcnt, &total);
      if (rc < 0) return rc;
      int64_t pos = (sc == 20) ? -1 : static_cast<int64_t>(a3);
      if (sc == 296 && pos < 0) return kEINVAL;
      if (sc == 328 && pos < -1) return kEINVAL;
      if (sc == 328 && (a5 & ~0x1fULL)) return kEOPNOTSUPP;
      auto buf = Gather(iov, iovcnt, total);
      if (pos < 0) {
        return WriteFileDescription(proc, d.get(), buf.data(), total);
      }
      return PwriteDesc(proc, d.get(), buf.data(), total,
                        static_cast<uint64_t>(pos));
    }
    case 2:  // SYS_open
      return OpenPathCommon(proc, kLinuxAT_FDCWD,
                            reinterpret_cast<const char*>(a0),
                            static_cast<uint32_t>(a1), static_cast<uint32_t>(a2));
    case 85:  // SYS_creat
      return OpenPathCommon(proc, kLinuxAT_FDCWD,
                            reinterpret_cast<const char*>(a0),
                            kLinuxO_CREAT | kLinuxO_WRONLY | kLinuxO_TRUNC,
                            static_cast<uint32_t>(a1));
    case 257:  // SYS_openat
      return OpenPathCommon(proc, static_cast<int>(a0),
                            reinterpret_cast<const char*>(a1),
                            static_cast<uint32_t>(a2), static_cast<uint32_t>(a3));
    case 3: {  // SYS_close
      std::shared_ptr<FileDescription> d;
      {
        std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
        d = DetachFdLocked(proc, static_cast<int>(a0));
      }
      if (!d) return kEBADF;
      d.reset();  // last reference: ~FileDescription notifies peers
      return 0;
    }
    case 436: {  // SYS_close_range
      uint32_t first = static_cast<uint32_t>(a0);
      uint32_t last = static_cast<uint32_t>(a1);
      uint32_t flags = static_cast<uint32_t>(a2);
      if (first > last) return kEINVAL;
      if (flags & ~(2U | 4U)) return kEINVAL;  // UNSHARE | CLOEXEC
      std::vector<std::shared_ptr<FileDescription>> closed;
      {
        std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
        std::vector<int> fds;
        for (const auto& kv : proc->fds) {
          uint32_t fd = static_cast<uint32_t>(kv.first);
          if (fd >= first && fd <= last) fds.push_back(kv.first);
        }
        for (int fd : fds) {
          if (flags & 4U) {
            proc->fds[fd].cloexec = true;
          } else {
            closed.push_back(DetachFdLocked(proc, fd));
          }
        }
      }
      closed.clear();
      return 0;
    }
    case 4:    // SYS_stat
    case 6: {  // SYS_lstat
      auto* st = reinterpret_cast<LinuxStat64*>(a1);
      if (!st) return kEFAULT;
      std::string norm;
      int64_t rc = ResolveUser(proc, kLinuxAT_FDCWD,
                               reinterpret_cast<const char*>(a0), &norm);
      if (rc < 0) return rc;
      return StatPath(proc, norm, sc == 4, st);
    }
    case 5: {  // SYS_fstat
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (!a1) return kEFAULT;
      FillStatFromDesc(proc, d.get(), reinterpret_cast<LinuxStat64*>(a1));
      return 0;
    }
    case 262: {  // SYS_newfstatat
      int dirfd = static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(a1);
      auto* st = reinterpret_cast<LinuxStat64*>(a2);
      int flags = static_cast<int>(a3);
      if (!st) return kEFAULT;
      if (flags & ~(kLinuxAT_SYMLINK_NOFOLLOW | kLinuxAT_EMPTY_PATH | 0x800)) {
        return kEINVAL;
      }
      if ((flags & kLinuxAT_EMPTY_PATH) && path && path[0] == '\0') {
        if (dirfd == kLinuxAT_FDCWD) return StatPath(proc, proc->cwd, true, st);
        auto d = LookupFd(proc, dirfd);
        if (!d) return kEBADF;
        FillStatFromDesc(proc, d.get(), st);
        return 0;
      }
      std::string norm;
      int64_t rc = ResolveUser(proc, dirfd, path, &norm);
      if (rc < 0) return rc;
      return StatPath(proc, norm, (flags & kLinuxAT_SYMLINK_NOFOLLOW) == 0, st);
    }
    case 332:  // SYS_statx
      return DoStatx(proc, static_cast<int>(a0),
                     reinterpret_cast<const char*>(a1), static_cast<int>(a2),
                     static_cast<uint32_t>(a3), reinterpret_cast<LinuxStatx*>(a4));
    case 8: {  // SYS_lseek
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      int64_t offset = static_cast<int64_t>(a1);
      int whence = static_cast<int>(a2);
      if (whence < 0 || whence > 4) return kEINVAL;
      switch (d->kind) {
        case FdKind::kDevNull:
        case FdKind::kDevZero:
        case FdKind::kDevFull:
        case FdKind::kDevRandom:
          return 0;  // these devices ignore the file position
        default:
          break;
      }
      bool is_dir = (d->node && d->node->type == VfsNodeType::kDirectory) ||
                    (d->kind == FdKind::kProcDynamic &&
                     Vfs::ProcLookup(proc, d->path) == Vfs::ProcEntryType::kDir);
      if (!IsSeekable(d.get())) return kESPIPE;
      std::lock_guard<std::mutex> dlock(d->mtx);
      if (is_dir) {
        if (whence == 0 && offset >= 0) {
          d->offset = static_cast<size_t>(offset);
          if (offset == 0) d->dir_last_name.clear();
          return offset;
        }
        if (whence == 1 && offset == 0) return static_cast<int64_t>(d->offset);
        return kEINVAL;
      }
      int64_t size = 0;
      if (d->node) {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        size = static_cast<int64_t>(d->node->data.size());
      } else if (d->kind == FdKind::kDevFb0) {
        size = GetFbHardwareInfo().size_bytes;
      } else if (d->kind == FdKind::kProcDynamic) {
        size = static_cast<int64_t>(d->dynamic_content.size());
      }
      int64_t next = 0;
      switch (whence) {
        case 0: next = offset; break;
        case 1: next = static_cast<int64_t>(d->offset) + offset; break;
        case 2: next = size + offset; break;
        case 3:  // SEEK_DATA: the whole file is data
          if (offset < 0 || offset >= size) return kENXIO;
          next = offset;
          break;
        case 4:  // SEEK_HOLE: the only hole is at EOF
          if (offset < 0 || offset >= size) return kENXIO;
          next = size;
          break;
      }
      if (next < 0) return kEINVAL;
      d->offset = static_cast<size_t>(next);
      return next;
    }
    case 21:     // SYS_access
    case 269:    // SYS_faccessat
    case 439: {  // SYS_faccessat2
      int dirfd = (sc == 21) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(sc == 21 ? a0 : a1);
      int mode = static_cast<int>(sc == 21 ? a1 : a2);
      int flags = (sc == 439) ? static_cast<int>(a3) : 0;
      if (mode & ~7) return kEINVAL;
      if (flags & ~(0x200 /*AT_EACCESS*/ | kLinuxAT_SYMLINK_NOFOLLOW |
                    kLinuxAT_EMPTY_PATH)) {
        return kEINVAL;
      }
      std::string norm;
      int64_t rc;
      if ((flags & kLinuxAT_EMPTY_PATH) && path && path[0] == '\0') {
        rc = ResolveAt(proc, dirfd, "", &norm);
      } else {
        rc = ResolveUser(proc, dirfd, path, &norm);
      }
      if (rc < 0) return rc;
      bool follow = (flags & kLinuxAT_SYMLINK_NOFOLLOW) == 0;
      norm = ExpandProcLinks(proc, norm, follow);
      auto ptype = Vfs::ProcLookup(proc, norm);
      if (ptype != Vfs::ProcEntryType::kNone) {
        if (mode & 2) return ptype == Vfs::ProcEntryType::kDir ? kEACCES : kEACCES;
        if ((mode & 1) && ptype == Vfs::ProcEntryType::kFile) return kEACCES;
        return 0;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      rc = Vfs::LookupErr(norm, follow, &node);
      if (rc < 0) return rc;
      if (mode == 0) return 0;  // F_OK
      return MayAccess(proc, node, mode, (flags & 0x200) == 0) ? 0 : kEACCES;
    }
    case 32: {  // SYS_dup
      std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
      auto it = proc->fds.find(static_cast<int>(a0));
      if (it == proc->fds.end()) return kEBADF;
      return AllocateFdLocked(proc, 0, it->second.desc, false);
    }
    case 33:     // SYS_dup2
    case 292: {  // SYS_dup3
      int oldfd = static_cast<int>(a0);
      int newfd = static_cast<int>(a1);
      if (sc == 292 && (a2 & ~static_cast<uint64_t>(kLinuxO_CLOEXEC))) {
        return kEINVAL;
      }
      bool cloexec = (sc == 292) && ((a2 & kLinuxO_CLOEXEC) != 0);
      std::shared_ptr<FileDescription> replaced;
      std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
      auto it = proc->fds.find(oldfd);
      if (it == proc->fds.end()) return kEBADF;
      if (newfd < 0 || newfd >= FdLimit(proc)) return kEBADF;
      if (oldfd == newfd) return sc == 292 ? kEINVAL : newfd;
      auto desc = it->second.desc;
      replaced = DetachFdLocked(proc, newfd);
      proc->fds[newfd] = OpenFd{desc, cloexec};
      return newfd;
    }
    case 72: {  // SYS_fcntl
      int fd = static_cast<int>(a0);
      int cmd = static_cast<int>(a1);
      uint64_t arg = a2;
      std::shared_ptr<FileDescription> desc;
      {
        std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
        auto it = proc->fds.find(fd);
        if (it == proc->fds.end()) return kEBADF;
        desc = it->second.desc;
        switch (cmd) {
          case 0:       // F_DUPFD
          case 1030: {  // F_DUPFD_CLOEXEC
            int64_t min_fd = static_cast<int64_t>(static_cast<int32_t>(arg));
            if (min_fd < 0 || min_fd >= FdLimit(proc)) return kEINVAL;
            return AllocateFdLocked(proc, static_cast<int>(min_fd), desc,
                                    cmd == 1030);
          }
          case 1:  // F_GETFD
            return it->second.cloexec ? 1 : 0;
          case 2:  // F_SETFD
            it->second.cloexec = (arg & 1) != 0;
            return 0;
          default:
            break;
        }
      }
      switch (cmd) {
        case 3:  // F_GETFL
          return desc->flags;
        case 4: {  // F_SETFL
          if (desc->flags & kLinuxO_PATH) return kEBADF;
          uint32_t req = static_cast<uint32_t>(arg);
          constexpr uint32_t kChangeable = kLinuxO_APPEND | kLinuxO_NONBLOCK |
                                           kLinuxO_ASYNC | kLinuxO_DIRECT |
                                           kLinuxO_NOATIME;
          if ((req & kLinuxO_ASYNC) && desc->kind != FdKind::kVfsFile) {
            // Signal-driven I/O (SIGIO) is not implemented for this object.
            return kEINVAL;
          }
          if ((req & kLinuxO_NOATIME) && desc->node &&
              !IsOwnerOrRoot(proc, desc->node)) {
            return kEPERM;
          }
          desc->flags = (desc->flags & ~kChangeable) | (req & kChangeable);
          return 0;
        }
        case 5:    // F_GETLK
        case 6:    // F_SETLK
        case 7:    // F_SETLKW
        case 36:   // F_OFD_GETLK
        case 37:   // F_OFD_SETLK
        case 38:   // F_OFD_SETLKW
          return FcntlLock(proc, desc.get(), cmd, reinterpret_cast<void*>(arg));
        case 8:  // F_SETOWN (only consulted for SIGIO/SIGURG, never raised here)
          desc->owner_pid = static_cast<int>(arg);
          return 0;
        case 9:  // F_GETOWN
          return desc->owner_pid;
        case 15: {  // F_SETOWN_EX {int type; pid_t pid;}
          if (!arg) return kEFAULT;
          const int32_t* o = reinterpret_cast<const int32_t*>(arg);
          if (o[0] == 0 || o[0] == 1) desc->owner_pid = o[1];      // TID/PID
          else if (o[0] == 2) desc->owner_pid = -o[1];             // PGRP
          else return kEINVAL;
          return 0;
        }
        case 16: {  // F_GETOWN_EX
          if (!arg) return kEFAULT;
          int32_t* o = reinterpret_cast<int32_t*>(arg);
          o[0] = desc->owner_pid < 0 ? 2 : 1;
          o[1] = desc->owner_pid < 0 ? -desc->owner_pid : desc->owner_pid;
          return 0;
        }
        case 11:  // F_GETSIG: SIGIO (0) since F_SETSIG is not supported
          return 0;
        case 1031: {  // F_SETPIPE_SZ
          if (!desc->pipe) return kEBADF;
          uint64_t want = static_cast<uint32_t>(arg);
          uint64_t sz = 4096;
          while (sz < want) sz <<= 1;
          if (sz > (1ULL << 31)) return kEINVAL;
          if (sz > 1048576 && proc->euid != 0) return kEPERM;
          std::lock_guard<std::mutex> lock(desc->pipe->mtx);
          if (desc->pipe->buf.size() > sz) return kEBUSY;
          desc->pipe->capacity = sz;
          NotifyPollWaiters();
          return static_cast<int64_t>(sz);
        }
        case 1032: {  // F_GETPIPE_SZ
          if (!desc->pipe) return kEBADF;
          std::lock_guard<std::mutex> lock(desc->pipe->mtx);
          return static_cast<int64_t>(desc->pipe->capacity);
        }
        default:
          // F_SETSIG, F_NOTIFY (dnotify), leases, file seals, write hints:
          // not supported.
          return kEINVAL;
      }
    }
    case 16: {  // SYS_ioctl
      int fd = static_cast<int>(a0);
      uint32_t req = static_cast<uint32_t>(a1);
      uintptr_t arg = a2;
      auto desc = LookupFd(proc, fd);
      if (!desc) return kEBADF;
      if (desc->flags & kLinuxO_PATH) return kEBADF;

      if (req == 0x5451 || req == 0x5450) {  // FIOCLEX / FIONCLEX
        std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
        auto it = proc->fds.find(fd);
        if (it == proc->fds.end()) return kEBADF;
        it->second.cloexec = (req == 0x5451);
        return 0;
      }
      if (req == 0x5421) {  // FIONBIO
        if (!arg) return kEFAULT;
        if (*reinterpret_cast<const int*>(arg)) desc->flags |= kLinuxO_NONBLOCK;
        else desc->flags &= ~kLinuxO_NONBLOCK;
        return 0;
      }
      if (req == 0x5452) {  // FIOASYNC: SIGIO is not implemented
        return kEINVAL;
      }

      std::shared_ptr<PtyPair> tty;
      bool master = false;
      if (desc->kind == FdKind::kSerialConsole) tty = GetConsoleTty();
      if (desc->kind == FdKind::kPtySlave) tty = desc->pty;
      if (desc->kind == FdKind::kPtyMaster) {
        tty = desc->pty;
        master = true;
      }

      if (req == 0x541B) {  // FIONREAD / TIOCINQ / SIOCINQ
        if (!arg) return kEFAULT;
        int64_t avail = 0;
        switch (desc->kind) {
          case FdKind::kPipeRead: {
            std::lock_guard<std::mutex> lock(desc->pipe->mtx);
            avail = static_cast<int64_t>(desc->pipe->buf.size());
            break;
          }
          case FdKind::kUnixSocket:
            avail = SocketBytesAvailable(desc.get());
            break;
          case FdKind::kEventFd:
          case FdKind::kTimerFd:
          case FdKind::kSignalFd:
          case FdKind::kInotify:
            avail = IpcBytesAvailable(desc.get());
            break;
          case FdKind::kVfsFile: {
            std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
            if (!desc->node || desc->node->type != VfsNodeType::kRegular) {
              return kENOTTY;
            }
            int64_t sz = static_cast<int64_t>(desc->node->data.size());
            avail = std::max<int64_t>(0, sz - static_cast<int64_t>(desc->offset));
            break;
          }
          default:
            if (!tty) return kENOTTY;
            avail = TtyBytesAvailable(tty, master);
            break;
        }
        if (avail < 0) return avail;
        *reinterpret_cast<int*>(arg) = static_cast<int>(avail);
        return 0;
      }

      if (tty) {
        if (master && req == 0x5441) {  // TIOCGPTPEER
          uint32_t flags = static_cast<uint32_t>(arg);
          return OpenPathCommon(proc, kLinuxAT_FDCWD,
                                ("/dev/pts/" + std::to_string(tty->index)).c_str(),
                                flags, 0);
        }
        return TtyIoctl(proc, desc.get(), tty, master, req, arg);
      }
      if (desc->kind == FdKind::kDevFb0) return FbIoctl(desc.get(), req, arg);
      if (desc->kind == FdKind::kUnixSocket &&
          (desc->inet_sock || (req >= 0x8910 && req <= 0x8938))) {
        return InetSocketIoctl(proc, desc.get(), req, arg);
      }
      return kENOTTY;
    }
    case 79: {  // SYS_getcwd
      char* buf = reinterpret_cast<char*>(a0);
      size_t size = static_cast<size_t>(a1);
      std::string cwd = proc->cwd;
      if (size < cwd.size() + 1) return kERANGE;
      if (!buf) return kEFAULT;
      memcpy(buf, cwd.c_str(), cwd.size() + 1);
      return static_cast<int64_t>(cwd.size() + 1);
    }
    case 80: {  // SYS_chdir
      std::string norm;
      int64_t rc = ResolveUser(proc, kLinuxAT_FDCWD,
                               reinterpret_cast<const char*>(a0), &norm);
      if (rc < 0) return rc;
      norm = ExpandProcLinks(proc, norm, true);
      auto ptype = Vfs::ProcLookup(proc, norm);
      if (ptype == Vfs::ProcEntryType::kDir) {
        proc->cwd = norm;
        return 0;
      }
      if (ptype != Vfs::ProcEntryType::kNone) return kENOTDIR;
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      rc = Vfs::LookupErr(norm, true, &node);
      if (rc < 0) return rc;
      if (node->type != VfsNodeType::kDirectory) return kENOTDIR;
      if (!MayAccess(proc, node, 1)) return kEACCES;
      proc->cwd = Vfs::NodePath(node);
      return 0;
    }
    case 81: {  // SYS_fchdir
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (d->node) {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        if (d->node->type != VfsNodeType::kDirectory) return kENOTDIR;
        if (!MayAccess(proc, d->node, 1)) return kEACCES;
        proc->cwd = Vfs::NodePath(d->node);
        return 0;
      }
      if (d->kind == FdKind::kProcDynamic &&
          Vfs::ProcLookup(proc, d->path) == Vfs::ProcEntryType::kDir) {
        proc->cwd = d->path;
        return 0;
      }
      return kENOTDIR;
    }
    case 82:     // SYS_rename
    case 264:    // SYS_renameat
    case 316: {  // SYS_renameat2
      int olddir = (sc == 82) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* oldp = reinterpret_cast<const char*>(sc == 82 ? a0 : a1);
      int newdir = (sc == 82) ? kLinuxAT_FDCWD : static_cast<int>(a2);
      const char* newp = reinterpret_cast<const char*>(sc == 82 ? a1 : a3);
      uint32_t flags = (sc == 316) ? static_cast<uint32_t>(a4) : 0;
      std::string o, n;
      int64_t rc = ResolveUser(proc, olddir, oldp, &o);
      if (rc < 0) return rc;
      rc = ResolveUser(proc, newdir, newp, &n);
      if (rc < 0) return rc;
      if (Vfs::ProcLookup(proc, o) != Vfs::ProcEntryType::kNone ||
          Vfs::ProcLookup(proc, n) != Vfs::ProcEntryType::kNone) {
        return kEXDEV;
      }
      {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        std::string b1, b2;
        VfsNode* p1 = Vfs::LookupParent("/", o, &b1);
        VfsNode* p2 = Vfs::LookupParent("/", n, &b2);
        if (p1 && !MayAccess(proc, p1, 3)) return kEACCES;
        if (p2 && !MayAccess(proc, p2, 3)) return kEACCES;
      }
      return Vfs::Rename("/", o, n, flags);
    }
    case 83:     // SYS_mkdir
    case 258: {  // SYS_mkdirat
      int dirfd = (sc == 83) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(sc == 83 ? a0 : a1);
      uint32_t mode = static_cast<uint32_t>(sc == 83 ? a1 : a2);
      std::string norm;
      int64_t rc = ResolveUser(proc, dirfd, path, &norm);
      if (rc < 0) return rc;
      if (Vfs::ProcLookup(proc, norm) != Vfs::ProcEntryType::kNone) {
        return kEEXIST;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      std::string base;
      VfsNode* parent = Vfs::LookupParent("/", norm, &base);
      if (parent && !parent->children.count(base) && !MayAccess(proc, parent, 3)) {
        return kEACCES;
      }
      VfsNode* d = nullptr;
      rc = Vfs::MakeDir(norm, mode & ~proc->umask_val & 07777, &d);
      if (rc < 0) return rc;
      d->uid = proc->fsuid;
      if (parent && (parent->mode & 02000)) {
        d->gid = parent->gid;
        d->mode |= 02000;
      } else {
        d->gid = proc->fsgid;
      }
      return 0;
    }
    case 84:     // SYS_rmdir
    case 87:     // SYS_unlink
    case 263: {  // SYS_unlinkat
      int dirfd = (sc == 263) ? static_cast<int>(a0) : kLinuxAT_FDCWD;
      const char* path = reinterpret_cast<const char*>(sc == 263 ? a1 : a0);
      bool rmdir = (sc == 84);
      if (sc == 263) {
        if (a2 & ~static_cast<uint64_t>(kLinuxAT_REMOVEDIR)) return kEINVAL;
        rmdir = (a2 & kLinuxAT_REMOVEDIR) != 0;
      }
      if (!path) return kEFAULT;
      std::string norm;
      int64_t rc = ResolveUser(proc, dirfd, path, &norm);
      if (rc < 0) return rc;
      if (Vfs::ProcLookup(proc, norm) != Vfs::ProcEntryType::kNone) {
        return kEACCES;
      }
      {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        std::string base;
        VfsNode* parent = Vfs::LookupParent("/", norm, &base);
        if (parent) {
          if (!MayAccess(proc, parent, 3)) return kEACCES;
          auto it = parent->children.find(base);
          // Sticky directories: only the owner of the entry or directory.
          if (it != parent->children.end() && (parent->mode & 01000) &&
              proc->fsuid != 0 && proc->fsuid != parent->uid &&
              proc->fsuid != it->second->uid) {
            return kEPERM;
          }
        }
      }
      // Pass the raw path tail so rmdir("dir/.") is rejected like Linux.
      if (rmdir) {
        std::string p(path);
        size_t e = p.find_last_not_of('/');
        if (e != std::string::npos &&
            (p.substr(0, e + 1) == "." || (e >= 1 && p.compare(e - 1, 2, "/.") == 0))) {
          return kEINVAL;
        }
      }
      return Vfs::Unlink("/", norm, rmdir);
    }
    case 86:     // SYS_link
    case 265: {  // SYS_linkat
      int olddir = (sc == 86) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* oldp = reinterpret_cast<const char*>(sc == 86 ? a0 : a1);
      int newdir = (sc == 86) ? kLinuxAT_FDCWD : static_cast<int>(a2);
      const char* newp = reinterpret_cast<const char*>(sc == 86 ? a1 : a3);
      int flags = (sc == 265) ? static_cast<int>(a4) : 0;
      if (flags & ~(0x400 /*AT_SYMLINK_FOLLOW*/ | kLinuxAT_EMPTY_PATH)) {
        return kEINVAL;
      }
      std::string o, n;
      int64_t rc;
      if ((flags & kLinuxAT_EMPTY_PATH) && oldp && oldp[0] == '\0') {
        auto d = LookupFd(proc, olddir);
        if (!d) return kEBADF;
        if (!d->node) return kENOENT;
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        if (d->node->nlink == 0) return kENOENT;
        o = Vfs::NodePath(d->node);
      } else {
        rc = ResolveUser(proc, olddir, oldp, &o);
        if (rc < 0) return rc;
      }
      rc = ResolveUser(proc, newdir, newp, &n);
      if (rc < 0) return rc;
      if (Vfs::ProcLookup(proc, o) != Vfs::ProcEntryType::kNone ||
          Vfs::ProcLookup(proc, n) != Vfs::ProcEntryType::kNone) {
        return kEXDEV;
      }
      {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        std::string base;
        VfsNode* parent = Vfs::LookupParent("/", n, &base);
        if (parent && !MayAccess(proc, parent, 3)) return kEACCES;
      }
      return Vfs::Link(o, n, (flags & 0x400) != 0);
    }
    case 88:     // SYS_symlink
    case 266: {  // SYS_symlinkat
      const char* target = reinterpret_cast<const char*>(a0);
      int newdir = (sc == 88) ? kLinuxAT_FDCWD : static_cast<int>(a1);
      const char* linkpath = reinterpret_cast<const char*>(sc == 88 ? a1 : a2);
      if (!target) return kEFAULT;
      if (target[0] == '\0') return kENOENT;
      std::string norm;
      int64_t rc = ResolveUser(proc, newdir, linkpath, &norm);
      if (rc < 0) return rc;
      if (Vfs::ProcLookup(proc, norm) != Vfs::ProcEntryType::kNone) {
        return kEEXIST;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      std::string base;
      VfsNode* parent = Vfs::LookupParent("/", norm, &base);
      if (parent && !parent->children.count(base) && !MayAccess(proc, parent, 3)) {
        return kEACCES;
      }
      rc = Vfs::MakeSymlink(norm, target);
      if (rc < 0) return rc;
      VfsNode* l = Vfs::Lookup("/", norm, false, 0);
      if (l) {
        l->uid = proc->fsuid;
        l->gid = proc->fsgid;
      }
      return 0;
    }
    case 89:     // SYS_readlink
    case 267: {  // SYS_readlinkat
      int dirfd = (sc == 89) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(sc == 89 ? a0 : a1);
      char* buf = reinterpret_cast<char*>(sc == 89 ? a1 : a2);
      int64_t bufsiz = static_cast<int64_t>(sc == 89 ? a2 : a3);
      if (bufsiz <= 0) return kEINVAL;
      if (!buf) return kEFAULT;
      std::string norm;
      int64_t rc = ResolveUser(proc, dirfd, path, &norm);
      if (rc < 0) return rc;
      norm = ExpandProcLinks(proc, norm, false);
      std::string target;
      auto ptype = Vfs::ProcLookup(proc, norm);
      if (ptype == Vfs::ProcEntryType::kSymlink) {
        if (!Vfs::ReadProcSymlink(proc, norm, &target)) return kENOENT;
      } else if (ptype != Vfs::ProcEntryType::kNone) {
        return kEINVAL;
      } else {
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        VfsNode* node = nullptr;
        rc = Vfs::LookupErr(norm, false, &node);
        if (rc < 0) return rc;
        if (node->type != VfsNodeType::kSymlink) return kEINVAL;
        target = node->symlink_target;
      }
      size_t n = std::min<size_t>(static_cast<size_t>(bufsiz), target.size());
      memcpy(buf, target.data(), n);
      return static_cast<int64_t>(n);
    }
    case 76:    // SYS_truncate
    case 77: {  // SYS_ftruncate
      int64_t len = static_cast<int64_t>(a1);
      if (len < 0) return kEINVAL;
      VfsNode* node = nullptr;
      std::shared_ptr<FileDescription> d;
      if (sc == 76) {
        std::string norm;
        int64_t rc = ResolveUser(proc, kLinuxAT_FDCWD,
                                 reinterpret_cast<const char*>(a0), &norm);
        if (rc < 0) return rc;
        norm = ExpandProcLinks(proc, norm, true);
        if (Vfs::ProcLookup(proc, norm) != Vfs::ProcEntryType::kNone) {
          return kEACCES;
        }
        rc = Vfs::LookupErr(norm, true, &node);
        if (rc < 0) return rc;
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        if (node->type == VfsNodeType::kDirectory) return kEISDIR;
        if (node->type != VfsNodeType::kRegular) return kEINVAL;
        if (!MayAccess(proc, node, 2)) return kEACCES;
      } else {
        d = LookupFd(proc, static_cast<int>(a0));
        if (!d) return kEBADF;
        if (!d->node || d->kind != FdKind::kVfsFile) return kEINVAL;
        uint32_t acc = d->flags & kLinuxO_ACCMODE;
        if ((acc != kLinuxO_WRONLY && acc != kLinuxO_RDWR) ||
            (d->flags & kLinuxO_PATH)) {
          return kEINVAL;
        }
        node = d->node;
        std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
        if (node->type != VfsNodeType::kRegular) return kEINVAL;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      if (len > (1LL << 40)) return kEFBIG;
      node->data.resize(static_cast<size_t>(len), 0);
      if (node->shared_vmo != ZX_HANDLE_INVALID) Vfs::SyncNodeToSharedVmo(node);
      Vfs::TouchModify(node);
      return 0;
    }
    case 90:     // SYS_chmod
    case 268:    // SYS_fchmodat
    case 452: {  // SYS_fchmodat2
      int dirfd = (sc == 90) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(sc == 90 ? a0 : a1);
      uint32_t mode = static_cast<uint32_t>(sc == 90 ? a1 : a2);
      int flags = (sc == 452) ? static_cast<int>(a3) : 0;
      if (flags & ~(kLinuxAT_SYMLINK_NOFOLLOW | kLinuxAT_EMPTY_PATH)) {
        return kEINVAL;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      int64_t rc = AttrTarget(proc, dirfd, path, flags, &node);
      if (rc < 0) return rc;
      if (node->type == VfsNodeType::kSymlink) return kEOPNOTSUPP;
      return ChmodNode(proc, node, mode);
    }
    case 91: {  // SYS_fchmod
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (d->flags & kLinuxO_PATH) return kEBADF;
      if (!d->node) return kEPERM;  // pipes/sockets: inode mode not stored
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      return ChmodNode(proc, d->node, static_cast<uint32_t>(a1));
    }
    case 92:     // SYS_chown
    case 94:     // SYS_lchown
    case 260: {  // SYS_fchownat
      int dirfd = (sc == 260) ? static_cast<int>(a0) : kLinuxAT_FDCWD;
      const char* path = reinterpret_cast<const char*>(sc == 260 ? a1 : a0);
      uint32_t uid = static_cast<uint32_t>(sc == 260 ? a2 : a1);
      uint32_t gid = static_cast<uint32_t>(sc == 260 ? a3 : a2);
      int flags = (sc == 260) ? static_cast<int>(a4)
                              : (sc == 94 ? kLinuxAT_SYMLINK_NOFOLLOW : 0);
      if (flags & ~(kLinuxAT_SYMLINK_NOFOLLOW | kLinuxAT_EMPTY_PATH)) {
        return kEINVAL;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      int64_t rc = AttrTarget(proc, dirfd, path, flags, &node);
      if (rc < 0) return rc;
      return ChownNode(proc, node, uid, gid);
    }
    case 93: {  // SYS_fchown
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (d->flags & kLinuxO_PATH) return kEBADF;
      if (!d->node) return kEPERM;
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      return ChownNode(proc, d->node, static_cast<uint32_t>(a1),
                       static_cast<uint32_t>(a2));
    }
    case 280: {  // SYS_utimensat
      int dirfd = static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(a1);
      const int64_t* ts = reinterpret_cast<const int64_t*>(a2);
      int flags = static_cast<int>(a3);
      if (flags & ~(kLinuxAT_SYMLINK_NOFOLLOW | kLinuxAT_EMPTY_PATH)) {
        return kEINVAL;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      if (path == nullptr) {  // Linux extension: operate on dirfd itself
        if (dirfd == kLinuxAT_FDCWD) return kEFAULT;
        auto d = LookupFd(proc, dirfd);
        if (!d) return kEBADF;
        if (!d->node) return kEPERM;
        node = d->node;
      } else {
        int64_t rc = AttrTarget(proc, dirfd, path, flags, &node);
        if (rc < 0) return rc;
      }
      return SetTimes(proc, node, ts);
    }
    case 132:    // SYS_utime   (struct utimbuf {time_t actime, modtime})
    case 235:    // SYS_utimes  (struct timeval[2])
    case 261: {  // SYS_futimesat
      int dirfd = (sc == 261) ? static_cast<int>(a0) : kLinuxAT_FDCWD;
      const char* path = reinterpret_cast<const char*>(sc == 261 ? a1 : a0);
      const int64_t* in = reinterpret_cast<const int64_t*>(sc == 261 ? a2 : a1);
      int64_t ts[4];
      const int64_t* tsp = nullptr;
      if (in) {
        if (sc == 132) {
          ts[0] = in[0]; ts[1] = 0; ts[2] = in[1]; ts[3] = 0;
        } else {
          for (int i : {1, 3}) {
            if (in[i] < 0 || in[i] >= 1000000) return kEINVAL;
          }
          ts[0] = in[0]; ts[1] = in[1] * 1000; ts[2] = in[2]; ts[3] = in[3] * 1000;
        }
        tsp = ts;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = nullptr;
      int64_t rc = AttrTarget(proc, dirfd, path, 0, &node);
      if (rc < 0) return rc;
      return SetTimes(proc, node, tsp);
    }
    case 73: {  // SYS_flock
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      return FlockOp(proc, d.get(), static_cast<int>(a1));
    }
    case 74:     // SYS_fsync
    case 75:     // SYS_fdatasync
    case 306:    // SYS_syncfs
    case 277: {  // SYS_sync_file_range
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (sc == 277) {
        if (static_cast<int64_t>(a1) < 0 || static_cast<int64_t>(a2) < 0) {
          return kEINVAL;
        }
        if (a3 & ~7ULL) return kEINVAL;
        if (d->kind == FdKind::kPipeRead || d->kind == FdKind::kPipeWrite) {
          return kESPIPE;
        }
      }
      if (sc != 306) {
        switch (d->kind) {
          case FdKind::kPipeRead:
          case FdKind::kPipeWrite:
          case FdKind::kUnixSocket:
          case FdKind::kEventFd:
          case FdKind::kTimerFd:
          case FdKind::kSignalFd:
          case FdKind::kInotify:
          case FdKind::kEpoll:
            return kEINVAL;  // no fsync operation for these objects
          default:
            break;
        }
      }
      // All data lives in memory: there is nothing to flush to storage.
      return 0;
    }
    case 162:  // SYS_sync: in-memory filesystem, nothing to write back
      return 0;
    case 221: {  // SYS_fadvise64
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      if (d->kind == FdKind::kPipeRead || d->kind == FdKind::kPipeWrite) {
        return kESPIPE;
      }
      if (static_cast<int64_t>(a2) < 0 || a3 > 5) return kEINVAL;
      return 0;  // advice only; the page cache does not exist here
    }
    case 285: {  // SYS_fallocate
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      int mode = static_cast<int>(a1);
      int64_t off = static_cast<int64_t>(a2), len = static_cast<int64_t>(a3);
      if (off < 0 || len <= 0) return kEINVAL;
      if (d->kind == FdKind::kPipeRead || d->kind == FdKind::kPipeWrite) {
        return kESPIPE;
      }
      if (!d->node || d->kind != FdKind::kVfsFile) return kENODEV;
      uint32_t acc = d->flags & kLinuxO_ACCMODE;
      if (acc != kLinuxO_WRONLY && acc != kLinuxO_RDWR) return kEBADF;
      constexpr int kKeepSize = 1, kPunchHole = 2, kZeroRange = 0x10;
      if (mode & ~(kKeepSize | kPunchHole | kZeroRange)) return kEOPNOTSUPP;
      if ((mode & kPunchHole) && !(mode & kKeepSize)) return kEINVAL;
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      VfsNode* node = d->node;
      if (node->type != VfsNodeType::kRegular) return kENODEV;
      uint64_t end = static_cast<uint64_t>(off) + static_cast<uint64_t>(len);
      if (end > (1ULL << 40)) return kEFBIG;
      if (node->shared_vmo != ZX_HANDLE_INVALID) Vfs::SyncNodeFromSharedVmo(node);
      if (mode & (kPunchHole | kZeroRange)) {
        uint64_t zend = std::min<uint64_t>(end, node->data.size());
        if (static_cast<uint64_t>(off) < zend) {
          memset(node->data.data() + off, 0, zend - static_cast<uint64_t>(off));
        }
      }
      if (!(mode & kKeepSize) && end > node->data.size()) {
        node->data.resize(end, 0);
      }
      if (node->shared_vmo != ZX_HANDLE_INVALID) Vfs::SyncNodeToSharedVmo(node);
      Vfs::TouchModify(node);
      return 0;
    }
    case 95: {  // SYS_umask
      uint32_t old = proc->umask_val;
      proc->umask_val = static_cast<uint32_t>(a0) & 0777;
      return old;
    }
    case 137:    // SYS_statfs
    case 138: {  // SYS_fstatfs
      uint64_t* buf = reinterpret_cast<uint64_t*>(a1);
      if (!buf) return kEFAULT;
      if (sc == 138) {
        auto d = LookupFd(proc, static_cast<int>(a0));
        if (!d) return kEBADF;
        return StatfsForDesc(proc, d.get(), buf);
      }
      std::string norm;
      int64_t rc = ResolveUser(proc, kLinuxAT_FDCWD,
                               reinterpret_cast<const char*>(a0), &norm);
      if (rc < 0) return rc;
      LinuxStat64 st{};
      rc = StatPath(proc, norm, true, &st);
      if (rc < 0) return rc;
      if (st.st_dev == kDevProcfs) {
        FillStatfs(0x9fa0, false, 2 | 4 | 8, buf);
      } else {
        FillStatfs(0x01021994, true, 0, buf);
      }
      return 0;
    }
    case 78:     // SYS_getdents (legacy struct linux_dirent)
    case 217: {  // SYS_getdents64
      auto d = LookupFd(proc, static_cast<int>(a0));
      if (!d) return kEBADF;
      return Getdents(proc, d.get(), reinterpret_cast<uint8_t*>(a1),
                      static_cast<size_t>(a2), sc == 78);
    }
    case 133:    // SYS_mknod
    case 259: {  // SYS_mknodat
      int dirfd = (sc == 133) ? kLinuxAT_FDCWD : static_cast<int>(a0);
      const char* path = reinterpret_cast<const char*>(sc == 133 ? a0 : a1);
      uint32_t mode = static_cast<uint32_t>(sc == 133 ? a1 : a2);
      uint64_t dev = sc == 133 ? a2 : a3;
      std::string norm;
      int64_t rc = ResolveUser(proc, dirfd, path, &norm);
      if (rc < 0) return rc;
      uint32_t type = mode & 0170000;
      VfsNodeType vt;
      if (type == 0 || type == 0100000) {
        vt = VfsNodeType::kRegular;
      } else if (type == 0020000) {
        if (proc->euid != 0) return kEPERM;
        vt = VfsNodeType::kCharDevice;
      } else if (type == 0140000) {
        vt = VfsNodeType::kSocket;
      } else if (type == 0010000 || type == 0060000) {
        return kEOPNOTSUPP;  // FIFOs and block devices are not supported
      } else {
        return kEINVAL;
      }
      std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
      std::string base;
      VfsNode* parent = Vfs::LookupParent("/", norm, &base);
      if (parent && !parent->children.count(base) && !MayAccess(proc, parent, 3)) {
        return kEACCES;
      }
      VfsNode* n = nullptr;
      rc = Vfs::MakeNode(norm, vt, mode & ~proc->umask_val & 07777,
                         vt == VfsNodeType::kCharDevice ? dev : 0, &n);
      if (rc < 0) return rc;
      n->uid = proc->fsuid;
      n->gid = proc->fsgid;
      return 0;
    }
    case 40: {  // SYS_sendfile
      auto out_d = LookupFd(proc, static_cast<int>(a0));
      auto in_d = LookupFd(proc, static_cast<int>(a1));
      if (!in_d || !out_d) return kEBADF;
      int64_t* offp = reinterpret_cast<int64_t*>(a2);
      size_t count = static_cast<size_t>(a3);
      if (!IsSeekable(in_d.get())) return kEINVAL;
      if (out_d->flags & kLinuxO_APPEND) return kEINVAL;
      if (offp && *offp < 0) return kEINVAL;
      uint64_t pos = offp ? static_cast<uint64_t>(*offp) : in_d->offset;
      size_t total = 0;
      std::vector<uint8_t> tmp(std::min<size_t>(count, 65536));
      while (total < count) {
        size_t want = std::min(tmp.size(), count - total);
        ssize_t r = PreadDesc(proc, in_d.get(), tmp.data(), want, pos);
        if (r < 0) {
          if (total > 0) break;
          return r;
        }
        if (r == 0) break;
        ssize_t w = WriteFileDescription(proc, out_d.get(), tmp.data(),
                                         static_cast<size_t>(r));
        if (w < 0) {
          if (total > 0) break;
          return w;
        }
        pos += static_cast<uint64_t>(w);
        total += static_cast<size_t>(w);
        if (w < r) break;
      }
      if (offp) {
        *offp = static_cast<int64_t>(pos);
      } else {
        std::lock_guard<std::mutex> lock(in_d->mtx);
        in_d->offset = pos;
      }
      return static_cast<int64_t>(total);
    }
    default:
      return HandlePollSyscall(proc, thrd, sc, a0, a1, a2, a3, a4, a5);
  }
}
