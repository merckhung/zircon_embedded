// poll(2), select(2) and epoll(7) for the Linux shim.

#include <algorithm>

#include "src/shim/fd_ops.h"
#include "src/shim/tty.h"
#include "src/shim/vfs.h"

namespace {

constexpr uint32_t kEpollIn = 0x001, kEpollPri = 0x002, kEpollOut = 0x004,
                   kEpollErr = 0x008, kEpollHup = 0x010, kEpollRdNorm = 0x040,
                   kEpollRdBand = 0x080, kEpollWrNorm = 0x100,
                   kEpollWrBand = 0x200, kEpollMsg = 0x400,
                   kEpollRdHup = 0x2000, kEpollExclusive = 1u << 28,
                   kEpollWakeup = 1u << 29, kEpollOneShot = 1u << 30,
                   kEpollEt = 1u << 31;
constexpr size_t kPipeBuf = 4096;

int64_t FdLimit(LinuxProcess* proc) {
  if (proc->rlimits_init) return static_cast<int64_t>(proc->rlimits[7][0]);
  return 1024;
}

// Installs a temporary signal mask for the duration of a ppoll/pselect6/
// epoll_pwait call. The signal return path (signals.cc) restores
// saved_sigmask (or stores it in the signal frame) when restore_sigmask is set.
void InstallTempSigmask(LinuxThread* thrd, uint64_t mask) {
  if (!thrd) return;
  constexpr uint64_t kUnblockable = (1ULL << (9 - 1)) | (1ULL << (19 - 1));
  if (!thrd->restore_sigmask) {
    thrd->saved_sigmask = thrd->sigmask.load();
    thrd->restore_sigmask = true;
  }
  thrd->sigmask.store(mask & ~kUnblockable);
}

// Converts a struct timespec to ns. Returns false (EINVAL) if invalid.
bool TimespecToNs(const int64_t* ts, int64_t* out) {
  if (ts[0] < 0 || ts[1] < 0 || ts[1] >= 1000000000LL) return false;
  if (ts[0] > INT64_MAX / 1000000000LL - 1) {
    *out = INT64_MAX;
  } else {
    *out = ts[0] * 1000000000LL + ts[1];
  }
  return true;
}

// Waits until `ready()` returns true, the timeout expires (timeout_ns < 0:
// infinite) or a signal is pending. Returns 1 (ready), 0 (timeout) or -1
// (signal). *remaining_ns gets the unexpired part of the timeout.
template <typename F>
int WaitLoop(LinuxProcess* proc, int64_t timeout_ns, F ready,
             int64_t* remaining_ns) {
  uint64_t start = zx_clock_get_monotonic();
  while (true) {
    uint64_t seq = CurrentPollSeq();
    if (ready()) {
      if (remaining_ns && timeout_ns >= 0) {
        int64_t el = static_cast<int64_t>(zx_clock_get_monotonic() - start);
        *remaining_ns = el >= timeout_ns ? 0 : timeout_ns - el;
      }
      return 1;
    }
    int64_t elapsed = static_cast<int64_t>(zx_clock_get_monotonic() - start);
    if (timeout_ns >= 0 && elapsed >= timeout_ns) {
      if (remaining_ns) *remaining_ns = 0;
      return 0;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) {
      if (remaining_ns && timeout_ns >= 0) *remaining_ns = timeout_ns - elapsed;
      return -1;
    }
    int64_t wait_ms = 10;
    if (timeout_ns >= 0) {
      int64_t rem_ms = (timeout_ns - elapsed + 999999) / 1000000;
      wait_ms = std::max<int64_t>(1, std::min<int64_t>(wait_ms, rem_ms));
    }
    WaitForPollEvent(wait_ms, seq);
  }
}

void WriteTimespec(int64_t* ts, int64_t ns) {
  ts[0] = ns / 1000000000LL;
  ts[1] = ns % 1000000000LL;
}

int64_t DoPoll(LinuxProcess* proc, LinuxPollFd* fds, size_t nfds,
               int64_t timeout_ns, int64_t* remaining_ns, bool is_ppoll) {
  if (static_cast<int64_t>(nfds) > FdLimit(proc)) return kEINVAL;
  if (nfds > 0 && fds == nullptr) return kEFAULT;
  int ready_count = 0;
  auto scan = [&]() {
    ready_count = 0;
    for (size_t i = 0; i < nfds; ++i) {
      fds[i].revents = 0;
      if (fds[i].fd < 0) continue;
      auto desc = LookupFd(proc, fds[i].fd);
      uint32_t ev = CheckFdPollEvents(desc.get());
      uint32_t want = static_cast<uint16_t>(fds[i].events) | kPollErr |
                      kPollHup | kPollNval;
      uint32_t matched = ev & want;
      if (matched != 0) {
        fds[i].revents = static_cast<int16_t>(matched);
        ready_count++;
      }
    }
    return ready_count > 0;
  };
  int r = WaitLoop(proc, timeout_ns, scan, remaining_ns);
  if (r < 0) return is_ppoll ? kERESTARTNOHAND : kERESTARTNOHAND;
  return ready_count;
}

int64_t DoSelect(LinuxProcess* proc, int nfds, uint64_t* rfds, uint64_t* wfds,
                 uint64_t* efds, int64_t timeout_ns, int64_t* remaining_ns) {
  if (nfds < 0) return kEINVAL;
  int64_t limit = FdLimit(proc);
  if (nfds > limit) nfds = static_cast<int>(limit);
  size_t words = (static_cast<size_t>(nfds) + 63) / 64;
  std::vector<uint64_t> in_r(words, 0), in_w(words, 0), in_e(words, 0);
  // Only copy the bytes covering nfds bits (fd_set may be smaller than words*8).
  size_t bytes = (static_cast<size_t>(nfds) + 7) / 8;
  if (rfds) memcpy(in_r.data(), rfds, bytes);
  if (wfds) memcpy(in_w.data(), wfds, bytes);
  if (efds) memcpy(in_e.data(), efds, bytes);
  auto bit_set = [](const std::vector<uint64_t>& v, int fd) {
    return (v[fd / 64] >> (fd % 64)) & 1;
  };
  // EBADF if any requested fd is not open.
  for (int fd = 0; fd < nfds; ++fd) {
    if (bit_set(in_r, fd) || bit_set(in_w, fd) || bit_set(in_e, fd)) {
      if (!LookupFd(proc, fd)) return kEBADF;
    }
  }
  std::vector<uint64_t> out_r(words), out_w(words), out_e(words);
  int ready_count = 0;
  auto scan = [&]() {
    ready_count = 0;
    std::fill(out_r.begin(), out_r.end(), 0);
    std::fill(out_w.begin(), out_w.end(), 0);
    std::fill(out_e.begin(), out_e.end(), 0);
    for (int fd = 0; fd < nfds; ++fd) {
      bool wr = bit_set(in_r, fd), ww = bit_set(in_w, fd), we = bit_set(in_e, fd);
      if (!wr && !ww && !we) continue;
      auto desc = LookupFd(proc, fd);
      uint32_t ev = CheckFdPollEvents(desc.get());
      uint64_t bit = 1ULL << (fd % 64);
      if (wr && (ev & (kPollIn | kPollHup | kPollErr))) {
        out_r[fd / 64] |= bit;
        ready_count++;
      }
      if (ww && (ev & (kPollOut | kPollErr))) {
        out_w[fd / 64] |= bit;
        ready_count++;
      }
      if (we && (ev & kPollPri)) {
        out_e[fd / 64] |= bit;
        ready_count++;
      }
    }
    return ready_count > 0;
  };
  int r = WaitLoop(proc, timeout_ns, scan, remaining_ns);
  if (r < 0) return kERESTARTNOHAND;
  if (rfds) memcpy(rfds, out_r.data(), bytes);
  if (wfds) memcpy(wfds, out_w.data(), bytes);
  if (efds) memcpy(efds, out_e.data(), bytes);
  return ready_count;
}

int64_t DoEpollWait(LinuxProcess* proc, int epfd, LinuxEpollEvent* out,
                    int maxevents, int64_t timeout_ns) {
  auto ep_desc = LookupFd(proc, epfd);
  if (!ep_desc) return kEBADF;
  if (ep_desc->kind != FdKind::kEpoll || !ep_desc->epoll) return kEINVAL;
  if (maxevents <= 0 ||
      static_cast<size_t>(maxevents) > INT32_MAX / sizeof(LinuxEpollEvent)) {
    return kEINVAL;
  }
  if (!out) return kEFAULT;
  auto ep = ep_desc->epoll;
  int count = 0;
  auto scan = [&]() {
    count = 0;
    std::lock_guard<std::mutex> lock(ep->mtx);
    for (auto it = ep->entries.begin(); it != ep->entries.end();) {
      EpollEntry& e = it->second;
      auto d = e.desc.lock();
      if (!d) {  // description released: Linux drops the registration
        it = ep->entries.erase(it);
        continue;
      }
      if (count >= maxevents) break;
      if (e.disabled) {
        ++it;
        continue;
      }
      uint32_t ready = CheckFdPollEvents(d.get()) &
                       ((e.events & ~(kEpollEt | kEpollOneShot | kEpollWakeup |
                                      kEpollExclusive)) |
                        kEpollErr | kEpollHup);
      bool report = ready != 0;
      if (report && (e.events & kEpollEt)) {
        uint64_t counter = FdEdgeCounter(d.get());
        report = (ready & ~e.last_reported) != 0 || counter != e.last_counter;
        e.last_counter = counter;
      }
      e.last_reported = ready;
      if (report) {
        out[count].events = ready;
        out[count].data = e.data;
        count++;
        if (e.events & kEpollOneShot) e.disabled = true;
      }
      ++it;
    }
    return count > 0;
  };
  int r = WaitLoop(proc, timeout_ns, scan, nullptr);
  if (r < 0) return kEINTR;  // epoll_wait is never restarted (signal(7))
  return count;
}

}  // namespace

