// File-descriptor core for the Linux shim: fd table helpers, open(2),
// read(2)/write(2) dispatch per object kind, stat helpers, record locks.
// The syscall dispatcher lives in fs_syscalls.cc, poll/epoll in fs_poll.cc.

#include <algorithm>

#include "src/shim/fs_internal.h"
#include "src/shim/tty.h"

thread_local LinuxThread* t_current_thread = nullptr;

namespace {

constexpr size_t kPipeBuf = 4096;
constexpr int kSIGPIPE = 13;

uint64_t DevMajor(uint64_t rdev) { return (rdev >> 8) & 0xfff; }
uint64_t DevMinor(uint64_t rdev) {
  return (rdev & 0xff) | ((rdev >> 12) & 0xfff00);
}

uint64_t SharedAnonInode() {
  static uint64_t ino = AllocAnonInode();
  return ino;
}

std::shared_ptr<FileDescription> NewDesc(FdKind kind, uint32_t flags,
                                         const std::string& path) {
  auto d = std::make_shared<FileDescription>();
  d->kind = kind;
  d->flags = flags;
  d->path = path;
  d->create_time_ns = Vfs::NowNs();
  return d;
}

bool Readable(FileDescription* d) {
  if (d->flags & kLinuxO_PATH) return false;
  return (d->flags & kLinuxO_ACCMODE) != kLinuxO_WRONLY;
}
bool Writable(FileDescription* d) {
  if (d->flags & kLinuxO_PATH) return false;
  uint32_t acc = d->flags & kLinuxO_ACCMODE;
  return acc == kLinuxO_WRONLY || acc == kLinuxO_RDWR;
}

// ---------------------------------------------------------------------------
// Record locks (POSIX + OFD) and flock(2)
// ---------------------------------------------------------------------------

struct RecLock {
  uintptr_t owner;  // pid (POSIX) or FileDescription* (OFD)
  bool ofd;
  int pid;          // reported by F_GETLK (-1 for OFD locks)
  std::weak_ptr<LinuxProcess> proc;  // POSIX owner liveness
  short type;       // F_RDLCK / F_WRLCK
  uint64_t start;
  uint64_t end;     // inclusive; UINT64_MAX = to EOF and beyond
};

struct FlockEntry {
  FileDescription* owner;
  bool exclusive;
};

std::mutex g_lock_mtx;
std::map<VfsNode*, std::vector<RecLock>> g_rec_locks;
std::map<VfsNode*, std::vector<FlockEntry>> g_flocks;

constexpr short kF_RDLCK = 0, kF_WRLCK = 1, kF_UNLCK = 2;

struct LinuxFlock {
  int16_t l_type;
  int16_t l_whence;
  int32_t pad;
  int64_t l_start;
  int64_t l_len;
  int32_t l_pid;
  int32_t pad2;
};

bool LockAlive(const RecLock& l) {
  if (l.ofd) return true;
  auto p = l.proc.lock();
  return p && !p->exited.load();
}

void PruneDeadLocked(std::vector<RecLock>& v) {
  v.erase(std::remove_if(v.begin(), v.end(),
                         [](const RecLock& l) { return !LockAlive(l); }),
          v.end());
}

const RecLock* FindConflictLocked(std::vector<RecLock>& v, uintptr_t owner,
                                  bool ofd, short type, uint64_t s,
                                  uint64_t e) {
  for (const auto& l : v) {
    if (l.owner == owner && l.ofd == ofd) continue;
    if (l.end < s || l.start > e) continue;
    if (l.type == kF_WRLCK || type == kF_WRLCK) return &l;
  }
  return nullptr;
}

// Replace the owner's locks in [s, e] with `type` (or remove for F_UNLCK).
void ApplyLockLocked(std::vector<RecLock>& v, const RecLock& nl) {
  std::vector<RecLock> out;
  for (const auto& l : v) {
    if (!(l.owner == nl.owner && l.ofd == nl.ofd) || l.end < nl.start ||
        l.start > nl.end) {
      out.push_back(l);
      continue;
    }
    if (l.start < nl.start) {
      RecLock left = l;
      left.end = nl.start - 1;
      out.push_back(left);
    }
    if (l.end > nl.end) {
      RecLock right = l;
      right.start = nl.end + 1;
      out.push_back(right);
    }
  }
  if (nl.type != kF_UNLCK) out.push_back(nl);
  v.swap(out);
}

void ReleasePosixLocks(LinuxProcess* proc, VfsNode* node) {
  if (!node) return;
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(g_lock_mtx);
    auto it = g_rec_locks.find(node);
    if (it == g_rec_locks.end()) return;
    auto& v = it->second;
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const RecLock& l) {
                             return !l.ofd && l.pid == proc->pid;
                           }),
            v.end());
    changed = v.size() != before;
    if (v.empty()) g_rec_locks.erase(it);
  }
  if (changed) NotifyPollWaiters();
}

void ReleaseDescLocks(FileDescription* desc) {
  bool changed = false;
  std::lock_guard<std::mutex> lock(g_lock_mtx);
  if (desc->node) {
    auto it = g_rec_locks.find(desc->node);
    if (it != g_rec_locks.end()) {
      auto& v = it->second;
      size_t before = v.size();
      v.erase(std::remove_if(v.begin(), v.end(),
                             [&](const RecLock& l) {
                               return l.ofd &&
                                      l.owner == reinterpret_cast<uintptr_t>(desc);
                             }),
              v.end());
      changed |= v.size() != before;
      if (v.empty()) g_rec_locks.erase(it);
    }
  }
  VfsNode* key = desc->node ? desc->node : reinterpret_cast<VfsNode*>(desc);
  auto fit = g_flocks.find(key);
  if (fit != g_flocks.end()) {
    auto& v = fit->second;
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const FlockEntry& f) { return f.owner == desc; }),
            v.end());
    changed |= v.size() != before;
    if (v.empty()) g_flocks.erase(fit);
  }
  if (changed) NotifyPollWaiters();
}

}  // namespace

// ---------------------------------------------------------------------------
// Generic helpers exported through fd_ops.h / fs_internal.h
// ---------------------------------------------------------------------------

bool CurrentThreadHasUnblockedSignal(LinuxProcess* proc) {
  LinuxThread* t = t_current_thread;
  if (!t) return false;
  return ShimSignalPending(proc, t);
}

uint64_t CurrentPollSeq() {
  return g_poll_seq.load(std::memory_order_acquire);
}

