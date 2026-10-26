// Linux signal semantics for the shim: generation (kill/tkill/tgkill/
// sigqueue, faults, kernel-generated signals), job control, delivery on the
// user stack with a Linux x86_64 rt_sigframe, rt_sigreturn, and the restart
// rules for interrupted syscalls.
#include <algorithm>
#include <cstddef>

#include "src/shim/fd_ops.h"
#include "src/shim/mem_space.h"
#include "src/shim/shim_common.h"
#include "src/shim/signals.h"

namespace {

enum class DefAct { kTerm, kCore, kIgnore, kStop, kCont };

DefAct DefaultAction(int sig) {
  switch (sig) {
    case kSIGCHLD:
    case kSIGURG:
    case kSIGWINCH:
      return DefAct::kIgnore;
    case kSIGCONT:
      return DefAct::kCont;
    case kSIGSTOP:
    case kSIGTSTP:
    case kSIGTTIN:
    case kSIGTTOU:
      return DefAct::kStop;
    case kSIGQUIT:
    case kSIGILL:
    case kSIGTRAP:
    case kSIGABRT:
    case kSIGBUS:
    case kSIGFPE:
    case kSIGSEGV:
    case kSIGXCPU:
    case kSIGXFSZ:
    case kSIGSYS:
      return DefAct::kCore;
    default:
      // SIGHUP, SIGINT, SIGKILL, SIGUSR1/2, SIGPIPE, SIGALRM, SIGTERM,
      // SIGSTKFLT, SIGVTALRM, SIGPROF, SIGIO, SIGPWR and all real-time signals.
      return DefAct::kTerm;
  }
}

// Linux delivers synchronous (fault) signals before others.
constexpr int kSyncSignals[] = {kSIGSEGV, kSIGBUS, kSIGILL, kSIGTRAP, kSIGFPE, kSIGSYS};
constexpr size_t kMaxQueuedRt = 1024;  // per-queue limit for queued RT signals

constexpr int kSS_ONSTACK = 1;
constexpr int kSS_DISABLE = 2;
constexpr int32_t kSS_AUTODISARM = static_cast<int32_t>(1U << 31);
constexpr size_t kMINSIGSTKSZ = 2048;

#if defined(__x86_64__)
constexpr uint64_t kUC_FP_XSTATE = 0x1;
constexpr uint64_t kUC_SIGCONTEXT_SS = 0x2;
constexpr uint64_t kUC_STRICT_RESTORE_SS = 0x4;
constexpr uint32_t kFP_XSTATE_MAGIC1 = 0x46505853;
constexpr uint32_t kFP_XSTATE_MAGIC2 = 0x46505845;

// EFLAGS bits user code may change through rt_sigreturn (Linux FIX_EFLAGS).
constexpr uint64_t kFixEflags = 0x40000 /*AC*/ | 0x800 /*OF*/ | 0x400 /*DF*/ |
                                0x100 /*TF*/ | 0x80 /*SF*/ | 0x40 /*ZF*/ |
                                0x10 /*AF*/ | 0x4 /*PF*/ | 0x1 /*CF*/ |
                                0x10000 /*RF*/;

// Linux x86_64 struct sigcontext / ucontext / rt_sigframe.
struct KSigcontext {
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
  uint64_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
  uint16_t cs, gs, fs, ss;
  uint64_t err, trapno, oldmask, cr2;
  uint64_t fpstate;
  uint64_t reserved1[8];
};
static_assert(sizeof(KSigcontext) == 256, "sigcontext layout");

struct KStack {
  uint64_t ss_sp;
  int32_t ss_flags;
  int32_t pad;
  uint64_t ss_size;
};
static_assert(sizeof(KStack) == 24, "stack_t layout");

struct KUcontext {
  uint64_t uc_flags;
  uint64_t uc_link;
  KStack uc_stack;
  KSigcontext uc_mcontext;
  uint64_t uc_sigmask;
};
static_assert(sizeof(KUcontext) == 304, "ucontext layout");

struct KRtSigframe {
  uint64_t pretcode;
  KUcontext uc;
  LinuxSiginfo info;
};
#elif defined(__aarch64__)
struct aarch64_ctx {
  uint32_t magic;
  uint32_t size;
};

constexpr uint32_t kFPSIMD_MAGIC = 0x46508001;

struct fpsimd_context {
  aarch64_ctx head;
  uint32_t fpsr;
  uint32_t fpcr;
  uint8_t vregs[32 * 16];
};

struct Arm64Sigcontext {
  uint64_t fault_address;
  uint64_t regs[31];
  uint64_t sp;
  uint64_t pc;
  uint64_t pstate;
  alignas(16) uint8_t __reserved[4096];
};

struct KStack {
  uint64_t ss_sp;
  int32_t ss_flags;
  int32_t pad;
  uint64_t ss_size;
};

struct Arm64Ucontext {
  uint64_t uc_flags;
  uint64_t uc_link;
  KStack uc_stack;
  uint64_t uc_sigmask;
  uint8_t __unused[1024 / 8 - sizeof(uint64_t)];
  Arm64Sigcontext uc_mcontext;
};

struct Arm64RtSigframe {
  LinuxSiginfo info;
  Arm64Ucontext uc;
  uint64_t fp;
  uint64_t lr;
  uint32_t tramp[2];
};
#endif
#if defined(__x86_64__)
static_assert(offsetof(KRtSigframe, uc) == 8, "rt_sigframe layout");
static_assert(offsetof(KRtSigframe, info) == 312, "rt_sigframe layout");
#endif

struct FpxSwBytes {
  uint32_t magic1;
  uint32_t extended_size;
  uint64_t xfeatures;
  uint32_t xstate_size;
  uint32_t padding[7];
};
static_assert(sizeof(FpxSwBytes) == 48, "_fpx_sw_bytes layout");

void PutI32(LinuxSiginfo* i, size_t off, int32_t v) { memcpy(i->fields + (off - 16), &v, 4); }
void PutU64(LinuxSiginfo* i, size_t off, uint64_t v) { memcpy(i->fields + (off - 16), &v, 8); }
int32_t GetI32(const LinuxSiginfo& i, size_t off) {
  int32_t v;
  memcpy(&v, i.fields + (off - 16), 4);
  return v;
}

bool IsRestartCode(int64_t r) {
  return r == kERESTARTSYS || r == kERESTARTNOINTR || r == kERESTARTNOHAND ||
         r == kERESTART_RESTARTBLOCK;
}

LinuxThread* LeaderThreadLocked(LinuxProcess* p) {
  for (const auto& t : p->threads) {
    if (t->tid == p->pid) return t.get();
  }
  return p->threads.empty() ? nullptr : p->threads.front().get();
}

// Linux sig_ignored(): blocked signals are never discarded at generation.
// Requires p->sig_mtx.
bool SigIgnoredLocked(LinuxProcess* p, LinuxThread* target, int sig, bool force) {
  if (target != nullptr && (target->sigmask.load() & SigBit(sig)) != 0) return false;
  const uint64_t h = p->sigactions[sig].sa_handler;
  // The global init only receives signals it has a handler for
  // (SIGNAL_UNKILLABLE), unless the kernel forces SIGKILL/SIGSTOP.
  if (p->pid == 1 && h == kSIG_DFL && !(force && (sig == kSIGKILL || sig == kSIGSTOP))) {
    return true;
  }
  if (h == kSIG_IGN) return true;
  if (h == kSIG_DFL) {
    const DefAct d = DefaultAction(sig);
    return d == DefAct::kIgnore || d == DefAct::kCont;
  }
  return false;
}

void RemoveFromQueue(std::deque<LinuxSiginfo>* q, std::atomic<uint64_t>* pend, uint64_t mask) {
  pend->fetch_and(~mask);
  q->erase(std::remove_if(q->begin(), q->end(),
                          [mask](const LinuxSiginfo& i) { return (SigBit(i.si_signo) & mask) != 0; }),
           q->end());
}

// Requires g_proc_mtx and p->sig_mtx.
void FlushSignalsLocked(LinuxProcess* p, uint64_t mask) {
  RemoveFromQueue(&p->sigqueue, &p->pending_signals, mask);
  for (const auto& t : p->threads) RemoveFromQueue(&t->sigqueue, &t->pending_signals, mask);
}

std::shared_ptr<LinuxProcess> FindProcessLocked(int pid) {
  auto it = g_processes.find(pid);
  return it == g_processes.end() ? nullptr : it->second;
}

LinuxThread* FindThreadLocked(int tid, LinuxProcess** out_proc) {
  for (auto& kv : g_processes) {
    for (const auto& t : kv.second->threads) {
      if (t->tid == tid) {
        *out_proc = kv.second.get();
        return t.get();
      }
    }
  }
  return nullptr;
}

// do_notify_parent_cldstop(): tell the parent about a stop or continue.
void NotifyParentJobControl(LinuxProcess* p, int why, int status) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  auto parent = FindProcessLocked(p->ppid);
  if (!parent) return;
  bool nocldstop;
  {
    std::lock_guard<std::mutex> sl(parent->sig_mtx);
    nocldstop = (parent->sigactions[kSIGCHLD].sa_flags & kSA_NOCLDSTOP) != 0;
  }
  if (!nocldstop) {
    SendSignalToProcess(parent.get(), MakeSiginfoChld(why, p->pid, status, ProcessCpuTimeNs(p)));
  }
  WakeProcessThreads(parent.get());
  g_wait4_cv.notify_all();
}

