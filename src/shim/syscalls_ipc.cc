// AF_UNIX sockets, pipes, eventfd, timerfd, signalfd and inotify.
//
// Implements the "Provided by syscalls_ipc.cc" half of fd_ops.h plus the
// socket/pipe/eventfd/timerfd/signalfd/inotify system calls. Behaviour follows
// Linux (net/unix/af_unix.c, fs/eventfd.c, fs/timerfd.c, fs/signalfd.c,
// fs/notify/inotify); where something is not modelled the Linux error for
// "not supported" is returned instead of pretending success.
//
// Locking:
//  * g_sock_mtx guards every UnixSocketState, its packets and the bind maps.
//    No FileDescription / UnixSocketState is ever destroyed while it is held
//    (a destroyed socket description re-enters SocketOnDescriptionDestroyed):
//    such references are parked in a Graveyard declared before the lock.
//  * g_inotify_mtx guards inotify instances and the watch registry.
//  * Order: g_bind_mtx -> Vfs::Mutex() -> g_sock_mtx / g_inotify_mtx.
//    proc->fd_mtx (InstallFd/LookupFd) is never taken while holding ours.

#include <algorithm>
#include <climits>
#include <cstring>

#include "src/shim/fd_ops.h"
#include "src/shim/net_inet.h"
#include "src/shim/shim_common.h"
#include "src/shim/vfs.h"

// Called by the clock owner whenever CLOCK_REALTIME is set discontinuously
// (clock_settime, settimeofday, adjtimex ADJ_SETOFFSET): cancels timerfds
// armed with TFD_TIMER_ABSTIME|TFD_TIMER_CANCEL_ON_SET (Linux
// timerfd_clock_was_set). Until someone calls it such timers are simply
// never cancelled, which is correct as long as the clock is never set.
void TimerfdClockWasSet();

// ---------------------------------------------------------------------------
// IPC object state (eventfd / timerfd / signalfd / inotify)
// ---------------------------------------------------------------------------

struct InotifyWatchRec {
  VfsNode* node = nullptr;
  uint32_t mask = 0;  // event bits | IN_ONESHOT | IN_EXCL_UNLINK
};

struct InotifyEventRec {
  int32_t wd = 0;
  uint32_t mask = 0;
  uint32_t cookie = 0;
  std::string name;
};

struct IpcObjectState {
  FdKind kind = FdKind::kEventFd;
  std::mutex mtx;  // eventfd / timerfd fields
  std::atomic<uint64_t> activity{0};

  // eventfd
  uint64_t efd_count = 0;
  bool efd_semaphore = false;

  // timerfd (times in ns of `clockid`)
  int clockid = 1;
  bool armed = false;
  int64_t deadline_ns = 0;
  int64_t interval_ns = 0;
  uint64_t ticks = 0;        // expirations not yet read
  uint64_t total_ticks = 0;  // all expirations ever (edge counter)
  bool cancel_on_set = false;
  bool canceled = false;

  // signalfd
  std::atomic<uint64_t> sigmask{0};

  // inotify (guarded by g_inotify_mtx)
  std::map<int, InotifyWatchRec> watches;
  std::map<VfsNode*, int> wd_of_node;
  std::deque<InotifyEventRec> events;
  size_t queued_bytes = 0;
  int next_wd = 1;
  bool overflow_queued = false;
};

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// errno values not (yet) in shim_common.h.
constexpr int64_t kErrDom = -33;
constexpr int64_t kErrNoData = -61;
constexpr int64_t kErrMsgSize = -90;
constexpr int64_t kErrProtoType = -91;
constexpr int64_t kErrNoProtoOpt = -92;
constexpr int64_t kErrProtoNoSupport = -93;
constexpr int64_t kErrSockTNoSupport = -94;
constexpr int64_t kErrNoBufs = -105;
constexpr int64_t kErrCanceled = -125;

constexpr int kAfUnspec = 0;
constexpr int kAfUnix = 1;
constexpr int kAfMax = 46;
constexpr int kSockStream = 1;
constexpr int kSockDgram = 2;
constexpr int kSockRaw = 3;
constexpr int kSockSeqpacket = 5;
constexpr int kSockMax = 11;
constexpr uint32_t kSockTypeMask = 0xf;
constexpr uint32_t kSockNonblock = kLinuxO_NONBLOCK;
constexpr uint32_t kSockCloexec = kLinuxO_CLOEXEC;

constexpr int kMsgOob = 0x1;
constexpr int kMsgPeek = 0x2;
constexpr int kMsgCtrunc = 0x8;
constexpr int kMsgTrunc = 0x20;
constexpr int kMsgDontwait = 0x40;
constexpr int kMsgWaitall = 0x100;
constexpr int kMsgNosignal = 0x4000;
constexpr int kMsgWaitforone = 0x10000;
constexpr int kMsgCmsgCloexec = 0x40000000;

constexpr int kSolSocket = 1;
constexpr int kScmRights = 1;
constexpr int kScmCredentials = 2;
constexpr size_t kScmMaxFd = 253;
constexpr size_t kUioMaxIov = 1024;

// Linux defaults: net.core.{w,r}mem_{default,max}, SOCK_MIN_{SND,RCV}BUF on
// x86_64, net.unix.max_dgram_qlen, net.core.somaxconn.
constexpr int kSysctlWmemMax = 212992;
constexpr int kSysctlRmemMax = 212992;
constexpr int kSockMinSndbuf = 4608;
constexpr int kSockMinRcvbuf = 2304;
constexpr size_t kMaxDgramQlen = 512;
constexpr int kSomaxconn = 4096;

constexpr int kSigPipe = 13;
constexpr int64_t kPollSliceMs = 10;

constexpr uint64_t kSigBitKill = 1ULL << (9 - 1);
constexpr uint64_t kSigBitStop = 1ULL << (19 - 1);

// inotify
constexpr uint32_t kInAllEvents = 0x00000fff;
constexpr uint32_t kInUnmount = 0x00002000;
constexpr uint32_t kInQOverflow = 0x00004000;
constexpr uint32_t kInIgnored = 0x00008000;
constexpr uint32_t kInOnlyDir = 0x01000000;
constexpr uint32_t kInDontFollow = 0x02000000;
constexpr uint32_t kInExclUnlink = 0x04000000;
constexpr uint32_t kInMaskCreate = 0x10000000;
constexpr uint32_t kInMaskAdd = 0x20000000;
constexpr uint32_t kInIsDir = 0x40000000;
constexpr uint32_t kInOneShot = 0x80000000;
constexpr uint32_t kInAllInotifyBits =
    kInAllEvents | kInUnmount | kInQOverflow | kInIgnored | kInOnlyDir |
    kInDontFollow | kInExclUnlink | kInMaskAdd | kInMaskCreate | kInIsDir |
    kInOneShot;
// Events about a child that IN_EXCL_UNLINK suppresses once it is unlinked.
constexpr uint32_t kInChildEvents = kInAccess | kInModify | kInAttrib |
                                    kInCloseWrite | kInCloseNoWrite | kInOpen;
constexpr size_t kInotifyMaxQueuedEvents = 16384;  // fs.inotify.max_queued_events
constexpr size_t kInotifyEventHdr = 16;            // sizeof(struct inotify_event)

// clock ids
constexpr int kClockRealtime = 0;
constexpr int kClockMonotonic = 1;
constexpr int kClockBoottime = 7;
constexpr int kClockRealtimeAlarm = 8;
constexpr int kClockBoottimeAlarm = 9;

constexpr uint32_t kTfdTimerAbstime = 1;
constexpr uint32_t kTfdTimerCancelOnSet = 2;
constexpr uint32_t kEfdSemaphore = 1;

// ---------------------------------------------------------------------------
// Linux ABI structures
// ---------------------------------------------------------------------------

struct LinuxMsghdr {
  void* msg_name;
  uint32_t msg_namelen;
  uint32_t __pad0;
  LinuxIovec* msg_iov;
  size_t msg_iovlen;
  void* msg_control;
  size_t msg_controllen;
  uint32_t msg_flags;
  uint32_t __pad1;
};
static_assert(sizeof(LinuxMsghdr) == 56, "struct msghdr");

struct LinuxMmsghdr {
  LinuxMsghdr msg_hdr;
  uint32_t msg_len;
  uint32_t __pad;
};
static_assert(sizeof(LinuxMmsghdr) == 64, "struct mmsghdr");

struct LinuxCmsghdr {
  size_t cmsg_len;
  int cmsg_level;
  int cmsg_type;
};
static_assert(sizeof(LinuxCmsghdr) == 16, "struct cmsghdr");

struct LinuxUcred {
  int32_t pid;
  uint32_t uid;
  uint32_t gid;
};

struct LinuxTimeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct LinuxTimespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

struct LinuxItimerspec {
  LinuxTimespec it_interval;
  LinuxTimespec it_value;
};

struct LinuxSignalfdSiginfo {
  uint32_t ssi_signo;
  int32_t ssi_errno;
  int32_t ssi_code;
  uint32_t ssi_pid;
  uint32_t ssi_uid;
  int32_t ssi_fd;
  uint32_t ssi_tid;
  uint32_t ssi_band;
  uint32_t ssi_overrun;
  uint32_t ssi_trapno;
  int32_t ssi_status;
  int32_t ssi_int;
  uint64_t ssi_ptr;
  uint64_t ssi_utime;
  uint64_t ssi_stime;
  uint64_t ssi_addr;
  uint16_t ssi_addr_lsb;
  uint16_t __pad2;
  int32_t ssi_syscall;
  uint64_t ssi_call_addr;
  uint32_t ssi_arch;
  uint8_t __pad[28];
};
static_assert(sizeof(LinuxSignalfdSiginfo) == 128, "signalfd_siginfo");

constexpr size_t CmsgAlign(size_t n) { return (n + 7) & ~size_t{7}; }
constexpr size_t CmsgLen(size_t n) { return sizeof(LinuxCmsghdr) + n; }
constexpr size_t CmsgSpace(size_t n) {
  return sizeof(LinuxCmsghdr) + CmsgAlign(n);
}

// ---------------------------------------------------------------------------
// Generic helpers
// ---------------------------------------------------------------------------

// Blocking helper shared by every wait loop. The caller checks readiness
// with no locks held between attempts and calls Wait() when nothing is ready.
class Waiter {
 public:
  // timeo_ns: -1 = no timeout, 0 = never wait, >0 = SO_*TIMEO.
  Waiter(LinuxProcess* proc, bool nonblock, int64_t timeo_ns)
      : proc_(proc),
        nonblock_(nonblock || timeo_ns == 0),
        has_timeout_(timeo_ns > 0),
        last_seq_(CurrentPollSeq()) {
    if (has_timeout_) deadline_ = zx_clock_get_monotonic() + timeo_ns;
  }

  bool nonblock() const { return nonblock_; }

  // Returns 0 to re-check, -EAGAIN (non-blocking or timed out), or the
  // signal code: Linux sock_intr_errno() gives -ERESTARTSYS without a
  // timeout and -EINTR with one.
  int64_t Wait() {
    if (nonblock_) return kEAGAIN;
    if (CurrentThreadHasUnblockedSignal(proc_)) {
      return has_timeout_ ? kEINTR : kERESTARTSYS;
    }
    int64_t ms = kPollSliceMs;
    if (has_timeout_) {
      int64_t now = zx_clock_get_monotonic();
      if (now >= deadline_) return kEAGAIN;
      ms = std::min<int64_t>(ms, (deadline_ - now + 999999) / 1000000);
    }
    WaitForPollEvent(ms, last_seq_);
    last_seq_ = CurrentPollSeq();
    return 0;
  }

 private:
  LinuxProcess* proc_;
  bool nonblock_;
  bool has_timeout_;
  int64_t deadline_ = 0;
  uint64_t last_seq_ = 0;
};

void RaiseSigpipe(LinuxProcess* proc) {
  if (t_current_thread != nullptr) {
    QueueSignalToThread(proc, t_current_thread, kSigPipe);
  } else {
    QueueSignalToPid(proc->pid, kSigPipe);
  }
}

// Removes a just-installed fd again (used to undo the first fd of a pair
// when installing the second one fails).
void UninstallFd(LinuxProcess* proc, int fd) {
  std::shared_ptr<FileDescription> keep;
  {
    std::lock_guard<std::recursive_mutex> lock(proc->fd_mtx);
    auto it = proc->fds.find(fd);
    if (it == proc->fds.end()) return;
    keep = std::move(it->second.desc);
    proc->fds.erase(it);
  }
}

// Permission check against a VFS node with the process' fs credentials.
bool HasPerm(LinuxProcess* proc, const VfsNode* node, uint32_t want) {
  if (proc->fsuid == 0) return true;  // CAP_DAC_OVERRIDE
  uint32_t mode = node->mode & 0777;
  uint32_t bits;
  if (proc->fsuid == node->uid) {
    bits = mode >> 6;
  } else if (proc->fsgid == node->gid ||
             std::find(proc->groups.begin(), proc->groups.end(), node->gid) !=
                 proc->groups.end()) {
    bits = mode >> 3;
  } else {
    bits = mode;
  }
  return (bits & want) == want;
}

int64_t ValidateIov(const LinuxIovec* iov, size_t iovlen, size_t* total) {
  *total = 0;
  if (iovlen == 0) return 0;
  if (iov == nullptr) return kEFAULT;
  for (size_t i = 0; i < iovlen; ++i) {
    if (iov[i].iov_len == 0) continue;
    if (iov[i].iov_base == nullptr) return kEFAULT;
    if (iov[i].iov_len > static_cast<size_t>(SSIZE_MAX) - *total) {
      return kEINVAL;
    }
    *total += iov[i].iov_len;
  }
  return 0;
}

void IovGather(const LinuxIovec* iov, size_t iovlen, size_t total,
               std::vector<uint8_t>* out) {
  out->clear();
  out->reserve(total);
  for (size_t i = 0; i < iovlen; ++i) {
    const uint8_t* p = static_cast<const uint8_t*>(iov[i].iov_base);
    if (iov[i].iov_len) out->insert(out->end(), p, p + iov[i].iov_len);
  }
}

// Scatter `len` bytes into the iovec starting at byte `offset`.
void IovScatter(const LinuxIovec* iov, size_t iovlen, size_t offset,
                const uint8_t* src, size_t len) {
  for (size_t i = 0; i < iovlen && len > 0; ++i) {
    size_t l = iov[i].iov_len;
    if (offset >= l) {
      offset -= l;
      continue;
    }
    size_t n = std::min(l - offset, len);
    memcpy(static_cast<uint8_t*>(iov[i].iov_base) + offset, src, n);
    src += n;
    len -= n;
    offset = 0;
  }
}

// ---------------------------------------------------------------------------
// AF_UNIX state
// ---------------------------------------------------------------------------

std::mutex g_sock_mtx;
std::mutex g_bind_mtx;  // serializes bind() (outermost lock)
// Filesystem bindings: socket inode -> socket (Linux unix_find_socket_byinode).
std::map<VfsNode*, std::weak_ptr<UnixSocketState>> g_fs_bindings;
// Abstract bindings: (socket type, sun_path bytes incl. leading NUL). Linux
// hashes abstract names together with the socket type, so different types
// may bind the same name and connect only finds sockets of its own type.
std::map<std::pair<int, std::string>, std::weak_ptr<UnixSocketState>>
    g_abstract_bindings;
