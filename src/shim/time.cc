// Clocks, sleeps and timers for the shim.
//
// Wall clock: read once from the CMOS RTC at boot and used to start a Zircon
// clock object, which is installed as libc's UTC reference and backs
// CLOCK_REALTIME everywhere (clock_gettime, gettimeofday, time, futex
// FUTEX_CLOCK_REALTIME, absolute sleeps and timers). clock_settime and
// settimeofday update the same clock.
#include <zircon/syscalls/clock.h>
#include <zircon/utc.h>

#include "src/shim/fd_ops.h"
#include "src/shim/shim_common.h"
#include "src/shim/signals.h"

namespace {

constexpr int kCLOCK_REALTIME = 0;
constexpr int kCLOCK_MONOTONIC = 1;
constexpr int kCLOCK_PROCESS_CPUTIME_ID = 2;
constexpr int kCLOCK_THREAD_CPUTIME_ID = 3;
constexpr int kCLOCK_MONOTONIC_RAW = 4;
constexpr int kCLOCK_REALTIME_COARSE = 5;
constexpr int kCLOCK_MONOTONIC_COARSE = 6;
constexpr int kCLOCK_BOOTTIME = 7;
constexpr int kCLOCK_REALTIME_ALARM = 8;
constexpr int kCLOCK_BOOTTIME_ALARM = 9;
constexpr int kCLOCK_TAI = 11;
constexpr int kTIMER_ABSTIME = 1;
constexpr int64_t kNsPerSec = 1000000000LL;
constexpr int64_t kNsPerTick = 10000000LL;  // USER_HZ = 100

constexpr int kSIGEV_SIGNAL = 0;
constexpr int kSIGEV_NONE = 1;
constexpr int kSIGEV_THREAD = 2;
constexpr int kSIGEV_THREAD_ID = 4;

zx_handle_t g_utc_clock = ZX_HANDLE_INVALID;
std::atomic<int32_t> g_tz_minuteswest{0};
std::atomic<int32_t> g_tz_dsttime{0};

#if defined(__x86_64__)
inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}
inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
  return ret;
}
#else
inline void outb(uint16_t, uint8_t) {}
inline uint8_t inb(uint16_t) { return 0; }
#endif

uint8_t CmosRead(uint8_t reg) {
  outb(0x70, reg);
  return inb(0x71);
}

struct RtcRegs {
  uint8_t sec, min, hour, day, mon, year, century, status_b;
  bool operator==(const RtcRegs& o) const {
    return sec == o.sec && min == o.min && hour == o.hour && day == o.day && mon == o.mon &&
           year == o.year && century == o.century;
  }
};

bool WaitRtcNotUpdating() {
  // Status register A bit 7 (UIP) is set for at most ~2 ms per second while
  // the RTC updates its registers.
  for (int i = 0; i < 1000000; ++i) {
    if ((CmosRead(0x0A) & 0x80) == 0) return true;
  }
  return false;
}

RtcRegs ReadRtcRegs() {
  RtcRegs r;
  r.sec = CmosRead(0x00);
  r.min = CmosRead(0x02);
  r.hour = CmosRead(0x04);
  r.day = CmosRead(0x07);
  r.mon = CmosRead(0x08);
  r.year = CmosRead(0x09);
  r.century = CmosRead(0x32);  // century register (ACPI FADT default index)
  r.status_b = CmosRead(0x0B);
  return r;
}

int FromBcd(uint8_t v) { return (v & 0x0f) + (v >> 4) * 10; }

int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// Reads the MC146818-compatible CMOS RTC (assumed to hold UTC, as Linux
// assumes by default). Returns false if the ports are unavailable or the
// registers do not hold a valid date.
bool ReadCmosRtc(int64_t* out_epoch_sec) {
#if defined(__x86_64__)
  if (g_ioport_rsrc == ZX_HANDLE_INVALID) return false;
  if (zx_ioports_request(g_ioport_rsrc, 0x70, 2) != ZX_OK) return false;
  if (!WaitRtcNotUpdating()) return false;
  RtcRegs a = ReadRtcRegs();
  for (int i = 0; i < 10; ++i) {
    if (!WaitRtcNotUpdating()) return false;
    RtcRegs b = ReadRtcRegs();
    if (a == b) break;
    a = b;
  }
  const bool binary = (a.status_b & 0x04) != 0;
  const bool h24 = (a.status_b & 0x02) != 0;
  const bool pm = (a.hour & 0x80) != 0;
  int sec = binary ? a.sec : FromBcd(a.sec);
  int min = binary ? a.min : FromBcd(a.min);
  int hour = binary ? (a.hour & 0x7f) : FromBcd(a.hour & 0x7f);
  int day = binary ? a.day : FromBcd(a.day);
  int mon = binary ? a.mon : FromBcd(a.mon);
  int year = binary ? a.year : FromBcd(a.year);
  int century = binary ? a.century : FromBcd(a.century);
  if (!h24) hour = (hour % 12) + (pm ? 12 : 0);
  if (century >= 19 && century <= 30) {
    year += century * 100;
  } else {
    // No usable century register: same window as Linux mc146818_get_time().
    year += (year < 70) ? 2000 : 1900;
  }
  if (sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 || mon < 1 || mon > 12) {
    return false;
  }
  *out_epoch_sec = DaysFromCivil(year, static_cast<unsigned>(mon), static_cast<unsigned>(day)) *
                       86400 +
                   hour * 3600 + min * 60 + sec;
  return true;
#else
  return false;
#endif
}