// __send_signal() + complete_signal(). Requires g_proc_mtx. `t` == nullptr
// means process-directed.
int64_t DoSendSignalLocked(LinuxProcess* p, LinuxThread* t, const LinuxSiginfo& info,
                           bool force) {
  const int sig = info.si_signo;
  if (sig < 1 || sig > kNSIG) return kEINVAL;
  if (p->zombie.load() || p->reaped) return 0;  // delivered to a zombie: discarded
  const uint64_t bit = SigBit(sig);
  bool notify_continued = false;
  bool fatal = false;
  {
    std::lock_guard<std::mutex> sl(p->sig_mtx);
    if (p->exited.load()) return 0;  // group exit in progress
    // prepare_signal(): job-control side effects happen at generation time.
    if ((bit & kStopSignals) != 0) {
      FlushSignalsLocked(p, SigBit(kSIGCONT));
    } else if (sig == kSIGCONT) {
      FlushSignalsLocked(p, kStopSignals);
      if (p->stopped) {
        p->stopped = false;
        p->wait_report = kWaitReportContinued;
        p->stop_cv.notify_all();
        notify_continued = true;
      }
    }
    LinuxThread* ignore_target = (t != nullptr) ? t : LeaderThreadLocked(p);
    if (!SigIgnoredLocked(p, ignore_target, sig, force)) {
      std::atomic<uint64_t>& pend = (t != nullptr) ? t->pending_signals : p->pending_signals;
      std::deque<LinuxSiginfo>& q = (t != nullptr) ? t->sigqueue : p->sigqueue;
      if (sig < kSIGRTMIN) {
        // Standard signals do not queue: one instance pending at most.
        if ((pend.load() & bit) == 0) {
          q.push_back(info);
          pend.fetch_or(bit);
        }
      } else {
        if (q.size() >= kMaxQueuedRt) {
          if (info.si_code < 0) return kEAGAIN;  // sigqueue(): queue full
          pend.fetch_or(bit);                    // kill(): pending without info
        } else {
          q.push_back(info);
          pend.fetch_or(bit);
        }
      }
      // Fatal-signal shortcut: a SIG_DFL terminating signal that some thread
      // does not block kills the whole group immediately.
      const uint64_t h = p->sigactions[sig].sa_handler;
      if (sig == kSIGKILL || (h == kSIG_DFL && DefaultAction(sig) == DefAct::kTerm)) {
        if (t != nullptr) {
          fatal = (t->sigmask.load() & bit) == 0;
        } else {
          for (const auto& th : p->threads) {
            if ((th->sigmask.load() & bit) == 0) {
              fatal = true;
              break;
            }
          }
        }
        if (p->pid == 1 && !(force && sig == kSIGKILL)) fatal = false;
      }
    }
  }
  if (fatal) {
    GroupExit(p, sig);
  } else if (t != nullptr) {
    WakeThread(t);
  } else {
    // Every thread re-checks; a thread that blocks the signal (or is in
    // sigtimedwait for it) simply goes back to sleep.
    for (const auto& th : p->threads) WakeThread(th.get());
  }
  if (notify_continued) NotifyParentJobControl(p, kCLD_CONTINUED, kSIGCONT);
  NotifyPollWaiters();
  g_wait4_cv.notify_all();
  return 0;
}

int PickSignal(uint64_t set) {
  for (int s : kSyncSignals) {
    if ((set & SigBit(s)) != 0) return s;
  }
  return __builtin_ctzll(set) + 1;
}

void TakeFromQueue(std::deque<LinuxSiginfo>* q, std::atomic<uint64_t>* pend, int sig,
                   LinuxSiginfo* out) {
  bool found = false;
  bool more = false;
  for (auto it = q->begin(); it != q->end(); ++it) {
    if (it->si_signo != sig) continue;
    if (!found) {
      *out = *it;
      it = q->erase(it);
      found = true;
      if (it == q->end()) break;
      if (it->si_signo == sig) more = true;
      continue;
    }
    more = true;
    break;
  }
  if (!found) {
    // Pending bit set without a siginfo record (queue overflow or a raw
    // pending_signals update): Linux reports SI_USER with no sender.
    *out = MakeSiginfo(sig, kSI_USER);
  }
  if (!more) pend->fetch_and(~SigBit(sig));
}

int DequeueSignal(LinuxProcess* p, LinuxThread* t, uint64_t mask, LinuxSiginfo* out) {
  int sig = 0;
  {
    std::lock_guard<std::mutex> sl(p->sig_mtx);
    const uint64_t tset = t->pending_signals.load() & mask;
    if (tset != 0) {
      sig = PickSignal(tset);
      TakeFromQueue(&t->sigqueue, &t->pending_signals, sig, out);
    } else {
      const uint64_t pset = p->pending_signals.load() & mask;
      if (pset == 0) return 0;
      sig = PickSignal(pset);
      TakeFromQueue(&p->sigqueue, &p->pending_signals, sig, out);
    }
  }
  if (out->si_code == kSI_TIMER) {
    PutI32(out, 20, TimeSignalDelivered(p->pid, GetI32(*out, 16)));
  }
  return sig;
}