void WaitForPollEvent(int64_t max_ms, uint64_t seen_seq) {
  if (max_ms <= 0) max_ms = 1;
  std::unique_lock<std::mutex> lock(g_poll_mtx);
  if (seen_seq != UINT64_MAX &&
      g_poll_seq.load(std::memory_order_relaxed) != seen_seq) {
    return;
  }
  g_poll_cv.wait_for(lock, std::chrono::milliseconds(max_ms));
}

uint64_t AllocAnonInode() { return Vfs::AllocInode(); }

int64_t FdLimit(LinuxProcess* proc) {
  if (proc->rlimits_init) {
    uint64_t v = proc->rlimits[7][0];
    return v > (1u << 20) ? (1 << 20) : static_cast<int64_t>(v);
  }
  return 1024;
}

int AllocateFdLocked(LinuxProcess* proc, int min_fd,
                     std::shared_ptr<FileDescription> desc, bool cloexec) {
  int fd = std::max(0, min_fd);
  int64_t limit = FdLimit(proc);
  while (proc->fds.find(fd) != proc->fds.end()) fd++;
  if (fd >= limit) return static_cast<int>(kEMFILE);
  proc->fds[fd] = OpenFd{std::move(desc), cloexec};
  return fd;
}

int InstallFd(LinuxProcess* proc, std::shared_ptr<FileDescription> desc,
              bool cloexec) {
  std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
  return AllocateFdLocked(proc, 0, std::move(desc), cloexec);
}

std::shared_ptr<FileDescription> LookupFd(LinuxProcess* proc, int fd) {
  std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
  auto it = proc->fds.find(fd);
  if (it == proc->fds.end()) return nullptr;
  return it->second.desc;
}

std::shared_ptr<FileDescription> DetachFdLocked(LinuxProcess* proc, int fd) {
  auto it = proc->fds.find(fd);
  if (it == proc->fds.end()) return nullptr;
  auto desc = std::move(it->second.desc);
  proc->fds.erase(it);
  // POSIX: closing any fd of a file drops all of the process's locks on it.
  if (desc && desc->node) ReleasePosixLocks(proc, desc->node);
  return desc;
}

FileDescription::~FileDescription() {
  switch (kind) {
    case FdKind::kPipeRead:
      if (pipe) {
        std::lock_guard<std::mutex> lock(pipe->mtx);
        if (pipe->readers > 0) pipe->readers--;
      }
      NotifyPollWaiters();
      break;
    case FdKind::kPipeWrite:
      if (pipe) {
        std::lock_guard<std::mutex> lock(pipe->mtx);
        if (pipe->writers > 0) pipe->writers--;
      }
      NotifyPollWaiters();
      break;
    case FdKind::kUnixSocket:
      SocketOnDescriptionDestroyed(this);
      break;
    case FdKind::kPtyMaster:
      if (pty) PtyMasterClosed(pty);
      break;
    case FdKind::kPtySlave:
      if (pty) PtySlaveClosed(pty);
      break;
    case FdKind::kVfsFile:
      if (node && !(flags & kLinuxO_PATH)) {
        std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
        Vfs::Notify(node, Writable(this) ? kInCloseWrite : kInCloseNoWrite);
      }
      break;
    default:
      break;
  }
  ReleaseDescLocks(this);
}

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

int64_t ResolveAt(LinuxProcess* proc, int dirfd, const std::string& raw,
                  std::string* out) {
  if (raw.size() >= 4096) return kENAMETOOLONG;
  if (!raw.empty() && raw[0] == '/') {
    *out = Vfs::NormalizePath("/", raw);
    return 0;
  }
  std::string base = proc->cwd;
  if (dirfd != kLinuxAT_FDCWD) {
    auto d = LookupFd(proc, dirfd);
    if (!d) return kEBADF;
    if (d->node) {
      std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
      if (d->node->type != VfsNodeType::kDirectory) {
        if (!raw.empty()) return kENOTDIR;
      }
      base = Vfs::NodePath(d->node);
    } else if (d->kind == FdKind::kProcDynamic &&
               Vfs::ProcLookup(proc, d->path) == Vfs::ProcEntryType::kDir) {
      base = d->path;
    } else {
      if (!raw.empty()) return kENOTDIR;
      base = d->path;
    }
  }
  *out = raw.empty() ? base : Vfs::NormalizePath(base, raw);
  return 0;
}

std::string ExpandProcLinks(LinuxProcess* proc, const std::string& norm,
                            bool follow_final) {
  std::string cur = norm;
  for (int iter = 0; iter < 8; ++iter) {
    if (cur != "/proc" && cur.rfind("/proc/", 0) != 0) return cur;
    std::vector<std::string> parts;
    size_t i = 6;
    while (i <= cur.size()) {
      size_t j = cur.find('/', i);
      if (j == std::string::npos) j = cur.size();
      if (j > i) parts.push_back(cur.substr(i, j - i));
      i = j + 1;
    }
    if (parts.empty()) return cur;
    auto rest_from = [&](size_t k) {
      std::string r;
      for (size_t x = k; x < parts.size(); ++x) r += "/" + parts[x];
      return r;
    };
    if (parts[0] == "self") {
      if (parts.size() == 1 && !follow_final) return cur;
      cur = "/proc/" + std::to_string(proc->pid) + rest_from(1);
      continue;
    }
    if (parts.size() >= 2 &&
        (parts[1] == "cwd" || parts[1] == "exe" || parts[1] == "root") &&
        (parts.size() > 2 || follow_final)) {
      std::string target;
      if (!Vfs::ReadProcSymlink(proc, "/proc/" + parts[0] + "/" + parts[1],
                                &target)) {
        return cur;
      }
      cur = Vfs::NormalizePath("/", target + rest_from(2));
      continue;
    }
    if (parts.size() >= 4 && parts[1] == "fd") {
      std::string target;
      if (Vfs::ReadProcSymlink(proc, "/proc/" + parts[0] + "/fd/" + parts[2],
                               &target) &&
          !target.empty() && target[0] == '/') {
        cur = Vfs::NormalizePath("/", target + rest_from(3));
        continue;
      }
    }
    return cur;
  }
  return cur;
}

std::shared_ptr<FileDescription> ProcFdTarget(LinuxProcess* proc,
                                              const std::string& norm) {
  std::string p = ExpandProcLinks(proc, norm, false);
  if (p.rfind("/proc/", 0) != 0) return nullptr;
  int pid = 0, fd = -1;
  char tail = 0;
  if (sscanf(p.c_str(), "/proc/%d/fd/%d%c", &pid, &fd, &tail) != 2) {
    return nullptr;
  }
  std::shared_ptr<LinuxProcess> target;
  {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    auto it = g_processes.find(pid);
    if (it == g_processes.end()) return nullptr;
    target = it->second;
  }
  return LookupFd(target.get(), fd);
}