uint32_t CheckFdPollEvents(FileDescription* desc) {
  if (!desc) return kPollNval;
  switch (desc->kind) {
    case FdKind::kVfsFile:
    case FdKind::kProcDynamic:
    case FdKind::kDevNull:
    case FdKind::kDevZero:
    case FdKind::kDevFull:
    case FdKind::kDevRandom:
    case FdKind::kDevFb0:
      return kPollIn | kPollOut | kPollRdNorm | kPollWrNorm;
    case FdKind::kSerialConsole:
      return TtyPollEvents(GetConsoleTty(), false);
    case FdKind::kDevInputEvent0:
      return InputHasEvents() ? (kPollIn | kPollRdNorm) : 0;
    case FdKind::kPipeRead: {
      auto pipe = desc->pipe;
      if (!pipe) return kPollHup;
      std::lock_guard<std::mutex> lock(pipe->mtx);
      uint32_t ev = 0;
      if (!pipe->buf.empty()) ev |= kPollIn | kPollRdNorm;
      if (pipe->writers <= 0) ev |= kPollHup;
      return ev;
    }
    case FdKind::kPipeWrite: {
      auto pipe = desc->pipe;
      if (!pipe) return kPollErr;
      std::lock_guard<std::mutex> lock(pipe->mtx);
      if (pipe->readers <= 0) return kPollErr | kPollOut | kPollWrNorm;
      size_t space = pipe->capacity > pipe->buf.size()
                         ? pipe->capacity - pipe->buf.size()
                         : 0;
      return space >= std::min(kPipeBuf, pipe->capacity)
                 ? (kPollOut | kPollWrNorm)
                 : 0;
    }
    case FdKind::kPtyMaster:
      return desc->pty ? TtyPollEvents(desc->pty, true) : kPollHup;
    case FdKind::kPtySlave:
      return desc->pty ? TtyPollEvents(desc->pty, false) : kPollHup;
    case FdKind::kUnixSocket:
      return SocketPollEvents(desc);
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
      return IpcPollEvents(desc);
    case FdKind::kEpoll: {
      auto ep = desc->epoll;
      if (!ep) return 0;
      std::vector<std::pair<std::shared_ptr<FileDescription>, uint32_t>> items;
      {
        std::lock_guard<std::mutex> lock(ep->mtx);
        for (const auto& kv : ep->entries) {
          if (kv.second.disabled) continue;
          auto d = kv.second.desc.lock();
          if (d && d.get() != desc) items.push_back({d, kv.second.events});
        }
      }
      for (const auto& it : items) {
        if (CheckFdPollEvents(it.first.get()) &
            (it.second | kEpollErr | kEpollHup) & ~(kEpollEt | kEpollOneShot)) {
          return kPollIn | kPollRdNorm;
        }
      }
      return 0;
    }
    default:
      return 0;
  }
}

