#include "src/shim/tty.h"

#include <algorithm>
#include <set>

#include "src/shim/fd_ops.h"
#include "src/shim/vfs.h"

namespace {

// termios flag bits (asm-generic/termbits.h)
constexpr uint32_t kISTRIP = 0000040, kINLCR = 0000100, kIGNCR = 0000200,
                   kICRNL = 0000400, kIXON = 0002000, kIXANY = 0004000,
                   kIUTF8 = 0040000;
constexpr uint32_t kOPOST = 0000001, kONLCR = 0000004, kOCRNL = 0000010,
                   kONOCR = 0000020, kONLRET = 0000040;
constexpr uint32_t kISIG = 0000001, kICANON = 0000002, kECHO = 0000010,
                   kECHOE = 0000020, kECHOK = 0000040, kECHONL = 0000100,
                   kNOFLSH = 0000200, kTOSTOP = 0000400, kECHOCTL = 0001000,
                   kECHOKE = 0004000, kIEXTEN = 0100000;
enum {
  kVINTR = 0, kVQUIT = 1, kVERASE = 2, kVKILL = 3, kVEOF = 4, kVTIME = 5,
  kVMIN = 6, kVSTART = 8, kVSTOP = 9, kVSUSP = 10, kVEOL = 11, kVREPRINT = 12,
  kVWERASE = 14, kVLNEXT = 15, kVEOL2 = 16,
};
constexpr uint8_t kPktFlushRead = 1, kPktFlushWrite = 2, kPktStop = 4,
                  kPktStart = 8, kPktNoStop = 16, kPktDoStop = 32;
constexpr size_t kOutputCapacity = 65536;  // slave -> master buffer limit

constexpr int kSIGHUP = 1, kSIGINT = 2, kSIGQUIT = 3, kSIGCONT = 18,
              kSIGTSTP = 20, kSIGTTIN = 21, kSIGTTOU = 22, kSIGWINCH = 28;

// Linux pty/tty_std_termios defaults.
LinuxTermios DefaultTermios() {
  LinuxTermios t{};
  t.c_iflag = kICRNL | kIXON;
  t.c_oflag = kOPOST | kONLCR;
  t.c_cflag = 0000017 /*B38400*/ | 0000060 /*CS8*/ | 0000200 /*CREAD*/ |
              0002000 /*HUPCL*/;
  t.c_lflag = kISIG | kICANON | kECHO | kECHOE | kECHOK | kECHOCTL | kECHOKE |
              kIEXTEN;
  const uint8_t cc[19] = {3, 28, 127, 21, 4, 0, 1, 0, 17, 19,
                          26, 0, 18, 15, 23, 22, 0, 0, 0};
  memcpy(t.c_cc, cc, sizeof(cc));
  return t;
}

uint64_t MakeDev(uint32_t major, uint32_t minor) {
  return (minor & 0xffULL) | (static_cast<uint64_t>(major) << 8) |
         (static_cast<uint64_t>(minor & ~0xffU) << 12);
}

std::mutex g_pty_mtx;
std::map<int, std::shared_ptr<PtyPair>> g_ptys;

std::mutex g_ctty_mtx;
std::map<int, std::weak_ptr<PtyPair>> g_session_ctty;  // sid -> ctty
std::set<int>* g_ctty_detached = new std::set<int>();   // TIOCNOTTY by non-leaders

bool IsCc(const LinuxTermios& t, int idx, uint8_t c) {
  return t.c_cc[idx] != 0 /*_POSIX_VDISABLE*/ && t.c_cc[idx] == c;
}

// Echo one input character (ECHOCTL renders control chars as ^X).
void EchoChar(const std::shared_ptr<PtyPair>& tty, uint8_t c,
              std::deque<uint8_t>* out) {
  const LinuxTermios& t = tty->termios;
  uint8_t tmp[2];
  size_t n = 0;
  if ((t.c_lflag & kECHOCTL) && (c < 32 || c == 0x7f) && c != '\n' &&
      c != '\t' && !IsCc(t, kVSTART, c) && !IsCc(t, kVSTOP, c)) {
    tmp[n++] = '^';
    tmp[n++] = c == 0x7f ? '?' : static_cast<uint8_t>(c + 64);
  } else {
    tmp[n++] = c;
  }
  if (tty->is_console) {
    out->insert(out->end(), tmp, tmp + n);
  } else {
    TtyProcessOutputLocked(t, tmp, n, out);
  }
}

void EchoRaw(const std::shared_ptr<PtyPair>& tty, const char* s,
             std::deque<uint8_t>* out) {
  if (tty->is_console) {
    out->insert(out->end(), s, s + strlen(s));
  } else {
    TtyProcessOutputLocked(tty->termios, reinterpret_cast<const uint8_t*>(s),
                           strlen(s), out);
  }
}

// Visually erase the last character of the line being edited.
void EraseOne(const std::shared_ptr<PtyPair>& tty, std::deque<uint8_t>* echo) {
  const LinuxTermios& t = tty->termios;
  if (tty->canon_line.empty()) return;
  // In UTF-8 mode remove a whole multi-byte sequence.
  uint8_t c = static_cast<uint8_t>(tty->canon_line.back());
  tty->canon_line.pop_back();
  if (t.c_iflag & kIUTF8) {
    while (!tty->canon_line.empty() && (c & 0xC0) == 0x80) {
      c = static_cast<uint8_t>(tty->canon_line.back());
      tty->canon_line.pop_back();
    }
  }
  if ((t.c_lflag & kECHO) == 0) return;
  if (t.c_lflag & kECHOE) {
    bool ctl = (t.c_lflag & kECHOCTL) && (c < 32 || c == 0x7f) && c != '\t';
    EchoRaw(tty, ctl ? "\b\b  \b\b" : "\b \b", echo);
  } else {
    EchoChar(tty, t.c_cc[kVERASE], echo);
  }
}

void CompleteLine(const std::shared_ptr<PtyPair>& tty) {
  for (char ch : tty->canon_line) {
    tty->master_to_slave.push_back(static_cast<uint8_t>(ch));
  }
  tty->m2s_total += tty->canon_line.size() + 1;
  tty->canon_lines.push_back(tty->canon_line.size());
  tty->canon_line.clear();
}

void FlushInputLocked(const std::shared_ptr<PtyPair>& tty) {
  tty->master_to_slave.clear();
  tty->canon_lines.clear();
  tty->canon_line.clear();
  tty->lnext_pending = false;
  if (!tty->is_console) tty->pkt_status |= kPktFlushRead;
}

// Bytes readable from the slave side right now.
size_t SlaveReadableLocked(const std::shared_ptr<PtyPair>& tty, bool for_poll) {
  const LinuxTermios& t = tty->termios;
  if (t.c_lflag & kICANON) {
    if (!tty->canon_lines.empty()) return std::max<size_t>(tty->canon_lines.front(), 1);
    return tty->master_to_slave.size();
  }
  size_t avail = tty->master_to_slave.size();
  if (for_poll) {
    size_t need = (t.c_cc[kVTIME] == 0 && t.c_cc[kVMIN] > 0) ? t.c_cc[kVMIN] : 1;
    return avail >= need ? avail : 0;
  }
  return avail;
}

bool SignalIgnoredOrBlocked(LinuxProcess* proc, int sig) {
  bool ignored;
  {
    std::lock_guard<std::mutex> lock(proc->sig_mtx);
    ignored = proc->sigactions[sig].sa_handler == 1;
  }
  LinuxThread* t = t_current_thread;
  bool blocked = t && (t->sigmask.load() & (1ULL << (sig - 1))) != 0;
  return ignored || blocked;
}

bool IsCttyOf(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty) {
  return TtyGetCtty(proc) == tty;
}

void ClearSessionCtty(int sid) {
  std::lock_guard<std::mutex> lock(g_ctty_mtx);
  g_session_ctty.erase(sid);
}

}  // namespace