bool OnSigStackRaw(const LinuxThread* t, uint64_t sp) {
  return sp > t->altstack_sp && sp - t->altstack_sp <= t->altstack_size;
}

bool OnSigStack(const LinuxThread* t, uint64_t sp) {
  // With SS_AUTODISARM the stack is disarmed while in use, so by
  // construction the thread is not "on" it (Linux on_sig_stack()).
  if ((t->altstack_flags & kSS_AUTODISARM) != 0) return false;
  return OnSigStackRaw(t, sp);
}

int SasSsFlags(const LinuxThread* t, uint64_t sp) {
  if (t->altstack_size == 0) return kSS_DISABLE;
  return OnSigStack(t, sp) ? kSS_ONSTACK : 0;
}

// do_sigaltstack(). `ss`/`oss` may be null.
int64_t DoSigaltstack(LinuxThread* t, const KStack* ss, KStack* oss, uint64_t sp) {
  if (oss != nullptr) {
    oss->ss_sp = t->altstack_sp;
    oss->ss_size = t->altstack_size;
    oss->ss_flags = SasSsFlags(t, sp) | (t->altstack_flags & kSS_AUTODISARM);
    oss->pad = 0;
  }
  if (ss != nullptr) {
    if (OnSigStack(t, sp)) return kEPERM;
    const int32_t mode = ss->ss_flags & ~kSS_AUTODISARM;
    if (mode != kSS_DISABLE && mode != kSS_ONSTACK && mode != 0) return kEINVAL;
    if (mode == kSS_DISABLE) {
      t->altstack_sp = 0;
      t->altstack_size = 0;
    } else {
      if (ss->ss_size < kMINSIGSTKSZ) return -12;  // ENOMEM
      t->altstack_sp = ss->ss_sp;
      t->altstack_size = ss->ss_size;
    }
    t->altstack_flags = ss->ss_flags & kSS_AUTODISARM;
  }
  return 0;
}