bool ReadPl031Rtc(int64_t* out_epoch_sec) {
#if defined(__aarch64__)
  if (g_mmio_rsrc == ZX_HANDLE_INVALID) return false;
  zx_handle_t rtc_rsrc = ZX_HANDLE_INVALID;
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, 0x09010000, 4096, "pl031", 5, &rtc_rsrc) != ZX_OK) {
    return false;
  }
  zx_handle_t rtc_vmo = ZX_HANDLE_INVALID;
  zx_status_t st = zx_vmo_create_physical(rtc_rsrc, 0x09010000, 4096, &rtc_vmo);
  zx_handle_close(rtc_rsrc);
  if (st != ZX_OK) return false;
  uintptr_t vaddr = 0;
  st = zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ, 0, rtc_vmo, 0, 4096, &vaddr);
  if (st != ZX_OK) {
    zx_handle_close(rtc_vmo);
    return false;
  }
  uint32_t sec = *reinterpret_cast<volatile uint32_t*>(vaddr);
  zx_vmar_unmap(zx_vmar_root_self(), vaddr, 4096);
  zx_handle_close(rtc_vmo);
  if (sec == 0) return false;
  *out_epoch_sec = sec;
  return true;
#else
  return false;
#endif
}

bool ReadHardwareRtc(int64_t* out_epoch_sec) {
#if defined(__x86_64__)
  return ReadCmosRtc(out_epoch_sec);
#elif defined(__aarch64__)
  return ReadPl031Rtc(out_epoch_sec);
#else
  return false;
#endif
}

bool SetRealtimeNs(int64_t ns) {
  if (g_utc_clock == ZX_HANDLE_INVALID || ns < 0) return false;
  zx_clock_update_args_v2_t args{};
  args.reference_value = zx_clock_get_monotonic();
  args.synthetic_value = ns;
  if (zx_clock_update(g_utc_clock,
                      ZX_CLOCK_ARGS_VERSION(2) | ZX_CLOCK_UPDATE_OPTION_BOTH_VALUES_VALID,
                      &args) != ZX_OK) {
    return false;
  }
  TimerfdClockWasSet();  // TFD_TIMER_CANCEL_ON_SET timers see -ECANCELED
  return true;
}

bool IsValidTs(const int64_t ts[2]) { return ts[0] >= 0 && ts[1] >= 0 && ts[1] < kNsPerSec; }

int64_t TsToNs(const int64_t ts[2]) {
  if (ts[0] >= INT64_MAX / kNsPerSec - 1) return INT64_MAX;
  return ts[0] * kNsPerSec + ts[1];
}

void NsToTs(int64_t ns, int64_t ts[2]) {
  if (ns < 0) ns = 0;
  ts[0] = ns / kNsPerSec;
  ts[1] = ns % kNsPerSec;
}

int64_t TaskRuntimeNs(zx_handle_t h) {
  if (h == ZX_HANDLE_INVALID) return -1;
  zx_info_task_runtime_t rt{};
  if (zx_object_get_info(h, ZX_INFO_TASK_RUNTIME, &rt, sizeof(rt), nullptr, nullptr) != ZX_OK) {
    return -1;
  }
  return rt.cpu_time;
}