// ---------------------------------------------------------------------------

std::shared_ptr<PtyPair> GetConsoleTty() {
  static std::shared_ptr<PtyPair> console = [] {
    auto t = std::make_shared<PtyPair>();
    t->index = -1;
    t->is_console = true;
    t->termios = DefaultTermios();
    t->termios.c_cflag &= ~0002000U;  // no HUPCL on the console
    t->winsize = {0, 0, 0, 0};        // a UART has no window size
    t->master_count = 1;              // the UART never hangs up
    t->slave_opened = true;
    return t;
  }();
  return console;
}

void TtyProcessOutputLocked(const LinuxTermios& t, const uint8_t* buf,
                            size_t len, std::deque<uint8_t>* out) {
  if ((t.c_oflag & kOPOST) == 0) {
    out->insert(out->end(), buf, buf + len);
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    uint8_t c = buf[i];
    if (c == '\n' && (t.c_oflag & kONLCR)) {
      out->push_back('\r');
      out->push_back('\n');
    } else if (c == '\r' && (t.c_oflag & kOCRNL)) {
      out->push_back('\n');
    } else if (c == '\r' && (t.c_oflag & kONOCR) && !out->empty() &&
               out->back() == '\n') {
      // ONOCR: no CR at column 0
    } else {
      out->push_back(c);
    }
    (void)kONLRET;  // ONLRET only affects column tracking
  }
}