bool MayAccess(LinuxProcess* proc, VfsNode* node, int mask, bool use_real_ids) {
  uint32_t uid = use_real_ids ? proc->ruid : proc->fsuid;
  uint32_t gid = use_real_ids ? proc->rgid : proc->fsgid;
  if (uid == 0) {
    if ((mask & 1) == 0) return true;
    return node->type == VfsNodeType::kDirectory || (node->mode & 0111) != 0;
  }
  uint32_t bits;
  if (uid == node->uid) {
    bits = (node->mode >> 6) & 7;
  } else {
    bool in_group = gid == node->gid;
    for (uint32_t g : proc->groups) in_group |= (g == node->gid);
    bits = in_group ? (node->mode >> 3) & 7 : node->mode & 7;
  }
  return (bits & static_cast<uint32_t>(mask)) == static_cast<uint32_t>(mask);
}

bool IsOwnerOrRoot(LinuxProcess* proc, VfsNode* node) {
  return proc->fsuid == 0 || proc->fsuid == node->uid;
}

// ---------------------------------------------------------------------------
// stat
// ---------------------------------------------------------------------------

void FillStatFromNode(VfsNode* node, LinuxStat64* st) {
  memset(st, 0, sizeof(*st));
  st->st_dev = kDevRootfs;
  st->st_ino = node->ino;
  st->st_nlink = node->nlink;
  st->st_mode = node->mode;
  st->st_uid = node->uid;
  st->st_gid = node->gid;
  st->st_rdev = node->rdev;
  if (node->type == VfsNodeType::kRegular) {
    st->st_size = static_cast<int64_t>(node->data.size());
  } else if (node->type == VfsNodeType::kSymlink) {
    st->st_size = static_cast<int64_t>(node->symlink_target.size());
  } else if (node->type == VfsNodeType::kDirectory) {
    st->st_size = 4096;
  }
  st->st_blksize = 4096;
  st->st_blocks = (st->st_size + 511) / 512;
  st->st_atime_sec = static_cast<uint64_t>(node->atime_ns / 1000000000LL);
  st->st_atime_nsec = static_cast<uint64_t>(node->atime_ns % 1000000000LL);
  st->st_mtime_sec = static_cast<uint64_t>(node->mtime_ns / 1000000000LL);
  st->st_mtime_nsec = static_cast<uint64_t>(node->mtime_ns % 1000000000LL);
  st->st_ctime_sec = static_cast<uint64_t>(node->ctime_ns / 1000000000LL);
  st->st_ctime_nsec = static_cast<uint64_t>(node->ctime_ns % 1000000000LL);
}

namespace {

void FillProcStat(LinuxProcess* proc, const std::string& norm,
                  Vfs::ProcEntryType type, LinuxStat64* st) {
  memset(st, 0, sizeof(*st));
  st->st_dev = kDevProcfs;
  st->st_ino = Vfs::ProcInode(norm);
  st->st_blksize = 1024;
  if (type == Vfs::ProcEntryType::kDir) {
    st->st_mode = 0040555;
    st->st_nlink = 2;
  } else if (type == Vfs::ProcEntryType::kSymlink) {
    st->st_mode = 0120777;
    st->st_nlink = 1;
    std::string target;
    if (Vfs::ReadProcSymlink(proc, norm, &target)) {
      st->st_size = static_cast<int64_t>(target.size());
    }
  } else {
    st->st_mode = 0100444;  // procfs reports st_size 0 for generated files
    st->st_nlink = 1;
  }
  // Per-process entries are owned by that process's effective ids.
  int pid = 0;
  if (sscanf(norm.c_str(), "/proc/%d", &pid) == 1) {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    auto it = g_processes.find(pid);
    if (it != g_processes.end()) {
      st->st_uid = it->second->euid;
      st->st_gid = it->second->egid;
    }
  }
  int64_t now = Vfs::NowNs();
  st->st_atime_sec = st->st_mtime_sec = st->st_ctime_sec =
      static_cast<uint64_t>(now / 1000000000LL);
  st->st_atime_nsec = st->st_mtime_nsec = st->st_ctime_nsec =
      static_cast<uint64_t>(now % 1000000000LL);
}

}  // namespace

void FillStatFromDesc(LinuxProcess* proc, FileDescription* desc,
                      LinuxStat64* st) {
  if (desc->node) {
    std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
    FillStatFromNode(desc->node, st);
    return;
  }
  if (desc->kind == FdKind::kProcDynamic) {
    FillProcStat(proc, desc->path, Vfs::ProcLookup(proc, desc->path), st);
    return;
  }
  // Character devices: report the device node they were opened through.
  bool is_dev = false;
  switch (desc->kind) {
    case FdKind::kSerialConsole:
    case FdKind::kPtyMaster:
    case FdKind::kPtySlave:
    case FdKind::kDevNull:
    case FdKind::kDevZero:
    case FdKind::kDevFull:
    case FdKind::kDevRandom:
    case FdKind::kDevFb0:
    case FdKind::kDevInputEvent0:
      is_dev = true;
      break;
    default:
      break;
  }
  if (is_dev && !desc->path.empty()) {
    std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
    VfsNode* n = Vfs::Lookup("/", desc->path, true, 0);
    if (n && n->type == VfsNodeType::kCharDevice) {
      FillStatFromNode(n, st);
      return;
    }
  }
  memset(st, 0, sizeof(*st));
  st->st_nlink = 1;
  st->st_blksize = 4096;
  st->st_uid = proc->euid;
  st->st_gid = proc->egid;
  int64_t t = desc->create_time_ns;
  st->st_atime_sec = st->st_mtime_sec = st->st_ctime_sec =
      static_cast<uint64_t>(t / 1000000000LL);
  st->st_atime_nsec = st->st_mtime_nsec = st->st_ctime_nsec =
      static_cast<uint64_t>(t % 1000000000LL);
  switch (desc->kind) {
    case FdKind::kPipeRead:
    case FdKind::kPipeWrite:
      st->st_dev = kDevPipefs;
      st->st_ino = desc->anon_ino;
      st->st_mode = 0010600;
      st->st_blksize = 4096;
      break;
    case FdKind::kUnixSocket:
      st->st_dev = kDevSockfs;
      st->st_ino = desc->anon_ino;
      st->st_mode = 0140777;
      break;
    case FdKind::kPtyMaster:
      st->st_dev = kDevRootfs;
      st->st_mode = 0020666;
      st->st_rdev = desc->pty ? TtyRdev(desc->pty, true) : 0;
      st->st_ino = desc->anon_ino;
      break;
    case FdKind::kPtySlave:
      st->st_dev = kDevRootfs;
      st->st_mode = 0020620;
      st->st_rdev = desc->pty ? TtyRdev(desc->pty, false) : 0;
      st->st_ino = desc->anon_ino;
      break;
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
    case FdKind::kEpoll:
      // All anon_inode files share one inode, mode 0600 without a type.
      st->st_dev = kDevAnonfs;
      st->st_ino = SharedAnonInode();
      st->st_mode = 0600;
      st->st_uid = 0;
      st->st_gid = 0;
      break;
    default:
      st->st_dev = kDevRootfs;
      st->st_ino = desc->anon_ino;
      st->st_mode = 0020600;
      break;
  }
}