// Reads Linux clock `clk` for the calling thread. Returns 0 or -EINVAL.
int64_t ReadClock(LinuxProcess* p, LinuxThread* t, int clk, int64_t* out) {
  switch (clk) {
    case kCLOCK_REALTIME:
    case kCLOCK_REALTIME_COARSE:
    case kCLOCK_REALTIME_ALARM:
    case kCLOCK_TAI:  // TAI offset is 0 until set via adjtimex (Linux default)
      *out = ShimRealtimeNs();
      return 0;
    case kCLOCK_MONOTONIC:
    case kCLOCK_MONOTONIC_RAW:
    case kCLOCK_MONOTONIC_COARSE:
      *out = zx_clock_get_monotonic();
      return 0;
    case kCLOCK_BOOTTIME:
    case kCLOCK_BOOTTIME_ALARM:
      *out = zx_clock_get_boot();
      return 0;
    case kCLOCK_PROCESS_CPUTIME_ID:
      *out = ProcessCpuTimeNs(p);
      return *out < 0 ? kEINVAL : 0;
    case kCLOCK_THREAD_CPUTIME_ID:
      *out = ThreadCpuTimeNs(t);
      return *out < 0 ? kEINVAL : 0;
    default:
      break;
  }
  if (clk < 0) {
    // Dynamic CPU clocks: clockid = (~pid << 3) | (perthread << 2) | which.
    // Zircon reports a single CPU-time figure, so CPUCLOCK_PROF, _VIRT and
    // _SCHED all return it.
    if ((clk & 3) == 3) return kEINVAL;
    const int id = ~(clk >> 3);
    const bool perthread = (clk & 4) != 0;
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    if (perthread) {
      const int tid = id == 0 ? t->tid : id;
      for (auto& kv : g_processes) {
        for (const auto& th : kv.second->threads) {
          if (th->tid == tid) {
            *out = ThreadCpuTimeNs(th.get());
            return *out < 0 ? kEINVAL : 0;
          }
        }
      }
      return kEINVAL;
    }
    const int pid = id == 0 ? p->pid : id;
    auto it = g_processes.find(pid);
    if (it == g_processes.end()) return kEINVAL;
    *out = ProcessCpuTimeNs(it->second.get());
    return *out < 0 ? kEINVAL : 0;
  }
  return kEINVAL;
}

// Sleeps until `deadline`; returns false if interrupted by a signal.
bool SleepUntil(LinuxProcess* p, LinuxThread* t, zx_time_t deadline) {
  for (;;) {
    ThreadClearWake(t);
    if (ShimSignalPending(p, t)) return false;
    if (zx_clock_get_monotonic() >= deadline) return true;
    ThreadWaitWake(t, deadline);
  }
}

// Relative sleep with `rem` reporting and restart_syscall(2) support.
int64_t RelativeSleep(LinuxProcess* p, LinuxThread* t, zx_time_t deadline, uintptr_t rem) {
  if (SleepUntil(p, t, deadline)) return 0;
  if (rem != 0) {
    int64_t ts[2];
    NsToTs(deadline - zx_clock_get_monotonic(), ts);
    if (!CopyToUser(p, rem, ts, sizeof(ts))) return kEFAULT;
  }
  t->restart_kind = kRestartNanosleep;
  t->restart_args[0] = static_cast<uint64_t>(deadline);
  t->restart_args[1] = rem;
  return kERESTART_RESTARTBLOCK;
}

// ---------------------------------------------------------------------------
// Timers (ITIMER_REAL / alarm and POSIX timer_create) serviced by one thread.

struct ShimTimer {
  int pid = 0;
  int id = -1;      // POSIX timer id; -1 for ITIMER_REAL
  int clock = kCLOCK_MONOTONIC;
  int notify = kSIGEV_SIGNAL;
  int sig = kSIGALRM;
  int tid = 0;      // SIGEV_THREAD_ID target
  uint64_t sigval = 0;
  zx_time_t deadline = 0;  // ZX_CLOCK_MONOTONIC; 0 = disarmed
  int64_t interval = 0;
  int overrun = 0;
  int last_overrun = 0;
  bool sig_pending = false;
};

std::mutex g_timer_mtx;
std::map<std::pair<int, int>, ShimTimer> g_timers;  // (pid, id)
zx_handle_t g_timer_event = ZX_HANDLE_INVALID;

void KickTimerService() {
  if (g_timer_event != ZX_HANDLE_INVALID) zx_object_signal(g_timer_event, 0, ZX_USER_SIGNAL_0);
}

struct TimerFire {
  int pid, tid, id, sig;
  uint64_t sigval;
};