// Builds a Linux rt_sigframe on the user stack and redirects the
// thread to the handler. Returns false if the frame cannot be written (Linux
// then forces SIGSEGV).
bool SetupRtFrame(LinuxProcess* p, LinuxThread* t, int sig, const LinuxSiginfo& info,
                  const LinuxSigaction& act) {
  zx_restricted_state_t* st = t->state;
#if defined(__x86_64__)
  if ((act.sa_flags & kSA_RESTORER) == 0) return false;  // required on x86_64
  const uint64_t orig_sp = st->rsp;
  const bool nested_altstack = OnSigStack(t, orig_sp);
  uint64_t sp = orig_sp - 128;  // skip the red zone
  bool entering_altstack = false;
  if ((act.sa_flags & kSA_ONSTACK) != 0 && SasSsFlags(t, orig_sp) == 0) {
    sp = t->altstack_sp + t->altstack_size;
    entering_altstack = true;
  }
  const bool xs = XstateUsesXsave();
  const size_t xsize = xs ? XstateSize() : 512;
  const size_t fpsize = xs ? xsize + sizeof(uint32_t) : xsize;
  sp = (sp - fpsize) & ~63ULL;
  const uint64_t fpstate = sp;
  const uint64_t frame = ((sp - sizeof(KRtSigframe)) & ~15ULL) - 8;
  if ((nested_altstack || entering_altstack) && !OnSigStackRaw(t, frame)) return false;
  if (frame >= orig_sp && !entering_altstack) return false;  // wrapped
  if (!UserRangeWritable(p, frame, fpstate + fpsize - frame)) return false;

  KRtSigframe f;
  memset(&f, 0, sizeof(f));
  f.pretcode = act.sa_restorer;
  f.uc.uc_flags = kUC_SIGCONTEXT_SS | kUC_STRICT_RESTORE_SS | (xs ? kUC_FP_XSTATE : 0);
  f.uc.uc_link = 0;
  f.uc.uc_stack.ss_sp = t->altstack_sp;
  f.uc.uc_stack.ss_size = t->altstack_size;
  f.uc.uc_stack.ss_flags = SasSsFlags(t, orig_sp) | (t->altstack_flags & kSS_AUTODISARM);
  KSigcontext& sc = f.uc.uc_mcontext;
  sc.r8 = st->r8;
  sc.r9 = st->r9;
  sc.r10 = st->r10;
  sc.r11 = st->r11;
  sc.r12 = st->r12;
  sc.r13 = st->r13;
  sc.r14 = st->r14;
  sc.r15 = st->r15;
  sc.rdi = st->rdi;
  sc.rsi = st->rsi;
  sc.rbp = st->rbp;
  sc.rbx = st->rbx;
  sc.rdx = st->rdx;
  sc.rax = st->rax;
  sc.rcx = st->rcx;
  sc.rsp = st->rsp;
  sc.rip = st->ip;
  sc.eflags = st->flags;
  sc.cs = 0x33;  // __USER_CS
  sc.ss = 0x2b;  // __USER_DS
  const bool fault = (info.si_signo == kSIGSEGV || info.si_signo == kSIGBUS ||
                      info.si_signo == kSIGILL || info.si_signo == kSIGFPE ||
                      info.si_signo == kSIGTRAP) && info.si_code > 0;
  if (fault) {
    sc.err = t->fault_err;
    sc.trapno = t->fault_trapno;
    sc.cr2 = t->fault_cr2;
  }
  const uint64_t saved_mask = t->restore_sigmask ? t->saved_sigmask : t->sigmask.load();
  sc.oldmask = saved_mask;
  sc.fpstate = fpstate;
  f.uc.uc_sigmask = saved_mask;
  f.info = info;

  std::vector<uint8_t> fp_buf(fpsize, 0);
  memcpy(fp_buf.data(), t->xstate, xsize);
  FpxSwBytes sw{};
  if (xs) {
    sw.magic1 = kFP_XSTATE_MAGIC1;
    sw.extended_size = static_cast<uint32_t>(fpsize);
    sw.xfeatures = XstateFeatures();
    sw.xstate_size = static_cast<uint32_t>(xsize);
    const uint32_t magic2 = kFP_XSTATE_MAGIC2;
    memcpy(fp_buf.data() + xsize, &magic2, sizeof(magic2));
  }
  memcpy(fp_buf.data() + 464, &sw, sizeof(sw));
  size_t actual1 = 0, actual2 = 0;
  if (zx_process_write_memory(p->zx_proc, fpstate, fp_buf.data(), fpsize, &actual1) != ZX_OK ||
      actual1 != fpsize ||
      zx_process_write_memory(p->zx_proc, frame, &f, sizeof(f), &actual2) != ZX_OK ||
      actual2 != sizeof(f)) {
    return false;
  }

  st->rsp = frame;
  st->ip = act.sa_handler;
  st->rdi = static_cast<uint64_t>(sig);
  st->rsi = frame + offsetof(KRtSigframe, info);
  st->rdx = frame + offsetof(KRtSigframe, uc);
  st->rax = 0;
  st->flags &= ~(0x400ULL /*DF*/ | 0x10000ULL /*RF*/ | 0x100ULL /*TF*/);
#elif defined(__aarch64__)
  const uint64_t orig_sp = st->sp;
  const bool nested_altstack = OnSigStack(t, orig_sp);
  uint64_t sp = orig_sp;
  bool entering_altstack = false;
  if ((act.sa_flags & kSA_ONSTACK) != 0 && SasSsFlags(t, orig_sp) == 0) {
    sp = t->altstack_sp + t->altstack_size;
    entering_altstack = true;
  }
  const uint64_t frame = (sp - sizeof(Arm64RtSigframe)) & ~15ULL;
  if ((nested_altstack || entering_altstack) && !OnSigStackRaw(t, frame)) return false;
  if (frame >= orig_sp && !entering_altstack) return false;
  if (!UserRangeWritable(p, frame, sizeof(Arm64RtSigframe))) return false;

  Arm64RtSigframe f;
  memset(&f, 0, sizeof(f));
  f.info = info;
  f.uc.uc_flags = 0;
  f.uc.uc_link = 0;
  f.uc.uc_stack.ss_sp = t->altstack_sp;
  f.uc.uc_stack.ss_size = t->altstack_size;
  f.uc.uc_stack.ss_flags = SasSsFlags(t, orig_sp) | (t->altstack_flags & kSS_AUTODISARM);
  const uint64_t saved_mask = t->restore_sigmask ? t->saved_sigmask : t->sigmask.load();
  f.uc.uc_sigmask = saved_mask;

  Arm64Sigcontext& sc = f.uc.uc_mcontext;
  for (int i = 0; i < 31; ++i) sc.regs[i] = st->x[i];
  sc.sp = st->sp;
  sc.pc = st->pc;
  sc.pstate = st->cpsr;
  const bool fault = (info.si_signo == kSIGSEGV || info.si_signo == kSIGBUS ||
                      info.si_signo == kSIGILL || info.si_signo == kSIGFPE ||
                      info.si_signo == kSIGTRAP) && info.si_code > 0;
  if (fault) {
    sc.fault_address = t->fault_cr2;
  }

  fpsimd_context* fpsimd = reinterpret_cast<fpsimd_context*>(sc.__reserved);
  fpsimd->head.magic = kFPSIMD_MAGIC;
  fpsimd->head.size = sizeof(fpsimd_context);
  fpsimd->fpsr = 0;
  fpsimd->fpcr = 0;
  if (t->xstate != nullptr) {
    memcpy(fpsimd->vregs, t->xstate, sizeof(fpsimd->vregs));
  }
  aarch64_ctx* term = reinterpret_cast<aarch64_ctx*>(sc.__reserved + sizeof(fpsimd_context));
  term->magic = 0;
  term->size = 0;
  f.fp = st->x[29];
  f.lr = st->x[30];
  // VDSO-less signal return trampoline on the signal frame:
  //   mov x8, #139   (0xd2801168) -> __NR_rt_sigreturn on AArch64
  //   svc #0         (0xd4000001)
  f.tramp[0] = 0xd2801168U;
  f.tramp[1] = 0xd4000001U;

  size_t actual = 0;
  if (zx_process_write_memory(p->zx_proc, frame, &f, sizeof(f), &actual) != ZX_OK ||
      actual != sizeof(f)) {
    return false;
  }
  if ((act.sa_flags & kSA_RESTORER) != 0 && act.sa_restorer != 0) {
    st->x[30] = act.sa_restorer;
  } else {
    // Fallback VDSO-like signal return trampoline page mapped RX
    static uintptr_t g_arm64_tramp_page = 0;
    {
      std::lock_guard<std::mutex> l(p->map_mtx);
      if (g_arm64_tramp_page == 0 || !RangeFullyMappedLocked(p, g_arm64_tramp_page, kPageSize)) {
        MemRegion tr;
        tr.kind = RegionKind::kAnonPrivate;
        tr.prot = kLinuxProtRead | kLinuxProtExec;
        if (CreateAnonVmo(kPageSize, &tr.vmo) == ZX_OK) {
          const uint32_t code[2] = {0xd2801168U /* mov x8, #139 */, 0xd4000001U /* svc #0 */};
          zx_vmo_write(tr.vmo->handle, code, 0, sizeof(code));
          uintptr_t addr = FindFreeRangeLocked(p, kPageSize, p->vmar_base, VmarEnd(p));
          if (addr != 0 && MapRegionLocked(p, addr, kPageSize, std::move(tr), false) == ZX_OK) {
            g_arm64_tramp_page = addr;
            zx_cache_flush(reinterpret_cast<const void*>(addr), sizeof(code),
                           ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INSN);
          }
        }
      }
    }
    st->x[30] = g_arm64_tramp_page;
  }

  st->sp = frame;
  st->pc = act.sa_handler;
  st->x[0] = static_cast<uint64_t>(sig);
  st->x[1] = frame + offsetof(Arm64RtSigframe, info);
  st->x[2] = frame + offsetof(Arm64RtSigframe, uc);
  st->x[29] = frame + offsetof(Arm64RtSigframe, fp);
#endif
  // The handler starts with a clean FPU state (Linux fpu__clear_user_states).
  XstateReset(t->xstate);
  if ((t->altstack_flags & kSS_AUTODISARM) != 0) {
    t->altstack_sp = 0;
    t->altstack_size = 0;
    t->altstack_flags = 0;
  }
  return true;
}

void ApplyRestart(LinuxThread* t, int64_t ret, bool handler, uint64_t sa_flags,
                  uint64_t orig_sc) {
  zx_restricted_state_t* st = t->state;
  bool restart = false;
  uint64_t restart_nr = orig_sc;
  switch (ret) {
    case kERESTART_RESTARTBLOCK:
      if (handler) {
#if defined(__x86_64__)
        st->rax = static_cast<uint64_t>(kEINTR);
#elif defined(__aarch64__)
        st->x[0] = static_cast<uint64_t>(kEINTR);
#endif
      } else {
        restart = true;
#if defined(__x86_64__)
        restart_nr = 219;  // restart_syscall
#elif defined(__aarch64__)
        restart_nr = 128;  // restart_syscall
#endif
      }
      break;
    case kERESTARTNOHAND:
      if (handler) {
#if defined(__x86_64__)
        st->rax = static_cast<uint64_t>(kEINTR);
#elif defined(__aarch64__)
        st->x[0] = static_cast<uint64_t>(kEINTR);
#endif
      } else {
        restart = true;
      }
      break;
    case kERESTARTSYS:
      if (handler && (sa_flags & kSA_RESTART) == 0) {
#if defined(__x86_64__)
        st->rax = static_cast<uint64_t>(kEINTR);
#elif defined(__aarch64__)
        st->x[0] = static_cast<uint64_t>(kEINTR);
#endif
      } else {
        restart = true;
      }
      break;
    case kERESTARTNOINTR:
      restart = true;
      break;
    default:
      return;
  }
  if (restart) {
#if defined(__x86_64__)
    st->rax = restart_nr;
    st->ip -= 2;  // back over the 2-byte `syscall` instruction
#elif defined(__aarch64__)
    st->x[0] = t->orig_x0;
    st->x[8] = restart_nr;
    st->pc -= 4;  // back over the 4-byte `svc #0` instruction
#endif
  }
}

