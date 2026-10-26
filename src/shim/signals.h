// Internal interfaces shared by restricted_exec.cc, signals.cc, futex.cc,
// time.cc and syscalls_proc.cc. Public helpers used by other shim files are
// declared in shim_common.h.
#ifndef ZIRCON_EMBEDDED_SRC_SHIM_SIGNALS_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_SIGNALS_H_

#include "src/shim/shim_common.h"

// Linux signal numbers (x86_64).
constexpr int kSIGHUP = 1;
constexpr int kSIGINT = 2;
constexpr int kSIGQUIT = 3;
constexpr int kSIGILL = 4;
constexpr int kSIGTRAP = 5;
constexpr int kSIGABRT = 6;
constexpr int kSIGBUS = 7;
constexpr int kSIGFPE = 8;
constexpr int kSIGKILL = 9;
constexpr int kSIGUSR1 = 10;
constexpr int kSIGSEGV = 11;
constexpr int kSIGUSR2 = 12;
constexpr int kSIGPIPE = 13;
constexpr int kSIGALRM = 14;
constexpr int kSIGTERM = 15;
constexpr int kSIGSTKFLT = 16;
constexpr int kSIGCHLD = 17;
constexpr int kSIGCONT = 18;
constexpr int kSIGSTOP = 19;
constexpr int kSIGTSTP = 20;
constexpr int kSIGTTIN = 21;
constexpr int kSIGTTOU = 22;
constexpr int kSIGURG = 23;
constexpr int kSIGXCPU = 24;
constexpr int kSIGXFSZ = 25;
constexpr int kSIGVTALRM = 26;
constexpr int kSIGPROF = 27;
constexpr int kSIGWINCH = 28;
constexpr int kSIGIO = 29;
constexpr int kSIGPWR = 30;
constexpr int kSIGSYS = 31;
constexpr int kSIGRTMIN = 32;
constexpr int kNSIG = 64;

constexpr uint64_t SigBit(int sig) { return 1ULL << (sig - 1); }
constexpr uint64_t kUnblockableSignals = SigBit(kSIGKILL) | SigBit(kSIGSTOP);
constexpr uint64_t kStopSignals = SigBit(kSIGSTOP) | SigBit(kSIGTSTP) |
                                  SigBit(kSIGTTIN) | SigBit(kSIGTTOU);

// si_code values.
constexpr int kSI_USER = 0;
constexpr int kSI_KERNEL = 0x80;
constexpr int kSI_QUEUE = -1;
constexpr int kSI_TIMER = -2;
constexpr int kSI_TKILL = -6;
constexpr int kCLD_EXITED = 1;
constexpr int kCLD_KILLED = 2;
constexpr int kCLD_DUMPED = 3;
constexpr int kCLD_STOPPED = 5;
constexpr int kCLD_CONTINUED = 6;

// sigaction flags.
constexpr uint64_t kSA_NOCLDSTOP = 0x00000001ULL;
constexpr uint64_t kSA_NOCLDWAIT = 0x00000002ULL;
constexpr uint64_t kSA_SIGINFO = 0x00000004ULL;
constexpr uint64_t kSA_RESTORER = 0x04000000ULL;
constexpr uint64_t kSA_ONSTACK = 0x08000000ULL;
constexpr uint64_t kSA_RESTART = 0x10000000ULL;
constexpr uint64_t kSA_NODEFER = 0x40000000ULL;
constexpr uint64_t kSA_RESETHAND = 0x80000000ULL;
constexpr uint64_t kSIG_DFL = 0;
constexpr uint64_t kSIG_IGN = 1;

// waitpid report kinds stored in LinuxProcess::wait_report.
constexpr int kWaitReportNone = 0;
constexpr int kWaitReportStopped = 1;
constexpr int kWaitReportContinued = 2;

// restart_syscall(2) kinds stored in LinuxThread::restart_kind.
constexpr int kRestartNone = 0;
constexpr int kRestartNanosleep = 1;  // args: deadline_mono, rem_ptr
constexpr int kRestartFutexWait = 2;  // args: uaddr, val, bitset, deadline_mono, op

// --- siginfo construction (signals.cc) ---
LinuxSiginfo MakeSiginfo(int sig, int code);
LinuxSiginfo MakeSiginfoKill(int sig, int code, int pid, uint32_t uid);
LinuxSiginfo MakeSiginfoChld(int code, int pid, int status, int64_t cpu_ns);
LinuxSiginfo MakeSiginfoFault(int sig, int code, uint64_t addr);
LinuxSiginfo MakeSiginfoTimer(int sig, int timer_id, int overrun,
                              uint64_t sigval);

// --- Signal generation (signals.cc) ---
// Process-directed signal. Takes g_proc_mtx. Returns 0 or a negative errno.
int64_t SendSignalToProcess(LinuxProcess* p, const LinuxSiginfo& info);
// Thread-directed signal (tkill/tgkill, SIGPIPE, timers with SIGEV_THREAD_ID).
int64_t SendSignalToThread(LinuxProcess* p, LinuxThread* t,
                           const LinuxSiginfo& info);