void TimerServiceLoop() {
  for (;;) {
    zx_time_t next = ZX_TIME_INFINITE;
    {
      std::lock_guard<std::mutex> l(g_timer_mtx);
      for (const auto& kv : g_timers) {
        if (kv.second.deadline != 0 && kv.second.deadline < next) next = kv.second.deadline;
      }
    }
    zx_object_wait_one(g_timer_event, ZX_USER_SIGNAL_0, next, nullptr);
    zx_object_signal(g_timer_event, ZX_USER_SIGNAL_0, 0);

    std::vector<TimerFire> fires;
    {
      std::lock_guard<std::mutex> l(g_timer_mtx);
      const zx_time_t now = zx_clock_get_monotonic();
      for (auto& kv : g_timers) {
        ShimTimer& tm = kv.second;
        if (tm.deadline == 0 || tm.deadline > now) continue;
        int64_t missed = 0;
        if (tm.interval > 0) {
          missed = (now - tm.deadline) / tm.interval;
          tm.deadline += (missed + 1) * tm.interval;
        } else {
          tm.deadline = 0;
        }
        if (tm.notify == kSIGEV_NONE) continue;
        if (tm.id >= 0) {
          if (tm.sig_pending) {  // previous expiry not yet delivered
            tm.overrun = static_cast<int>(std::min<int64_t>(INT32_MAX, tm.overrun + 1 + missed));
            continue;
          }
          tm.sig_pending = true;
          tm.overrun = static_cast<int>(std::min<int64_t>(INT32_MAX, missed));
        }
        fires.push_back(TimerFire{tm.pid, tm.notify == kSIGEV_THREAD_ID ? tm.tid : 0, tm.id,
                                  tm.sig, tm.sigval});
      }
    }
    for (const TimerFire& f : fires) {
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      auto it = g_processes.find(f.pid);
      if (it == g_processes.end()) continue;
      LinuxProcess* p = it->second.get();
      if (f.id < 0) {
        SendSignalToProcess(p, MakeSiginfo(kSIGALRM, kSI_KERNEL));  // it_real_fn()
        continue;
      }
      const LinuxSiginfo info = MakeSiginfoTimer(f.sig, f.id, 0, f.sigval);
      LinuxThread* target = nullptr;
      for (const auto& th : p->threads) {
        if (f.tid != 0 && th->tid == f.tid) target = th.get();
      }
      if (f.tid != 0 && target != nullptr) {
        SendSignalToThread(p, target, info);
      } else {
        SendSignalToProcess(p, info);
      }
    }
  }
}

// Remaining time (ns) and interval of a timer; requires g_timer_mtx.
void TimerRemaining(const ShimTimer& tm, int64_t* value, int64_t* interval) {
  *interval = tm.interval;
  *value = 0;
  if (tm.deadline != 0) {
    const zx_time_t now = zx_clock_get_monotonic();
    *value = tm.deadline > now ? tm.deadline - now : 1;  // expired, not yet serviced
  }
}

void ArmTimerLocked(ShimTimer* tm, zx_time_t deadline, int64_t interval) {
  tm->deadline = deadline;
  tm->interval = interval;
  tm->overrun = 0;
  KickTimerService();
}

int64_t SysGetitimer(LinuxProcess* p, int which, uintptr_t out) {
  if (which < 0 || which > 2) return kEINVAL;
  int64_t value = 0, interval = 0;
  if (which == 0) {
    std::lock_guard<std::mutex> l(g_timer_mtx);
    auto it = g_timers.find({p->pid, -1});
    if (it != g_timers.end()) TimerRemaining(it->second, &value, &interval);
  }
  // ITIMER_VIRTUAL/ITIMER_PROF can never be armed (setitimer rejects them).
  const int64_t tv[4] = {interval / kNsPerSec, (interval % kNsPerSec) / 1000, value / kNsPerSec,
                         (value % kNsPerSec + 999) / 1000};
  if (out != 0 && !CopyToUser(p, out, tv, sizeof(tv))) return kEFAULT;
  return 0;
}

int64_t SetItimerReal(LinuxProcess* p, int64_t value_ns, int64_t interval_ns, int64_t* old_value,
                      int64_t* old_interval) {
  std::lock_guard<std::mutex> l(g_timer_mtx);
  ShimTimer& tm = g_timers[{p->pid, -1}];
  tm.pid = p->pid;
  tm.id = -1;
  TimerRemaining(tm, old_value, old_interval);
  if (value_ns == 0) {
    ArmTimerLocked(&tm, 0, 0);
  } else {
    ArmTimerLocked(&tm, zx_deadline_after(value_ns), interval_ns);
  }
  return 0;
}