void WaitWhileStopped(LinuxProcess* p, LinuxThread* t) {
  std::unique_lock<std::mutex> sl(p->sig_mtx);
  while (p->stopped && !p->exited.load() && !t->kill_requested.load()) {
    p->stop_cv.wait(sl);
  }
}

// do_signal_stop(): stop every thread of the process and report to the parent.
void DoGroupStop(LinuxProcess* p, int sig) {
  bool notify = false;
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    {
      std::lock_guard<std::mutex> sl(p->sig_mtx);
      if (!p->stopped && !p->exited.load()) {
        p->stopped = true;
        p->stop_sig = sig;
        p->wait_report = kWaitReportStopped;
        notify = true;
      }
    }
    // Kick running threads so they park in WaitWhileStopped.
    for (const auto& th : p->threads) WakeThread(th.get());
  }
  if (notify) NotifyParentJobControl(p, kCLD_STOPPED, sig);
}

int64_t SysRtSigreturn(LinuxProcess* p, LinuxThread* t) {
  zx_restricted_state_t* st = t->state;
#if defined(__x86_64__)
  const uint64_t frame = st->rsp - 8;
  KRtSigframe f;
  if (!CopyFromUser(p, &f, frame, sizeof(f))) {
    GroupExit(p, kSIGSEGV);  // Linux: badframe -> force SIGSEGV
    return 0;
  }
  const KSigcontext& sc = f.uc.uc_mcontext;
  st->r8 = sc.r8;
  st->r9 = sc.r9;
  st->r10 = sc.r10;
  st->r11 = sc.r11;
  st->r12 = sc.r12;
  st->r13 = sc.r13;
  st->r14 = sc.r14;
  st->r15 = sc.r15;
  st->rdi = sc.rdi;
  st->rsi = sc.rsi;
  st->rbp = sc.rbp;
  st->rbx = sc.rbx;
  st->rdx = sc.rdx;
  st->rax = sc.rax;
  st->rcx = sc.rcx;
  st->rsp = sc.rsp;
  st->ip = sc.rip;
  st->flags = (st->flags & ~kFixEflags) | (sc.eflags & kFixEflags);
  t->restart_kind = kRestartNone;  // restart_block.fn = do_no_restart_syscall

  if (sc.fpstate != 0) {
    const bool xs = XstateUsesXsave();
    std::vector<uint8_t> buf(xs ? XstateSize() : 512);
    bool full = false;
    if (!CopyFromUser(p, buf.data(), sc.fpstate, 512)) {
      GroupExit(p, kSIGSEGV);
      return 0;
    }
    if (xs && (f.uc.uc_flags & kUC_FP_XSTATE) != 0) {
      FpxSwBytes sw;
      memcpy(&sw, buf.data() + 464, sizeof(sw));
      if (sw.magic1 == kFP_XSTATE_MAGIC1 && sw.xstate_size >= buf.size() &&
          CopyFromUser(p, buf.data(), sc.fpstate, buf.size())) {
        full = true;
      }
    }
    XstateLoadFromUser(t->xstate, buf.data(), full ? buf.size() : 512, full);
  } else {
    XstateReset(t->xstate);
  }

  t->sigmask.store(f.uc.uc_sigmask & ~kUnblockableSignals);
  // restore_altstack(): errors (e.g. EPERM while on the stack) are ignored.
  DoSigaltstack(t, &f.uc.uc_stack, nullptr, st->rsp);
  return static_cast<int64_t>(st->rax);
#elif defined(__aarch64__)
  const uint64_t frame = st->sp;
  Arm64RtSigframe f;
  if (!CopyFromUser(p, &f, frame, sizeof(f))) {
    GroupExit(p, kSIGSEGV);
    return 0;
  }
  const Arm64Sigcontext& sc = f.uc.uc_mcontext;
  for (int i = 0; i < 31; ++i) st->x[i] = sc.regs[i];
  st->sp = sc.sp;
  st->pc = sc.pc;
  st->cpsr = static_cast<uint32_t>(sc.pstate);
  t->restart_kind = kRestartNone;

  const fpsimd_context* fpsimd = reinterpret_cast<const fpsimd_context*>(sc.__reserved);
  if (fpsimd->head.magic == kFPSIMD_MAGIC && t->xstate != nullptr) {
    memcpy(t->xstate, fpsimd->vregs, sizeof(fpsimd->vregs));
  } else {
    XstateReset(t->xstate);
  }

  t->sigmask.store(f.uc.uc_sigmask & ~kUnblockableSignals);
  DoSigaltstack(t, &f.uc.uc_stack, nullptr, st->sp);
  return static_cast<int64_t>(st->x[0]);
#endif
}

// Sleeps until any unblocked signal is pending (pause, rt_sigsuspend).
void WaitForSignal(LinuxProcess* p, LinuxThread* t) {
  for (;;) {
    ThreadClearWake(t);
    if (ShimSignalPending(p, t)) return;
    ThreadWaitWake(t, ZX_TIME_INFINITE);
  }
}

int64_t SysKill(LinuxProcess* p, int pid, int sig) {
  if (sig < 0 || sig > kNSIG) return kEINVAL;
  const LinuxSiginfo info = MakeSiginfoKill(sig, kSI_USER, p->pid, p->ruid);
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  if (pid > 0) {
    auto target = FindProcessLocked(pid);
    if (!target) return kESRCH;
    return sig == 0 ? 0 : DoSendSignalLocked(target.get(), nullptr, info, false);
  }
  std::vector<std::shared_ptr<LinuxProcess>> targets;
  for (auto& kv : g_processes) {
    LinuxProcess* q = kv.second.get();
    if (q->reaped) continue;
    if (pid == 0 && q->pgid == p->pgid) targets.push_back(kv.second);
    if (pid == -1 && q->pid > 1 && q != p) targets.push_back(kv.second);
    if (pid < -1 && q->pgid == -pid) targets.push_back(kv.second);
  }
  if (targets.empty()) return kESRCH;
  if (sig == 0) return 0;
  for (auto& q : targets) DoSendSignalLocked(q.get(), nullptr, info, false);
  return 0;
}

int64_t SysTgkill(LinuxProcess* p, int tgid, int tid, int sig) {
  if (tid <= 0 || (tgid != -1 && tgid <= 0)) return kEINVAL;
  if (sig < 0 || sig > kNSIG) return kEINVAL;
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  LinuxProcess* tp = nullptr;
  LinuxThread* tt = FindThreadLocked(tid, &tp);
  if (tt == nullptr || (tgid != -1 && tp->pid != tgid)) return kESRCH;
  if (sig == 0) return 0;
  return DoSendSignalLocked(tp, tt, MakeSiginfoKill(sig, kSI_TKILL, p->pid, p->ruid), false);
}

}  // namespace

// --- siginfo construction ---

LinuxSiginfo MakeSiginfo(int sig, int code) {
  LinuxSiginfo i;
  i.si_signo = sig;
  i.si_code = code;
  return i;
}

LinuxSiginfo MakeSiginfoKill(int sig, int code, int pid, uint32_t uid) {
  LinuxSiginfo i = MakeSiginfo(sig, code);
  PutI32(&i, 16, pid);
  PutI32(&i, 20, static_cast<int32_t>(uid));
  return i;
}