uint64_t FdEdgeCounter(FileDescription* desc) {
  if (!desc) return 0;
  switch (desc->kind) {
    case FdKind::kPipeRead:
    case FdKind::kPipeWrite: {
      if (!desc->pipe) return 0;
      std::lock_guard<std::mutex> lock(desc->pipe->mtx);
      return desc->pipe->total_written + desc->pipe->total_read +
             static_cast<uint64_t>(desc->pipe->readers + desc->pipe->writers);
    }
    case FdKind::kPtyMaster:
    case FdKind::kPtySlave:
      return desc->pty ? TtyEdgeCounter(desc->pty, desc->kind == FdKind::kPtyMaster)
                       : 0;
    case FdKind::kSerialConsole:
      return TtyEdgeCounter(GetConsoleTty(), false);
    case FdKind::kDevInputEvent0:
      return InputEventCounter();
    case FdKind::kUnixSocket:
    case FdKind::kEventFd:
    case FdKind::kTimerFd:
    case FdKind::kSignalFd:
    case FdKind::kInotify:
      return IpcEdgeCounter(desc);
    default:
      return 0;
  }
}

int64_t HandlePollSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                          uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5) {
  switch (sc) {
    case 7: {  // SYS_poll
      int32_t timeout_ms = static_cast<int32_t>(a2);
      int64_t timeout_ns = timeout_ms < 0 ? -1 : timeout_ms * 1000000LL;
      return DoPoll(proc, reinterpret_cast<LinuxPollFd*>(a0),
                    static_cast<size_t>(a1), timeout_ns, nullptr, false);
    }
    case 271: {  // SYS_ppoll
      int64_t* ts = reinterpret_cast<int64_t*>(a2);
      int64_t timeout_ns = -1;
      if (ts && !TimespecToNs(ts, &timeout_ns)) return kEINVAL;
      if (a3 != 0) {
        if (a4 != 8) return kEINVAL;
        InstallTempSigmask(thrd, *reinterpret_cast<const uint64_t*>(a3));
      }
      int64_t remaining = 0;
      int64_t rc = DoPoll(proc, reinterpret_cast<LinuxPollFd*>(a0),
                          static_cast<size_t>(a1), timeout_ns, &remaining, true);
      if (ts && timeout_ns >= 0) WriteTimespec(ts, remaining);
      return rc;
    }
    case 23: {  // SYS_select
      int64_t* tv = reinterpret_cast<int64_t*>(a4);
      int64_t timeout_ns = -1;
      if (tv) {
        if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000) return kEINVAL;
        timeout_ns = tv[0] * 1000000000LL + tv[1] * 1000LL;
      }
      int64_t remaining = 0;
      int64_t rc = DoSelect(proc, static_cast<int>(a0),
                            reinterpret_cast<uint64_t*>(a1),
                            reinterpret_cast<uint64_t*>(a2),
                            reinterpret_cast<uint64_t*>(a3), timeout_ns,
                            &remaining);
      if (tv && timeout_ns >= 0 && rc != kEBADF && rc != kEINVAL) {
        tv[0] = remaining / 1000000000LL;
        tv[1] = (remaining % 1000000000LL) / 1000;
      }
      return rc;
    }
    case 270: {  // SYS_pselect6
      int64_t* ts = reinterpret_cast<int64_t*>(a4);
      int64_t timeout_ns = -1;
      if (ts && !TimespecToNs(ts, &timeout_ns)) return kEINVAL;
      if (a5 != 0) {
        const uint64_t* data = reinterpret_cast<const uint64_t*>(a5);
        const uint64_t* ss = reinterpret_cast<const uint64_t*>(data[0]);
        if (ss) {
          if (data[1] != 8) return kEINVAL;
          InstallTempSigmask(thrd, *ss);
        }
      }
      int64_t remaining = 0;
      int64_t rc = DoSelect(proc, static_cast<int>(a0),
                            reinterpret_cast<uint64_t*>(a1),
                            reinterpret_cast<uint64_t*>(a2),
                            reinterpret_cast<uint64_t*>(a3), timeout_ns,
                            &remaining);
      if (ts && timeout_ns >= 0 && rc != kEBADF && rc != kEINVAL) {
        WriteTimespec(ts, remaining);
      }
      return rc;
    }
    case 213:    // SYS_epoll_create
    case 291: {  // SYS_epoll_create1
      bool cloexec = false;
      if (sc == 213) {
        if (static_cast<int>(a0) <= 0) return kEINVAL;
      } else {
        if ((a0 & ~static_cast<uint64_t>(kLinuxO_CLOEXEC)) != 0) return kEINVAL;
        cloexec = (a0 & kLinuxO_CLOEXEC) != 0;
      }
      auto desc = std::make_shared<FileDescription>();
      desc->kind = FdKind::kEpoll;
      desc->flags = kLinuxO_RDWR;
      desc->epoll = std::make_shared<EpollState>();
      desc->anon_ino = AllocAnonInode();
      desc->create_time_ns = Vfs::NowNs();
      return InstallFd(proc, std::move(desc), cloexec);
    }
    case 233: {  // SYS_epoll_ctl
      int epfd = static_cast<int>(a0);
      int op = static_cast<int>(a1);
      int fd = static_cast<int>(a2);
      const auto* ev = reinterpret_cast<const LinuxEpollEvent*>(a3);
      auto ep_desc = LookupFd(proc, epfd);
      if (!ep_desc) return kEBADF;
      auto tdesc = LookupFd(proc, fd);
      if (!tdesc) return kEBADF;
      if (ep_desc->kind != FdKind::kEpoll || !ep_desc->epoll) return kEINVAL;
      if (fd == epfd || tdesc == ep_desc) return kEINVAL;
      if (op != 1 && op != 2 && op != 3) return kEINVAL;
      if (op != 2 && !ev) return kEFAULT;
      // Regular files and directories do not support poll: EPERM on Linux.
      if (tdesc->kind == FdKind::kVfsFile) return kEPERM;
      auto ep = ep_desc->epoll;
      std::lock_guard<std::mutex> lock(ep->mtx);
      auto key = std::make_pair(fd, tdesc.get());
      auto it = ep->entries.find(key);
      if (it != ep->entries.end() && it->second.desc.expired()) {
        ep->entries.erase(it);
        it = ep->entries.end();
      }
      if (op == 2) {  // EPOLL_CTL_DEL
        if (it == ep->entries.end()) return kENOENT;
        ep->entries.erase(it);
        return 0;
      }
      uint32_t events = ev->events;
      if (op == 1) {  // EPOLL_CTL_ADD
        if (it != ep->entries.end()) return kEEXIST;
        if ((events & kEpollExclusive) &&
            (events & ~(kEpollExclusive | kEpollIn | kEpollOut | kEpollErr |
                        kEpollHup | kEpollWakeup | kEpollEt | kEpollRdNorm |
                        kEpollWrNorm | kEpollRdHup | kEpollPri))) {
          return kEINVAL;
        }
        if ((events & kEpollExclusive) && tdesc->kind == FdKind::kEpoll) {
          return kEINVAL;
        }
        EpollEntry e;
        e.fd = fd;
        e.desc = tdesc;
        e.events = events & ~kEpollWakeup;  // needs CAP_BLOCK_SUSPEND semantics
        e.data = ev->data;
        ep->entries[key] = e;
        NotifyPollWaiters();
        return 0;
      }
      // EPOLL_CTL_MOD
      if (it == ep->entries.end()) return kENOENT;
      if ((events & kEpollExclusive) || (it->second.events & kEpollExclusive)) {
        return kEINVAL;
      }
      it->second.events = events & ~kEpollWakeup;
      it->second.data = ev->data;
      it->second.disabled = false;
      it->second.last_reported = 0;
      NotifyPollWaiters();
      return 0;
    }
    case 232:    // SYS_epoll_wait
    case 281:    // SYS_epoll_pwait
    case 441: {  // SYS_epoll_pwait2
      int64_t timeout_ns = -1;
      if (sc == 441) {
        const int64_t* ts = reinterpret_cast<const int64_t*>(a3);
        if (ts && !TimespecToNs(ts, &timeout_ns)) return kEINVAL;
      } else {
        int32_t timeout_ms = static_cast<int32_t>(a3);
        timeout_ns = timeout_ms < 0 ? -1 : timeout_ms * 1000000LL;
      }
      if (sc != 232 && a4 != 0) {
        if (a5 != 8) return kEINVAL;
        InstallTempSigmask(thrd, *reinterpret_cast<const uint64_t*>(a4));
      }
      return DoEpollWait(proc, static_cast<int>(a0),
                         reinterpret_cast<LinuxEpollEvent*>(a1),
                         static_cast<int>(a2), timeout_ns);
    }
    default:
      return kENOSYS;
  }
  (void)kEpollPri;
  (void)kEpollRdBand;
  (void)kEpollWrBand;
  (void)kEpollMsg;
  (void)kEpollIn;
  (void)kEpollOut;
}