int64_t SysSetitimer(LinuxProcess* p, int which, uintptr_t in, uintptr_t out) {
  if (which < 0 || which > 2) return kEINVAL;
  int64_t tv[4] = {0, 0, 0, 0};  // it_interval {sec, usec}, it_value {sec, usec}
  if (in != 0 && !CopyFromUser(p, tv, in, sizeof(tv))) return kEFAULT;
  if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000 || tv[2] < 0 || tv[3] < 0 || tv[3] >= 1000000) {
    return kEINVAL;
  }
  if (which != 0) {
    // ITIMER_VIRTUAL / ITIMER_PROF count user (+system) CPU time; Zircon gives
    // no way to be notified when a task has consumed a CPU budget.
    return kEINVAL;
  }
  const int64_t interval = tv[0] * kNsPerSec + tv[1] * 1000;
  const int64_t value = tv[2] * kNsPerSec + tv[3] * 1000;
  int64_t ov = 0, oi = 0;
  SetItimerReal(p, value, interval, &ov, &oi);
  if (out != 0) {
    const int64_t o[4] = {oi / kNsPerSec, (oi % kNsPerSec) / 1000, ov / kNsPerSec,
                          (ov % kNsPerSec + 999) / 1000};
    if (!CopyToUser(p, out, o, sizeof(o))) return kEFAULT;
  }
  return 0;
}

bool TimerClockSupported(int clk) {
  return clk == kCLOCK_REALTIME || clk == kCLOCK_MONOTONIC || clk == kCLOCK_BOOTTIME ||
         clk == kCLOCK_REALTIME_ALARM || clk == kCLOCK_BOOTTIME_ALARM || clk == kCLOCK_TAI;
}

int64_t SysTimerCreate(LinuxProcess* p, int clk, uintptr_t sevp, uintptr_t idp) {
  if (!TimerClockSupported(clk)) {
    // CPU-time clocks would need CPU-budget notifications Zircon lacks.
    return kEINVAL;
  }
  ShimTimer tm;
  tm.pid = p->pid;
  tm.clock = clk;
  bool sigval_is_id = true;
  if (sevp != 0) {
    uint8_t sev[64];
    if (!CopyFromUser(p, sev, sevp, sizeof(sev))) return kEFAULT;
    memcpy(&tm.sigval, sev + 0, 8);
    memcpy(&tm.sig, sev + 8, 4);
    memcpy(&tm.notify, sev + 12, 4);
    memcpy(&tm.tid, sev + 16, 4);
    sigval_is_id = false;
    if (tm.notify == kSIGEV_THREAD) return kEINVAL;  // implemented by libc, not the kernel
    if (tm.notify != kSIGEV_NONE && tm.notify != kSIGEV_SIGNAL &&
        tm.notify != (kSIGEV_SIGNAL | kSIGEV_THREAD_ID)) {
      return kEINVAL;
    }
    if (tm.notify != kSIGEV_NONE && (tm.sig < 1 || tm.sig > kNSIG)) return kEINVAL;
    if (tm.notify == kSIGEV_THREAD_ID) {
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      bool found = false;
      for (const auto& th : p->threads) found = found || th->tid == tm.tid;
      if (!found) return kEINVAL;
    }
  }
  std::lock_guard<std::mutex> l(g_timer_mtx);
  int id = 0;
  while (g_timers.count({p->pid, id}) != 0) ++id;
  tm.id = id;
  if (sigval_is_id) tm.sigval = static_cast<uint64_t>(id);
  const int32_t id32 = id;
  if (!CopyToUser(p, idp, &id32, sizeof(id32))) return kEFAULT;
  g_timers[{p->pid, id}] = tm;
  return 0;
}

int64_t SysTimerSettime(LinuxProcess* p, int id, int flags, uintptr_t newp, uintptr_t oldp) {
  int64_t its[4];  // it_interval {sec, nsec}, it_value {sec, nsec}
  if (newp == 0 || !CopyFromUser(p, its, newp, sizeof(its))) return kEFAULT;
  if (!IsValidTs(its) || !IsValidTs(its + 2)) return kEINVAL;
  int64_t ov = 0, oi = 0;
  {
    std::lock_guard<std::mutex> l(g_timer_mtx);
    auto it = g_timers.find({p->pid, id});
    if (it == g_timers.end() || id < 0) return kEINVAL;
    ShimTimer& tm = it->second;
    TimerRemaining(tm, &ov, &oi);
    const int64_t value = TsToNs(its + 2);
    const int64_t interval = TsToNs(its);
    zx_time_t deadline = 0;
    if (value != 0) {
      if ((flags & kTIMER_ABSTIME) != 0) {
        if (!ClockDeadlineToMonotonic(tm.clock, value, &deadline)) return kEINVAL;
        if (deadline <= 0) deadline = 1;  // already expired: fire now
      } else {
        deadline = zx_deadline_after(value);
      }
    }
    tm.sig_pending = false;
    ArmTimerLocked(&tm, deadline, interval);
  }
  if (oldp != 0) {
    int64_t o[4];
    NsToTs(oi, o);
    NsToTs(ov, o + 2);
    if (!CopyToUser(p, oldp, o, sizeof(o))) return kEFAULT;
  }
  return 0;
}

}  // namespace