LinuxSiginfo MakeSiginfoChld(int code, int pid, int status, int64_t cpu_ns) {
  LinuxSiginfo i = MakeSiginfo(kSIGCHLD, code);
  PutI32(&i, 16, pid);
  PutI32(&i, 20, 0);
  PutI32(&i, 24, status);
  // si_utime/si_stime in clock ticks (USER_HZ=100). Zircon reports total CPU
  // time only, so all of it is attributed to user time.
  PutU64(&i, 32, cpu_ns > 0 ? static_cast<uint64_t>(cpu_ns / 10000000) : 0);
  PutU64(&i, 40, 0);
  return i;
}

LinuxSiginfo MakeSiginfoFault(int sig, int code, uint64_t addr) {
  LinuxSiginfo i = MakeSiginfo(sig, code);
  PutU64(&i, 16, addr);
  return i;
}

LinuxSiginfo MakeSiginfoTimer(int sig, int timer_id, int overrun, uint64_t sigval) {
  LinuxSiginfo i = MakeSiginfo(sig, kSI_TIMER);
  PutI32(&i, 16, timer_id);
  PutI32(&i, 20, overrun);
  PutU64(&i, 24, sigval);
  return i;
}

// --- generation ---

int64_t SendSignalToProcess(LinuxProcess* p, const LinuxSiginfo& info) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  return DoSendSignalLocked(p, nullptr, info, false);
}

int64_t SendSignalToThread(LinuxProcess* p, LinuxThread* t, const LinuxSiginfo& info) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  return DoSendSignalLocked(p, t, info, false);
}

void ForceFaultSignal(LinuxProcess* p, LinuxThread* t, const LinuxSiginfo& info) {
  const int sig = info.si_signo;
  bool kill_now = false;
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    {
      std::lock_guard<std::mutex> sl(p->sig_mtx);
      LinuxSigaction& act = p->sigactions[sig];
      const bool blocked = (t->sigmask.load() & SigBit(sig)) != 0;
      if (blocked || act.sa_handler == kSIG_IGN) {
        act.sa_handler = kSIG_DFL;
        if (blocked) t->sigmask.fetch_and(~SigBit(sig));
      }
      kill_now = act.sa_handler == kSIG_DFL;
    }
    if (!kill_now) DoSendSignalLocked(p, t, info, true);
  }
  // All fault signals default to "terminate + core". No core file is written,
  // so the wait status carries only the signal number (no 0x80 flag).
  if (kill_now) GroupExit(p, sig);
}

void GroupExit(LinuxProcess* p, int wait_status) {
  {
    std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
    {
      std::lock_guard<std::mutex> sl(p->sig_mtx);
      if (p->exited.load()) return;
      p->exit_status = wait_status;
      p->exited.store(true);
      p->stopped = false;
      p->pending_signals.fetch_or(SigBit(kSIGKILL));
      p->stop_cv.notify_all();
    }
    for (const auto& t : p->threads) {
      t->kill_requested.store(true);
      t->pending_signals.fetch_or(SigBit(kSIGKILL));
      WakeThread(t.get());
    }
  }
  NotifyPollWaiters();
  g_wait4_cv.notify_all();
}

void WakeThread(LinuxThread* t) {
  std::lock_guard<std::mutex> wl(t->wake_mtx);
  if (t->wake_event != ZX_HANDLE_INVALID) {
    zx_object_signal(t->wake_event, 0, ZX_USER_SIGNAL_0);
  }
  if (t->zx_thread != ZX_HANDLE_INVALID) {
    zx_restricted_kick(t->zx_thread, 0);
  }
}

void WakeProcessThreads(LinuxProcess* p) {
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  for (const auto& t : p->threads) WakeThread(t.get());
}

void ThreadClearWake(LinuxThread* t) {
  std::lock_guard<std::mutex> wl(t->wake_mtx);
  if (t->wake_event != ZX_HANDLE_INVALID) {
    zx_object_signal(t->wake_event, ZX_USER_SIGNAL_0, 0);
  }
}

zx_status_t ThreadWaitWake(LinuxThread* t, zx_time_t deadline) {
  // Only the owning thread waits on (and eventually closes) its event.
  zx_handle_t ev = t->wake_event;
  if (ev == ZX_HANDLE_INVALID) return zx_nanosleep(deadline) == ZX_OK ? ZX_ERR_TIMED_OUT : ZX_ERR_BAD_STATE;
  return zx_object_wait_one(ev, ZX_USER_SIGNAL_0, deadline, nullptr);
}

// --- public helpers (shim_common.h) ---

void QueueSignalToPid(int pid, int sig) {
  if (sig <= 0 || sig > kNSIG) return;
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  auto p = FindProcessLocked(pid);
  if (p) DoSendSignalLocked(p.get(), nullptr, MakeSiginfo(sig, kSI_KERNEL), false);
}

void QueueSignalToProcessGroup(int pgid, int sig) {
  if (sig <= 0 || sig > kNSIG || pgid <= 0) return;
  std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
  std::vector<std::shared_ptr<LinuxProcess>> targets;
  for (auto& kv : g_processes) {
    if (kv.second->pgid == pgid && !kv.second->reaped) targets.push_back(kv.second);
  }
  for (auto& p : targets) DoSendSignalLocked(p.get(), nullptr, MakeSiginfo(sig, kSI_KERNEL), false);
}

void QueueSignalToThread(LinuxProcess* proc, LinuxThread* thrd, int sig) {
  if (sig <= 0 || sig > kNSIG) return;
  SendSignalToThread(proc, thrd, MakeSiginfoKill(sig, kSI_USER, proc->pid, proc->ruid));
}

bool ShimSignalPending(LinuxProcess* proc, LinuxThread* thrd) {
  if (proc->exited.load() || thrd->kill_requested.load()) return true;
  const uint64_t pend = proc->pending_signals.load() | thrd->pending_signals.load();
  return (pend & ~thrd->sigmask.load()) != 0;
}

uint64_t ShimPendingSignalSet(LinuxProcess* proc, LinuxThread* thrd) {
  return proc->pending_signals.load() | thrd->pending_signals.load();
}

int ShimDequeueSignal(LinuxProcess* proc, LinuxThread* thrd, uint64_t mask, LinuxSiginfo* out) {
  return DequeueSignal(proc, thrd, mask, out);
}

// --- user memory ---

namespace {

bool UserRangeReadableFast(LinuxProcess* p, uintptr_t addr, size_t len) {
  std::lock_guard<std::mutex> l(p->map_mtx);
  const auto& regions = MmLocked(p).regions;
  const uintptr_t end = addr + len;
  uintptr_t cur = addr;
  auto it = regions.upper_bound(cur);
  if (it != regions.begin()) --it;
  for (; it != regions.end() && cur < end; ++it) {
    const uintptr_t start = it->first;
    const uintptr_t rend = start + it->second.len;
    if (rend <= cur) continue;
    // ProtToVmPerms grants ZX_VM_PERM_READ whenever any PROT_{READ,WRITE,EXEC} bit is set.
    if (start > cur ||
        (it->second.prot & (kLinuxProtRead | kLinuxProtWrite | kLinuxProtExec)) == 0) {
      return false;
    }
    cur = rend;
  }
  return cur >= end;
}

}  // namespace