void TtyInput(const std::shared_ptr<PtyPair>& tty, const uint8_t* buf,
              size_t len) {
  std::vector<int> signals;
  int fg = 0;
  std::deque<uint8_t> console_echo;
  {
    std::lock_guard<std::mutex> lock(tty->mtx);
    std::deque<uint8_t>* echo =
        tty->is_console ? &console_echo : &tty->slave_to_master;
    size_t echo_before = echo->size();
    const LinuxTermios& t = tty->termios;
    for (size_t i = 0; i < len; ++i) {
      uint8_t c = buf[i];
      if (t.c_iflag & kISTRIP) c &= 0x7f;
      bool canon = (t.c_lflag & kICANON) != 0;
      bool do_echo = (t.c_lflag & kECHO) != 0;

      if (tty->lnext_pending) {
        tty->lnext_pending = false;
        if (canon) {
          tty->canon_line.push_back(static_cast<char>(c));
        } else {
          tty->master_to_slave.push_back(c);
          tty->m2s_total++;
        }
        if (do_echo) EchoChar(tty, c, echo);
        continue;
      }

      if (c == '\r') {
        if (t.c_iflag & kIGNCR) continue;
        if (t.c_iflag & kICRNL) c = '\n';
      } else if (c == '\n' && (t.c_iflag & kINLCR)) {
        c = '\r';
      }

      if (t.c_iflag & kIXON) {
        if (IsCc(t, kVSTOP, c)) {
          tty->stopped = true;
          tty->pkt_status = (tty->pkt_status & ~kPktStart) | kPktStop;
          continue;
        }
        if (IsCc(t, kVSTART, c)) {
          tty->stopped = false;
          tty->pkt_status = (tty->pkt_status & ~kPktStop) | kPktStart;
          continue;
        }
        if (tty->stopped && (t.c_iflag & kIXANY)) {
          tty->stopped = false;
          tty->pkt_status = (tty->pkt_status & ~kPktStop) | kPktStart;
        }
      }

      if (t.c_lflag & kISIG) {
        int sig = 0;
        if (IsCc(t, kVINTR, c)) sig = kSIGINT;
        else if (IsCc(t, kVQUIT, c)) sig = kSIGQUIT;
        else if (IsCc(t, kVSUSP, c)) sig = kSIGTSTP;
        if (sig) {
          if ((t.c_lflag & kNOFLSH) == 0) {
            FlushInputLocked(tty);
            if (!tty->is_console) tty->pkt_status |= kPktFlushWrite;
          }
          if (do_echo) EchoChar(tty, c, echo);
          signals.push_back(sig);
          continue;
        }
      }

      if (canon) {
        bool iexten = (t.c_lflag & kIEXTEN) != 0;
        if (IsCc(t, kVERASE, c)) {
          EraseOne(tty, echo);
          continue;
        }
        if (iexten && IsCc(t, kVWERASE, c)) {
          while (!tty->canon_line.empty() &&
                 isspace(static_cast<unsigned char>(tty->canon_line.back()))) {
            EraseOne(tty, echo);
          }
          while (!tty->canon_line.empty() &&
                 !isspace(static_cast<unsigned char>(tty->canon_line.back()))) {
            EraseOne(tty, echo);
          }
          continue;
        }
        if (IsCc(t, kVKILL, c)) {
          if (do_echo && (t.c_lflag & kECHOKE) && (t.c_lflag & kECHOE)) {
            while (!tty->canon_line.empty()) EraseOne(tty, echo);
          } else {
            tty->canon_line.clear();
            if (do_echo) {
              EchoChar(tty, c, echo);
              if (t.c_lflag & kECHOK) EchoRaw(tty, "\n", echo);
            }
          }
          tty->canon_line.clear();
          continue;
        }
        if (iexten && IsCc(t, kVLNEXT, c)) {
          tty->lnext_pending = true;
          if (do_echo && (t.c_lflag & kECHOCTL)) EchoRaw(tty, "^\b", echo);
          continue;
        }
        if (iexten && IsCc(t, kVREPRINT, c)) {
          if (do_echo) {
            EchoChar(tty, c, echo);
            EchoRaw(tty, "\n", echo);
            for (char ch : tty->canon_line) {
              EchoChar(tty, static_cast<uint8_t>(ch), echo);
            }
          }
          continue;
        }
        if (IsCc(t, kVEOF, c)) {
          // The pending line (possibly empty -> read() returns 0) is made
          // available without the EOF character.
          CompleteLine(tty);
          continue;
        }
        bool eol = c == '\n' || IsCc(t, kVEOL, c) || (iexten && IsCc(t, kVEOL2, c));
        if (do_echo || (c == '\n' && (t.c_lflag & kECHONL))) EchoChar(tty, c, echo);
        tty->canon_line.push_back(static_cast<char>(c));
        if (eol) CompleteLine(tty);
        continue;
      }

      // Non-canonical input.
      tty->master_to_slave.push_back(c);
      tty->m2s_total++;
      if (do_echo) EchoChar(tty, c, echo);
    }
    if (!tty->is_console) tty->s2m_total += echo->size() - echo_before;
    fg = tty->fg_pgrp;
  }
  if (!console_echo.empty()) {
    std::string s(console_echo.begin(), console_echo.end());
    SerialWriteBytes(s.data(), s.size());
  }
  for (int sig : signals) {
    if (fg > 0) QueueSignalToProcessGroup(fg, sig);
  }
  NotifyPollWaiters();
}