uint32_t g_autobind_ordernum = 0;
bool g_autobind_seeded = false;

struct Cred {
  int32_t pid = 0;
  uint32_t uid = 0;
  uint32_t gid = 0;
};

// Default SCM_CREDENTIALS of a sender: tgid and REAL uid/gid (Linux
// scm_send -> scm_set_cred(current_uid(), current_gid())).
Cred ScmCredsOf(LinuxProcess* p) { return Cred{p->pid, p->ruid, p->rgid}; }

// Linux init_peercred(): pid and effective ids + supplementary groups.
void SetPeerCredFrom(UnixSocketState* s, LinuxProcess* p) {
  s->has_peer_cred = true;
  s->peer_cred_pid = p->pid;
  s->peer_cred_uid = p->euid;
  s->peer_cred_gid = p->egid;
  s->peer_groups = p->groups;
}

void CopyPeerCred(UnixSocketState* dst, const UnixSocketState* src) {
  dst->has_peer_cred = src->has_peer_cred;
  dst->peer_cred_pid = src->peer_cred_pid;
  dst->peer_cred_uid = src->peer_cred_uid;
  dst->peer_cred_gid = src->peer_cred_gid;
  dst->peer_groups = src->peer_groups;
}

// References that must outlive g_sock_mtx (declare before the lock_guard).
struct Graveyard {
  std::vector<std::shared_ptr<UnixSocketState>> socks;
  std::vector<std::shared_ptr<FileDescription>> fds;
  std::deque<UnixSocketPacket> pkts;
};

std::shared_ptr<UnixSocketState> Pin(const std::weak_ptr<UnixSocketState>& w,
                                     Graveyard* g) {
  std::shared_ptr<UnixSocketState> s = w.lock();
  if (s) g->socks.push_back(s);
  return s;
}

bool WeakIs(const std::weak_ptr<UnixSocketState>& w,
            const std::shared_ptr<UnixSocketState>& s) {
  return s && !w.expired() && !w.owner_before(s) && !s.owner_before(w);
}

bool IsConnOriented(const UnixSocketState* s) {
  return s->type == kSockStream || s->type == kSockSeqpacket;
}

// Releases send-buffer charge for `n` consumed/dropped bytes of `p`.
void Uncharge(UnixSocketPacket& p, size_t n, Graveyard* g) {
  if (n == 0) return;
  std::shared_ptr<UnixSocketState> snd = Pin(p.sender, g);
  if (!snd) return;
  snd->wmem -= std::min(snd->wmem, n);
  snd->activity++;
}

// Linux unix_release_sock(). Caller holds g_sock_mtx.
void ReleaseLocked(const std::shared_ptr<UnixSocketState>& s, bool embryo,
                   Graveyard* g) {
  if (s->dead) return;
  s->dead = true;
  s->activity++;

  // Unbind. The filesystem inode itself stays (connect then gets
  // ECONNREFUSED), exactly like Linux.
  if (s->bound_node != nullptr) {
    auto it = g_fs_bindings.find(s->bound_node);
    if (it != g_fs_bindings.end() && WeakIs(it->second, s)) {
      g_fs_bindings.erase(it);
    }
    s->bound_node = nullptr;
  }
  if (!s->local_name.empty() && s->local_name[0] == '\0') {
    auto it = g_abstract_bindings.find({s->type, s->local_name});
    if (it != g_abstract_bindings.end() && WeakIs(it->second, s)) {
      g_abstract_bindings.erase(it);
    }
  }

  std::shared_ptr<UnixSocketState> skpair = Pin(s->peer, g);
  if (skpair && IsConnOriented(s.get())) {
    if (!s->rx_packets.empty() || embryo) skpair->so_error = 104;  // ECONNRESET
    skpair->rcv_shutdown = true;
    skpair->send_shutdown = true;
    skpair->activity++;
  }
  s->peer.reset();

  // Drop unread data (and in-flight fds, closed after the lock is dropped).
  while (!s->rx_packets.empty()) {
    UnixSocketPacket& p = s->rx_packets.front();
    Uncharge(p, p.data.size() - p.offset, g);
    g->pkts.push_back(std::move(p));
    s->rx_packets.pop_front();
  }
  // Pending connections of a listener are reset.
  while (!s->backlog.empty()) {
    std::shared_ptr<UnixSocketState> e = std::move(s->backlog.front());
    s->backlog.pop_front();
    ReleaseLocked(e, true, g);
    g->socks.push_back(std::move(e));
  }
}

// Parsed sockaddr_un.
struct ParsedAddr {
  bool abstract = false;
  std::string name;  // sun_path bytes as Linux stores them (path: + NUL)
  std::string path;  // filesystem path (non-abstract)
};

// move_addr_to_kernel() + unix_validate_addr() + unix_mkname_bsd().
int64_t ParseUnixAddr(const void* uaddr, int addrlen, ParsedAddr* out) {
  if (addrlen < 0 || addrlen > 128) return kEINVAL;
  if (addrlen > 0 && uaddr == nullptr) return kEFAULT;
  uint8_t buf[130] = {};
  if (addrlen > 0) memcpy(buf, uaddr, addrlen);
  if (addrlen <= 2 || addrlen > 110) return kEINVAL;
  uint16_t family = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
  if (family != kAfUnix) return kEINVAL;
  const char* sp = reinterpret_cast<const char*>(buf + 2);
  size_t n = static_cast<size_t>(addrlen) - 2;
  if (sp[0] == '\0') {
    out->abstract = true;
    out->name.assign(sp, n);
  } else {
    out->abstract = false;
    out->path.assign(sp, strnlen(sp, n));
    out->name = out->path;
    out->name.push_back('\0');
  }
  return 0;
}

// move_addr_to_user(): copies min(*ulen, klen) bytes and stores klen.
// `name` empty: klen = 2 (family only) if family_only_unnamed, else 0.
int64_t PutUnixAddr(const std::string& name, bool family_only_unnamed,
                    void* uaddr, void* ulen_ptr) {
  if (ulen_ptr == nullptr) return kEFAULT;
  uint8_t buf[2 + 128] = {};
  buf[0] = kAfUnix;
  size_t klen;
  if (name.empty()) {
    klen = family_only_unnamed ? 2 : 0;
  } else {
    size_t n = std::min<size_t>(name.size(), 128);
    memcpy(buf + 2, name.data(), n);
    klen = 2 + n;
  }
  int32_t len;
  memcpy(&len, ulen_ptr, sizeof(len));
  if (len < 0) return kEINVAL;
  size_t copy = std::min<size_t>(static_cast<size_t>(len), klen);
  if (copy > 0) {
    if (uaddr == nullptr) return kEFAULT;
    memcpy(uaddr, buf, copy);
  }
  int32_t k = static_cast<int32_t>(klen);
  memcpy(ulen_ptr, &k, sizeof(k));
  return 0;
}

// Linux unix_autobind(): abstract name "\0%05x". Caller holds g_sock_mtx.
int64_t AutobindLocked(const std::shared_ptr<UnixSocketState>& s) {
  if (!s->local_name.empty()) return 0;
  if (!g_autobind_seeded) {
    zx_cprng_draw(&g_autobind_ordernum, sizeof(g_autobind_ordernum));
    g_autobind_seeded = true;
  }
  for (uint32_t tries = 0; tries <= 0xFFFFF; ++tries) {
    g_autobind_ordernum = (g_autobind_ordernum + 1) & 0xFFFFF;
    char hex[8];
    snprintf(hex, sizeof(hex), "%05x", g_autobind_ordernum);
    std::string name(1, '\0');
    name.append(hex, 5);
    auto key = std::make_pair(s->type, name);
    auto it = g_abstract_bindings.find(key);
    if (it != g_abstract_bindings.end() && !it->second.expired()) continue;
    g_abstract_bindings[key] = s;
    s->local_name = name;
    return 0;
  }
  return kENOSPC;
}

// Resolves a socket path for connect/sendto. Caller holds Vfs::Mutex().
int64_t ResolveSocketPath(LinuxProcess* proc, const ParsedAddr& a,
                          VfsNode** out) {
  std::string abs = Vfs::NormalizePath(proc->cwd, a.path);
  VfsNode* node = nullptr;
  int64_t r = Vfs::LookupErr(abs, true, &node);
  if (r < 0) return r;
  if (node == nullptr) return kENOENT;
  if (!HasPerm(proc, node, 2)) return kEACCES;  // MAY_WRITE
  *out = node;
  return 0;
}

// Linux unix_find_other(). Caller holds g_sock_mtx (and Vfs::Mutex() if the
// address is a path, `node` being its resolution).
int64_t FindBoundLocked(const ParsedAddr& a, VfsNode* node, int type,
                        Graveyard* g, std::shared_ptr<UnixSocketState>* out) {
  std::shared_ptr<UnixSocketState> o;
  if (a.abstract) {
    auto it = g_abstract_bindings.find({type, a.name});
    if (it == g_abstract_bindings.end()) return kECONNREFUSED;
    o = Pin(it->second, g);
    if (!o || o->dead) return kECONNREFUSED;
  } else {
    if (node->type != VfsNodeType::kSocket) return kECONNREFUSED;
    auto it = g_fs_bindings.find(node);
    if (it == g_fs_bindings.end()) return kECONNREFUSED;
    o = Pin(it->second, g);
    if (!o || o->dead) return kECONNREFUSED;
    if (o->type != type) return kErrProtoType;
  }
  *out = std::move(o);
  return 0;
}

// Address of the peer: the peer socket's current address if it still
// exists, else the address captured at connect time.
std::string PeerNameLocked(UnixSocketState* s, Graveyard* g) {
  std::shared_ptr<UnixSocketState> p = Pin(s->peer, g);
  if (p) return p->local_name;
  return s->peer_name;
}

int64_t GetSocket(LinuxProcess* proc, uint64_t fd,
                  std::shared_ptr<FileDescription>* out) {
  std::shared_ptr<FileDescription> d = LookupFd(proc, static_cast<int>(fd));
  if (!d) return kEBADF;
  if (d->kind != FdKind::kUnixSocket || (!d->sock && !d->inet_sock)) {
    return kENOTSOCK;
  }
  *out = std::move(d);
  return 0;
}

std::shared_ptr<FileDescription> NewSocketDesc(
    std::shared_ptr<UnixSocketState> s, bool nonblock) {
  auto d = std::make_shared<FileDescription>();
  d->kind = FdKind::kUnixSocket;
  d->flags = kLinuxO_RDWR | (nonblock ? kLinuxO_NONBLOCK : 0);
  d->sock = std::move(s);
  d->anon_ino = AllocAnonInode();
  return d;
}

// ---------------------------------------------------------------------------
// Data path
// ---------------------------------------------------------------------------

struct SendArgs {
  std::vector<uint8_t> data;
  int flags = 0;
  bool has_addr = false;
  const void* uaddr = nullptr;
  int addrlen = 0;
  std::vector<std::shared_ptr<FileDescription>> fds;
  bool has_creds = false;
  Cred creds;
};

struct RecvOut {
  bool want_name = false;
  bool name_set = false;
  std::string name;
  bool has_creds = false;
  Cred creds;
  std::vector<std::shared_ptr<FileDescription>> fds;
  bool fds_present = false;
  bool passcred = false;  // receiver had SO_PASSCRED
  int msg_flags = 0;
};

// Linux __scm_send() for AF_UNIX.
int64_t ParseControl(LinuxProcess* proc, const uint8_t* ctl, size_t ctl_len,
                     SendArgs* a) {
  if (ctl_len < sizeof(LinuxCmsghdr)) return 0;  // CMSG_FIRSTHDR == NULL
  size_t off = 0;
  while (off + sizeof(LinuxCmsghdr) <= ctl_len) {
    LinuxCmsghdr h;
    memcpy(&h, ctl + off, sizeof(h));
    // CMSG_OK
    if (h.cmsg_len < sizeof(LinuxCmsghdr) || h.cmsg_len > ctl_len - off) {
      return kEINVAL;
    }
    if (h.cmsg_level == kSolSocket) {
      const uint8_t* data = ctl + off + sizeof(LinuxCmsghdr);
      size_t dlen = h.cmsg_len - sizeof(LinuxCmsghdr);
      if (h.cmsg_type == kScmRights) {
        size_t num = dlen / sizeof(int);
        if (num > 0) {
          if (num > kScmMaxFd || a->fds.size() + num > kScmMaxFd) {
            return kEINVAL;
          }
          for (size_t i = 0; i < num; ++i) {
            int fd;
            memcpy(&fd, data + i * sizeof(int), sizeof(int));
            std::shared_ptr<FileDescription> d = LookupFd(proc, fd);
            if (!d) return kEBADF;
            a->fds.push_back(std::move(d));
          }
        }
      } else if (h.cmsg_type == kScmCredentials) {
        if (h.cmsg_len != CmsgLen(sizeof(LinuxUcred))) return kEINVAL;
        LinuxUcred uc;
        memcpy(&uc, data, sizeof(uc));
        // scm_check_creds(); the shim has no capability sets, euid 0 stands
        // for CAP_SYS_ADMIN/CAP_SETUID/CAP_SETGID.
        if (uc.uid == UINT32_MAX || uc.gid == UINT32_MAX) return kEINVAL;
        bool root = proc->euid == 0;
        bool pid_ok = uc.pid == proc->pid || root;
        bool uid_ok = uc.uid == proc->ruid || uc.uid == proc->euid ||
                      uc.uid == proc->suid || root;
        bool gid_ok = uc.gid == proc->rgid || uc.gid == proc->egid ||
                      uc.gid == proc->sgid || root;
        if (!(pid_ok && uid_ok && gid_ok)) return kEPERM;
        if (uc.pid != proc->pid) {
          std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
          if (g_processes.find(uc.pid) == g_processes.end()) return kESRCH;
        }
        a->has_creds = true;
        a->creds = Cred{uc.pid, uc.uid, uc.gid};
      } else {
        return kEINVAL;
      }
    }
    off += CmsgAlign(h.cmsg_len);
  }
  return 0;
}