int64_t StatPath(LinuxProcess* proc, const std::string& norm_in, bool follow,
                 LinuxStat64* st) {
  std::string norm = ExpandProcLinks(proc, norm_in, follow);
  auto ptype = Vfs::ProcLookup(proc, norm);
  if (ptype != Vfs::ProcEntryType::kNone) {
    if (ptype == Vfs::ProcEntryType::kSymlink && follow) {
      // /proc/<pid>/fd/<n> pointing at a non-path object (pipe, socket...)
      // or at a deleted file.
      auto d = ProcFdTarget(proc, norm);
      if (!d) return kENOENT;
      FillStatFromDesc(proc, d.get(), st);
      return 0;
    }
    FillProcStat(proc, norm, ptype, st);
    return 0;
  }
  std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
  VfsNode* node = nullptr;
  int64_t rc = Vfs::LookupErr(norm, follow, &node);
  if (rc < 0) return rc;
  FillStatFromNode(node, st);
  return 0;
}

// ---------------------------------------------------------------------------
// open
// ---------------------------------------------------------------------------

namespace {

int64_t OpenDevice(LinuxProcess* proc, const std::string& norm, uint64_t rdev,
                   uint32_t flags, bool cloexec) {
  uint64_t major = DevMajor(rdev), minor = DevMinor(rdev);
  uint32_t stored =
      flags & ~(kLinuxO_CREAT | kLinuxO_EXCL | kLinuxO_NOCTTY | kLinuxO_TRUNC |
                kLinuxO_CLOEXEC);
  bool noctty = (flags & kLinuxO_NOCTTY) != 0;
  std::shared_ptr<FileDescription> d;
  if (major == 1 && minor == 3) {
    d = NewDesc(FdKind::kDevNull, stored, norm);
  } else if (major == 1 && minor == 5) {
    d = NewDesc(FdKind::kDevZero, stored, norm);
  } else if (major == 1 && minor == 7) {
    d = NewDesc(FdKind::kDevFull, stored, norm);
  } else if (major == 1 && (minor == 8 || minor == 9)) {
    d = NewDesc(FdKind::kDevRandom, stored, norm);
  } else if (major == 5 && minor == 0) {  // /dev/tty -> controlling terminal
    auto ctty = TtyGetCtty(proc);
    if (!ctty) return kENXIO;
    if (ctty->is_console) {
      d = NewDesc(FdKind::kSerialConsole, stored, norm);
    } else {
      {
        std::lock_guard<std::mutex> lock(ctty->mtx);
        if (ctty->master_count <= 0) return kEIO;
        ctty->slave_count++;
        ctty->slave_opened = true;
      }
      d = NewDesc(FdKind::kPtySlave, stored, norm);
      d->pty = ctty;
    }
  } else if (major == 5 && minor == 1) {  // /dev/console: never a ctty
    d = NewDesc(FdKind::kSerialConsole, stored, norm);
  } else if (major == 4 && minor == 64) {  // ttyS0: the UART console
    d = NewDesc(FdKind::kSerialConsole, stored, norm);
    if (!noctty && Readable(d.get())) TtyMaybeAcquireCtty(proc, GetConsoleTty());
  } else if (major == 5 && minor == 2) {  // /dev/ptmx
    auto pty = PtyAllocate();
    d = NewDesc(FdKind::kPtyMaster, stored, norm);
    d->pty = pty;
  } else if (major == 136) {  // /dev/pts/N
    auto pty = PtyLookup(static_cast<int>(minor));
    if (!pty) return kEIO;
    {
      std::lock_guard<std::mutex> lock(pty->mtx);
      if (pty->master_count <= 0 || pty->locked) return kEIO;
      if (pty->exclusive && proc->euid != 0) return kEBUSY;
      pty->slave_count++;
      pty->slave_opened = true;
    }
    d = NewDesc(FdKind::kPtySlave, stored, norm);
    d->pty = pty;
    if (!noctty && Readable(d.get())) TtyMaybeAcquireCtty(proc, pty);
  } else if (major == 29 && minor == 0) {
    if (!GetFbHardwareInfo().active) return kENODEV;
    d = NewDesc(FdKind::kDevFb0, stored, norm);
  } else if (major == 13 && minor == 64) {
    if (!InputDeviceAvailable()) return kENODEV;
    d = NewDesc(FdKind::kDevInputEvent0, stored, norm);
  } else {
    return kENXIO;  // no driver for this device number
  }
  d->anon_ino = AllocAnonInode();
  return InstallFd(proc, std::move(d), cloexec);
}

// open("/proc/<pid>/fd/<n>"): reopen the object behind that fd.
int64_t ReopenProcFd(LinuxProcess* proc, const std::string& norm,
                     uint32_t flags, bool cloexec) {
  auto t = ProcFdTarget(proc, norm);
  if (!t) return kENOENT;
  uint32_t acc = flags & kLinuxO_ACCMODE;
  bool want_write = acc == kLinuxO_WRONLY || acc == kLinuxO_RDWR;
  uint32_t stored =
      flags & ~(kLinuxO_CREAT | kLinuxO_EXCL | kLinuxO_NOCTTY | kLinuxO_TRUNC |
                kLinuxO_CLOEXEC);
  if (t->kind == FdKind::kVfsFile && t->node) {
    std::shared_ptr<FileDescription> d;
    {
      std::lock_guard<std::recursive_mutex> lock(Vfs::Mutex());
      VfsNode* n = t->node;
      if (n->type == VfsNodeType::kDirectory && want_write) return kEISDIR;
      int mask = (acc == kLinuxO_RDONLY ? 4 : 0) | (want_write ? 2 : 0) |
                 (acc == kLinuxO_RDWR ? 4 : 0);
      if (!MayAccess(proc, n, mask)) return kEACCES;
      if ((flags & kLinuxO_TRUNC) && want_write &&
          n->type == VfsNodeType::kRegular) {
        n->data.clear();
        if (n->shared_vmo != ZX_HANDLE_INVALID) Vfs::SyncNodeToSharedVmo(n);
        Vfs::TouchModify(n);
      }
      Vfs::Notify(n, kInOpen);
      d = NewDesc(FdKind::kVfsFile, stored, Vfs::NodePath(n));
      d->node = n;
      d->owned_node = t->owned_node;
    }
    return InstallFd(proc, std::move(d), cloexec);
  }
  if ((t->kind == FdKind::kPipeRead || t->kind == FdKind::kPipeWrite) &&
      t->pipe) {
    // Like opening a FIFO: O_RDONLY gives a read end, O_WRONLY a write end.
    if (acc == kLinuxO_RDWR || acc == 3) return kEINVAL;
    FdKind k = acc == kLinuxO_RDONLY ? FdKind::kPipeRead : FdKind::kPipeWrite;
    auto d = NewDesc(k, stored, "");
    d->pipe = t->pipe;
    d->anon_ino = t->anon_ino;
    {
      std::lock_guard<std::mutex> lock(t->pipe->mtx);
      if (k == FdKind::kPipeRead) t->pipe->readers++;
      else t->pipe->writers++;
    }
    return InstallFd(proc, std::move(d), cloexec);
  }
  switch (t->kind) {
    case FdKind::kSerialConsole:
    case FdKind::kPtyMaster:
    case FdKind::kPtySlave:
    case FdKind::kDevNull:
    case FdKind::kDevZero:
    case FdKind::kDevFull:
    case FdKind::kDevRandom:
    case FdKind::kDevFb0:
    case FdKind::kDevInputEvent0:
    case FdKind::kProcDynamic:
      if (!t->path.empty()) {
        return OpenPathCommon(proc, kLinuxAT_FDCWD, t->path.c_str(), flags, 0);
      }
      return kENXIO;
    default:
      return kENXIO;  // sockets and anon inodes cannot be reopened
  }
}

}  // namespace

