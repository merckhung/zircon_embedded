// Linux futex(2) for the shim: per-key waiter queues with real wake counts.
//
// Keys: private futexes (FUTEX_PRIVATE_FLAG, or any futex on private memory)
// are keyed by (process, address). Futexes on MAP_SHARED / SysV memory are
// keyed by (VMO koid, offset), so waiters in different processes sharing the
// mapping find each other.
#include <list>

#include "src/shim/shim_common.h"
#include "src/shim/signals.h"

namespace {

constexpr int kFutexWait = 0;
constexpr int kFutexWake = 1;
constexpr int kFutexFd = 2;
constexpr int kFutexRequeue = 3;
constexpr int kFutexCmpRequeue = 4;
constexpr int kFutexWakeOp = 5;
constexpr int kFutexLockPi = 6;
constexpr int kFutexUnlockPi = 7;
constexpr int kFutexTrylockPi = 8;
constexpr int kFutexWaitBitset = 9;
constexpr int kFutexWakeBitset = 10;
constexpr int kFutexWaitRequeuePi = 11;
constexpr int kFutexCmpRequeuePi = 12;
constexpr int kFutexLockPi2 = 13;
constexpr int kFutexPrivateFlag = 128;
constexpr int kFutexClockRealtime = 256;
constexpr uint32_t kFutexBitsetMatchAny = 0xffffffffU;

constexpr uint32_t kFutexWaiters = 0x80000000U;
constexpr uint32_t kFutexOwnerDied = 0x40000000U;
constexpr uint32_t kFutexTidMask = 0x3fffffffU;
constexpr int kRobustListLimit = 2048;

struct FutexKey {
  uint64_t kind;  // 0: private (id = process), 1: shared (id = VMO koid)
  uint64_t id;
  uint64_t offset;
  bool operator<(const FutexKey& o) const {
    if (kind != o.kind) return kind < o.kind;
    if (id != o.id) return id < o.id;
    return offset < o.offset;
  }
};

struct FutexWaiter {
  FutexKey key;
  uint32_t bitset;
  LinuxThread* thrd;
  bool woken = false;
};

std::mutex g_futex_mtx;
std::map<FutexKey, std::list<FutexWaiter*>> g_futex_queues;

FutexKey ComputeKey(LinuxProcess* proc, uintptr_t uaddr, bool priv) {
  if (!priv) {
    std::lock_guard<std::mutex> lock(proc->map_mtx);
    for (const auto& sm : proc->shared_mappings) {
      if (sm.vmo == ZX_HANDLE_INVALID) continue;
      if (uaddr < sm.vaddr || uaddr + 4 > sm.vaddr + sm.size) continue;
      zx_koid_t koid = sm.vmo_koid;
      if (koid == ZX_KOID_INVALID) {
        zx_info_handle_basic_t info{};
        if (zx_object_get_info(sm.vmo, ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr,
                               nullptr) == ZX_OK) {
          koid = info.koid;
        }
      }
      if (koid != ZX_KOID_INVALID) {
        return FutexKey{1, koid, sm.vmo_offset + (uaddr - sm.vaddr)};
      }
    }
  }
  return FutexKey{0, reinterpret_cast<uintptr_t>(proc), uaddr};
}

// Requires g_futex_mtx.
void Unqueue(FutexWaiter* w) {
  auto it = g_futex_queues.find(w->key);
  if (it == g_futex_queues.end()) return;
  it->second.remove(w);
  if (it->second.empty()) g_futex_queues.erase(it);
}

// Requires g_futex_mtx. Returns the number of waiters woken.
int WakeKeyLocked(const FutexKey& key, int max_wake, uint32_t bitset) {
  if (max_wake <= 0) return 0;
  auto it = g_futex_queues.find(key);
  if (it == g_futex_queues.end()) return 0;
  int woken = 0;
  auto& q = it->second;
  for (auto w = q.begin(); w != q.end() && woken < max_wake;) {
    if (((*w)->bitset & bitset) == 0) {
      ++w;
      continue;
    }
    FutexWaiter* waiter = *w;
    w = q.erase(w);
    waiter->woken = true;
    WakeThread(waiter->thrd);
    ++woken;
  }
  if (q.empty()) g_futex_queues.erase(it);
  return woken;
}

// Requires g_futex_mtx. Moves up to max_requeue waiters from `from` to `to`.
int RequeueLocked(const FutexKey& from, const FutexKey& to, int max_requeue) {
  if (max_requeue <= 0) return 0;
  auto it = g_futex_queues.find(from);
  if (it == g_futex_queues.end()) return 0;
  int moved = 0;
  auto& src = it->second;
  auto& dst = g_futex_queues[to];
  while (!src.empty() && moved < max_requeue) {
    FutexWaiter* w = src.front();
    src.pop_front();
    w->key = to;
    dst.push_back(w);
    ++moved;
  }
  if (src.empty()) g_futex_queues.erase(from);
  if (dst.empty()) g_futex_queues.erase(to);
  return moved;
}

bool ReadU32(LinuxProcess* proc, uintptr_t uaddr, uint32_t* out) {
  return CopyFromUser(proc, out, uaddr, sizeof(*out));
}

bool ReadTimespec(LinuxProcess* proc, uintptr_t addr, int64_t* out_ns) {
  int64_t ts[2];
  if (!CopyFromUser(proc, ts, addr, sizeof(ts))) return false;
  if (ts[0] < 0 || ts[1] < 0 || ts[1] >= 1000000000LL) {
    *out_ns = -1;
    return true;
  }
  *out_ns = (ts[0] > (INT64_MAX / 1000000000LL) - 1) ? INT64_MAX : ts[0] * 1000000000LL + ts[1];
  return true;
}

// The wait itself. `deadline` is ZX_CLOCK_MONOTONIC (ZX_TIME_INFINITE if
// none). Returns 0, -EAGAIN, -ETIMEDOUT, -EFAULT or a restart code.
int64_t FutexWaitUntil(LinuxProcess* proc, LinuxThread* thrd, uintptr_t uaddr, uint32_t val,
                       uint32_t bitset, zx_time_t deadline, bool priv, int op) {
  FutexWaiter w{ComputeKey(proc, uaddr, priv), bitset, thrd};
  {
    std::lock_guard<std::mutex> lock(g_futex_mtx);
    uint32_t cur = 0;
    if (!ReadU32(proc, uaddr, &cur)) return kEFAULT;
    if (cur != val) return kEAGAIN;
    g_futex_queues[w.key].push_back(&w);
  }
  for (;;) {
    ThreadClearWake(thrd);
    {
      std::lock_guard<std::mutex> lock(g_futex_mtx);
      if (w.woken) return 0;
      const bool interrupted = ShimSignalPending(proc, thrd);
      const bool timed_out = deadline != ZX_TIME_INFINITE && zx_clock_get_monotonic() >= deadline;
      if (interrupted || timed_out) {
        Unqueue(&w);
        if (timed_out && !interrupted) return kETIMEDOUT;
        if (deadline == ZX_TIME_INFINITE) return kERESTARTSYS;
        // Timed wait: restart_syscall(2) continues with the same deadline.
        thrd->restart_kind = kRestartFutexWait;
        thrd->restart_args[0] = uaddr;
        thrd->restart_args[1] = val;
        thrd->restart_args[2] = bitset;
        thrd->restart_args[3] = static_cast<uint64_t>(deadline);
        thrd->restart_args[4] = static_cast<uint64_t>(op);
        return kERESTART_RESTARTBLOCK;
      }
    }
    ThreadWaitWake(thrd, deadline);
  }
}

bool FutexOpCmp(int cmp, int32_t oldval, int32_t cmparg, bool* result) {
  switch (cmp) {
    case 0: *result = oldval == cmparg; return true;  // FUTEX_OP_CMP_EQ
    case 1: *result = oldval != cmparg; return true;  // NE
    case 2: *result = oldval < cmparg; return true;   // LT
    case 3: *result = oldval <= cmparg; return true;  // LE
    case 4: *result = oldval > cmparg; return true;   // GT
    case 5: *result = oldval >= cmparg; return true;  // GE
    default: return false;
  }
}

int32_t SignExtend12(uint32_t v) { return static_cast<int32_t>(v << 20) >> 20; }

}  // namespace