// unix_stream_sendmsg()
int64_t StreamSend(LinuxProcess* proc, const std::shared_ptr<UnixSocketState>& s,
                   bool nonblock, SendArgs* a) {
  int64_t timeo;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (a->has_addr) return s->connected ? kEISCONN : kEOPNOTSUPP;
    if (!s->connected) return kENOTCONN;
    timeo = s->sndtimeo_ns;
  }
  Waiter w(proc, nonblock, timeo);
  const Cred cr = a->has_creds ? a->creds : ScmCredsOf(proc);
  const size_t total = a->data.size();
  size_t sent = 0;
  bool fds_attached = false;
  while (true) {
    Graveyard g;
    bool pipe = false;
    bool done = false;
    bool progressed = false;
    {
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      std::shared_ptr<UnixSocketState> other = Pin(s->peer, &g);
      if (s->send_shutdown) {
        pipe = true;
      } else {
        while (true) {
          if (!other || other->dead || other->rcv_shutdown) {
            pipe = true;
            break;
          }
          if (sent >= total) {
            done = true;
            break;
          }
          // sock_alloc_send_pskb() waits while wmem >= sndbuf.
          if (s->wmem >= static_cast<size_t>(s->sndbuf)) break;
          size_t max_chunk =
              static_cast<size_t>(std::max(s->sndbuf / 2 - 64, 1));
          size_t chunk = std::min(total - sent, max_chunk);
          UnixSocketPacket p;
          p.data.assign(a->data.begin() + sent, a->data.begin() + sent + chunk);
          if (!fds_attached) {
            p.scm_fds = std::move(a->fds);  // fds ride on the first segment
            fds_attached = true;
          }
          p.sender_name = s->local_name;
          p.cred_pid = cr.pid;
          p.cred_uid = cr.uid;
          p.cred_gid = cr.gid;
          p.sender = s;
          other->rx_packets.push_back(std::move(p));
          other->activity++;
          s->wmem += chunk;
          sent += chunk;
          progressed = true;
        }
      }
    }
    if (progressed) NotifyPollWaiters();
    if (pipe) {
      if (sent == 0 && !(a->flags & kMsgNosignal)) RaiseSigpipe(proc);
      return sent ? static_cast<int64_t>(sent) : kEPIPE;
    }
    if (done) return static_cast<int64_t>(sent);
    int64_t werr = w.Wait();
    if (werr) return sent ? static_cast<int64_t>(sent) : werr;
  }
}

// unix_dgram_sendmsg() / unix_seqpacket_sendmsg()
int64_t DgramSend(LinuxProcess* proc, const std::shared_ptr<UnixSocketState>& s,
                  bool nonblock, SendArgs* a) {
  const bool seq = s->type == kSockSeqpacket;
  ParsedAddr addr;
  bool use_addr = false;
  if (seq) {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (!s->connected) return kENOTCONN;  // address is ignored
  } else if (a->has_addr) {
    int64_t r = ParseUnixAddr(a->uaddr, a->addrlen, &addr);
    if (r) return r;
    use_addr = true;
  }
  int64_t timeo;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (!use_addr && !s->connected) return kENOTCONN;
    if (s->passcred && s->local_name.empty()) {
      int64_t r = AutobindLocked(s);
      if (r) return r;
    }
    if (a->data.size() + 32 > static_cast<size_t>(s->sndbuf)) {
      return kErrMsgSize;
    }
    timeo = s->sndtimeo_ns;
  }
  Waiter w(proc, nonblock, timeo);
  const Cred cr = a->has_creds ? a->creds : ScmCredsOf(proc);
  const size_t len = a->data.size();
  while (true) {
    Graveyard g;
    int64_t rc = 0;
    bool wait = false;
    bool queued = false;
    {
      std::unique_lock<std::recursive_mutex> vl(Vfs::Mutex(), std::defer_lock);
      VfsNode* node = nullptr;
      int64_t lerr = 0;
      if (use_addr && !addr.abstract) {
        vl.lock();
        lerr = ResolveSocketPath(proc, addr, &node);
      }
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      if (s->wmem >= static_cast<size_t>(s->sndbuf)) {
        wait = true;
      } else {
        std::shared_ptr<UnixSocketState> other;
        if (use_addr) {
          rc = lerr ? lerr : FindBoundLocked(addr, node, s->type, &g, &other);
        } else {
          other = Pin(s->peer, &g);
        }
        if (rc == 0) {
          if (!other || other->dead) {
            if (seq) {
              rc = kEPIPE;
            } else {
              // Connected datagram peer is gone: disconnect, ECONNREFUSED.
              s->peer.reset();
              s->peer_name.clear();
              s->connected = false;
              rc = kECONNREFUSED;
            }
          } else if (other->connected && !WeakIs(other->peer, s)) {
            rc = kEPERM;  // unix_may_send(): other is connected elsewhere
          } else if (other->rcv_shutdown) {
            rc = kEPIPE;
          } else if (!WeakIs(other->peer, s) &&
                     other->rx_packets.size() > kMaxDgramQlen) {
            wait = true;  // unix_recvq_full()
          } else {
            UnixSocketPacket p;
            p.data = std::move(a->data);
            p.scm_fds = std::move(a->fds);
            p.sender_name = s->local_name;
            p.cred_pid = cr.pid;
            p.cred_uid = cr.uid;
            p.cred_gid = cr.gid;
            p.sender = s;
            other->rx_packets.push_back(std::move(p));
            other->activity++;
            s->wmem += len;
            rc = static_cast<int64_t>(len);
            queued = true;
          }
        }
      }
    }
    if (queued) NotifyPollWaiters();
    if (!wait) return rc;
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

int64_t SockSend(LinuxProcess* proc, FileDescription* desc, SendArgs* a) {
  if (a->flags & kMsgOob) return kEOPNOTSUPP;  // no CONFIG_AF_UNIX_OOB
  std::shared_ptr<UnixSocketState> s = desc->sock;
  bool nonblock =
      (desc->flags & kLinuxO_NONBLOCK) != 0 || (a->flags & kMsgDontwait) != 0;
  if (s->type == kSockStream) return StreamSend(proc, s, nonblock, a);
  return DgramSend(proc, s, nonblock, a);
}

// unix_stream_read_generic()
int64_t StreamRecv(LinuxProcess* proc, FileDescription* desc,
                   const LinuxIovec* iov, size_t iovlen, size_t total,
                   int flags, RecvOut* out) {
  std::shared_ptr<UnixSocketState> s = desc->sock;
  const bool peek = (flags & kMsgPeek) != 0;
  const bool nonblock =
      (desc->flags & kLinuxO_NONBLOCK) != 0 || (flags & kMsgDontwait) != 0;
  int64_t timeo;
  size_t target;
  size_t base_skip;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (!s->connected) return kEINVAL;
    timeo = s->rcvtimeo_ns;
    target = (flags & kMsgWaitall)
                 ? total
                 : std::min(static_cast<size_t>(s->rcvlowat), total);
    base_skip = (peek && s->peek_off >= 0) ? static_cast<size_t>(s->peek_off)
                                           : 0;
  }
  Waiter w(proc, nonblock, timeo);
  size_t copied = 0;
  bool check_creds = false;
  while (true) {
    Graveyard g;
    bool stop = false;
    bool consumed = false;
    int64_t err = 0;
    {
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      out->passcred = s->passcred;
      size_t skip = peek ? base_skip + copied : 0;
      size_t i = 0;
      while (i < s->rx_packets.size()) {
        UnixSocketPacket& p = s->rx_packets[i];
        size_t avail = p.data.size() - p.offset;
        if (peek && skip >= avail) {
          skip -= avail;
          ++i;
          continue;
        }
        // Never glue data from different writers when passing credentials.
        if (check_creds) {
          if (p.cred_pid != out->creds.pid || p.cred_uid != out->creds.uid ||
              p.cred_gid != out->creds.gid) {
            stop = true;
            break;
          }
        } else if (s->passcred) {
          out->creds = Cred{p.cred_pid, p.cred_uid, p.cred_gid};
          out->has_creds = true;
          check_creds = true;
        }
        if (out->want_name && !out->name_set) {
          out->name = p.sender_name;
          out->name_set = true;
        }
        size_t chunk = std::min(avail - skip, total - copied);
        IovScatter(iov, iovlen, copied, p.data.data() + p.offset + skip, chunk);
        copied += chunk;
        if (!peek) {
          p.offset += chunk;
          Uncharge(p, chunk, &g);
          consumed = true;
          if (s->peek_off >= 0) {
            s->peek_off = static_cast<int>(
                std::max<int64_t>(s->peek_off - static_cast<int64_t>(chunk), 0));
          }
          bool had_fds = !p.scm_fds.empty();
          if (had_fds) {
            out->fds_present = true;
            for (auto& f : p.scm_fds) out->fds.push_back(std::move(f));
            p.scm_fds.clear();
          }
          if (p.offset < p.data.size()) {
            stop = true;
            break;
          }
          g.pkts.push_back(std::move(p));
          s->rx_packets.pop_front();
          // fds are delivered with the data they were sent with; a read
          // never continues past a segment that carried fds.
          if (had_fds) {
            stop = true;
            break;
          }
        } else {
          if (s->peek_off >= 0) s->peek_off += static_cast<int>(chunk);
          if (!p.scm_fds.empty()) {
            out->fds_present = true;
            for (auto& f : p.scm_fds) out->fds.push_back(f);  // dup on peek
            stop = true;
            break;
          }
          skip = 0;
          ++i;
        }
        if (copied == total) {
          stop = true;
          break;
        }
      }
      if (!stop) {  // queue exhausted
        if (copied >= target) {
          stop = true;
        } else if (s->so_error) {
          err = -s->so_error;
          s->so_error = 0;
          stop = true;
        } else if (s->rcv_shutdown) {
          stop = true;
        }
      }
      if (consumed) s->activity++;
    }
    if (consumed) NotifyPollWaiters();
    if (stop) {
      if (err && copied == 0) return err;
      return static_cast<int64_t>(copied);
    }
    int64_t werr = w.Wait();
    if (werr) {
      if (werr != kEAGAIN && copied > 0) {
        // Linux scm_destroy()s what was gathered when a signal interrupts a
        // partially satisfied read.
        out->has_creds = false;
        out->fds_present = false;
        out->fds.clear();
      }
      return copied ? static_cast<int64_t>(copied) : werr;
    }
  }
}

// unix_dgram_recvmsg() / unix_seqpacket_recvmsg()
int64_t DgramRecv(LinuxProcess* proc, FileDescription* desc,
                  const LinuxIovec* iov, size_t iovlen, size_t total, int flags,
                  RecvOut* out) {
  std::shared_ptr<UnixSocketState> s = desc->sock;
  const bool seq = s->type == kSockSeqpacket;
  const bool peek = (flags & kMsgPeek) != 0;
  const bool nonblock =
      (desc->flags & kLinuxO_NONBLOCK) != 0 || (flags & kMsgDontwait) != 0;
  int64_t timeo;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (seq && !s->connected) return kENOTCONN;
    timeo = s->rcvtimeo_ns;
  }
  Waiter w(proc, nonblock, timeo);
  while (true) {
    Graveyard g;
    bool done = false;
    bool consumed = false;
    bool shut = false;
    int64_t ret = 0;
    {
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      out->passcred = s->passcred;
      size_t skip = (peek && s->peek_off >= 0)
                        ? static_cast<size_t>(s->peek_off)
                        : 0;
      size_t i = 0;
      while (i < s->rx_packets.size()) {
        size_t l = s->rx_packets[i].data.size();
        if (skip >= l && (l || skip)) {
          skip -= l;
          ++i;
          continue;
        }
        break;
      }
      if (i < s->rx_packets.size()) {
        UnixSocketPacket& p = s->rx_packets[i];
        size_t len = p.data.size() - skip;
        size_t n = std::min(len, total);
        if (n < len) out->msg_flags |= kMsgTrunc;
        IovScatter(iov, iovlen, 0, p.data.data() + skip, n);
        ret = static_cast<int64_t>((flags & kMsgTrunc) ? len : n);
        if (out->want_name) {
          out->name = p.sender_name;
          out->name_set = true;
        }
        if (s->passcred) {
          out->creds = Cred{p.cred_pid, p.cred_uid, p.cred_gid};
          out->has_creds = true;
        }
        if (!peek) {
          if (!p.scm_fds.empty()) {
            out->fds_present = true;
            for (auto& f : p.scm_fds) out->fds.push_back(std::move(f));
            p.scm_fds.clear();
          }
          Uncharge(p, p.data.size(), &g);
          if (s->peek_off >= 0) {
            s->peek_off = static_cast<int>(std::max<int64_t>(
                s->peek_off - static_cast<int64_t>(p.data.size()), 0));
          }
          g.pkts.push_back(std::move(p));
          s->rx_packets.erase(s->rx_packets.begin() + i);
          s->activity++;
          consumed = true;
        } else {
          if (!p.scm_fds.empty()) {
            out->fds_present = true;
            for (auto& f : p.scm_fds) out->fds.push_back(f);
          }
          if (s->peek_off >= 0) s->peek_off += static_cast<int>(n);
        }
        done = true;
      } else if (!w.nonblock()) {
        if (s->so_error) {
          ret = -s->so_error;
          s->so_error = 0;
          done = true;
        } else if (s->rcv_shutdown) {
          ret = 0;
          done = true;
        }
      }
      shut = s->rcv_shutdown;
    }
    if (consumed) NotifyPollWaiters();
    if (done) return ret;
    int64_t werr = w.Wait();
    if (werr) {
      if (werr == kEAGAIN && seq && shut) return 0;
      return werr;
    }
  }
}

int64_t SockRecv(LinuxProcess* proc, FileDescription* desc,
                 const LinuxIovec* iov, size_t iovlen, size_t total, int flags,
                 RecvOut* out) {
  if (flags & kMsgOob) return kEOPNOTSUPP;
  if (desc->sock->type == kSockStream) {
    return StreamRecv(proc, desc, iov, iovlen, total, flags, out);
  }
  return DgramRecv(proc, desc, iov, iovlen, total, flags, out);
}

// put_cmsg()
void PutCmsg(uint8_t** ctl, size_t* remaining, int* msg_flags, int level,
             int type, const void* data, size_t len) {
  if (*ctl == nullptr || *remaining < sizeof(LinuxCmsghdr)) {
    *msg_flags |= kMsgCtrunc;
    return;
  }
  size_t cmlen = CmsgLen(len);
  if (*remaining < cmlen) {
    *msg_flags |= kMsgCtrunc;
    cmlen = *remaining;
  }
  LinuxCmsghdr h{cmlen, level, type};
  memcpy(*ctl, &h, sizeof(h));
  memcpy(*ctl + sizeof(h), data, cmlen - sizeof(h));
  size_t adv = std::min(CmsgSpace(len), *remaining);
  *ctl += adv;
  *remaining -= adv;
}

// scm_recv() + scm_detach_fds(). Returns the number of control bytes used.
size_t DeliverControl(LinuxProcess* proc, void* control, size_t controllen,
                      int recv_flags, RecvOut* out) {
  if (control == nullptr) {
    if (out->passcred || out->fds_present) out->msg_flags |= kMsgCtrunc;
    out->fds.clear();
    return 0;
  }
  uint8_t* ctl = static_cast<uint8_t*>(control);
  size_t remaining = controllen;
  if (out->passcred) {
    // A read that returned no data carries no sender: Linux reports pid 0
    // and the overflow ids.
    LinuxUcred uc = out->has_creds
                        ? LinuxUcred{out->creds.pid, out->creds.uid,
                                     out->creds.gid}
                        : LinuxUcred{0, 65534, 65534};
    PutCmsg(&ctl, &remaining, &out->msg_flags, kSolSocket, kScmCredentials,
            &uc, sizeof(uc));
  }
  if (!out->fds.empty()) {
    size_t fdmax = remaining > sizeof(LinuxCmsghdr)
                       ? (remaining - sizeof(LinuxCmsghdr)) / sizeof(int)
                       : 0;
    bool cloexec = (recv_flags & kMsgCmsgCloexec) != 0;
    size_t i = 0;
    for (; i < fdmax && i < out->fds.size(); ++i) {
      int fd = InstallFd(proc, out->fds[i], cloexec);
      if (fd < 0) break;
      memcpy(ctl + sizeof(LinuxCmsghdr) + i * sizeof(int), &fd, sizeof(int));
    }
    if (i > 0) {
      LinuxCmsghdr h{CmsgLen(i * sizeof(int)), kSolSocket, kScmRights};
      memcpy(ctl, &h, sizeof(h));
      size_t adv = std::min(CmsgSpace(i * sizeof(int)), remaining);
      ctl += adv;
      remaining -= adv;
    }
    if (i < out->fds.size()) out->msg_flags |= kMsgCtrunc;
    out->fds.clear();  // fds that did not fit are discarded (closed)
  }
  return controllen - remaining;
}