bool CopyFromUser(LinuxProcess* p, void* dst, uintptr_t src, size_t len) {
  if (len == 0) return true;
  if (src + len < src || src < p->vmar_base || src + len > p->vmar_base + p->vmar_len) {
    return false;
  }
  if (t_current_thread != nullptr && t_current_thread->proc == p &&
      UserRangeReadableFast(p, src, len)) {
    memcpy(dst, reinterpret_cast<const void*>(src), len);
    return true;
  }
  size_t done = 0;
  while (done < len) {
    size_t actual = 0;
    zx_status_t st = zx_process_read_memory(p->zx_proc, src + done,
                                            static_cast<uint8_t*>(dst) + done, len - done, &actual);
    if (st != ZX_OK || actual == 0) return false;
    done += actual;
  }
  return true;
}

namespace {

// Asks the kernel which mappings of the restricted VMAR are writable.
bool KernelRangeWritable(LinuxProcess* p, uintptr_t addr, size_t len) {
  std::vector<zx_info_maps_t> maps(128);
  size_t actual = 0, avail = 0;
  zx_handle_t h = p->restricted_vmar;
  uint32_t topic = ZX_INFO_VMAR_MAPS;
  for (int attempt = 0; attempt < 4; ++attempt) {
    zx_status_t st = zx_object_get_info(h, topic, maps.data(), maps.size() * sizeof(zx_info_maps_t),
                                        &actual, &avail);
    if (st != ZX_OK && topic == ZX_INFO_VMAR_MAPS) {
      h = p->zx_proc;
      topic = ZX_INFO_PROCESS_MAPS;
      continue;
    }
    if (st != ZX_OK) return false;
    if (avail <= maps.size()) break;
    maps.resize(avail + 32);
  }
  std::vector<std::pair<uintptr_t, uintptr_t>> ranges;
  const uintptr_t end = addr + len;
  for (size_t i = 0; i < actual && i < maps.size(); ++i) {
    const zx_info_maps_t& m = maps[i];
    if (m.type != ZX_INFO_MAPS_TYPE_MAPPING) continue;
    if ((m.u.mapping.mmu_flags & ZX_VM_PERM_WRITE) == 0) continue;
    if (m.base + m.size <= addr || m.base >= end) continue;
    ranges.emplace_back(m.base, m.base + m.size);
  }
  std::sort(ranges.begin(), ranges.end());
  uintptr_t cur = addr;
  for (const auto& r : ranges) {
    if (r.first > cur) return false;
    cur = std::max(cur, r.second);
    if (cur >= end) return true;
  }
  return cur >= end;
}

}  // namespace

bool UserRangeWritable(LinuxProcess* p, uintptr_t addr, size_t len) {
  if (len == 0) return true;
  if (addr + len < addr || addr < p->vmar_base || addr + len > p->vmar_base + p->vmar_len) {
    return false;
  }
  // Fast path: the shim's mapping table (mem_space.h) with Linux protections.
  {
    std::lock_guard<std::mutex> l(p->map_mtx);
    const auto& regions = MmLocked(p).regions;
    const uintptr_t end = addr + len;
    uintptr_t cur = addr;
    auto it = regions.upper_bound(cur);
    if (it != regions.begin()) --it;
    for (; it != regions.end() && cur < end; ++it) {
      const uintptr_t start = it->first;
      const uintptr_t rend = start + it->second.len;
      if (rend <= cur) continue;
      if (start > cur || (it->second.prot & kLinuxProtWrite) == 0) break;
      cur = rend;
    }
    if (cur >= end) return true;
  }
  // Not covered by tracked writable regions: let the kernel decide.
  return KernelRangeWritable(p, addr, len);
}

bool CopyToUser(LinuxProcess* p, uintptr_t dst, const void* src, size_t len) {
  if (!UserRangeWritable(p, dst, len)) return false;
  memcpy(reinterpret_cast<void*>(dst), src, len);
  return true;
}

// --- return to user mode ---

void HandleSignalsOnReturn(LinuxProcess* p, LinuxThread* t, bool from_syscall, int64_t sysret,
                           uint64_t orig_sc) {
  bool pending_restart = from_syscall && IsRestartCode(sysret);
  for (;;) {
    for (;;) {
      if (p->exited.load() || t->kill_requested.load()) return;
      WaitWhileStopped(p, t);
      if (p->exited.load() || t->kill_requested.load()) return;

      LinuxSiginfo info;
      const int sig = DequeueSignal(p, t, ~t->sigmask.load(), &info);
      if (sig == 0) break;

      LinuxSigaction act;
      {
        std::lock_guard<std::mutex> sl(p->sig_mtx);
        act = p->sigactions[sig];
        if (act.sa_handler > kSIG_IGN && (act.sa_flags & kSA_RESETHAND) != 0) {
          p->sigactions[sig].sa_handler = kSIG_DFL;
        }
      }
      if (act.sa_handler == kSIG_IGN) continue;
      if (act.sa_handler == kSIG_DFL) {
        const DefAct d = DefaultAction(sig);
        if (d == DefAct::kIgnore || d == DefAct::kCont) continue;
        // Global init ignores default actions (SIGNAL_UNKILLABLE).
        if (p->pid == 1 && sig != kSIGKILL && sig != kSIGSTOP) continue;
        if (d == DefAct::kStop) {
          DoGroupStop(p, sig);
          continue;
        }
        // Terminate (no core file is written, so no core-dump flag).
        GroupExit(p, sig);
        return;
      }

      if (pending_restart) {
        ApplyRestart(t, sysret, true, act.sa_flags, orig_sc);
        pending_restart = false;
      }
      if (!SetupRtFrame(p, t, sig, info, act)) {
#if defined(__x86_64__)
        ShimLogf("[signal] PID %d TID %d: cannot set up frame for signal %d (sp=0x%llx) -> SIGSEGV\n",
                 p->pid, t->tid, sig, static_cast<unsigned long long>(t->state->rsp));
#elif defined(__aarch64__)
        ShimLogf("[signal] PID %d TID %d: cannot set up frame for signal %d (sp=0x%llx) -> SIGSEGV\n",
                 p->pid, t->tid, sig, static_cast<unsigned long long>(t->state->sp));
#endif
        GroupExit(p, kSIGSEGV);
        return;
      }
      uint64_t blocked = t->sigmask.load() | act.sa_mask;
      if ((act.sa_flags & kSA_NODEFER) == 0) blocked |= SigBit(sig);
      t->sigmask.store(blocked & ~kUnblockableSignals);
      t->restore_sigmask = false;
    }
    if (pending_restart) {
      ApplyRestart(t, sysret, false, 0, orig_sc);
      pending_restart = false;
    }
    if (!t->restore_sigmask) return;
    // Restore the mask a sigsuspend/ppoll-style syscall replaced, then check
    // again for signals it unblocks (Linux restore_saved_sigmask()).
    t->sigmask.store(t->saved_sigmask & ~kUnblockableSignals);
    t->restore_sigmask = false;
  }
}

// --- syscalls ---