// --- public ---

int64_t ShimRealtimeNs() {
  zx_time_t now = 0;
  if (g_utc_clock != ZX_HANDLE_INVALID && zx_clock_read(g_utc_clock, &now) == ZX_OK) return now;
  return 0;
}

void TimeInitFromRtc() {
  zx_clock_create_args_v1_t cargs{};
  cargs.backstop_time = 0;  // the clock may be set to any time >= the epoch
  zx_handle_t clk = ZX_HANDLE_INVALID;
  zx_status_t st = zx_clock_create(ZX_CLOCK_ARGS_VERSION(1), &cargs, &clk);
  if (st != ZX_OK) {
    ShimLogf("[time] zx_clock_create failed: %d; CLOCK_REALTIME unavailable\n", st);
    return;
  }
  g_utc_clock = clk;
  int64_t rtc_sec = 0;
  if (ReadHardwareRtc(&rtc_sec)) {
    SetRealtimeNs(rtc_sec * kNsPerSec);
    ShimLogf("[time] Hardware RTC: %lld seconds since the epoch (UTC)\n",
             static_cast<long long>(rtc_sec));
  } else {
    // Without an RTC the wall clock is unknown; like Linux without an RTC,
    // start counting from the epoch.
    SetRealtimeNs(0);
    ShimLogf("[time] Hardware RTC unavailable; CLOCK_REALTIME starts at the epoch\n");
  }
  // Install the same clock as libc's UTC reference (std::chrono::system_clock,
  // condition_variable timed waits, and other shim code use it).
  zx_handle_t libc_ref = ZX_HANDLE_INVALID;
  if (zx_handle_duplicate(g_utc_clock, ZX_RIGHT_SAME_RIGHTS, &libc_ref) == ZX_OK) {
    zx_handle_t prev = ZX_HANDLE_INVALID;
    zx_utc_reference_swap(libc_ref, &prev);
    if (prev != ZX_HANDLE_INVALID) zx_handle_close(prev);
  }

  // Timer service thread (lives in the shim's own Zircon process).
  zx_event_create(0, &g_timer_event);
  std::thread(TimerServiceLoop).detach();
}

bool ClockDeadlineToMonotonic(int clock_id, int64_t abs_ns, zx_time_t* out) {
  switch (clock_id) {
    case kCLOCK_REALTIME:
    case kCLOCK_REALTIME_ALARM:
    case kCLOCK_TAI: {
      const int64_t mono = zx_clock_get_monotonic();
      const int64_t delta = abs_ns - ShimRealtimeNs();
      *out = (delta > INT64_MAX - mono) ? ZX_TIME_INFINITE : mono + delta;
      return true;
    }
    case kCLOCK_MONOTONIC:
      *out = abs_ns;
      return true;
    case kCLOCK_BOOTTIME:
    case kCLOCK_BOOTTIME_ALARM: {
      const int64_t mono = zx_clock_get_monotonic();
      const int64_t delta = abs_ns - zx_clock_get_boot();
      *out = (delta > INT64_MAX - mono) ? ZX_TIME_INFINITE : mono + delta;
      return true;
    }
    default:
      return false;
  }
}

int64_t ThreadCpuTimeNs(LinuxThread* t) {
  std::lock_guard<std::mutex> wl(t->wake_mtx);
  return TaskRuntimeNs(t->zx_thread);
}

int64_t ProcessCpuTimeNs(LinuxProcess* p) {
  if (p->zombie.load()) return p->cpu_time_ns;
  return TaskRuntimeNs(p->zx_proc);
}

void TimeProcessExit(LinuxProcess* p) {
  std::lock_guard<std::mutex> l(g_timer_mtx);
  for (auto it = g_timers.begin(); it != g_timers.end();) {
    it = (it->first.first == p->pid) ? g_timers.erase(it) : std::next(it);
  }
}