int64_t DoSendmsg(LinuxProcess* proc, FileDescription* desc,
                  const LinuxMsghdr* umsg, int flags) {
  if (desc->inet_sock) {
    return InetDoSendmsg(proc, t_current_thread, desc,
                         reinterpret_cast<uintptr_t>(umsg), flags);
  }
  if (umsg == nullptr) return kEFAULT;
  LinuxMsghdr m;
  memcpy(&m, umsg, sizeof(m));
  if (m.msg_iovlen > kUioMaxIov) return kErrMsgSize;
  int namelen = static_cast<int>(m.msg_namelen);
  if (m.msg_name == nullptr) namelen = 0;
  if (namelen < 0) return kEINVAL;
  if (namelen > 128) namelen = 128;
  size_t total = 0;
  int64_t r = ValidateIov(m.msg_iov, m.msg_iovlen, &total);
  if (r) return r;
  if (m.msg_controllen > static_cast<size_t>(INT_MAX)) return kErrNoBufs;
  if (m.msg_controllen > 0 && m.msg_control == nullptr) return kEFAULT;
  SendArgs a;
  a.flags = flags;
  a.has_addr = namelen != 0;
  a.uaddr = m.msg_name;
  a.addrlen = namelen;
  r = ParseControl(proc, static_cast<const uint8_t*>(m.msg_control),
                   m.msg_controllen, &a);
  if (r) return r;
  IovGather(m.msg_iov, m.msg_iovlen, total, &a.data);
  return SockSend(proc, desc, &a);
}

int64_t DoRecvmsg(LinuxProcess* proc, FileDescription* desc, LinuxMsghdr* umsg,
                  int flags) {
  if (desc->inet_sock) {
    return InetDoRecvmsg(proc, t_current_thread, desc,
                         reinterpret_cast<uintptr_t>(umsg), flags);
  }
  if (umsg == nullptr) return kEFAULT;
  LinuxMsghdr m;
  memcpy(&m, umsg, sizeof(m));
  if (m.msg_iovlen > kUioMaxIov) return kErrMsgSize;
  if (m.msg_name != nullptr && static_cast<int>(m.msg_namelen) < 0) {
    return kEINVAL;
  }
  size_t total = 0;
  int64_t r = ValidateIov(m.msg_iov, m.msg_iovlen, &total);
  if (r) return r;
  RecvOut out;
  out.want_name = m.msg_name != nullptr;
  out.msg_flags = flags & kMsgCmsgCloexec;  // Linux echoes this flag back
  r = SockRecv(proc, desc, m.msg_iov, m.msg_iovlen, total, flags, &out);
  if (r < 0) return r;
  if (m.msg_name != nullptr) {
    int64_t e = PutUnixAddr(out.name_set ? out.name : std::string(), false,
                            m.msg_name, &umsg->msg_namelen);
    if (e) return e;
  }
  size_t used = DeliverControl(proc, m.msg_control, m.msg_controllen, flags,
                               &out);
  umsg->msg_controllen = used;
  umsg->msg_flags = static_cast<uint32_t>(out.msg_flags);
  return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// fd_ops.h: AF_UNIX hooks
// ---------------------------------------------------------------------------

ssize_t SocketRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                   size_t count) {
  if (desc && desc->inet_sock) {
    return InetSocketRead(proc, t_current_thread, desc, buf, count);
  }
  if (desc == nullptr || !desc->sock) return kEBADF;
  if (count == 0) return 0;  // sock_read_iter(): "Match SYS5 behaviour"
  if (buf == nullptr) return kEFAULT;
  LinuxIovec iov{buf, count};
  RecvOut out;
  return SockRecv(proc, desc, &iov, 1, count, 0, &out);
}

ssize_t SocketWrite(LinuxProcess* proc, FileDescription* desc, const void* buf,
                    size_t count) {
  if (desc && desc->inet_sock) {
    return InetSocketWrite(proc, t_current_thread, desc, buf, count);
  }
  if (desc == nullptr || !desc->sock) return kEBADF;
  if (count > 0 && buf == nullptr) return kEFAULT;
  SendArgs a;
  const uint8_t* p = static_cast<const uint8_t*>(buf);
  if (count) a.data.assign(p, p + count);
  return SockSend(proc, desc, &a);
}

uint32_t SocketPollEvents(FileDescription* desc) {
  if (desc && desc->inet_sock) {
    return static_cast<uint32_t>(InetSocketPollEvents(desc, nullptr)) & 0xFFFFU;
  }
  if (desc == nullptr || !desc->sock) return kPollNval;
  std::shared_ptr<UnixSocketState> s = desc->sock;
  Graveyard g;
  std::lock_guard<std::mutex> lk(g_sock_mtx);
  uint32_t mask = 0;
  const bool listening = s->listening;
  const bool closed_state = !listening && !s->connected;  // TCP_CLOSE
  if (s->so_error) mask |= kPollErr;
  if (s->rcv_shutdown && s->send_shutdown) mask |= kPollHup;
  if (s->rcv_shutdown) mask |= kPollRdHup | kPollIn | kPollRdNorm;
  bool readable = listening ? !s->backlog.empty() : !s->rx_packets.empty();
  if (readable) mask |= kPollIn | kPollRdNorm;
  constexpr uint32_t kPollWrBand = 0x0200;
  // unix_writable(): wmem * 4 <= sndbuf and not listening.
  bool writable = !listening && (s->wmem << 2) <= static_cast<size_t>(s->sndbuf);
  if (s->type == kSockStream) {
    if (closed_state) mask |= kPollHup;
  } else {
    if (s->type == kSockSeqpacket && closed_state) mask |= kPollHup;
    if (writable && s->connected) {
      std::shared_ptr<UnixSocketState> other = Pin(s->peer, &g);
      if (other && !WeakIs(other->peer, s) &&
          other->rx_packets.size() > kMaxDgramQlen) {
        writable = false;
      }
    }
  }
  if (writable) mask |= kPollOut | kPollWrNorm | kPollWrBand;
  return mask;
}

int SocketBytesAvailable(FileDescription* desc) {
  if (desc && desc->inet_sock) return InetSocketBytesAvailable(desc);
  if (desc == nullptr || !desc->sock) return static_cast<int>(kEBADF);
  std::shared_ptr<UnixSocketState> s = desc->sock;
  std::lock_guard<std::mutex> lk(g_sock_mtx);
  if (s->listening) return static_cast<int>(kEINVAL);
  if (s->type == kSockDgram) {
    if (s->rx_packets.empty()) return 0;
    return static_cast<int>(
        std::min<size_t>(s->rx_packets.front().data.size(), INT_MAX));
  }
  size_t amount = 0;
  for (const auto& p : s->rx_packets) amount += p.data.size() - p.offset;
  return static_cast<int>(std::min<size_t>(amount, INT_MAX));
}

void SocketOnDescriptionDestroyed(FileDescription* desc) {
  if (desc && desc->inet_sock) {
    InetSocketOnDescriptionDestroyed(desc);
    return;
  }
  if (desc == nullptr || !desc->sock) return;
  Graveyard g;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    ReleaseLocked(desc->sock, false, &g);
  }
  NotifyPollWaiters();
}

void SocketPathUnlinked(VfsNode* node) {
  // The socket keeps its name (getsockname still reports it); only the
  // inode -> socket association used by connect() goes away.
  std::lock_guard<std::mutex> lk(g_sock_mtx);
  g_fs_bindings.erase(node);
}

namespace {

// ---------------------------------------------------------------------------
// Socket system calls
// ---------------------------------------------------------------------------

// Common type/flags/family validation of socket() and socketpair().
int64_t CheckSocketArgs(int64_t domain, uint32_t type_arg, int protocol,
                        int* out_type, bool* nonblock, bool* cloexec) {
  uint32_t flags = type_arg & ~kSockTypeMask;
  if (flags & ~(kSockCloexec | kSockNonblock)) return kEINVAL;
  int type = static_cast<int>(type_arg & kSockTypeMask);
  if (domain < 0 || domain >= kAfMax) return kEAFNOSUPPORT;
  if (type >= kSockMax) return kEINVAL;
  // Only AF_UNIX is implemented: no IP, netlink, packet, ... stack exists.
  if (domain != kAfUnix) return kEAFNOSUPPORT;
  if (protocol != 0 && protocol != kAfUnix) return kErrProtoNoSupport;
  switch (type) {
    case kSockStream:
    case kSockDgram:
    case kSockSeqpacket:
      break;
    case kSockRaw:
      type = kSockDgram;  // Linux unix_create() maps SOCK_RAW to SOCK_DGRAM
      break;
    default:
      return kErrSockTNoSupport;
  }
  *out_type = type;
  *nonblock = (flags & kSockNonblock) != 0;
  *cloexec = (flags & kSockCloexec) != 0;
  return 0;
}

int64_t SysSocket(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2) {
  int domain = static_cast<int>(a0);
  if (domain == 2 || domain == 16) {
    return InetSysSocket(proc, domain, static_cast<uint32_t>(a1),
                         static_cast<int>(a2));
  }
  int type;
  bool nonblock, cloexec;
  int64_t r = CheckSocketArgs(domain, static_cast<uint32_t>(a1),
                              static_cast<int>(a2), &type, &nonblock, &cloexec);
  if (r) return r;
  auto s = std::make_shared<UnixSocketState>();
  s->domain = kAfUnix;
  s->type = type;
  return InstallFd(proc, NewSocketDesc(std::move(s), nonblock), cloexec);
}

int64_t SysSocketpair(LinuxProcess* proc, uint64_t a0, uint64_t a1,
                      uint64_t a2, uint64_t a3) {
  int type;
  bool nonblock, cloexec;
  int64_t r = CheckSocketArgs(static_cast<int>(a0), static_cast<uint32_t>(a1),
                              static_cast<int>(a2), &type, &nonblock, &cloexec);
  if (r) return r;
  int* sv = reinterpret_cast<int*>(a3);
  if (sv == nullptr) return kEFAULT;
  auto s0 = std::make_shared<UnixSocketState>();
  auto s1 = std::make_shared<UnixSocketState>();
  for (auto* s : {s0.get(), s1.get()}) {
    s->domain = kAfUnix;
    s->type = type;
    s->connected = true;
    SetPeerCredFrom(s, proc);
  }
  s0->peer = s1;
  s1->peer = s0;
  int fd0 = InstallFd(proc, NewSocketDesc(s0, nonblock), cloexec);
  if (fd0 < 0) return fd0;
  int fd1 = InstallFd(proc, NewSocketDesc(s1, nonblock), cloexec);
  if (fd1 < 0) {
    UninstallFd(proc, fd0);
    return fd1;
  }
  sv[0] = fd0;
  sv[1] = fd1;
  return 0;
}

int64_t SysBind(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) return InetSysBind(proc, desc.get(), a1, a2);
  std::shared_ptr<UnixSocketState> s = desc->sock;
  int addrlen = static_cast<int>(a2);
  const void* uaddr = reinterpret_cast<const void*>(a1);
  if (addrlen < 0 || addrlen > 128) return kEINVAL;
  if (addrlen > 0 && uaddr == nullptr) return kEFAULT;
  if (addrlen == 2) {
    uint16_t fam;
    memcpy(&fam, uaddr, sizeof(fam));
    if (fam == kAfUnix) {  // autobind
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      return AutobindLocked(s);
    }
  }
  ParsedAddr addr;
  r = ParseUnixAddr(uaddr, addrlen, &addr);
  if (r) return r;

  if (addr.abstract) {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (!s->local_name.empty()) return kEINVAL;
    auto key = std::make_pair(s->type, addr.name);
    auto it = g_abstract_bindings.find(key);
    if (it != g_abstract_bindings.end() && !it->second.expired()) {
      return kEADDRINUSE;
    }
    g_abstract_bindings[key] = s;
    s->local_name = addr.name;
    s->activity++;
    return 0;
  }

  std::lock_guard<std::mutex> bl(g_bind_mtx);
  std::string abs = Vfs::NormalizePath(proc->cwd, addr.path);
  std::lock_guard<std::recursive_mutex> vl(Vfs::Mutex());
  VfsNode* node = nullptr;
  r = Vfs::MakeNode(abs, VfsNodeType::kSocket, 0777 & ~proc->umask_val, 0,
                    &node);
  if (r == kEEXIST) return kEADDRINUSE;
  if (r < 0) return r;
  if (node == nullptr) return kEIO;
  bool already_bound;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    already_bound = !s->local_name.empty();
    if (!already_bound) {
      s->local_name = addr.name;
      s->bound_node = node;
      g_fs_bindings[node] = s;
      s->activity++;
    }
  }
  if (already_bound) {
    // Linux creates the inode first and removes it again on this error.
    Vfs::Unlink("/", abs, false);
    return kEINVAL;
  }
  return 0;
}

int64_t SysListen(LinuxProcess* proc, uint64_t a0, uint64_t a1) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) return InetSysListen(desc.get(), static_cast<int>(a1));
  std::shared_ptr<UnixSocketState> s = desc->sock;
  int backlog = static_cast<int>(a1);
  if (static_cast<unsigned>(backlog) > static_cast<unsigned>(kSomaxconn)) {
    backlog = kSomaxconn;
  }
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (s->type != kSockStream && s->type != kSockSeqpacket) {
      return kEOPNOTSUPP;
    }
    if (s->local_name.empty()) return kEINVAL;  // no listen on unbound socket
    if (s->connected) return kEINVAL;
    s->max_backlog = backlog;
    s->listening = true;
    SetPeerCredFrom(s.get(), proc);
    s->activity++;
  }
  NotifyPollWaiters();  // a larger backlog may unblock connectors
  return 0;
}