int64_t OpenPathCommon(LinuxProcess* proc, int dirfd, const char* user_path,
                       uint32_t flags, uint32_t mode) {
  if (!user_path) return kEFAULT;
  std::string raw(user_path);
  if (raw.empty()) return kENOENT;
  if ((flags & kLinuxO_TMPFILE) == kLinuxO_TMPFILE) {
    return kEOPNOTSUPP;  // unnamed temporary files are not supported
  }
  std::string norm;
  int64_t rc = ResolveAt(proc, dirfd, raw, &norm);
  if (rc < 0) return rc;
  {
    std::lock_guard<std::recursive_mutex> flock(proc->fd_mtx);
    proc->last_path = norm;
  }
  bool cloexec = (flags & kLinuxO_CLOEXEC) != 0;
  bool follow = (flags & kLinuxO_NOFOLLOW) == 0;
  bool opath = (flags & kLinuxO_PATH) != 0;
  if (opath) flags &= kLinuxO_PATH | kLinuxO_DIRECTORY | kLinuxO_NOFOLLOW |
                      kLinuxO_CLOEXEC;
  uint32_t acc = flags & kLinuxO_ACCMODE;
  bool want_write = !opath && (acc == kLinuxO_WRONLY || acc == kLinuxO_RDWR);
  bool want_read = !opath && (acc == kLinuxO_RDONLY || acc == kLinuxO_RDWR);
  uint32_t stored =
      flags & ~(kLinuxO_CREAT | kLinuxO_EXCL | kLinuxO_NOCTTY | kLinuxO_TRUNC |
                kLinuxO_CLOEXEC);

  norm = ExpandProcLinks(proc, norm, follow);

  // Synthesized /proc and /sys entries.
  auto ptype = Vfs::ProcLookup(proc, norm);
  if (ptype != Vfs::ProcEntryType::kNone) {
    if ((flags & kLinuxO_CREAT) && (flags & kLinuxO_EXCL)) return kEEXIST;
    if (ptype == Vfs::ProcEntryType::kSymlink) {
      if (!follow && !opath) return kELOOP;
      if (follow) return ReopenProcFd(proc, norm, flags, cloexec);
    }
    if (ptype == Vfs::ProcEntryType::kDir) {
      if (want_write || (flags & kLinuxO_CREAT)) return kEISDIR;
    } else if (ptype == Vfs::ProcEntryType::kFile) {
      if (flags & kLinuxO_DIRECTORY) return kENOTDIR;
      if (want_write || (flags & kLinuxO_TRUNC)) return kEACCES;
    }
    auto desc = NewDesc(FdKind::kProcDynamic, stored, norm);
    if (ptype == Vfs::ProcEntryType::kFile && !opath) {
      if (!Vfs::GenerateDynamicProcFile(proc, norm, &desc->dynamic_content)) {
        return kENOENT;
      }
    }
    desc->anon_ino = Vfs::ProcInode(norm);
    return InstallFd(proc, std::move(desc), cloexec);
  }

  VfsNode* node = nullptr;
  bool is_dev = false;
  uint64_t rdev = 0;
  {
    std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
    rc = Vfs::LookupErr(norm, follow, &node);
    if (rc == kENOENT && (flags & kLinuxO_CREAT)) {
      std::string create_path = norm;
      VfsNode* link = nullptr;
      if (follow && Vfs::LookupErr(norm, false, &link) == 0 &&
          link->type == VfsNodeType::kSymlink) {
        // Dangling symlink: O_CREAT creates its target (unless O_EXCL).
        if (flags & kLinuxO_EXCL) return kEEXIST;
        std::string dir = norm.substr(0, norm.rfind('/'));
        create_path = Vfs::NormalizePath(dir.empty() ? "/" : dir,
                                         link->symlink_target);
      }
      std::string base;
      VfsNode* parent = Vfs::LookupParent("/", create_path, &base);
      if (parent && !MayAccess(proc, parent, 3)) return kEACCES;
      rc = Vfs::MakeNode(create_path, VfsNodeType::kRegular,
                         mode & ~proc->umask_val & 07777, 0, &node);
      if (rc < 0) return rc;
      node->uid = proc->fsuid;
      node->gid = (parent && (parent->mode & 02000)) ? parent->gid : proc->fsgid;
    } else if (rc < 0) {
      return rc;
    } else {
      if ((flags & kLinuxO_CREAT) && (flags & kLinuxO_EXCL)) return kEEXIST;
      if (node->type == VfsNodeType::kSymlink && !opath) return kELOOP;
      if ((flags & kLinuxO_DIRECTORY) && node->type != VfsNodeType::kDirectory) {
        return kENOTDIR;
      }
      if (node->type == VfsNodeType::kDirectory &&
          (want_write || (flags & kLinuxO_CREAT))) {
        return kEISDIR;
      }
      if (!opath) {
        int mask = (want_read ? 4 : 0) | (want_write ? 2 : 0);
        if (!MayAccess(proc, node, mask)) return kEACCES;
        if (node->type == VfsNodeType::kSocket) return kENXIO;
        if (node->type == VfsNodeType::kCharDevice) {
          is_dev = true;
          rdev = node->rdev;
        } else if (node->type == VfsNodeType::kRegular &&
                   (flags & kLinuxO_TRUNC) && want_write) {
          node->data.clear();
          if (node->shared_vmo != ZX_HANDLE_INVALID) {
            Vfs::SyncNodeToSharedVmo(node);
          }
          Vfs::TouchModify(node);
        }
      }
    }
    if (!is_dev && !opath) Vfs::Notify(node, kInOpen);
  }
  if (is_dev) return OpenDevice(proc, norm, rdev, flags, cloexec);

  auto desc = NewDesc(FdKind::kVfsFile, stored, norm);
  desc->node = node;
  return InstallFd(proc, std::move(desc), cloexec);
}