int64_t SysFutex(LinuxProcess* proc, LinuxThread* thrd, uintptr_t uaddr, int op, uint32_t val,
                 uintptr_t timeout_addr, uintptr_t uaddr2, uint32_t val3) {
  const int cmd = op & ~(kFutexPrivateFlag | kFutexClockRealtime);
  const bool priv = (op & kFutexPrivateFlag) != 0;
  const bool clock_rt = (op & kFutexClockRealtime) != 0;
  if (clock_rt && cmd != kFutexWait && cmd != kFutexWaitBitset && cmd != kFutexWaitRequeuePi &&
      cmd != kFutexLockPi2) {
    return kENOSYS;
  }
  if ((uaddr & 3) != 0) return kEINVAL;

  switch (cmd) {
    case kFutexWait:
    case kFutexWaitBitset: {
      const uint32_t bitset = (cmd == kFutexWait) ? kFutexBitsetMatchAny : val3;
      if (bitset == 0) return kEINVAL;
      zx_time_t deadline = ZX_TIME_INFINITE;
      if (timeout_addr != 0) {
        int64_t ns = 0;
        if (!ReadTimespec(proc, timeout_addr, &ns)) return kEFAULT;
        if (ns < 0) return kEINVAL;
        if (cmd == kFutexWait) {
          deadline = zx_deadline_after(ns);  // relative (CLOCK_MONOTONIC)
        } else if (!ClockDeadlineToMonotonic(clock_rt ? 0 : 1, ns, &deadline)) {
          return kEINVAL;  // absolute on CLOCK_REALTIME or CLOCK_MONOTONIC
        }
      }
      return FutexWaitUntil(proc, thrd, uaddr, val, bitset, deadline, priv, op);
    }
    case kFutexWake:
    case kFutexWakeBitset: {
      const uint32_t bitset = (cmd == kFutexWake) ? kFutexBitsetMatchAny : val3;
      if (bitset == 0) return kEINVAL;
      const FutexKey key = ComputeKey(proc, uaddr, priv);
      std::lock_guard<std::mutex> lock(g_futex_mtx);
      return WakeKeyLocked(key, static_cast<int>(std::min<uint32_t>(val, INT32_MAX)), bitset);
    }
    case kFutexRequeue:
    case kFutexCmpRequeue: {
      if ((uaddr2 & 3) != 0) return kEINVAL;
      const int nr_wake = static_cast<int>(std::min<uint32_t>(val, INT32_MAX));
      // The requeue limit is passed in the timeout argument slot.
      const int nr_requeue =
          static_cast<int>(std::min<uint64_t>(static_cast<uint32_t>(timeout_addr), INT32_MAX));
      if (static_cast<int32_t>(val) < 0 || static_cast<int32_t>(timeout_addr) < 0) return kEINVAL;
      const FutexKey k1 = ComputeKey(proc, uaddr, priv);
      const FutexKey k2 = ComputeKey(proc, uaddr2, priv);
      std::lock_guard<std::mutex> lock(g_futex_mtx);
      if (cmd == kFutexCmpRequeue) {
        uint32_t cur = 0;
        if (!ReadU32(proc, uaddr, &cur)) return kEFAULT;
        if (cur != val3) return kEAGAIN;
      }
      const int woken = WakeKeyLocked(k1, nr_wake, kFutexBitsetMatchAny);
      const int moved = (k1 < k2 || k2 < k1) ? RequeueLocked(k1, k2, nr_requeue) : 0;
      return woken + moved;
    }
    case kFutexWakeOp: {
      if ((uaddr2 & 3) != 0) return kEINVAL;
      const uint32_t enc_op = (val3 >> 28) & 0xf;
      const int cmp = static_cast<int>((val3 >> 24) & 0xf);
      int32_t oparg = SignExtend12((val3 >> 12) & 0xfff);
      const int32_t cmparg = SignExtend12(val3 & 0xfff);
      if ((enc_op & 8) != 0) {  // FUTEX_OP_OPARG_SHIFT
        oparg = static_cast<int32_t>(1U << (oparg & 31));
      }
      const uint32_t arith = enc_op & 7;
      if (arith > 4) return kENOSYS;
      bool dummy;
      if (!FutexOpCmp(cmp, 0, 0, &dummy)) return kENOSYS;
      const int nr_wake = static_cast<int>(std::min<uint32_t>(val, INT32_MAX));
      const int nr_wake2 =
          static_cast<int>(std::min<uint64_t>(static_cast<uint32_t>(timeout_addr), INT32_MAX));
      const FutexKey k1 = ComputeKey(proc, uaddr, priv);
      const FutexKey k2 = ComputeKey(proc, uaddr2, priv);
      std::lock_guard<std::mutex> lock(g_futex_mtx);
      // The atomic read-modify-write is done in place on user memory, which
      // must be mapped writable.
      if (!UserRangeWritable(proc, uaddr2, 4)) return kEFAULT;
      auto* word = reinterpret_cast<int32_t*>(uaddr2);
      int32_t oldval = __atomic_load_n(word, __ATOMIC_SEQ_CST);
      for (;;) {
        int32_t newval = oldval;
        switch (arith) {
          case 0: newval = oparg; break;                                  // SET
          case 1: newval = static_cast<int32_t>(static_cast<uint32_t>(oldval) +
                                                static_cast<uint32_t>(oparg));
            break;                                                        // ADD
          case 2: newval = oldval | oparg; break;                         // OR
          case 3: newval = oldval & ~oparg; break;                        // ANDN
          case 4: newval = oldval ^ oparg; break;                         // XOR
        }
        if (__atomic_compare_exchange_n(word, &oldval, newval, false, __ATOMIC_SEQ_CST,
                                        __ATOMIC_SEQ_CST)) {
          break;
        }
      }
      int woken = WakeKeyLocked(k1, nr_wake, kFutexBitsetMatchAny);
      bool cond = false;
      FutexOpCmp(cmp, oldval, cmparg, &cond);
      if (cond) woken += WakeKeyLocked(k2, nr_wake2, kFutexBitsetMatchAny);
      return woken;
    }
    case kFutexLockPi:
    case kFutexUnlockPi:
    case kFutexTrylockPi:
    case kFutexWaitRequeuePi:
    case kFutexCmpRequeuePi:
    case kFutexLockPi2:
      // Priority-inheritance futexes are not implemented.
      return kENOSYS;
    case kFutexFd:  // removed from Linux in 2.6.26
    default:
      return kENOSYS;
  }
}