int64_t StreamConnect(LinuxProcess* proc, FileDescription* desc,
                      const ParsedAddr& addr) {
  std::shared_ptr<UnixSocketState> s = desc->sock;
  int64_t timeo;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (s->passcred && s->local_name.empty()) {
      int64_t r = AutobindLocked(s);
      if (r) return r;
    }
    timeo = s->sndtimeo_ns;
  }
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, timeo);
  while (true) {
    Graveyard g;
    int64_t rc = 0;
    bool wait = false;
    {
      std::unique_lock<std::recursive_mutex> vl(Vfs::Mutex(), std::defer_lock);
      VfsNode* node = nullptr;
      int64_t lerr = 0;
      if (!addr.abstract) {
        vl.lock();
        lerr = ResolveSocketPath(proc, addr, &node);
      }
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      std::shared_ptr<UnixSocketState> other;
      rc = lerr ? lerr : FindBoundLocked(addr, node, s->type, &g, &other);
      if (rc == 0) {
        if (!other->listening || other->rcv_shutdown) {
          rc = kECONNREFUSED;
        } else if (other->backlog.size() >
                   static_cast<size_t>(other->max_backlog)) {
          wait = true;  // unix_recvq_full(): connect blocks / EAGAIN
        } else if (s->connected) {
          rc = kEISCONN;
        } else if (s->listening || s->dead) {
          rc = kEINVAL;
        } else {
          auto ns = std::make_shared<UnixSocketState>();
          ns->domain = kAfUnix;
          ns->type = s->type;
          ns->connected = true;
          ns->local_name = other->local_name;  // accepted socket shares it
          ns->peer = s;
          ns->peer_name = s->local_name;
          SetPeerCredFrom(ns.get(), proc);
          s->peer = ns;
          s->peer_name = other->local_name;
          s->connected = true;
          CopyPeerCred(s.get(), other.get());
          s->activity++;
          other->backlog.push_back(std::move(ns));
          other->activity++;
        }
      }
    }
    if (!wait) {
      if (rc == 0) NotifyPollWaiters();
      return rc;
    }
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

int64_t SysConnect(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysConnect(proc, t_current_thread, desc.get(), a1,
                          static_cast<int>(a2));
  }
  std::shared_ptr<UnixSocketState> s = desc->sock;
  int addrlen = static_cast<int>(a2);
  const void* uaddr = reinterpret_cast<const void*>(a1);
  if (addrlen < 0 || addrlen > 128) return kEINVAL;
  if (addrlen > 0 && uaddr == nullptr) return kEFAULT;

  if (s->type == kSockDgram) {
    if (addrlen < 2) return kEINVAL;
    uint16_t fam;
    memcpy(&fam, uaddr, sizeof(fam));
    if (fam == kAfUnspec) {  // dissolve the association
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      s->peer.reset();
      s->peer_name.clear();
      s->connected = false;
      s->has_peer_cred = false;
      s->activity++;
      return 0;
    }
    ParsedAddr addr;
    r = ParseUnixAddr(uaddr, addrlen, &addr);
    if (r) return r;
    Graveyard g;
    std::unique_lock<std::recursive_mutex> vl(Vfs::Mutex(), std::defer_lock);
    VfsNode* node = nullptr;
    if (!addr.abstract) {
      vl.lock();
      r = ResolveSocketPath(proc, addr, &node);
      if (r) return r;
    }
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (s->passcred && s->local_name.empty()) {
      r = AutobindLocked(s);
      if (r) return r;
    }
    std::shared_ptr<UnixSocketState> other;
    r = FindBoundLocked(addr, node, s->type, &g, &other);
    if (r) return r;
    if (other->connected && !WeakIs(other->peer, s)) return kEPERM;
    s->peer = other;
    s->peer_name = other->local_name;
    s->connected = true;
    s->activity++;
    return 0;
  }

  ParsedAddr addr;
  r = ParseUnixAddr(uaddr, addrlen, &addr);
  if (r) return r;
  return StreamConnect(proc, desc.get(), addr);
}

int64_t SysAccept(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                  uint32_t flags) {
  if (flags & ~(kSockCloexec | kSockNonblock)) return kEINVAL;
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysAccept(proc, t_current_thread, desc.get(), a1, a2,
                         static_cast<int>(flags));
  }
  std::shared_ptr<UnixSocketState> s = desc->sock;
  void* uaddr = reinterpret_cast<void*>(a1);
  void* ulen = reinterpret_cast<void*>(a2);
  if (uaddr != nullptr) {
    if (ulen == nullptr) return kEFAULT;
    int32_t l;
    memcpy(&l, ulen, sizeof(l));
    if (l < 0) return kEINVAL;
  }
  int64_t timeo;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (s->type != kSockStream && s->type != kSockSeqpacket) {
      return kEOPNOTSUPP;
    }
    if (!s->listening) return kEINVAL;
    timeo = s->rcvtimeo_ns;
  }
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, timeo);
  std::shared_ptr<UnixSocketState> ns;
  while (true) {
    int64_t err = 0;
    {
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      if (!s->backlog.empty()) {
        ns = std::move(s->backlog.front());
        s->backlog.pop_front();
        s->activity++;
        // unix_sock_inherit_flags()
        ns->passcred = s->passcred;
        ns->passsec = s->passsec;
      } else if (s->so_error) {
        err = -s->so_error;
        s->so_error = 0;
      } else if (s->rcv_shutdown || s->dead || !s->listening) {
        err = kEINVAL;
      }
    }
    if (ns) break;
    if (err) return err;
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
  NotifyPollWaiters();  // backlog space for blocked connectors

  std::shared_ptr<FileDescription> nd =
      NewSocketDesc(ns, (flags & kSockNonblock) != 0);
  int fd = InstallFd(proc, nd, (flags & kSockCloexec) != 0);
  if (fd < 0) {
    // Put the connection back (Linux reserves the fd before dequeuing).
    nd->sock.reset();
    Graveyard g;
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (s->listening && !s->dead) {
      s->backlog.push_front(ns);
    } else {
      ReleaseLocked(ns, true, &g);
    }
    return fd;
  }
  if (uaddr != nullptr) {
    std::string pname;
    {
      Graveyard g;
      std::lock_guard<std::mutex> lk(g_sock_mtx);
      pname = PeerNameLocked(ns.get(), &g);
    }
    PutUnixAddr(pname, true, uaddr, ulen);
  }
  return fd;
}

int64_t SysGetName(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                   bool peer) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) return InetSysGetName(proc, desc.get(), a1, a2, peer);
  std::shared_ptr<UnixSocketState> s = desc->sock;
  std::string name;
  {
    Graveyard g;
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (peer) {
      if (!s->connected) return kENOTCONN;
      name = PeerNameLocked(s.get(), &g);
    } else {
      name = s->local_name;
    }
  }
  return PutUnixAddr(name, true, reinterpret_cast<void*>(a1),
                     reinterpret_cast<void*>(a2));
}

int64_t SysShutdown(LinuxProcess* proc, uint64_t a0, uint64_t a1) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) return InetSysShutdown(desc.get(), static_cast<int>(a1));
  int how = static_cast<int>(a1);
  if (how < 0 || how > 2) return kEINVAL;
  // SHUT_RD -> RCV_SHUTDOWN, SHUT_WR -> SEND_SHUTDOWN, SHUT_RDWR -> both.
  // (Linux unix_shutdown() does not require a connected socket.)
  bool rd = (how == 0 || how == 2);
  bool wr = (how == 1 || how == 2);
  std::shared_ptr<UnixSocketState> s = desc->sock;
  {
    Graveyard g;
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (rd) s->rcv_shutdown = true;
    if (wr) s->send_shutdown = true;
    s->activity++;
    std::shared_ptr<UnixSocketState> other = Pin(s->peer, &g);
    if (other && IsConnOriented(s.get())) {
      if (rd) other->send_shutdown = true;
      if (wr) other->rcv_shutdown = true;
      other->activity++;
    }
  }
  NotifyPollWaiters();
  return 0;
}

int64_t SysSendto(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                  uint64_t a3, uint64_t a4, uint64_t a5) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysSendto(proc, t_current_thread, desc.get(), a1,
                         static_cast<size_t>(a2), static_cast<int>(a3), a4,
                         static_cast<int>(a5));
  }
  SendArgs a;
  a.flags = static_cast<int>(a3);
  if (a4 != 0) {
    int al = static_cast<int>(a5);
    if (al < 0 || al > 128) return kEINVAL;
    a.uaddr = reinterpret_cast<const void*>(a4);
    a.addrlen = al;
    a.has_addr = al != 0;
  }
  const uint8_t* buf = reinterpret_cast<const uint8_t*>(a1);
  size_t len = static_cast<size_t>(a2);
  if (len > 0 && buf == nullptr) return kEFAULT;
  if (len) a.data.assign(buf, buf + len);
  return SockSend(proc, desc.get(), &a);
}

int64_t SysRecvfrom(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4, uint64_t a5) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysRecvfrom(proc, t_current_thread, desc.get(), a1,
                           static_cast<size_t>(a2), static_cast<int>(a3), a4,
                           a5);
  }
  void* buf = reinterpret_cast<void*>(a1);
  size_t len = static_cast<size_t>(a2);
  if (len > 0 && buf == nullptr) return kEFAULT;
  void* uaddr = reinterpret_cast<void*>(a4);
  void* ulen = reinterpret_cast<void*>(a5);
  if (uaddr != nullptr) {
    if (ulen == nullptr) return kEFAULT;
    int32_t l;
    memcpy(&l, ulen, sizeof(l));
    if (l < 0) return kEINVAL;
  }
  LinuxIovec iov{buf, len};
  RecvOut out;
  out.want_name = uaddr != nullptr;
  int flags = static_cast<int>(a3);
  r = SockRecv(proc, desc.get(), &iov, len ? 1 : 0, len, flags, &out);
  if (r >= 0 && uaddr != nullptr) {
    int64_t e = PutUnixAddr(out.name_set ? out.name : std::string(), false,
                            uaddr, ulen);
    if (e) return e;
  }
  return r;  // no control buffer: passed fds are discarded (closed)
}

int64_t SysSendmmsg(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint64_t a3) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  auto* vec = reinterpret_cast<LinuxMmsghdr*>(a1);
  size_t vlen = std::min<size_t>(static_cast<uint32_t>(a2), kUioMaxIov);
  int flags = static_cast<int>(a3);
  if (vlen > 0 && vec == nullptr) return kEFAULT;
  size_t n = 0;
  int64_t err = 0;
  for (; n < vlen; ++n) {
    err = DoSendmsg(proc, desc.get(), &vec[n].msg_hdr, flags);
    if (err < 0) break;
    vec[n].msg_len = static_cast<uint32_t>(err);
  }
  if (n == 0) return err;
  return static_cast<int64_t>(n);
}

int64_t SysRecvmmsg(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  auto* vec = reinterpret_cast<LinuxMmsghdr*>(a1);
  size_t vlen = std::min<size_t>(static_cast<uint32_t>(a2), kUioMaxIov);
  int flags = static_cast<int>(a3);
  auto* uts = reinterpret_cast<LinuxTimespec*>(a4);
  int64_t deadline = 0;
  if (uts != nullptr) {
    LinuxTimespec ts;
    memcpy(&ts, uts, sizeof(ts));
    if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
      return kEINVAL;
    }
    deadline = zx_clock_get_monotonic() + ts.tv_sec * 1000000000LL + ts.tv_nsec;
  }
  if (vlen > 0 && vec == nullptr) return kEFAULT;
  size_t n = 0;
  int64_t err = 0;
  // As in Linux, the timeout is only checked after each received datagram.
  for (; n < vlen; ++n) {
    err = DoRecvmsg(proc, desc.get(), &vec[n].msg_hdr, flags);
    if (err < 0) break;
    vec[n].msg_len = static_cast<uint32_t>(err);
    if (flags & kMsgWaitforone) flags |= kMsgDontwait;
    if (uts != nullptr && zx_clock_get_monotonic() >= deadline) {
      ++n;
      break;
    }
  }
  if (uts != nullptr) {
    int64_t rem = std::max<int64_t>(deadline - zx_clock_get_monotonic(), 0);
    LinuxTimespec ts{rem / 1000000000LL, rem % 1000000000LL};
    memcpy(uts, &ts, sizeof(ts));
  }
  if (n == 0) return err;
  if (err < 0 && err != kEAGAIN && desc->sock) {
    // Linux reports the error on the next call through sk_err.
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    desc->sock->so_error = static_cast<int>(-err);
  }
  return static_cast<int64_t>(n);
}

// SOL_SOCKET options that Linux stores for any socket and that have no
// effect on AF_UNIX (so storing them IS the Linux behaviour).
bool IsPlainBoolOpt(int opt) {
  switch (opt) {
    case 1:   // SO_DEBUG
    case 2:   // SO_REUSEADDR
    case 5:   // SO_DONTROUTE
    case 6:   // SO_BROADCAST
    case 9:   // SO_KEEPALIVE
    case 10:  // SO_OOBINLINE
    case 11:  // SO_NO_CHECK
    case 15:  // SO_REUSEPORT
      return true;
    default:
      return false;
  }
}

int64_t SysSetsockopt(LinuxProcess* proc, uint64_t a0, uint64_t a1,
                      uint64_t a2, uint64_t a3, uint64_t a4) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysSetsockopt(proc, desc.get(), a1, a2, a3, a4);
  }
  int level = static_cast<int>(a1);
  int opt = static_cast<int>(a2);
  const uint8_t* optval = reinterpret_cast<const uint8_t*>(a3);
  int optlen = static_cast<int>(a4);
  if (optlen < 0) return kEINVAL;
  // AF_UNIX has no protocol-level options (no ->setsockopt op).
  if (level != kSolSocket) return kEOPNOTSUPP;
  if (optlen < static_cast<int>(sizeof(int))) return kEINVAL;
  if (optval == nullptr) return kEFAULT;
  int val;
  memcpy(&val, optval, sizeof(val));
  const bool root = proc->euid == 0;  // stands in for CAP_NET_ADMIN
  std::shared_ptr<UnixSocketState> s = desc->sock;
  bool wake = false;
  {
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (IsPlainBoolOpt(opt)) {
      if (opt == 1 && val && !root) return kEACCES;
      if (opt == 15 && val) return kEOPNOTSUPP;  // SO_REUSEPORT: inet only
      s->plain_opts[opt] = val ? 1 : 0;
      return 0;
    }
    switch (opt) {
      case 12:  // SO_PRIORITY
        if ((val < 0 || val > 6) && !root) return kEPERM;
        s->plain_opts[opt] = val;
        return 0;
      case 36:  // SO_MARK
        if (!root) return kEPERM;
        s->plain_opts[opt] = val;
        return 0;
      case 7: {  // SO_SNDBUF
        uint32_t v = std::min<uint32_t>(static_cast<uint32_t>(val),
                                        kSysctlWmemMax);
        s->sndbuf = std::max(static_cast<int>(v) * 2, kSockMinSndbuf);
        wake = true;
        break;
      }
      case 32: {  // SO_SNDBUFFORCE
        if (!root) return kEPERM;
        int v = std::min(std::max(val, 0), INT_MAX / 2);
        s->sndbuf = std::max(v * 2, kSockMinSndbuf);
        wake = true;
        break;
      }
      case 8: {  // SO_RCVBUF (stored; AF_UNIX does not use it, like Linux)
        uint32_t v = std::min<uint32_t>(static_cast<uint32_t>(val),
                                        kSysctlRmemMax);
        s->rcvbuf = std::max(static_cast<int>(v) * 2, kSockMinRcvbuf);
        break;
      }
      case 33: {  // SO_RCVBUFFORCE
        if (!root) return kEPERM;
        int v = std::min(std::max(val, 0), INT_MAX / 2);
        s->rcvbuf = std::max(v * 2, kSockMinRcvbuf);
        break;
      }
      case 16:  // SO_PASSCRED
        s->passcred = val != 0;
        break;
      case 34:  // SO_PASSSEC (no LSM, so no SCM_SECURITY is ever produced)
        s->passsec = val != 0;
        break;
      case 18:  // SO_RCVLOWAT
        if (val < 0) val = INT_MAX;
        s->rcvlowat = val ? val : 1;
        break;
      case 19:  // SO_SNDLOWAT is not settable (1003.1g)
        return kErrNoProtoOpt;
      case 20:    // SO_RCVTIMEO_OLD
      case 21:    // SO_SNDTIMEO_OLD
      case 66:    // SO_RCVTIMEO_NEW
      case 67: {  // SO_SNDTIMEO_NEW
        if (optlen < static_cast<int>(sizeof(LinuxTimeval))) return kEINVAL;
        LinuxTimeval tv;
        memcpy(&tv, optval, sizeof(tv));
        if (tv.tv_usec < 0 || tv.tv_usec >= 1000000) return kErrDom;
        int64_t ns;
        if (tv.tv_sec < 0) {
          ns = 0;
        } else if (tv.tv_sec == 0 && tv.tv_usec == 0) {
          ns = -1;
        } else if (tv.tv_sec >= INT64_MAX / 1000000000LL - 1) {
          ns = -1;
        } else {
          ns = tv.tv_sec * 1000000000LL + tv.tv_usec * 1000LL;
        }
        if (opt == 20 || opt == 66) {
          s->rcvtimeo_ns = ns;
        } else {
          s->sndtimeo_ns = ns;
        }
        break;
      }
      case 13: {  // SO_LINGER (stored; close() never lingers on AF_UNIX)
        if (optlen < 8) return kEINVAL;
        int32_t l[2];
        memcpy(l, optval, sizeof(l));
        if (l[0] == 0) {
          s->linger_onoff = 0;
        } else {
          s->linger_onoff = 1;
          s->linger_secs = l[1];
        }
        break;
      }
      case 42:  // SO_PEEK_OFF
        s->peek_off = val;
        break;
      default:
        // Includes SO_TYPE/SO_ERROR/... (read-only) and options this shim
        // does not model (SO_TIMESTAMP*, SO_ATTACH_FILTER, SO_BINDTODEVICE).
        return kErrNoProtoOpt;
    }
  }
  if (wake) NotifyPollWaiters();
  return 0;
}