// ---------------------------------------------------------------------------
// read / write
// ---------------------------------------------------------------------------

namespace {

ssize_t PipeRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                 size_t count) {
  auto pipe = desc->pipe;
  if (!pipe) return kEBADF;
  bool nonblock = (desc->flags & kLinuxO_NONBLOCK) != 0;
  uint8_t* dst = static_cast<uint8_t*>(buf);
  while (true) {
    uint64_t seq = CurrentPollSeq();
    {
      std::lock_guard<std::mutex> lock(pipe->mtx);
      if (!pipe->buf.empty()) {
        size_t n = std::min(count, pipe->buf.size());
        std::copy(pipe->buf.begin(), pipe->buf.begin() + n, dst);
        pipe->buf.erase(pipe->buf.begin(), pipe->buf.begin() + n);
        pipe->total_read += n;
        NotifyPollWaiters();
        return static_cast<ssize_t>(n);
      }
      if (pipe->writers <= 0) return 0;
      if (nonblock) return kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    WaitForPollEvent(10, seq);
  }
}

ssize_t PipeWrite(LinuxProcess* proc, FileDescription* desc, const void* buf,
                  size_t count) {
  auto pipe = desc->pipe;
  if (!pipe) return kEBADF;
  bool nonblock = (desc->flags & kLinuxO_NONBLOCK) != 0;
  const uint8_t* src = static_cast<const uint8_t*>(buf);
  size_t done = 0;
  while (true) {
    uint64_t seq = CurrentPollSeq();
    {
      std::lock_guard<std::mutex> lock(pipe->mtx);
      if (pipe->readers <= 0) {
        if (done > 0) return static_cast<ssize_t>(done);
        QueueSignalToThread(proc, t_current_thread, kSIGPIPE);
        return kEPIPE;
      }
      size_t space = pipe->capacity > pipe->buf.size()
                         ? pipe->capacity - pipe->buf.size()
                         : 0;
      size_t remaining = count - done;
      // Writes of at most PIPE_BUF bytes are atomic.
      bool atomic = count <= kPipeBuf;
      if (space > 0 && (!atomic || space >= remaining)) {
        size_t n = std::min(space, remaining);
        pipe->buf.insert(pipe->buf.end(), src + done, src + done + n);
        pipe->total_written += n;
        done += n;
        NotifyPollWaiters();
        if (done == count) return static_cast<ssize_t>(done);
      }
      if (nonblock) return done > 0 ? static_cast<ssize_t>(done) : kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) {
      return done > 0 ? static_cast<ssize_t>(done) : kERESTARTSYS;
    }
    WaitForPollEvent(10, seq);
  }
}

ssize_t VfsRead(FileDescription* desc, void* buf, size_t count, uint64_t pos,
                bool advance) {
  std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
  VfsNode* node = desc->node;
  if (!node) return kEBADF;
  if (node->type == VfsNodeType::kDirectory) return kEISDIR;
  if (node->type != VfsNodeType::kRegular) return kEINVAL;
  size_t n = 0;
  if (pos < node->data.size()) {
    n = std::min<size_t>(count, node->data.size() - pos);
    if (node->shared_vmo != ZX_HANDLE_INVALID) {
      if (node->shared_host_ptr != 0 &&
          pos + n <= node->shared_vmo_size) {
        memcpy(buf,
               reinterpret_cast<const uint8_t*>(node->shared_host_ptr) + pos,
               n);
      } else {
        zx_vmo_read(node->shared_vmo, buf, pos, n);
      }
    } else {
      memcpy(buf, node->data.data() + pos, n);
    }
  }
  if (advance) desc->offset = pos + n;
  if (n > 0) Vfs::Notify(node, kInAccess);
  return static_cast<ssize_t>(n);
}

ssize_t VfsWrite(FileDescription* desc, const void* buf, size_t count,
                 uint64_t pos, bool advance) {
  std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
  VfsNode* node = desc->node;
  if (!node) return kEBADF;
  if (node->type == VfsNodeType::kDirectory) return kEISDIR;
  if (node->type != VfsNodeType::kRegular) return kEINVAL;
  if (desc->flags & kLinuxO_APPEND) pos = node->data.size();
  if (pos > (1ULL << 40)) return kEFBIG;
  if (pos + count > node->data.size()) node->data.resize(pos + count, 0);
  if (count > 0) {
    memcpy(node->data.data() + pos, buf, count);
    if (node->shared_vmo != ZX_HANDLE_INVALID) {
      Vfs::EnsureSharedVmo(node, node->data.size());
      if (node->shared_host_ptr != 0 &&
          pos + count <= node->shared_vmo_size) {
        memcpy(reinterpret_cast<uint8_t*>(node->shared_host_ptr) + pos, buf,
               count);
      } else {
        zx_vmo_write(node->shared_vmo, buf, pos, count);
      }
    }
  }
  if (advance) desc->offset = pos + count;
  // Writing by a non-root user clears setuid/setgid like Linux.
  Vfs::TouchModify(node);
  return static_cast<ssize_t>(count);
}

ssize_t FbRead(FileDescription* desc, void* buf, size_t count, uint64_t pos) {
  const auto& fb = GetFbHardwareInfo();
  if (!fb.active) return kENODEV;
  if (pos >= fb.size_bytes) return 0;
  size_t n = std::min<size_t>(count, fb.size_bytes - pos);
  FbReadPixels(pos, buf, n);
  (void)desc;
  return static_cast<ssize_t>(n);
}

ssize_t FbWrite(FileDescription* desc, const void* buf, size_t count,
                uint64_t pos) {
  const auto& fb = GetFbHardwareInfo();
  if (!fb.active) return kENODEV;
  if (pos > fb.size_bytes) return kEFBIG;
  if (pos == fb.size_bytes) return kENOSPC;
  size_t n = std::min<size_t>(count, fb.size_bytes - pos);
  FbWritePixels(pos, buf, n);
  (void)desc;
  return static_cast<ssize_t>(n);
}

}  // namespace