void TimeProcessExec(LinuxProcess* p) {
  // POSIX timers are deleted by execve; ITIMER_REAL is preserved.
  std::lock_guard<std::mutex> l(g_timer_mtx);
  for (auto it = g_timers.begin(); it != g_timers.end();) {
    it = (it->first.first == p->pid && it->first.second >= 0) ? g_timers.erase(it)
                                                               : std::next(it);
  }
}

int TimeSignalDelivered(int pid, int timer_id) {
  std::lock_guard<std::mutex> l(g_timer_mtx);
  auto it = g_timers.find({pid, timer_id});
  if (it == g_timers.end()) return 0;
  it->second.sig_pending = false;
  it->second.last_overrun = it->second.overrun;
  it->second.overrun = 0;
  return it->second.last_overrun;
}

int64_t HandleTimeSyscall(LinuxProcess* p, LinuxThread* t, uint64_t sc, uint64_t a0, uint64_t a1,
                          uint64_t a2, uint64_t a3, bool* handled) {
  *handled = true;
  switch (sc) {
    case 228: {  // clock_gettime(clk, tp)
      int64_t ns = 0;
      const int64_t r = ReadClock(p, t, static_cast<int>(a0), &ns);
      if (r != 0) return r;
      int64_t ts[2];
      NsToTs(ns, ts);
      if (!CopyToUser(p, a1, ts, sizeof(ts))) return kEFAULT;
      return 0;
    }
    case 229: {  // clock_getres(clk, res)
      int64_t ns = 0;
      const int64_t r = ReadClock(p, t, static_cast<int>(a0), &ns);
      if (r != 0) return r;
      // Every supported clock is read at nanosecond granularity (the COARSE
      // variants are the same clocks as the precise ones here).
      const int64_t res[2] = {0, 1};
      if (a1 != 0 && !CopyToUser(p, a1, res, sizeof(res))) return kEFAULT;
      return 0;
    }
    case 227: {  // clock_settime(clk, tp)
      int64_t ts[2];
      if (!CopyFromUser(p, ts, a1, sizeof(ts))) return kEFAULT;
      if (static_cast<int>(a0) != kCLOCK_REALTIME) return kEINVAL;
      if (!IsValidTs(ts)) return kEINVAL;
      return SetRealtimeNs(TsToNs(ts)) ? 0 : kEINVAL;
    }
    case 96: {  // gettimeofday(tv, tz)
      if (a0 != 0) {
        const int64_t ns = ShimRealtimeNs();
        const int64_t tv[2] = {ns / kNsPerSec, (ns % kNsPerSec) / 1000};
        if (!CopyToUser(p, a0, tv, sizeof(tv))) return kEFAULT;
      }
      if (a1 != 0) {
        const int32_t tz[2] = {g_tz_minuteswest.load(), g_tz_dsttime.load()};
        if (!CopyToUser(p, a1, tz, sizeof(tz))) return kEFAULT;
      }
      return 0;
    }
    case 164: {  // settimeofday(tv, tz)
      int64_t tv[2] = {0, 0};
      int32_t tz[2] = {0, 0};
      if (a0 != 0) {
        if (!CopyFromUser(p, tv, a0, sizeof(tv))) return kEFAULT;
        if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000) return kEINVAL;
      }
      if (a1 != 0) {
        if (!CopyFromUser(p, tz, a1, sizeof(tz))) return kEFAULT;
        if (tz[0] < -15 * 60 || tz[0] > 15 * 60) return kEINVAL;
      }
      if (a0 != 0 && !SetRealtimeNs(tv[0] * kNsPerSec + tv[1] * 1000)) return kEINVAL;
      if (a1 != 0) {
        g_tz_minuteswest.store(tz[0]);
        g_tz_dsttime.store(tz[1]);
      }
      return 0;
    }
    case 201: {  // time(tloc)
      const int64_t sec = ShimRealtimeNs() / kNsPerSec;
      if (a0 != 0 && !CopyToUser(p, a0, &sec, sizeof(sec))) return kEFAULT;
      return sec;
    }
    case 100: {  // times(buf)
      if (a0 != 0) {
        int64_t self = ProcessCpuTimeNs(p);
        int64_t children;
        {
          std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
          children = p->children_cpu_ns;
        }
        // Zircon reports total CPU time only; it is all reported as user time.
        const int64_t tms[4] = {self > 0 ? self / kNsPerTick : 0, 0, children / kNsPerTick, 0};
        if (!CopyToUser(p, a0, tms, sizeof(tms))) return kEFAULT;
      }
      return zx_clock_get_boot() / kNsPerTick;
    }
    case 35: {  // nanosleep(req, rem)
      int64_t ts[2];
      if (!CopyFromUser(p, ts, a0, sizeof(ts))) return kEFAULT;
      if (!IsValidTs(ts)) return kEINVAL;
      return RelativeSleep(p, t, zx_deadline_after(TsToNs(ts)), static_cast<uintptr_t>(a1));
    }
    case 230: {  // clock_nanosleep(clk, flags, req, rem)
      const int clk = static_cast<int>(a0);
      const int flags = static_cast<int>(a1);
      int64_t ts[2];
      if (!CopyFromUser(p, ts, a2, sizeof(ts))) return kEFAULT;
      if (!IsValidTs(ts)) return kEINVAL;
      if (clk == kCLOCK_THREAD_CPUTIME_ID) return kEINVAL;
      if (clk == kCLOCK_PROCESS_CPUTIME_ID || clk < 0) return kEOPNOTSUPP;
      const int64_t ns = TsToNs(ts);
      if ((flags & kTIMER_ABSTIME) != 0) {
        zx_time_t deadline = 0;
        if (!ClockDeadlineToMonotonic(clk, ns, &deadline)) return kEINVAL;
        // Absolute sleeps restart with the same absolute time; rem unused.
        return SleepUntil(p, t, deadline) ? 0 : kERESTARTNOHAND;
      }
      zx_time_t probe = 0;
      if (!ClockDeadlineToMonotonic(clk, 0, &probe)) return kEINVAL;
      return RelativeSleep(p, t, zx_deadline_after(ns), static_cast<uintptr_t>(a3));
    }
    case 219: {  // restart_syscall()
      const int kind = t->restart_kind;
      if (kind == kRestartNanosleep) {
        t->restart_kind = kRestartNone;
        return RelativeSleep(p, t, static_cast<zx_time_t>(t->restart_args[0]),
                             static_cast<uintptr_t>(t->restart_args[1]));
      }
      if (kind == kRestartFutexWait) return FutexRestart(p, t);
      return kEINTR;  // do_no_restart_syscall()
    }
    case 36:  // getitimer(which, value)
      return SysGetitimer(p, static_cast<int>(a0), static_cast<uintptr_t>(a1));
    case 38:  // setitimer(which, new, old)
      return SysSetitimer(p, static_cast<int>(a0), static_cast<uintptr_t>(a1),
                          static_cast<uintptr_t>(a2));
    case 37: {  // alarm(seconds)
      int64_t ov = 0, oi = 0;
      SetItimerReal(p, static_cast<int64_t>(static_cast<uint32_t>(a0)) * kNsPerSec, 0, &ov, &oi);
      int64_t secs = ov / kNsPerSec;
      const int64_t usec = (ov % kNsPerSec) / 1000;
      if ((secs == 0 && usec != 0) || usec >= 500000) secs++;
      return secs;
    }
    case 222:  // timer_create(clk, sevp, timerid)
      return SysTimerCreate(p, static_cast<int>(a0), static_cast<uintptr_t>(a1),
                            static_cast<uintptr_t>(a2));
    case 223:  // timer_settime(id, flags, new, old)
      return SysTimerSettime(p, static_cast<int>(a0), static_cast<int>(a1),
                             static_cast<uintptr_t>(a2), static_cast<uintptr_t>(a3));
    case 224: {  // timer_gettime(id, cur)
      int64_t v = 0, i = 0;
      {
        std::lock_guard<std::mutex> l(g_timer_mtx);
        auto it = g_timers.find({p->pid, static_cast<int>(a0)});
        if (it == g_timers.end() || static_cast<int>(a0) < 0) return kEINVAL;
        TimerRemaining(it->second, &v, &i);
      }
      int64_t o[4];
      NsToTs(i, o);
      NsToTs(v, o + 2);
      if (!CopyToUser(p, a1, o, sizeof(o))) return kEFAULT;
      return 0;
    }
    case 225: {  // timer_getoverrun(id)
      std::lock_guard<std::mutex> l(g_timer_mtx);
      auto it = g_timers.find({p->pid, static_cast<int>(a0)});
      if (it == g_timers.end() || static_cast<int>(a0) < 0) return kEINVAL;
      return it->second.last_overrun;
    }
    case 226: {  // timer_delete(id)
      std::lock_guard<std::mutex> l(g_timer_mtx);
      auto it = g_timers.find({p->pid, static_cast<int>(a0)});
      if (it == g_timers.end() || static_cast<int>(a0) < 0) return kEINVAL;
      g_timers.erase(it);
      KickTimerService();
      return 0;
    }
    default:
      *handled = false;
      return kENOSYS;
  }
}