int64_t HandleSignalSyscall(LinuxProcess* p, LinuxThread* t, uint64_t sc, uint64_t a0,
                            uint64_t a1, uint64_t a2, uint64_t a3, bool* handled) {
  *handled = true;
  switch (sc) {
    case 13: {  // rt_sigaction(sig, act, oact, sigsetsize)
      const int sig = static_cast<int>(a0);
      if (a3 != 8) return kEINVAL;
      if (sig < 1 || sig > kNSIG) return kEINVAL;
      LinuxSigaction newact{};
      if (a1 != 0) {
        if (sig == kSIGKILL || sig == kSIGSTOP) return kEINVAL;
        if (!CopyFromUser(p, &newact, a1, sizeof(newact))) return kEFAULT;
        newact.sa_mask &= ~kUnblockableSignals;
      }
      LinuxSigaction old;
      {
        std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
        std::lock_guard<std::mutex> sl(p->sig_mtx);
        old = p->sigactions[sig];
        if (a1 != 0) {
          p->sigactions[sig] = newact;
          // POSIX: setting a pending signal to be ignored discards it.
          const bool ign = newact.sa_handler == kSIG_IGN ||
                           (newact.sa_handler == kSIG_DFL &&
                            (DefaultAction(sig) == DefAct::kIgnore ||
                             DefaultAction(sig) == DefAct::kCont));
          if (ign) FlushSignalsLocked(p, SigBit(sig));
        }
      }
      if (a2 != 0 && !CopyToUser(p, a2, &old, sizeof(old))) return kEFAULT;
      return 0;
    }
    case 14: {  // rt_sigprocmask(how, set, oset, sigsetsize)
      const int how = static_cast<int>(a0);
      if (a3 != 8) return kEINVAL;
      uint64_t set = 0;
      if (a1 != 0) {
        if (how < 0 || how > 2) return kEINVAL;
        if (!CopyFromUser(p, &set, a1, sizeof(set))) return kEFAULT;
      }
      const uint64_t old = t->sigmask.load();
      if (a2 != 0 && !CopyToUser(p, a2, &old, sizeof(old))) return kEFAULT;
      if (a1 != 0) {
        set &= ~kUnblockableSignals;
        if (how == 0) t->sigmask.fetch_or(set);          // SIG_BLOCK
        else if (how == 1) t->sigmask.fetch_and(~set);   // SIG_UNBLOCK
        else t->sigmask.store(set);                      // SIG_SETMASK
      }
      return 0;
    }
    case 15:  // rt_sigreturn
      return SysRtSigreturn(p, t);
    case 127: {  // rt_sigpending(set, sigsetsize)
      if (a1 > 8) return kEINVAL;
      const uint64_t pend = ShimPendingSignalSet(p, t) & t->sigmask.load();
      if (!CopyToUser(p, a0, &pend, static_cast<size_t>(a1))) return kEFAULT;
      return 0;
    }
    case 128: {  // rt_sigtimedwait(set, info, timeout, sigsetsize)
      if (a3 != 8) return kEINVAL;
      uint64_t set = 0;
      if (!CopyFromUser(p, &set, a0, sizeof(set))) return kEFAULT;
      set &= ~kUnblockableSignals;
      zx_time_t deadline = ZX_TIME_INFINITE;
      if (a2 != 0) {
        int64_t ts[2];
        if (!CopyFromUser(p, ts, a2, sizeof(ts))) return kEFAULT;
        if (ts[0] < 0 || ts[1] < 0 || ts[1] >= 1000000000LL) return kEINVAL;
        deadline = zx_deadline_after(ts[0] * 1000000000LL + ts[1]);
      }
      for (;;) {
        ThreadClearWake(t);
        LinuxSiginfo info;
        const int sig = DequeueSignal(p, t, set, &info);
        if (sig != 0) {
          if (a1 != 0 && !CopyToUser(p, a1, &info, sizeof(info))) return kEFAULT;
          return sig;
        }
        if (ShimSignalPending(p, t)) return kEINTR;
        if (zx_clock_get_monotonic() >= deadline) return kEAGAIN;
        ThreadWaitWake(t, deadline);
      }
    }
    case 129:    // rt_sigqueueinfo(tgid, sig, uinfo)
    case 297: {  // rt_tgsigqueueinfo(tgid, tid, sig, uinfo)
      const int tgid = static_cast<int>(a0);
      const int sig = static_cast<int>(sc == 129 ? a1 : a2);
      const uintptr_t uinfo = static_cast<uintptr_t>(sc == 129 ? a2 : a3);
      LinuxSiginfo info;
      if (!CopyFromUser(p, &info, uinfo, sizeof(info))) return kEFAULT;
      if (sig < 1 || sig > kNSIG) return kEINVAL;
      // Only the kernel (or the caller to itself) may claim SI_USER/SI_KERNEL.
      if ((info.si_code >= 0 || info.si_code == kSI_TKILL) && tgid != p->pid) return kEPERM;
      info.si_signo = sig;
      std::lock_guard<std::recursive_mutex> pl(g_proc_mtx);
      if (sc == 129) {
        auto target = FindProcessLocked(tgid);
        if (!target) return kESRCH;
        return DoSendSignalLocked(target.get(), nullptr, info, false);
      }
      LinuxProcess* tp = nullptr;
      LinuxThread* tt = FindThreadLocked(static_cast<int>(a1), &tp);
      if (tt == nullptr || tp->pid != tgid) return kESRCH;
      return DoSendSignalLocked(tp, tt, info, false);
    }
    case 130: {  // rt_sigsuspend(mask, sigsetsize)
      if (a1 != 8) return kEINVAL;
      uint64_t mask = 0;
      if (!CopyFromUser(p, &mask, a0, sizeof(mask))) return kEFAULT;
      t->saved_sigmask = t->sigmask.load();
      t->restore_sigmask = true;
      t->sigmask.store(mask & ~kUnblockableSignals);
      WaitForSignal(p, t);
      return kERESTARTNOHAND;
    }
    case 34:  // pause
      WaitForSignal(p, t);
      return kERESTARTNOHAND;
    case 131: {  // sigaltstack(ss, oss)
      KStack ss{}, oss{};
      if (a0 != 0 && !CopyFromUser(p, &ss, a0, sizeof(ss))) return kEFAULT;
#if defined(__x86_64__)
      const int64_t r = DoSigaltstack(t, a0 != 0 ? &ss : nullptr, &oss, t->state->rsp);
#elif defined(__aarch64__)
      const int64_t r = DoSigaltstack(t, a0 != 0 ? &ss : nullptr, &oss, t->state->sp);
#endif
      if (r != 0) return r;
      if (a1 != 0 && !CopyToUser(p, a1, &oss, sizeof(oss))) return kEFAULT;
      return 0;
    }
    case 62:  // kill(pid, sig)
      return SysKill(p, static_cast<int>(a0), static_cast<int>(a1));
    case 200:  // tkill(tid, sig)
      return SysTgkill(p, -1, static_cast<int>(a0), static_cast<int>(a1));
    case 234:  // tgkill(tgid, tid, sig)
      return SysTgkill(p, static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2));
    default:
      *handled = false;
      return kENOSYS;
  }
}