ssize_t TtySlaveRead(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                     void* buf, size_t count, bool nonblock) {
  if (count == 0) return 0;
  uint8_t* dst = static_cast<uint8_t*>(buf);
  uint64_t timer_deadline = 0;  // VTIME timer (monotonic ns), 0 = not armed
  size_t last_avail = 0;
  while (true) {
    uint64_t seq = CurrentPollSeq();
    {
      std::unique_lock<std::mutex> lock(tty->mtx);
      // Job control: a background process group reading its controlling
      // terminal gets SIGTTIN (or EIO if SIGTTIN is ignored/blocked).
      if (tty->session != 0 && tty->session == proc->sid && tty->fg_pgrp > 0 &&
          proc->pgid != tty->fg_pgrp) {
        int pgid = proc->pgid;
        lock.unlock();
        if (SignalIgnoredOrBlocked(proc, kSIGTTIN)) return kEIO;
        QueueSignalToProcessGroup(pgid, kSIGTTIN);
        return kERESTARTSYS;
      }
      const LinuxTermios& t = tty->termios;
      if (t.c_lflag & kICANON) {
        if (!tty->canon_lines.empty()) {
          size_t& line = tty->canon_lines.front();
          if (line == 0) {  // VEOF on an empty line
            tty->canon_lines.pop_front();
            NotifyPollWaiters();
            return 0;
          }
          size_t n = std::min(count, line);
          for (size_t i = 0; i < n; ++i) {
            dst[i] = tty->master_to_slave.front();
            tty->master_to_slave.pop_front();
          }
          line -= n;
          if (line == 0) tty->canon_lines.pop_front();
          NotifyPollWaiters();
          return static_cast<ssize_t>(n);
        }
        if (!tty->master_to_slave.empty()) {
          // Bytes queued while the tty was in non-canonical mode.
          size_t n = std::min(count, tty->master_to_slave.size());
          for (size_t i = 0; i < n; ++i) {
            dst[i] = tty->master_to_slave.front();
            tty->master_to_slave.pop_front();
          }
          NotifyPollWaiters();
          return static_cast<ssize_t>(n);
        }
      } else {
        size_t avail = tty->master_to_slave.size();
        size_t vmin = t.c_cc[kVMIN];
        uint64_t vtime_ns = static_cast<uint64_t>(t.c_cc[kVTIME]) * 100000000ULL;
        uint64_t now = zx_clock_get_monotonic();
        bool ready = false;
        if (vmin == 0) {
          if (avail > 0) ready = true;
          else if (vtime_ns == 0) return 0;
          else if (timer_deadline == 0) timer_deadline = now + vtime_ns;
          else if (now >= timer_deadline) return 0;
        } else {
          if (avail >= std::min(vmin, count)) {
            ready = true;
          } else if (vtime_ns > 0 && avail > 0) {
            // Inter-byte timer, restarted whenever a byte arrives.
            if (avail != last_avail) timer_deadline = now + vtime_ns;
            if (now >= timer_deadline) ready = true;
          }
        }
        last_avail = avail;
        if (ready) {
          size_t n = std::min(count, avail);
          for (size_t i = 0; i < n; ++i) {
            dst[i] = tty->master_to_slave.front();
            tty->master_to_slave.pop_front();
          }
          NotifyPollWaiters();
          return static_cast<ssize_t>(n);
        }
      }
      // Master side gone (hangup): EOF.
      if (!tty->is_console && tty->master_count <= 0) return 0;
      if (nonblock) return kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    WaitForPollEvent(10, seq);
  }
}

ssize_t TtySlaveWrite(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                      const void* buf, size_t count, bool nonblock) {
  const uint8_t* src = static_cast<const uint8_t*>(buf);
  size_t done = 0;
  while (true) {
    uint64_t seq = CurrentPollSeq();
    {
      std::unique_lock<std::mutex> lock(tty->mtx);
      if (!tty->is_console && tty->master_count <= 0) {
        return done > 0 ? static_cast<ssize_t>(done) : kEIO;
      }
      if (done == 0 && (tty->termios.c_lflag & kTOSTOP) && tty->session != 0 &&
          tty->session == proc->sid && tty->fg_pgrp > 0 &&
          proc->pgid != tty->fg_pgrp && !SignalIgnoredOrBlocked(proc, kSIGTTOU)) {
        int pgid = proc->pgid;
        lock.unlock();
        QueueSignalToProcessGroup(pgid, kSIGTTOU);
        return kERESTARTSYS;
      }
      if (!tty->stopped) {
        if (tty->is_console) {
          // The UART path: Zircon's serial driver performs the \n -> \r\n
          // translation itself, so bytes are written as-is.
          lock.unlock();
          SerialWriteBytes(src, count);
          return static_cast<ssize_t>(count);
        }
        size_t space = tty->slave_to_master.size() < kOutputCapacity
                           ? kOutputCapacity - tty->slave_to_master.size()
                           : 0;
        if (space > 0) {
          size_t n = std::min(space, count - done);
          size_t before = tty->slave_to_master.size();
          TtyProcessOutputLocked(tty->termios, src + done, n,
                                 &tty->slave_to_master);
          tty->s2m_total += tty->slave_to_master.size() - before;
          done += n;
          NotifyPollWaiters();
          if (done == count) return static_cast<ssize_t>(done);
        }
      }
      if (nonblock) return done > 0 ? static_cast<ssize_t>(done) : kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) {
      return done > 0 ? static_cast<ssize_t>(done) : kERESTARTSYS;
    }
    WaitForPollEvent(10, seq);
  }
}

ssize_t TtyMasterRead(LinuxProcess* proc, const std::shared_ptr<PtyPair>& tty,
                      void* buf, size_t count, bool nonblock) {
  if (count == 0) return 0;
  uint8_t* dst = static_cast<uint8_t*>(buf);
  while (true) {
    uint64_t seq = CurrentPollSeq();
    {
      std::lock_guard<std::mutex> lock(tty->mtx);
      if (tty->packet_mode && tty->pkt_status != 0) {
        dst[0] = tty->pkt_status;
        tty->pkt_status = 0;
        return 1;
      }
      if (!tty->slave_to_master.empty()) {
        size_t off = tty->packet_mode ? 1 : 0;
        if (tty->packet_mode) dst[0] = 0;  // TIOCPKT_DATA
        size_t n = std::min(count - off, tty->slave_to_master.size());
        for (size_t i = 0; i < n; ++i) {
          dst[off + i] = tty->slave_to_master.front();
          tty->slave_to_master.pop_front();
        }
        NotifyPollWaiters();
        return static_cast<ssize_t>(off + n);
      }
      // All slave descriptions closed after having been opened.
      if (tty->slave_opened && tty->slave_count <= 0) return kEIO;
      if (nonblock) return kEAGAIN;
    }
    if (CurrentThreadHasUnblockedSignal(proc)) return kERESTARTSYS;
    WaitForPollEvent(10, seq);
  }
}

uint32_t TtyPollEvents(const std::shared_ptr<PtyPair>& tty, bool master) {
  std::lock_guard<std::mutex> lock(tty->mtx);
  uint32_t ev = 0;
  if (master) {
    if (!tty->slave_to_master.empty()) ev |= kPollIn | kPollRdNorm;
    if (tty->packet_mode && tty->pkt_status) ev |= kPollIn | kPollPri;
    if (tty->slave_opened && tty->slave_count <= 0) ev |= kPollHup;
    else ev |= kPollOut | kPollWrNorm;
    return ev;
  }
  if (SlaveReadableLocked(tty, true) > 0) ev |= kPollIn | kPollRdNorm;
  if (!tty->is_console && tty->master_count <= 0) {
    ev |= kPollHup | kPollIn;
  } else if (!tty->stopped &&
             (tty->is_console || tty->slave_to_master.size() < kOutputCapacity)) {
    ev |= kPollOut | kPollWrNorm;
  }
  return ev;
}

uint64_t TtyEdgeCounter(const std::shared_ptr<PtyPair>& tty, bool master) {
  (void)master;
  std::lock_guard<std::mutex> lock(tty->mtx);
  return tty->m2s_total + tty->s2m_total;
}

int TtyBytesAvailable(const std::shared_ptr<PtyPair>& tty, bool master) {
  std::lock_guard<std::mutex> lock(tty->mtx);
  if (master) return static_cast<int>(tty->slave_to_master.size());
  return static_cast<int>(tty->master_to_slave.size());
}

std::shared_ptr<PtyPair> PtyAllocate() {
  auto pty = std::make_shared<PtyPair>();
  {
    std::lock_guard<std::mutex> lock(g_pty_mtx);
    int idx = 0;
    while (g_ptys.count(idx)) idx++;  // Linux reuses the lowest free index
    pty->index = idx;
    g_ptys[idx] = pty;
  }
  pty->termios = DefaultTermios();
  pty->winsize = {0, 0, 0, 0};
  pty->locked = 1;  // unlockpt() (TIOCSPTLCK 0) required before slave open
  pty->master_count = 1;
  std::string path = "/dev/pts/" + std::to_string(pty->index);
  VfsNode* n = Vfs::CreateDevice(path, 0620, MakeDev(136, pty->index));
  if (n) {
    std::lock_guard<std::recursive_mutex> vlock(Vfs::Mutex());
    n->gid = 5;  // tty group, as devpts' default gid=5
  }
  return pty;
}

std::shared_ptr<PtyPair> PtyLookup(int index) {
  std::lock_guard<std::mutex> lock(g_pty_mtx);
  auto it = g_ptys.find(index);
  return it == g_ptys.end() ? nullptr : it->second;
}

void PtyMasterClosed(const std::shared_ptr<PtyPair>& pty) {
  int session = 0;
  {
    std::lock_guard<std::mutex> lock(pty->mtx);
    pty->master_count = 0;
    session = pty->session;
    pty->session = 0;
    pty->fg_pgrp = 0;
  }
  if (session != 0) {
    ClearSessionCtty(session);
    // Hangup: Linux signals the session leader with SIGHUP and SIGCONT.
    QueueSignalToPid(session, kSIGHUP);
    QueueSignalToPid(session, kSIGCONT);
  }
  {
    std::lock_guard<std::mutex> lock(g_pty_mtx);
    auto it = g_ptys.find(pty->index);
    if (it != g_ptys.end() && it->second == pty) g_ptys.erase(it);
  }
  Vfs::Unlink("/", "/dev/pts/" + std::to_string(pty->index), false);
  NotifyPollWaiters();
}

void PtySlaveClosed(const std::shared_ptr<PtyPair>& pty) {
  {
    std::lock_guard<std::mutex> lock(pty->mtx);
    if (pty->slave_count > 0) pty->slave_count--;
  }
  NotifyPollWaiters();
}

std::shared_ptr<PtyPair> TtyGetCtty(LinuxProcess* proc) {
  std::lock_guard<std::mutex> lock(g_ctty_mtx);
  if (g_ctty_detached->count(proc->pid)) return nullptr;
  auto it = g_session_ctty.find(proc->sid);
  if (it == g_session_ctty.end()) return nullptr;
  auto t = it->second.lock();
  if (!t) g_session_ctty.erase(it);
  return t;
}

void TtyMaybeAcquireCtty(LinuxProcess* proc,
                         const std::shared_ptr<PtyPair>& tty) {
  if (proc->pid != proc->sid) return;  // not a session leader
  if (TtyGetCtty(proc)) return;
  std::lock_guard<std::mutex> lock(tty->mtx);
  if (tty->session != 0) return;
  tty->session = proc->sid;
  tty->fg_pgrp = proc->pgid;
  std::lock_guard<std::mutex> clock(g_ctty_mtx);
  g_session_ctty[proc->sid] = tty;
}

bool TtySessionInfo(int sid, uint64_t* rdev, int* fg_pgrp) {
  std::shared_ptr<PtyPair> t;
  {
    std::lock_guard<std::mutex> lock(g_ctty_mtx);
    auto it = g_session_ctty.find(sid);
    if (it == g_session_ctty.end()) return false;
    t = it->second.lock();
  }
  if (!t) return false;
  *rdev = TtyRdev(t, false);
  std::lock_guard<std::mutex> lock(t->mtx);
  *fg_pgrp = t->fg_pgrp;
  return true;
}

uint64_t TtyRdev(const std::shared_ptr<PtyPair>& tty, bool master) {
  if (tty->is_console) return MakeDev(4, 64);  // ttyS0
  if (master) return MakeDev(5, 2);            // ptmx
  return MakeDev(136, tty->index);
}

int64_t TtyIoctl(LinuxProcess* proc, FileDescription* desc,
                 const std::shared_ptr<PtyPair>& tty, bool master,
                 uint32_t req, uintptr_t arg) {
  auto need_arg = [&]() { return arg == 0; };
  switch (req) {
    case 0x5401: {  // TCGETS
      if (need_arg()) return kEFAULT;
      std::lock_guard<std::mutex> lock(tty->mtx);
      *reinterpret_cast<LinuxTermios*>(arg) = tty->termios;
      return 0;
    }
    case 0x5402:    // TCSETS
    case 0x5403:    // TCSETSW (output drains immediately: no driver buffer)
    case 0x5404: {  // TCSETSF
      if (need_arg()) return kEFAULT;
      LinuxTermios nt = *reinterpret_cast<const LinuxTermios*>(arg);
      {
        std::lock_guard<std::mutex> lock(tty->mtx);
        LinuxTermios& ot = tty->termios;
        if (req == 0x5404) FlushInputLocked(tty);
        bool was_canon = (ot.c_lflag & kICANON) != 0;
        bool now_canon = (nt.c_lflag & kICANON) != 0;
        if (was_canon && !now_canon) {
          // The partially edited line becomes readable in raw mode.
          for (char ch : tty->canon_line) {
            tty->master_to_slave.push_back(static_cast<uint8_t>(ch));
          }
          tty->m2s_total += tty->canon_line.size();
          tty->canon_line.clear();
          tty->canon_lines.clear();
        } else if (!was_canon && now_canon) {
          tty->canon_lines.clear();
        }
        bool old_ixon = (ot.c_iflag & kIXON) && ot.c_cc[kVSTART] == 021 &&
                        ot.c_cc[kVSTOP] == 023;
        bool new_ixon = (nt.c_iflag & kIXON) && nt.c_cc[kVSTART] == 021 &&
                        nt.c_cc[kVSTOP] == 023;
        if (old_ixon != new_ixon) {
          tty->pkt_status &= ~(kPktNoStop | kPktDoStop);
          tty->pkt_status |= new_ixon ? kPktDoStop : kPktNoStop;
        }
        if ((nt.c_iflag & kIXON) == 0 && tty->stopped) {
          tty->stopped = false;
          tty->pkt_status = (tty->pkt_status & ~kPktStop) | kPktStart;
        }
        ot = nt;
      }
      NotifyPollWaiters();
      return 0;
    }
    case 0x5409:  // TCSBRK: tcdrain/tcsendbreak; no driver output buffer and
    case 0x5425:  // TCSBRKP  no break control on these ttys -> nothing to do
    case 0x5427:  // TIOCSBRK
    case 0x5428:  // TIOCCBRK
      return 0;
    case 0x540A: {  // TCXONC
      std::shared_ptr<PtyPair> t = tty;
      uint8_t ch = 0;
      {
        std::lock_guard<std::mutex> lock(tty->mtx);
        switch (arg) {
          case 0:  // TCOOFF
            tty->stopped = true;
            tty->pkt_status = (tty->pkt_status & ~kPktStart) | kPktStop;
            break;
          case 1:  // TCOON
            tty->stopped = false;
            tty->pkt_status = (tty->pkt_status & ~kPktStop) | kPktStart;
            break;
          case 2:  // TCIOFF: transmit STOP
            ch = tty->termios.c_cc[kVSTOP];
            break;
          case 3:  // TCION: transmit START
            ch = tty->termios.c_cc[kVSTART];
            break;
          default:
            return kEINVAL;
        }
      }
      if (ch != 0) {
        if (master) {
          TtyInput(tty, &ch, 1);
        } else if (tty->is_console) {
          SerialWriteBytes(&ch, 1);
        } else {
          std::lock_guard<std::mutex> lock(tty->mtx);
          tty->slave_to_master.push_back(ch);
          tty->s2m_total++;
        }
      }
      NotifyPollWaiters();
      return 0;
    }
    case 0x540B: {  // TCFLSH
      if (arg > 2) return kEINVAL;
      std::lock_guard<std::mutex> lock(tty->mtx);
      bool in = (arg == 0 || arg == 2), out = (arg == 1 || arg == 2);
      if (master) {
        if (in) tty->slave_to_master.clear();  // master's input queue
      } else {
        if (in) FlushInputLocked(tty);
        if (out && !tty->is_console) tty->pkt_status |= kPktFlushWrite;
      }
      NotifyPollWaiters();
      return 0;
    }
    case 0x540C:  // TIOCEXCL
    case 0x540D: {  // TIOCNXCL
      std::lock_guard<std::mutex> lock(tty->mtx);
      tty->exclusive = (req == 0x540C);
      return 0;
    }
    case 0x80045440U: {  // TIOCGEXCL
      if (need_arg()) return kEFAULT;
      std::lock_guard<std::mutex> lock(tty->mtx);
      *reinterpret_cast<int*>(arg) = tty->exclusive ? 1 : 0;
      return 0;
    }
    case 0x540E: {  // TIOCSCTTY
      if (tty->is_console) {
        // /dev/console itself can't be a ctty, but /dev/ttyS0 can.
        if (desc && desc->path == "/dev/console") return kEPERM;
      }
      auto cur = TtyGetCtty(proc);
      if (proc->pid == proc->sid && cur == tty) return 0;
      if (proc->pid != proc->sid || cur) return kEPERM;
      int old_session = 0;
      {
        std::lock_guard<std::mutex> lock(tty->mtx);
        old_session = tty->session;
      }
      if (old_session != 0) {
        if (arg == 1 && proc->euid == 0) {
          ClearSessionCtty(old_session);  // steal (CAP_SYS_ADMIN)
        } else {
          return kEPERM;
        }
      }
      if (desc && (desc->flags & kLinuxO_ACCMODE) == kLinuxO_WRONLY &&
          proc->euid != 0) {
        return kEPERM;
      }
      {
        std::lock_guard<std::mutex> lock(tty->mtx);
        tty->session = proc->sid;
        tty->fg_pgrp = proc->pgid;
      }
      std::lock_guard<std::mutex> clock(g_ctty_mtx);
      g_ctty_detached->erase(proc->pid);
      g_session_ctty[proc->sid] = tty;
      return 0;
    }
    case 0x5422: {  // TIOCNOTTY
      if (!IsCttyOf(proc, tty)) return kENOTTY;
      if (proc->pid == proc->sid) {
        int fg = 0;
        {
          std::lock_guard<std::mutex> lock(tty->mtx);
          fg = tty->fg_pgrp;
          tty->session = 0;
          tty->fg_pgrp = 0;
        }
        ClearSessionCtty(proc->sid);
        if (fg > 0) {
          QueueSignalToProcessGroup(fg, kSIGHUP);
          QueueSignalToProcessGroup(fg, kSIGCONT);
        }
      } else {
        // Per-process disassociation (not inherited across fork here).
        std::lock_guard<std::mutex> clock(g_ctty_mtx);
        g_ctty_detached->insert(proc->pid);
      }
      return 0;
    }
    case 0x540F: {  // TIOCGPGRP
      if (need_arg()) return kEFAULT;
      if (!master && !IsCttyOf(proc, tty)) return kENOTTY;
      std::lock_guard<std::mutex> lock(tty->mtx);
      *reinterpret_cast<int*>(arg) = tty->fg_pgrp;
      return 0;
    }
    case 0x5429: {  // TIOCGSID
      if (need_arg()) return kEFAULT;
      if (!master && !IsCttyOf(proc, tty)) return kENOTTY;
      std::lock_guard<std::mutex> lock(tty->mtx);
      if (tty->session == 0) return kENOTTY;
      *reinterpret_cast<int*>(arg) = tty->session;
      return 0;
    }
    case 0x5410: {  // TIOCSPGRP
      if (need_arg()) return kEFAULT;
      if (!IsCttyOf(proc, tty)) return kENOTTY;
      int pgrp = *reinterpret_cast<const int*>(arg);
      if (pgrp < 0) return kEINVAL;
      bool exists = false, same_session = false;
      {
        std::lock_guard<std::recursive_mutex> plock(g_proc_mtx);
        for (const auto& kv : g_processes) {
          if (kv.second->pgid == pgrp && !kv.second->reaped) {
            exists = true;
            if (kv.second->sid == proc->sid) same_session = true;
          }
        }
      }
      if (!exists) return kESRCH;
      if (!same_session) return kEPERM;
      std::lock_guard<std::mutex> lock(tty->mtx);
      tty->fg_pgrp = pgrp;
      return 0;
    }
    case 0x5411: {  // TIOCOUTQ
      if (need_arg()) return kEFAULT;
      std::lock_guard<std::mutex> lock(tty->mtx);
      size_t n = master ? tty->master_to_slave.size() : tty->slave_to_master.size();
      *reinterpret_cast<int*>(arg) = tty->is_console ? 0 : static_cast<int>(n);
      return 0;
    }
    case 0x5412: {  // TIOCSTI
      if (need_arg()) return kEFAULT;
      // Linux: only on the caller's controlling tty unless CAP_SYS_ADMIN.
      if (!IsCttyOf(proc, tty) && proc->euid != 0) return kEPERM;
      if (desc && (desc->flags & kLinuxO_ACCMODE) == kLinuxO_WRONLY &&
          proc->euid != 0) {
        return kEPERM;
      }
      uint8_t ch = *reinterpret_cast<const uint8_t*>(arg);
      if (master) {
        // The master's own input queue (what reading the master returns);
        // the master side has raw termios on Linux.
        std::lock_guard<std::mutex> lock(tty->mtx);
        tty->slave_to_master.push_back(ch);
        tty->s2m_total++;
      } else {
        TtyInput(tty, &ch, 1);  // as if typed on this terminal
      }
      NotifyPollWaiters();
      return 0;
    }
    case 0x5413: {  // TIOCGWINSZ
      if (need_arg()) return kEFAULT;
      std::lock_guard<std::mutex> lock(tty->mtx);
      *reinterpret_cast<LinuxWinsize*>(arg) = tty->winsize;
      return 0;
    }
    case 0x5414: {  // TIOCSWINSZ
      if (need_arg()) return kEFAULT;
      LinuxWinsize ws = *reinterpret_cast<const LinuxWinsize*>(arg);
      int fg = 0;
      {
        std::lock_guard<std::mutex> lock(tty->mtx);
        if (memcmp(&ws, &tty->winsize, sizeof(ws)) == 0) return 0;
        tty->winsize = ws;
        fg = tty->fg_pgrp;
      }
      if (fg > 0) QueueSignalToProcessGroup(fg, kSIGWINCH);
      return 0;
    }
    case 0x80045432U: {  // TIOCGDEV
      if (need_arg()) return kEFAULT;
      uint64_t d = TtyRdev(tty, false);
      *reinterpret_cast<uint32_t*>(arg) = static_cast<uint32_t>(d);
      return 0;
    }
    default:
      break;
  }

  // PTY master-only ioctls.
  if (master) {
    switch (req) {
      case 0x5420: {  // TIOCPKT
        if (need_arg()) return kEFAULT;
        bool on = *reinterpret_cast<const int*>(arg) != 0;
        std::lock_guard<std::mutex> lock(tty->mtx);
        if (on && !tty->packet_mode) tty->pkt_status = 0;
        tty->packet_mode = on;
        return 0;
      }
      case 0x80045438U: {  // TIOCGPKT
        if (need_arg()) return kEFAULT;
        std::lock_guard<std::mutex> lock(tty->mtx);
        *reinterpret_cast<int*>(arg) = tty->packet_mode ? 1 : 0;
        return 0;
      }
      case 0x80045430U: {  // TIOCGPTN
        if (need_arg()) return kEFAULT;
        *reinterpret_cast<uint32_t*>(arg) = static_cast<uint32_t>(tty->index);
        return 0;
      }
      case 0x40045431U: {  // TIOCSPTLCK
        if (need_arg()) return kEFAULT;
        std::lock_guard<std::mutex> lock(tty->mtx);
        tty->locked = *reinterpret_cast<const int*>(arg) != 0 ? 1 : 0;
        return 0;
      }
      case 0x80045439U: {  // TIOCGPTLCK
        if (need_arg()) return kEFAULT;
        std::lock_guard<std::mutex> lock(tty->mtx);
        *reinterpret_cast<int*>(arg) = tty->locked;
        return 0;
      }
      default:
        break;
    }
  }
  return kENOTTY;
}