int64_t SysGetsockopt(LinuxProcess* proc, uint64_t a0, uint64_t a1,
                      uint64_t a2, uint64_t a3, uint64_t a4) {
  std::shared_ptr<FileDescription> desc;
  int64_t r = GetSocket(proc, a0, &desc);
  if (r) return r;
  if (desc->inet_sock) {
    return InetSysGetsockopt(proc, desc.get(), a1, a2, a3, a4);
  }
  int level = static_cast<int>(a1);
  int opt = static_cast<int>(a2);
  uint8_t* optval = reinterpret_cast<uint8_t*>(a3);
  void* optlen_ptr = reinterpret_cast<void*>(a4);
  if (level != kSolSocket) return kEOPNOTSUPP;
  if (optlen_ptr == nullptr) return kEFAULT;
  int32_t len;
  memcpy(&len, optlen_ptr, sizeof(len));
  if (len < 0) return kEINVAL;

  std::shared_ptr<UnixSocketState> s = desc->sock;
  uint8_t v[256] = {};
  size_t lv = sizeof(int);
  auto put_int = [&](int x) { memcpy(v, &x, sizeof(x)); };
  {
    Graveyard g;
    std::lock_guard<std::mutex> lk(g_sock_mtx);
    if (IsPlainBoolOpt(opt) || opt == 12 || opt == 36) {
      auto it = s->plain_opts.find(opt);
      put_int(it == s->plain_opts.end() ? 0 : it->second);
    } else {
      switch (opt) {
        case 3:  // SO_TYPE
          put_int(s->type);
          break;
        case 4:  // SO_ERROR (read and clear)
          put_int(s->so_error);
          s->so_error = 0;
          break;
        case 7:
          put_int(s->sndbuf);
          break;
        case 8:
          put_int(s->rcvbuf);
          break;
        case 16:
          put_int(s->passcred ? 1 : 0);
          break;
        case 18:
          put_int(s->rcvlowat);
          break;
        case 19:  // SO_SNDLOWAT
          put_int(1);
          break;
        case 30:  // SO_ACCEPTCONN
          put_int(s->listening ? 1 : 0);
          break;
        case 34:
          put_int(s->passsec ? 1 : 0);
          break;
        case 38:  // SO_PROTOCOL
          put_int(s->protocol);
          break;
        case 39:  // SO_DOMAIN
          put_int(kAfUnix);
          break;
        case 42:
          put_int(s->peek_off);
          break;
        case 13: {  // SO_LINGER
          int32_t l[2] = {s->linger_onoff, s->linger_secs};
          memcpy(v, l, sizeof(l));
          lv = sizeof(l);
          break;
        }
        case 20:
        case 21:
        case 66:
        case 67: {
          int64_t ns = (opt == 20 || opt == 66) ? s->rcvtimeo_ns : s->sndtimeo_ns;
          LinuxTimeval tv{0, 0};
          if (ns > 0) {
            tv.tv_sec = ns / 1000000000LL;
            tv.tv_usec = (ns % 1000000000LL) / 1000;
          }
          memcpy(v, &tv, sizeof(tv));
          lv = sizeof(tv);
          break;
        }
        case 17: {  // SO_PEERCRED
          LinuxUcred uc = s->has_peer_cred
                              ? LinuxUcred{s->peer_cred_pid, s->peer_cred_uid,
                                           s->peer_cred_gid}
                              : LinuxUcred{0, UINT32_MAX, UINT32_MAX};
          memcpy(v, &uc, sizeof(uc));
          lv = sizeof(uc);
          break;
        }
        case 28: {  // SO_PEERNAME
          if (!s->connected) return kENOTCONN;
          std::string name = PeerNameLocked(s.get(), &g);
          v[0] = kAfUnix;
          size_t n = std::min<size_t>(name.size(), sizeof(v) - 2);
          memcpy(v + 2, name.data(), n);
          lv = 2 + n;
          if (lv < static_cast<size_t>(len)) return kEINVAL;
          if (len > 0) {
            if (optval == nullptr) return kEFAULT;
            memcpy(optval, v, len);
          }
          memcpy(optlen_ptr, &len, sizeof(len));
          return 0;
        }
        case 59: {  // SO_PEERGROUPS
          if (!s->has_peer_cred) return kErrNoData;
          size_t need = s->peer_groups.size() * sizeof(uint32_t);
          if (static_cast<size_t>(len) < need) {
            int32_t nl = static_cast<int32_t>(need);
            memcpy(optlen_ptr, &nl, sizeof(nl));
            return kERANGE;
          }
          if (need > 0) {
            if (optval == nullptr) return kEFAULT;
            memcpy(optval, s->peer_groups.data(), need);
          }
          int32_t nl = static_cast<int32_t>(need);
          memcpy(optlen_ptr, &nl, sizeof(nl));
          return 0;
        }
        case 31:  // SO_PEERSEC: no LSM -> -ENOPROTOOPT, as in Linux
        default:
          return kErrNoProtoOpt;
      }
    }
  }
  size_t copy = std::min(static_cast<size_t>(len), lv);
  if (copy > 0) {
    if (optval == nullptr) return kEFAULT;
    memcpy(optval, v, copy);
  }
  int32_t out_len = static_cast<int32_t>(copy);
  memcpy(optlen_ptr, &out_len, sizeof(out_len));
  return 0;
}

// ---------------------------------------------------------------------------
// pipe / pipe2
// ---------------------------------------------------------------------------

int64_t SysPipe2(LinuxProcess* proc, uint64_t a0, uint32_t flags) {
  int* pipefd = reinterpret_cast<int*>(a0);
  // O_DIRECT (040000, packet mode) is not implemented, so it is rejected
  // with -EINVAL (like pre-3.4 Linux) rather than silently giving
  // byte-stream semantics. O_NOTIFICATION_PIPE is not supported either.
  if (flags & ~(kLinuxO_NONBLOCK | kLinuxO_CLOEXEC)) return kEINVAL;
  if (pipefd == nullptr) return kEFAULT;
  auto pipe = std::make_shared<PipeState>();
  uint64_t ino = AllocAnonInode();
  auto rdesc = std::make_shared<FileDescription>();
  rdesc->kind = FdKind::kPipeRead;
  rdesc->flags = kLinuxO_RDONLY | (flags & kLinuxO_NONBLOCK);
  rdesc->pipe = pipe;
  rdesc->anon_ino = ino;
  auto wdesc = std::make_shared<FileDescription>();
  wdesc->kind = FdKind::kPipeWrite;
  wdesc->flags = kLinuxO_WRONLY | (flags & kLinuxO_NONBLOCK);
  wdesc->pipe = pipe;
  wdesc->anon_ino = ino;
  bool cloexec = (flags & kLinuxO_CLOEXEC) != 0;
  int rfd = InstallFd(proc, std::move(rdesc), cloexec);
  if (rfd < 0) return rfd;
  int wfd = InstallFd(proc, std::move(wdesc), cloexec);
  if (wfd < 0) {
    UninstallFd(proc, rfd);
    return wfd;
  }
  pipefd[0] = rfd;
  pipefd[1] = wfd;
  return 0;
}

}  // namespace