bool IsSeekable(FileDescription* desc) {
  switch (desc->kind) {
    case FdKind::kVfsFile:
    case FdKind::kProcDynamic:
    case FdKind::kDevFb0:
    case FdKind::kDevNull:
    case FdKind::kDevZero:
    case FdKind::kDevFull:
    case FdKind::kDevRandom:
      return true;
    default:
      return false;
  }
}

ssize_t ReadFileDescription(LinuxProcess* proc, FileDescription* desc,
                            void* buf, size_t count) {
  if (!desc) return kEBADF;
  bool nonblock = (desc->flags & kLinuxO_NONBLOCK) != 0;
  switch (desc->kind) {
    case FdKind::kUnixSocket:
      return SocketRead(proc, desc, buf, count);
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
      return IpcRead(proc, desc, buf, count);
    case FdKind::kEpoll:
      return kEINVAL;
    default:
      break;
  }
  if (!Readable(desc)) return kEBADF;
  if (count == 0) return 0;
  switch (desc->kind) {
    case FdKind::kDevNull:
      return 0;
    case FdKind::kDevZero:
    case FdKind::kDevFull:
      memset(buf, 0, count);
      return static_cast<ssize_t>(count);
    case FdKind::kDevRandom:
      zx_cprng_draw(buf, count);
      return static_cast<ssize_t>(count);
    case FdKind::kSerialConsole:
      return TtySlaveRead(proc, GetConsoleTty(), buf, count, nonblock);
    case FdKind::kPtyMaster:
      return TtyMasterRead(proc, desc->pty, buf, count, nonblock);
    case FdKind::kPtySlave:
      return TtySlaveRead(proc, desc->pty, buf, count, nonblock);
    case FdKind::kDevFb0: {
      std::lock_guard<std::mutex> lock(desc->mtx);
      ssize_t n = FbRead(desc, buf, count, desc->offset);
      if (n > 0) desc->offset += static_cast<size_t>(n);
      return n;
    }
    case FdKind::kDevInputEvent0: {
      if (count < sizeof(LinuxInputEvent)) return kEINVAL;
      while (true) {
        size_t n = InputReadEvents(buf, count, false);
        if (n > 0) return static_cast<ssize_t>(n);
        if (nonblock) return kEAGAIN;
        if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
        WaitForPollEvent(10);
      }
    }
    case FdKind::kProcDynamic: {
      if (Vfs::ProcLookup(proc, desc->path) == Vfs::ProcEntryType::kDir) {
        return kEISDIR;
      }
      std::lock_guard<std::mutex> lock(desc->mtx);
      if (desc->offset >= desc->dynamic_content.size()) return 0;
      size_t n = std::min(count, desc->dynamic_content.size() - desc->offset);
      memcpy(buf, desc->dynamic_content.data() + desc->offset, n);
      desc->offset += n;
      return static_cast<ssize_t>(n);
    }
    case FdKind::kVfsFile: {
      std::lock_guard<std::mutex> dlock(desc->mtx);
      return VfsRead(desc, buf, count, desc->offset, true);
    }
    case FdKind::kPipeRead:
      return PipeRead(proc, desc, buf, count);
    case FdKind::kPipeWrite:
      return kEBADF;
    default:
      return kEINVAL;
  }
}

ssize_t WriteFileDescription(LinuxProcess* proc, FileDescription* desc,
                             const void* buf, size_t count) {
  if (!desc) return kEBADF;
  bool nonblock = (desc->flags & kLinuxO_NONBLOCK) != 0;
  switch (desc->kind) {
    case FdKind::kUnixSocket:
      return SocketWrite(proc, desc, buf, count);
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
      return IpcWrite(proc, desc, buf, count);
    case FdKind::kEpoll:
      return kEINVAL;
    default:
      break;
  }
  if (!Writable(desc)) return kEBADF;
  if (count == 0 && desc->kind != FdKind::kPipeWrite) return 0;
  switch (desc->kind) {
    case FdKind::kDevNull:
    case FdKind::kDevZero:
      return static_cast<ssize_t>(count);
    case FdKind::kDevFull:
      return kENOSPC;
    case FdKind::kDevRandom:
      // Mixed into the kernel CPRNG, as writes to /dev/(u)random are on Linux.
      for (size_t off = 0; off < count; off += ZX_CPRNG_ADD_ENTROPY_MAX_LEN) {
        size_t n = std::min<size_t>(ZX_CPRNG_ADD_ENTROPY_MAX_LEN, count - off);
        zx_cprng_add_entropy(static_cast<const uint8_t*>(buf) + off, n);
      }
      return static_cast<ssize_t>(count);
    case FdKind::kSerialConsole:
      return TtySlaveWrite(proc, GetConsoleTty(), buf, count, nonblock);
    case FdKind::kPtyMaster:
      if (!desc->pty) return kEBADF;
      TtyInput(desc->pty, static_cast<const uint8_t*>(buf), count);
      return static_cast<ssize_t>(count);
    case FdKind::kPtySlave:
      return TtySlaveWrite(proc, desc->pty, buf, count, nonblock);
    case FdKind::kDevFb0: {
      std::lock_guard<std::mutex> lock(desc->mtx);
      ssize_t n = FbWrite(desc, buf, count, desc->offset);
      if (n > 0) desc->offset += static_cast<size_t>(n);
      return n;
    }
    case FdKind::kDevInputEvent0:
      return kEINVAL;  // event injection into the PS/2 device is unsupported
    case FdKind::kVfsFile: {
      std::lock_guard<std::mutex> dlock(desc->mtx);
      return VfsWrite(desc, buf, count, desc->offset, true);
    }
    case FdKind::kPipeWrite:
      if (count == 0) return 0;
      return PipeWrite(proc, desc, buf, count);
    case FdKind::kPipeRead:
    case FdKind::kProcDynamic:
      return kEBADF;
    default:
      return kEINVAL;
  }
}