int FutexWakeAddress(LinuxProcess* proc, uintptr_t uaddr, int max_wake) {
  // Used for CLONE_CHILD_CLEARTID and robust-list cleanup; Linux wakes with a
  // shared key, which equals the private key for private memory.
  const FutexKey key = ComputeKey(proc, uaddr, false);
  std::lock_guard<std::mutex> lock(g_futex_mtx);
  return WakeKeyLocked(key, max_wake, kFutexBitsetMatchAny);
}

int64_t FutexRestart(LinuxProcess* proc, LinuxThread* thrd) {
  const uintptr_t uaddr = thrd->restart_args[0];
  const uint32_t val = static_cast<uint32_t>(thrd->restart_args[1]);
  const uint32_t bitset = static_cast<uint32_t>(thrd->restart_args[2]);
  const zx_time_t deadline = static_cast<zx_time_t>(thrd->restart_args[3]);
  const int op = static_cast<int>(thrd->restart_args[4]);
  thrd->restart_kind = kRestartNone;
  return FutexWaitUntil(proc, thrd, uaddr, val, bitset, deadline,
                        (op & kFutexPrivateFlag) != 0, op);
}

void FutexExitRobustList(LinuxProcess* proc, LinuxThread* thrd) {
  const uintptr_t head = thrd->robust_list_head;
  thrd->robust_list_head = 0;
  if (head == 0) return;
  // struct robust_list_head { robust_list* next; long futex_offset;
  //                           robust_list* list_op_pending; }
  uint64_t h[3];
  if (!CopyFromUser(proc, h, head, sizeof(h))) return;
  const int64_t futex_offset = static_cast<int64_t>(h[1]);
  // Bit 0 of each entry pointer flags a PI futex (fetch_robust_entry()).
  const uintptr_t pending = h[2] & ~1ULL;
  auto handle_death = [&](uintptr_t entry) {
    const uintptr_t uaddr = entry + futex_offset;
    if ((uaddr & 3) != 0 || !UserRangeWritable(proc, uaddr, 4)) return;
    auto* word = reinterpret_cast<uint32_t*>(uaddr);
    uint32_t v = __atomic_load_n(word, __ATOMIC_SEQ_CST);
    for (;;) {
      if ((v & kFutexTidMask) != static_cast<uint32_t>(thrd->tid)) return;
      const uint32_t nv = (v & kFutexWaiters) | kFutexOwnerDied;
      if (__atomic_compare_exchange_n(word, &v, nv, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        break;
      }
    }
    if ((v & kFutexWaiters) != 0) FutexWakeAddress(proc, uaddr, 1);
  };
  uintptr_t entry = h[0] & ~1ULL;
  int limit = kRobustListLimit;
  while (entry != head && entry != 0 && limit-- > 0) {
    uint64_t next = 0;
    if (!CopyFromUser(proc, &next, entry, sizeof(next))) break;
    if (entry != pending) handle_death(entry);
    entry = next & ~1ULL;
  }
  if (pending != 0) handle_death(pending);
}