namespace {

// ---------------------------------------------------------------------------
// eventfd
// ---------------------------------------------------------------------------

int64_t SysEventfd(LinuxProcess* proc, uint64_t a0, uint32_t flags) {
  if (flags & ~(kEfdSemaphore | kLinuxO_NONBLOCK | kLinuxO_CLOEXEC)) {
    return kEINVAL;
  }
  auto st = std::make_shared<IpcObjectState>();
  st->kind = FdKind::kEventFd;
  st->efd_count = static_cast<uint32_t>(a0);  // initval is an unsigned int
  st->efd_semaphore = (flags & kEfdSemaphore) != 0;
  auto d = std::make_shared<FileDescription>();
  d->kind = FdKind::kEventFd;
  d->flags = kLinuxO_RDWR | (flags & kLinuxO_NONBLOCK);
  d->ipc = std::move(st);
  d->anon_ino = AllocAnonInode();
  return InstallFd(proc, std::move(d), (flags & kLinuxO_CLOEXEC) != 0);
}

ssize_t EventfdRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                    size_t count) {
  if (count < sizeof(uint64_t)) return kEINVAL;
  if (buf == nullptr) return kEFAULT;
  IpcObjectState* st = desc->ipc.get();
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, -1);
  while (true) {
    {
      std::lock_guard<std::mutex> lk(st->mtx);
      if (st->efd_count > 0) {
        uint64_t v = st->efd_semaphore ? 1 : st->efd_count;
        st->efd_count -= v;
        st->activity++;
        memcpy(buf, &v, sizeof(v));
        NotifyPollWaiters();  // writers may proceed
        return sizeof(v);
      }
    }
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

ssize_t EventfdWrite(LinuxProcess* proc, FileDescription* desc,
                     const void* buf, size_t count) {
  if (count < sizeof(uint64_t)) return kEINVAL;
  if (buf == nullptr) return kEFAULT;
  uint64_t v;
  memcpy(&v, buf, sizeof(v));
  if (v == UINT64_MAX) return kEINVAL;
  IpcObjectState* st = desc->ipc.get();
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, -1);
  while (true) {
    {
      std::lock_guard<std::mutex> lk(st->mtx);
      if (UINT64_MAX - st->efd_count > v) {
        st->efd_count += v;
        st->activity++;
        NotifyPollWaiters();
        return sizeof(v);
      }
    }
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

// ---------------------------------------------------------------------------
// timerfd
// ---------------------------------------------------------------------------

std::mutex g_timer_cancel_mtx;
std::vector<std::weak_ptr<IpcObjectState>> g_cancel_timers;

int64_t ClockNow(int clockid) {
  switch (clockid) {
    case kClockRealtime:
    case kClockRealtimeAlarm:
      return ShimRealtimeNs();
    case kClockBoottime:
    case kClockBoottimeAlarm:
      return zx_clock_get_boot();
    default:
      return zx_clock_get_monotonic();
  }
}

// Accounts for expirations up to now (what Linux's hrtimer callback plus
// hrtimer_forward_now() do). Caller holds st->mtx.
void TimerAdvanceLocked(IpcObjectState* st) {
  if (!st->armed) return;
  int64_t now = ClockNow(st->clockid);
  if (now < st->deadline_ns) return;
  uint64_t n = 1;
  if (st->interval_ns > 0) {
    n += static_cast<uint64_t>((now - st->deadline_ns) / st->interval_ns);
    st->deadline_ns += static_cast<int64_t>(n) * st->interval_ns;
  } else {
    st->armed = false;
  }
  st->ticks += n;
  st->total_ticks += n;
  st->activity++;
}

bool ValidTimespec(const LinuxTimespec& ts) {
  return ts.tv_sec >= 0 && ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000;
}

int64_t TsToNs(const LinuxTimespec& ts) {
  if (ts.tv_sec >= INT64_MAX / 1000000000LL) return INT64_MAX;
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

LinuxTimespec NsToTs(int64_t ns) {
  if (ns < 0) ns = 0;
  return LinuxTimespec{ns / 1000000000LL, ns % 1000000000LL};
}

void TimerCurrentLocked(IpcObjectState* st, LinuxItimerspec* out) {
  out->it_interval = NsToTs(st->interval_ns);
  out->it_value =
      st->armed ? NsToTs(st->deadline_ns - ClockNow(st->clockid)) : NsToTs(0);
}

std::shared_ptr<FileDescription> GetIpcDesc(LinuxProcess* proc, uint64_t fd,
                                            FdKind kind, int64_t* err) {
  std::shared_ptr<FileDescription> d = LookupFd(proc, static_cast<int>(fd));
  if (!d) {
    *err = kEBADF;
    return nullptr;
  }
  if (d->kind != kind || !d->ipc) {
    *err = kEINVAL;
    return nullptr;
  }
  return d;
}

int64_t SysTimerfdCreate(LinuxProcess* proc, uint64_t a0, uint64_t a1) {
  int clockid = static_cast<int>(a0);
  uint32_t flags = static_cast<uint32_t>(a1);
  if (flags & ~(kLinuxO_NONBLOCK | kLinuxO_CLOEXEC)) return kEINVAL;
  if (clockid != kClockRealtime && clockid != kClockMonotonic &&
      clockid != kClockBoottime && clockid != kClockRealtimeAlarm &&
      clockid != kClockBoottimeAlarm) {
    return kEINVAL;
  }
  // *_ALARM clocks need CAP_WAKE_ALARM; they behave like their base clock
  // here because this system never suspends (nothing to wake up from).
  if ((clockid == kClockRealtimeAlarm || clockid == kClockBoottimeAlarm) &&
      proc->euid != 0) {
    return kEPERM;
  }
  auto st = std::make_shared<IpcObjectState>();
  st->kind = FdKind::kTimerFd;
  st->clockid = clockid;
  auto d = std::make_shared<FileDescription>();
  d->kind = FdKind::kTimerFd;
  d->flags = kLinuxO_RDWR | (flags & kLinuxO_NONBLOCK);
  d->ipc = std::move(st);
  d->anon_ino = AllocAnonInode();
  return InstallFd(proc, std::move(d), (flags & kLinuxO_CLOEXEC) != 0);
}

int64_t SysTimerfdSettime(LinuxProcess* proc, uint64_t a0, uint64_t a1,
                          uint64_t a2, uint64_t a3) {
  uint32_t flags = static_cast<uint32_t>(a1);
  auto* unew = reinterpret_cast<const LinuxItimerspec*>(a2);
  auto* uold = reinterpret_cast<LinuxItimerspec*>(a3);
  if (unew == nullptr) return kEFAULT;
  LinuxItimerspec nv;
  memcpy(&nv, unew, sizeof(nv));
  if ((flags & ~(kTfdTimerAbstime | kTfdTimerCancelOnSet)) ||
      !ValidTimespec(nv.it_interval) || !ValidTimespec(nv.it_value)) {
    return kEINVAL;
  }
  int64_t err = 0;
  std::shared_ptr<FileDescription> d =
      GetIpcDesc(proc, a0, FdKind::kTimerFd, &err);
  if (!d) return err;
  std::shared_ptr<IpcObjectState> st = d->ipc;
  LinuxItimerspec old{};
  bool register_cancel;
  {
    std::lock_guard<std::mutex> lk(st->mtx);
    TimerAdvanceLocked(st.get());
    TimerCurrentLocked(st.get(), &old);
    st->ticks = 0;  // pending expirations are discarded by a new setting
    st->canceled = false;
    st->interval_ns = TsToNs(nv.it_interval);
    int64_t value = TsToNs(nv.it_value);
    if (value == 0) {
      st->armed = false;
    } else {
      st->armed = true;
      st->deadline_ns = (flags & kTfdTimerAbstime)
                            ? value
                            : ClockNow(st->clockid) + value;
    }
    // CANCEL_ON_SET only applies to absolute REALTIME timers; otherwise
    // Linux ignores the flag.
    st->cancel_on_set =
        (st->clockid == kClockRealtime || st->clockid == kClockRealtimeAlarm) &&
        (flags & kTfdTimerAbstime) && (flags & kTfdTimerCancelOnSet);
    register_cancel = st->cancel_on_set;
    st->activity++;
  }
  if (register_cancel) {
    std::lock_guard<std::mutex> lk(g_timer_cancel_mtx);
    bool present = false;
    for (const auto& w : g_cancel_timers) {
      if (!w.owner_before(st) && !st.owner_before(w)) present = true;
    }
    if (!present) g_cancel_timers.push_back(st);
  }
  NotifyPollWaiters();
  if (uold != nullptr) memcpy(uold, &old, sizeof(old));
  return 0;
}

int64_t SysTimerfdGettime(LinuxProcess* proc, uint64_t a0, uint64_t a1) {
  int64_t err = 0;
  std::shared_ptr<FileDescription> d =
      GetIpcDesc(proc, a0, FdKind::kTimerFd, &err);
  if (!d) return err;
  auto* ucur = reinterpret_cast<LinuxItimerspec*>(a1);
  LinuxItimerspec cur{};
  {
    std::lock_guard<std::mutex> lk(d->ipc->mtx);
    TimerAdvanceLocked(d->ipc.get());
    TimerCurrentLocked(d->ipc.get(), &cur);
  }
  if (ucur == nullptr) return kEFAULT;
  memcpy(ucur, &cur, sizeof(cur));
  return 0;
}

ssize_t TimerfdRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                    size_t count) {
  if (count < sizeof(uint64_t)) return kEINVAL;
  if (buf == nullptr) return kEFAULT;
  IpcObjectState* st = desc->ipc.get();
  const bool nonblock = (desc->flags & kLinuxO_NONBLOCK) != 0;
  while (true) {
    int64_t remaining_ns = -1;  // -1: disarmed, waits for a settime()
    {
      std::lock_guard<std::mutex> lk(st->mtx);
      TimerAdvanceLocked(st);
      if (st->canceled) {
        st->canceled = false;
        st->ticks = 0;
        return kErrCanceled;
      }
      if (st->ticks > 0) {
        uint64_t v = st->ticks;
        st->ticks = 0;
        memcpy(buf, &v, sizeof(v));
        return sizeof(v);
      }
      if (nonblock) return kEAGAIN;
      if (st->armed) {
        remaining_ns =
            std::max<int64_t>(st->deadline_ns - ClockNow(st->clockid), 0);
      }
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    if (remaining_ns >= 0 && remaining_ns <= kPollSliceMs * 1000000LL) {
      zx_nanosleep(zx_deadline_after(remaining_ns));
    } else {
      WaitForPollEvent(kPollSliceMs);
    }
  }
}

// ---------------------------------------------------------------------------
// signalfd
// ---------------------------------------------------------------------------

int64_t SysSignalfd(LinuxProcess* proc, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint32_t flags) {
  if (flags & ~(kLinuxO_NONBLOCK | kLinuxO_CLOEXEC)) return kEINVAL;
  if (a2 != sizeof(uint64_t)) return kEINVAL;
  const uint64_t* umask = reinterpret_cast<const uint64_t*>(a1);
  if (umask == nullptr) return kEFAULT;
  uint64_t mask;
  memcpy(&mask, umask, sizeof(mask));
  mask &= ~(kSigBitKill | kSigBitStop);  // silently dropped, like Linux
  int ufd = static_cast<int>(a0);
  if (ufd == -1) {
    auto st = std::make_shared<IpcObjectState>();
    st->kind = FdKind::kSignalFd;
    st->sigmask.store(mask);
    auto d = std::make_shared<FileDescription>();
    d->kind = FdKind::kSignalFd;
    d->flags = kLinuxO_RDWR | (flags & kLinuxO_NONBLOCK);
    d->ipc = std::move(st);
    d->anon_ino = AllocAnonInode();
    return InstallFd(proc, std::move(d), (flags & kLinuxO_CLOEXEC) != 0);
  }
  int64_t err = 0;
  std::shared_ptr<FileDescription> d =
      GetIpcDesc(proc, static_cast<uint64_t>(static_cast<int64_t>(ufd)),
                 FdKind::kSignalFd, &err);
  if (!d) return err;
  d->ipc->sigmask.store(mask);
  d->ipc->activity++;
  NotifyPollWaiters();
  return ufd;
}

// Linux signalfd_copyinfo(): which siginfo union members are meaningful
// depends on the layout (kernel/signal.c siginfo_layout()).
void FillSignalfdInfo(const LinuxSiginfo& si, LinuxSignalfdSiginfo* o) {
  memset(o, 0, sizeof(*o));
  o->ssi_signo = static_cast<uint32_t>(si.si_signo);
  o->ssi_errno = si.si_errno;
  o->ssi_code = si.si_code;
  auto i32 = [&](size_t off) {
    int32_t v;
    memcpy(&v, si.fields + off, sizeof(v));
    return v;
  };
  auto u64 = [&](size_t off) {
    uint64_t v;
    memcpy(&v, si.fields + off, sizeof(v));
    return v;
  };
  enum { kKill, kTimer, kPoll, kFault, kFaultMce, kChld, kRt, kSys } layout =
      kKill;
  const int sig = si.si_signo;
  const int code = si.si_code;
  if (code > 0 && code < 0x80) {  // SI_USER < code < SI_KERNEL
    struct {
      int sig, limit, layout;
    } static const kTable[] = {
        {4, 11, kFault},  // SIGILL / NSIGILL
        {8, 15, kFault},  // SIGFPE
        {11, 10, kFault}, // SIGSEGV
        {7, 5, kFault},   // SIGBUS
        {5, 6, kFault},   // SIGTRAP
        {17, 6, kChld},   // SIGCHLD
        {29, 6, kPoll},   // SIGPOLL
        {31, 2, kSys},    // SIGSYS
    };
    bool found = false;
    for (const auto& e : kTable) {
      if (e.sig == sig && code <= e.limit) {
        layout = static_cast<decltype(layout)>(e.layout);
        found = true;
        if (sig == 7 && (code == 4 || code == 5)) layout = kFaultMce;
      }
    }
    if (!found && code <= 6) layout = kPoll;  // NSIGPOLL
  } else if (code == -2) {  // SI_TIMER
    layout = kTimer;
  } else if (code == -5) {  // SI_SIGIO
    layout = kPoll;
  } else if (code < 0) {
    layout = kRt;
  }
  switch (layout) {
    case kKill:
      o->ssi_pid = static_cast<uint32_t>(i32(0));
      o->ssi_uid = static_cast<uint32_t>(i32(4));
      break;
    case kTimer:
      o->ssi_tid = static_cast<uint32_t>(i32(0));
      o->ssi_overrun = static_cast<uint32_t>(i32(4));
      o->ssi_ptr = u64(8);
      o->ssi_int = i32(8);
      break;
    case kPoll:
      o->ssi_band = static_cast<uint32_t>(u64(0));
      o->ssi_fd = i32(8);
      break;
    case kFault:
      o->ssi_addr = u64(0);
      break;
    case kFaultMce:
      o->ssi_addr = u64(0);
      memcpy(&o->ssi_addr_lsb, si.fields + 8, sizeof(o->ssi_addr_lsb));
      break;
    case kChld:
      o->ssi_pid = static_cast<uint32_t>(i32(0));
      o->ssi_uid = static_cast<uint32_t>(i32(4));
      o->ssi_status = i32(8);
      o->ssi_utime = u64(16);
      o->ssi_stime = u64(24);
      break;
    case kRt:
      o->ssi_pid = static_cast<uint32_t>(i32(0));
      o->ssi_uid = static_cast<uint32_t>(i32(4));
      o->ssi_ptr = u64(8);
      o->ssi_int = i32(8);
      break;
    case kSys:
      o->ssi_call_addr = u64(0);
      o->ssi_syscall = i32(8);
      o->ssi_arch = static_cast<uint32_t>(i32(12));
      break;
  }
}

ssize_t SignalfdRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                     size_t count) {
  if (count < sizeof(LinuxSignalfdSiginfo)) return kEINVAL;
  if (buf == nullptr) return kEFAULT;
  IpcObjectState* st = desc->ipc.get();
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, -1);
  const size_t nmax = count / sizeof(LinuxSignalfdSiginfo);
  while (true) {
    size_t n = 0;
    // Signals queued for the reading thread / its process are consumed.
    while (n < nmax) {
      LinuxSiginfo si;
      int sig = ShimDequeueSignal(proc, t_current_thread, st->sigmask.load(),
                                  &si);
      if (sig == 0) break;
      LinuxSignalfdSiginfo o;
      FillSignalfdInfo(si, &o);
      memcpy(static_cast<uint8_t*>(buf) + n * sizeof(o), &o, sizeof(o));
      ++n;
    }
    if (n > 0) return static_cast<ssize_t>(n * sizeof(LinuxSignalfdSiginfo));
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

// ---------------------------------------------------------------------------
// inotify
// ---------------------------------------------------------------------------

std::mutex g_inotify_mtx;
std::map<VfsNode*, std::vector<std::weak_ptr<IpcObjectState>>>
    g_inotify_watchers;
std::atomic<uint32_t> g_inotify_cookie{0};

size_t InotifyEventSize(const InotifyEventRec& e) {
  if (e.name.empty()) return kInotifyEventHdr;
  size_t padded = (e.name.size() + 1 + kInotifyEventHdr - 1) &
                  ~(kInotifyEventHdr - 1);
  return kInotifyEventHdr + padded;
}

// Caller holds g_inotify_mtx.
void QueueEventLocked(IpcObjectState* inst, InotifyEventRec ev) {
  if (inst->events.size() >= kInotifyMaxQueuedEvents) {
    if (inst->overflow_queued) return;
    ev = InotifyEventRec{-1, kInQOverflow, 0, std::string()};
    inst->overflow_queued = true;
  } else if (!inst->events.empty()) {
    // inotify_merge(): drop an event identical to the last queued one.
    const InotifyEventRec& last = inst->events.back();
    if (!(last.mask & kInIgnored) && last.mask == ev.mask && last.wd == ev.wd &&
        last.name == ev.name) {
      return;
    }
  }
  inst->queued_bytes += InotifyEventSize(ev);
  inst->events.push_back(std::move(ev));
  inst->activity++;
}

void UnregisterWatcherLocked(VfsNode* node,
                             const std::shared_ptr<IpcObjectState>& inst) {
  auto rit = g_inotify_watchers.find(node);
  if (rit == g_inotify_watchers.end()) return;
  auto& v = rit->second;
  v.erase(std::remove_if(v.begin(), v.end(),
                         [&](const std::weak_ptr<IpcObjectState>& w) {
                           return w.expired() ||
                                  (!w.owner_before(inst) &&
                                   !inst.owner_before(w));
                         }),
          v.end());
  if (v.empty()) g_inotify_watchers.erase(rit);
}

// Removes watch `wd` and queues IN_IGNORED. Caller holds g_inotify_mtx.
void RemoveWatchLocked(const std::shared_ptr<IpcObjectState>& inst, int wd) {
  auto it = inst->watches.find(wd);
  if (it == inst->watches.end()) return;
  VfsNode* node = it->second.node;
  inst->watches.erase(it);
  inst->wd_of_node.erase(node);
  UnregisterWatcherLocked(node, inst);
  QueueEventLocked(inst.get(),
                   InotifyEventRec{wd, kInIgnored, 0, std::string()});
}

// Delivers an event to every instance watching `node`. Called by the VFS
// with Vfs::Mutex() held (so `node`/`dir->children` are stable).
void InotifyDeliver(VfsNode* node, uint32_t mask, const std::string& name,
                    uint32_t cookie, bool is_dir, bool self) {
  std::vector<std::shared_ptr<IpcObjectState>> insts;  // outlive the lock
  bool queued = false;
  {
    std::lock_guard<std::mutex> lk(g_inotify_mtx);
    auto rit = g_inotify_watchers.find(node);
    if (rit == g_inotify_watchers.end()) return;
    for (const auto& w : rit->second) {
      if (auto sp = w.lock()) insts.push_back(std::move(sp));
    }
    const uint32_t ev = mask & kInAllEvents;
    for (const auto& inst : insts) {
      auto wit = inst->wd_of_node.find(node);
      if (wit == inst->wd_of_node.end()) continue;
      const int wd = wit->second;
      const uint32_t wmask = inst->watches[wd].mask;
      bool deliver = (ev & wmask) != 0;
      if (deliver && !self && (wmask & kInExclUnlink) &&
          (ev & kInChildEvents) &&
          node->children.find(name) == node->children.end()) {
        deliver = false;  // child was unlinked from the watched directory
      }
      bool removed = false;
      if (deliver) {
        QueueEventLocked(inst.get(),
                         InotifyEventRec{wd, ev | (is_dir ? kInIsDir : 0),
                                         cookie, self ? std::string() : name});
        queued = true;
        if (wmask & kInOneShot) {
          RemoveWatchLocked(inst, wd);
          removed = true;
        }
      }
      if (self && (ev & kInDeleteSelf) && !removed) {
        RemoveWatchLocked(inst, wd);  // the inode is gone: IN_IGNORED
        queued = true;
      }
    }
    // Drop registry entries of closed instances.
    auto it2 = g_inotify_watchers.find(node);
    if (it2 != g_inotify_watchers.end()) {
      auto& v = it2->second;
      v.erase(std::remove_if(v.begin(), v.end(),
                             [](const std::weak_ptr<IpcObjectState>& w) {
                               return w.expired();
                             }),
              v.end());
      if (v.empty()) g_inotify_watchers.erase(it2);
    }
  }
  if (queued) NotifyPollWaiters();
}

int64_t SysInotifyInit(LinuxProcess* proc, uint32_t flags) {
  if (flags & ~(kLinuxO_NONBLOCK | kLinuxO_CLOEXEC)) return kEINVAL;
  auto st = std::make_shared<IpcObjectState>();
  st->kind = FdKind::kInotify;
  auto d = std::make_shared<FileDescription>();
  d->kind = FdKind::kInotify;
  d->flags = kLinuxO_RDONLY | (flags & kLinuxO_NONBLOCK);
  d->ipc = std::move(st);
  d->anon_ino = AllocAnonInode();
  return InstallFd(proc, std::move(d), (flags & kLinuxO_CLOEXEC) != 0);
}

int64_t SysInotifyAddWatch(LinuxProcess* proc, uint64_t a0, uint64_t a1,
                           uint64_t a2) {
  uint32_t mask = static_cast<uint32_t>(a2);
  if ((mask & kInMaskAdd) && (mask & kInMaskCreate)) return kEINVAL;
  if (!(mask & kInAllInotifyBits)) return kEINVAL;
  int64_t err = 0;
  std::shared_ptr<FileDescription> d =
      GetIpcDesc(proc, a0, FdKind::kInotify, &err);
  if (!d) return err;
  const char* upath = reinterpret_cast<const char*>(a1);
  if (upath == nullptr) return kEFAULT;
  std::string raw(upath);
  if (raw.empty()) return kENOENT;
  std::string abs = Vfs::NormalizePath(proc->cwd, raw);

  std::shared_ptr<IpcObjectState> inst = d->ipc;
  // Hold Vfs::Mutex() across lookup and registration so the node cannot be
  // freed in between (lock order Vfs -> inotify, same as the VFS hooks).
  std::lock_guard<std::recursive_mutex> vl(Vfs::Mutex());
  VfsNode* node = nullptr;
  int64_t r = Vfs::LookupErr(abs, !(mask & kInDontFollow), &node);
  if (r < 0) return r;
  if (node == nullptr) return kENOENT;
  if ((mask & kInOnlyDir) && node->type != VfsNodeType::kDirectory) {
    return kENOTDIR;
  }
  if (!HasPerm(proc, node, 4)) return kEACCES;  // MAY_READ
  const uint32_t new_mask = mask & (kInAllEvents | kInOneShot | kInExclUnlink);

  std::lock_guard<std::mutex> lk(g_inotify_mtx);
  auto it = inst->wd_of_node.find(node);
  if (it != inst->wd_of_node.end()) {
    if (mask & kInMaskCreate) return kEEXIST;
    InotifyWatchRec& w = inst->watches[it->second];
    w.mask = (mask & kInMaskAdd) ? (w.mask | new_mask) : new_mask;
    return it->second;
  }
  if (inst->next_wd <= 0) inst->next_wd = 1;  // wrapped past INT_MAX
  while (inst->watches.count(inst->next_wd)) inst->next_wd++;
  int wd = inst->next_wd++;
  inst->watches[wd] = InotifyWatchRec{node, new_mask};
  inst->wd_of_node[node] = wd;
  g_inotify_watchers[node].push_back(inst);
  return wd;
}

int64_t SysInotifyRmWatch(LinuxProcess* proc, uint64_t a0, uint64_t a1) {
  int64_t err = 0;
  std::shared_ptr<FileDescription> d =
      GetIpcDesc(proc, a0, FdKind::kInotify, &err);
  if (!d) return err;
  int wd = static_cast<int>(a1);
  {
    std::lock_guard<std::mutex> lk(g_inotify_mtx);
    if (d->ipc->watches.find(wd) == d->ipc->watches.end()) return kEINVAL;
    RemoveWatchLocked(d->ipc, wd);
  }
  NotifyPollWaiters();
  return 0;
}

ssize_t InotifyRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                    size_t count) {
  IpcObjectState* inst = desc->ipc.get();
  Waiter w(proc, (desc->flags & kLinuxO_NONBLOCK) != 0, -1);
  while (true) {
    {
      std::lock_guard<std::mutex> lk(g_inotify_mtx);
      if (!inst->events.empty()) {
        size_t copied = 0;
        while (!inst->events.empty()) {
          const InotifyEventRec& e = inst->events.front();
          size_t sz = InotifyEventSize(e);
          if (sz > count - copied) {
            if (copied == 0) return kEINVAL;  // buffer too small
            break;
          }
          if (buf == nullptr) return kEFAULT;
          uint8_t* out = static_cast<uint8_t*>(buf) + copied;
          uint32_t name_len = static_cast<uint32_t>(sz - kInotifyEventHdr);
          memcpy(out + 0, &e.wd, 4);
          memcpy(out + 4, &e.mask, 4);
          memcpy(out + 8, &e.cookie, 4);
          memcpy(out + 12, &name_len, 4);
          if (name_len) {
            memset(out + kInotifyEventHdr, 0, name_len);
            memcpy(out + kInotifyEventHdr, e.name.data(), e.name.size());
          }
          if (e.mask & kInQOverflow) inst->overflow_queued = false;
          inst->queued_bytes -= sz;
          inst->events.pop_front();
          copied += sz;
        }
        return static_cast<ssize_t>(copied);
      }
    }
    int64_t werr = w.Wait();
    if (werr) return werr;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// fd_ops.h: eventfd / timerfd / signalfd / inotify hooks
// ---------------------------------------------------------------------------

ssize_t IpcRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                size_t count) {
  if (desc == nullptr || !desc->ipc) return kEBADF;
  switch (desc->kind) {
    case FdKind::kEventFd:
      return EventfdRead(proc, desc, buf, count);
    case FdKind::kTimerFd:
      return TimerfdRead(proc, desc, buf, count);
    case FdKind::kSignalFd:
      return SignalfdRead(proc, desc, buf, count);
    case FdKind::kInotify:
      return InotifyRead(proc, desc, buf, count);
    default:
      return kEINVAL;
  }
}

ssize_t IpcWrite(LinuxProcess* proc, FileDescription* desc, const void* buf,
                 size_t count) {
  if (desc == nullptr || !desc->ipc) return kEBADF;
  switch (desc->kind) {
    case FdKind::kEventFd:
      return EventfdWrite(proc, desc, buf, count);
    case FdKind::kInotify:
      return kEBADF;  // inotify fds are opened O_RDONLY
    default:
      return kEINVAL;  // timerfd/signalfd have no write operation
  }
}

uint32_t IpcPollEvents(FileDescription* desc) {
  if (desc == nullptr || !desc->ipc) return kPollNval;
  IpcObjectState* st = desc->ipc.get();
  switch (desc->kind) {
    case FdKind::kEventFd: {
      std::lock_guard<std::mutex> lk(st->mtx);
      uint32_t ev = 0;
      if (st->efd_count > 0) ev |= kPollIn;
      if (st->efd_count == UINT64_MAX) ev |= kPollErr;
      if (UINT64_MAX - 1 > st->efd_count) ev |= kPollOut;
      return ev;
    }
    case FdKind::kTimerFd: {
      std::lock_guard<std::mutex> lk(st->mtx);
      TimerAdvanceLocked(st);
      return (st->ticks > 0 || st->canceled) ? kPollIn : 0;
    }
    case FdKind::kSignalFd: {
      // Readiness is relative to the polling thread, as in Linux.
      LinuxThread* t = t_current_thread;
      if (t == nullptr || t->proc == nullptr) return 0;
      return (ShimPendingSignalSet(t->proc, t) & st->sigmask.load()) ? kPollIn
                                                                     : 0;
    }
    case FdKind::kInotify: {
      std::lock_guard<std::mutex> lk(g_inotify_mtx);
      return st->events.empty() ? 0 : (kPollIn | kPollRdNorm);
    }
    default:
      return 0;
  }
}

uint64_t IpcEdgeCounter(FileDescription* desc) {
  if (desc == nullptr) return 0;
  if (desc->kind == FdKind::kUnixSocket) {
    if (desc->inet_sock) {
      uint64_t act = 0;
      InetSocketPollEvents(desc, &act);
      return act;
    }
    return desc->sock ? desc->sock->activity.load() : 0;
  }
  if (!desc->ipc) return 0;
  IpcObjectState* st = desc->ipc.get();
  switch (desc->kind) {
    case FdKind::kTimerFd: {
      std::lock_guard<std::mutex> lk(st->mtx);
      TimerAdvanceLocked(st);
      return st->activity.load();
    }
    case FdKind::kSignalFd: {
      // Only the pending set exists; a signal that is consumed and raised
      // again between two epoll_wait calls is not seen as a new edge.
      LinuxThread* t = t_current_thread;
      uint64_t pend = (t && t->proc) ? ShimPendingSignalSet(t->proc, t) : 0;
      return pend & st->sigmask.load();
    }
    default:
      return st->activity.load();
  }
}

int IpcBytesAvailable(FileDescription* desc) {
  if (desc == nullptr || !desc->ipc) return static_cast<int>(kEBADF);
  if (desc->kind != FdKind::kInotify) {
    return static_cast<int>(kENOTTY);  // no FIONREAD on these anon inodes
  }
  std::lock_guard<std::mutex> lk(g_inotify_mtx);
  return static_cast<int>(std::min<size_t>(desc->ipc->queued_bytes, INT_MAX));
}

void InotifyDirEvent(VfsNode* dir, uint32_t mask, const std::string& name,
                     uint32_t cookie, bool child_is_dir) {
  if (dir == nullptr) return;
  InotifyDeliver(dir, mask, name, cookie, child_is_dir, false);
}

void InotifySelfEvent(VfsNode* node, uint32_t mask) {
  if (node == nullptr) return;
  InotifyDeliver(node, mask, std::string(), 0,
                 node->type == VfsNodeType::kDirectory, true);
}

uint32_t InotifyNextCookie() { return ++g_inotify_cookie; }

void TimerfdClockWasSet() {
  std::vector<std::shared_ptr<IpcObjectState>> timers;
  {
    std::lock_guard<std::mutex> lk(g_timer_cancel_mtx);
    auto& v = g_cancel_timers;
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const std::weak_ptr<IpcObjectState>& w) {
                             return w.expired();
                           }),
            v.end());
    for (const auto& w : v) {
      if (auto sp = w.lock()) timers.push_back(std::move(sp));
    }
  }
  bool any = false;
  for (const auto& t : timers) {
    std::lock_guard<std::mutex> lk(t->mtx);
    if (t->cancel_on_set) {
      t->canceled = true;
      t->activity++;
      any = true;
    }
  }
  if (any) NotifyPollWaiters();
}

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------