ssize_t PreadDesc(LinuxProcess* proc, FileDescription* desc, void* buf,
                  size_t count, uint64_t pos) {
  if (!IsSeekable(desc)) return kESPIPE;
  if (!Readable(desc)) return kEBADF;
  if (count == 0) return 0;
  switch (desc->kind) {
    case FdKind::kVfsFile:
      return VfsRead(desc, buf, count, pos, false);
    case FdKind::kDevFb0:
      return FbRead(desc, buf, count, pos);
    case FdKind::kProcDynamic: {
      if (Vfs::ProcLookup(proc, desc->path) == Vfs::ProcEntryType::kDir) {
        return kEISDIR;
      }
      std::lock_guard<std::mutex> lock(desc->mtx);
      if (pos >= desc->dynamic_content.size()) return 0;
      size_t n = std::min<size_t>(count, desc->dynamic_content.size() - pos);
      memcpy(buf, desc->dynamic_content.data() + pos, n);
      return static_cast<ssize_t>(n);
    }
    default:
      return ReadFileDescription(proc, desc, buf, count);  // null/zero/random
  }
}

ssize_t PwriteDesc(LinuxProcess* proc, FileDescription* desc, const void* buf,
                   size_t count, uint64_t pos) {
  if (!IsSeekable(desc)) return kESPIPE;
  if (!Writable(desc)) return kEBADF;
  if (count == 0) return 0;
  switch (desc->kind) {
    case FdKind::kVfsFile:
      return VfsWrite(desc, buf, count, pos, false);
    case FdKind::kDevFb0:
      return FbWrite(desc, buf, count, pos);
    default:
      return WriteFileDescription(proc, desc, buf, count);
  }
}

// ---------------------------------------------------------------------------
// Locks
// ---------------------------------------------------------------------------

int64_t FcntlLock(LinuxProcess* proc, FileDescription* desc, int cmd,
                  void* user_flock) {
  if (!user_flock) return kEFAULT;
  if (!desc->node || desc->kind != FdKind::kVfsFile) {
    return kEINVAL;  // record locks are supported on VFS files only
  }
  LinuxFlock* fl = static_cast<LinuxFlock*>(user_flock);
  bool ofd = (cmd == 36 || cmd == 37 || cmd == 38);
  bool getlk = (cmd == 5 || cmd == 36);
  bool wait = (cmd == 7 || cmd == 38);
  if (ofd && fl->l_pid != 0) return kEINVAL;
  short type = fl->l_type;
  if (type != kF_RDLCK && type != kF_WRLCK && type != kF_UNLCK) return kEINVAL;
  if (!getlk) {
    if (type == kF_RDLCK && !Readable(desc)) return kEBADF;
    if (type == kF_WRLCK && !Writable(desc)) return kEBADF;
  }
  int64_t base = 0;
  if (fl->l_whence == 1) {
    base = static_cast<int64_t>(desc->offset);
  } else if (fl->l_whence == 2) {
    std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
    base = static_cast<int64_t>(desc->node->data.size());
  } else if (fl->l_whence != 0) {
    return kEINVAL;
  }
  int64_t start = base + fl->l_start;
  uint64_t s, e;
  if (fl->l_len > 0) {
    if (start < 0) return kEINVAL;
    s = static_cast<uint64_t>(start);
    e = s + static_cast<uint64_t>(fl->l_len) - 1;
  } else if (fl->l_len == 0) {
    if (start < 0) return kEINVAL;
    s = static_cast<uint64_t>(start);
    e = UINT64_MAX;
  } else {
    int64_t ns = start + fl->l_len;
    if (ns < 0) return kEINVAL;
    s = static_cast<uint64_t>(ns);
    e = static_cast<uint64_t>(start) - 1;
  }
  uintptr_t owner = ofd ? reinterpret_cast<uintptr_t>(desc)
                        : static_cast<uintptr_t>(proc->pid);
  std::shared_ptr<LinuxProcess> self;
  {
    std::lock_guard<std::recursive_mutex> lock(g_proc_mtx);
    auto it = g_processes.find(proc->pid);
    if (it != g_processes.end()) self = it->second;
  }
  VfsNode* node = desc->node;
  while (true) {
    {
      std::lock_guard<std::mutex> lock(g_lock_mtx);
      auto& v = g_rec_locks[node];
      PruneDeadLocked(v);
      const RecLock* c = FindConflictLocked(v, owner, ofd,
                                            getlk ? type : type, s, e);
      if (getlk) {
        if (type == kF_UNLCK) return kEINVAL;
        if (c) {
          fl->l_type = c->type;
          fl->l_whence = 0;
          fl->l_start = static_cast<int64_t>(c->start);
          fl->l_len = c->end == UINT64_MAX
                          ? 0
                          : static_cast<int64_t>(c->end - c->start + 1);
          fl->l_pid = c->ofd ? -1 : c->pid;
        } else {
          fl->l_type = kF_UNLCK;
        }
        if (v.empty()) g_rec_locks.erase(node);
        return 0;
      }
      if (type == kF_UNLCK || !c) {
        RecLock nl{owner, ofd, ofd ? -1 : proc->pid, self, type, s, e};
        ApplyLockLocked(v, nl);
        if (v.empty()) g_rec_locks.erase(node);
        NotifyPollWaiters();
        return 0;
      }
      if (!wait) return kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    WaitForPollEvent(10);
  }
}

int64_t FlockOp(LinuxProcess* proc, FileDescription* desc, int op) {
  constexpr int kSh = 1, kEx = 2, kNb = 4, kUn = 8;
  int base = op & ~kNb;
  if (base != kSh && base != kEx && base != kUn) return kEINVAL;
  if (desc->flags & kLinuxO_PATH) return kEBADF;
  VfsNode* key = desc->node ? desc->node : reinterpret_cast<VfsNode*>(desc);
  while (true) {
    {
      std::lock_guard<std::mutex> lock(g_lock_mtx);
      auto& v = g_flocks[key];
      // Converting a lock first drops the existing one (Linux semantics).
      v.erase(std::remove_if(v.begin(), v.end(),
                             [&](const FlockEntry& f) { return f.owner == desc; }),
              v.end());
      if (base == kUn) {
        if (v.empty()) g_flocks.erase(key);
        NotifyPollWaiters();
        return 0;
      }
      bool conflict = false;
      for (const auto& f : v) {
        if (f.exclusive || base == kEx) conflict = true;
      }
      if (!conflict) {
        v.push_back({desc, base == kEx});
        return 0;
      }
      if (v.empty()) g_flocks.erase(key);
      if (op & kNb) return kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    WaitForPollEvent(10);
  }
}