// Synchronous fault signal raised by the current thread (exceptions). If the
// signal is blocked or ignored it is unblocked and reset to SIG_DFL (Linux
// force_sig_fault semantics).
void ForceFaultSignal(LinuxProcess* p, LinuxThread* t, const LinuxSiginfo& info);
// Terminates every thread of p; `wait_status` is the wait(2) status word.
void GroupExit(LinuxProcess* p, int wait_status);
// Wakes thrd from interruptible waits and kicks it out of restricted mode.
void WakeThread(LinuxThread* t);
// Wakes all threads of p (child state changes, de-threading on execve).
void WakeProcessThreads(LinuxProcess* p);

// --- Return-to-user processing (signals.cc) ---
// Handles job-control stops and delivers pending signals before the thread
// re-enters restricted mode. If from_syscall, `sysret` is the value the
// syscall returned (already stored in rax) and `orig_sc` the syscall number;
// restart codes are resolved here.
void HandleSignalsOnReturn(LinuxProcess* p, LinuxThread* t, bool from_syscall,
                           int64_t sysret, uint64_t orig_sc);
// Signal syscalls (rt_sigaction, rt_sigprocmask, rt_sigreturn, sigaltstack,
// kill, tkill, tgkill, rt_sigqueueinfo, rt_tgsigqueueinfo, rt_sigpending,
// rt_sigsuspend, pause, rt_sigtimedwait). Sets *handled.
int64_t HandleSignalSyscall(LinuxProcess* p, LinuxThread* t, uint64_t sc,
                            uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                            bool* handled);

// --- Interruptible waits (signals.cc) ---
// Clears the thread's wake event. Call before checking a wait condition.
void ThreadClearWake(LinuxThread* t);
// Blocks until the wake event is signaled or `deadline` (ZX_CLOCK_MONOTONIC)
// passes. Returns ZX_ERR_TIMED_OUT on deadline.
zx_status_t ThreadWaitWake(LinuxThread* t, zx_time_t deadline);

// --- User memory access ---
// Safe copy from the Linux process address space (zx_process_read_memory).
bool CopyFromUser(LinuxProcess* p, void* dst, uintptr_t src, size_t len);
// True if [addr, addr+len) is entirely covered by writable mappings.
bool UserRangeWritable(LinuxProcess* p, uintptr_t addr, size_t len);
// Validated copy into the Linux process (current process only).
bool CopyToUser(LinuxProcess* p, uintptr_t dst, const void* src, size_t len);

// --- Extended (FPU/vector) register state (restricted_exec.cc) ---
size_t XstateSize();        // bytes in a LinuxThread::xstate buffer
bool XstateUsesXsave();     // true: XSAVE format, false: FXSAVE format
uint64_t XstateFeatures();  // XCR0 when XSAVE is used
void* XstateAlloc();        // zeroed + initialized to the reset state
void XstateFree(void* p);
void XstateReset(void* p);  // reset state (as after execve / handler entry)
// Copies a user-supplied fpstate into the thread's buffer, sanitizing fields
// that would make XRSTOR/FXRSTOR fault. `full` selects XSAVE vs legacy only.
void XstateLoadFromUser(void* dst, const uint8_t* src, size_t len, bool full);

// --- Process lifecycle (syscalls_proc.cc) ---
// Called by the last exiting thread of a process.
void OnLastThreadExit(LinuxProcess* p);
// Applies the process nice value / thread CPU affinity to a starting thread.
void ApplyThreadSchedProfile(LinuxProcess* p, LinuxThread* t);
// Drops per-thread scheduling bookkeeping for an exiting thread.
void ForgetThreadSchedState(LinuxThread* t);

// --- Futex (futex.cc) ---
// Walks the thread's robust futex list at exit (Linux exit_robust_list).
void FutexExitRobustList(LinuxProcess* p, LinuxThread* t);
// Continues an interrupted FUTEX_WAIT via restart_syscall(2).
int64_t FutexRestart(LinuxProcess* p, LinuxThread* t);

// --- Time (time.cc) ---
void TimeInitFromRtc();  // main.cc, before PID 1 starts
int64_t HandleTimeSyscall(LinuxProcess* p, LinuxThread* t, uint64_t sc,
                          uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                          bool* handled);
// Converts a Linux clock id + absolute time to a ZX_CLOCK_MONOTONIC deadline.
// Returns false for clocks that cannot be waited on.
bool ClockDeadlineToMonotonic(int clock_id, int64_t abs_ns, zx_time_t* out);
void TimeProcessExit(LinuxProcess* p);   // cancels itimers and POSIX timers
void TimeProcessExec(LinuxProcess* p);   // deletes POSIX timers
// A SI_TIMER signal was dequeued; returns its overrun count.
int TimeSignalDelivered(int pid, int timer_id);
// Per-thread CPU time from ZX_INFO_TASK_RUNTIME (ns), or -1.
int64_t ThreadCpuTimeNs(LinuxThread* t);
int64_t ProcessCpuTimeNs(LinuxProcess* p);

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_SIGNALS_H_