int64_t HandleIpcSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                         uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5) {
  (void)thrd;
  switch (sc) {
    case 22:  // SYS_pipe
      return SysPipe2(proc, a0, 0);
    case 293:  // SYS_pipe2
      return SysPipe2(proc, a0, static_cast<uint32_t>(a1));
    case 41:  // SYS_socket
      return SysSocket(proc, a0, a1, a2);
    case 53:  // SYS_socketpair
      return SysSocketpair(proc, a0, a1, a2, a3);
    case 49:  // SYS_bind
      return SysBind(proc, a0, a1, a2);
    case 50:  // SYS_listen
      return SysListen(proc, a0, a1);
    case 42:  // SYS_connect
      return SysConnect(proc, a0, a1, a2);
    case 43:  // SYS_accept
      return SysAccept(proc, a0, a1, a2, 0);
    case 288:  // SYS_accept4
      return SysAccept(proc, a0, a1, a2, static_cast<uint32_t>(a3));
    case 44:  // SYS_sendto
      return SysSendto(proc, a0, a1, a2, a3, a4, a5);
    case 45:  // SYS_recvfrom
      return SysRecvfrom(proc, a0, a1, a2, a3, a4, a5);
    case 46: {  // SYS_sendmsg
      std::shared_ptr<FileDescription> desc;
      int64_t r = GetSocket(proc, a0, &desc);
      if (r) return r;
      return DoSendmsg(proc, desc.get(),
                       reinterpret_cast<const LinuxMsghdr*>(a1),
                       static_cast<int>(a2));
    }
    case 47: {  // SYS_recvmsg
      std::shared_ptr<FileDescription> desc;
      int64_t r = GetSocket(proc, a0, &desc);
      if (r) return r;
      return DoRecvmsg(proc, desc.get(), reinterpret_cast<LinuxMsghdr*>(a1),
                       static_cast<int>(a2));
    }
    case 307:  // SYS_sendmmsg
      return SysSendmmsg(proc, a0, a1, a2, a3);
    case 299:  // SYS_recvmmsg
      return SysRecvmmsg(proc, a0, a1, a2, a3, a4);
    case 48:  // SYS_shutdown
      return SysShutdown(proc, a0, a1);
    case 51:  // SYS_getsockname
      return SysGetName(proc, a0, a1, a2, false);
    case 52:  // SYS_getpeername
      return SysGetName(proc, a0, a1, a2, true);
    case 54:  // SYS_setsockopt
      return SysSetsockopt(proc, a0, a1, a2, a3, a4);
    case 55:  // SYS_getsockopt
      return SysGetsockopt(proc, a0, a1, a2, a3, a4);
    case 284:  // SYS_eventfd
      return SysEventfd(proc, a0, 0);
    case 290:  // SYS_eventfd2
      return SysEventfd(proc, a0, static_cast<uint32_t>(a1));
    case 283:  // SYS_timerfd_create
      return SysTimerfdCreate(proc, a0, a1);
    case 286:  // SYS_timerfd_settime
      return SysTimerfdSettime(proc, a0, a1, a2, a3);
    case 287:  // SYS_timerfd_gettime
      return SysTimerfdGettime(proc, a0, a1);
    case 282:  // SYS_signalfd
      return SysSignalfd(proc, a0, a1, a2, 0);
    case 289:  // SYS_signalfd4
      return SysSignalfd(proc, a0, a1, a2, static_cast<uint32_t>(a3));
    case 253:  // SYS_inotify_init
      return SysInotifyInit(proc, 0);
    case 294:  // SYS_inotify_init1
      return SysInotifyInit(proc, static_cast<uint32_t>(a0));
    case 254:  // SYS_inotify_add_watch
      return SysInotifyAddWatch(proc, a0, a1, a2);
    case 255:  // SYS_inotify_rm_watch
      return SysInotifyRmWatch(proc, a0, a1);
    default:
      return kENOSYS;
  }
}
