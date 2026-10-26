// Linux-semantics checks for the shim's signal, process, futex and time
// syscalls. Built as a static Linux binary. Prints one line per check and a
// summary computed from the counters: "[PROC_TEST: PASS n/n]" if every check
// passed, otherwise "[PROC_TEST: FAIL k/n] <failed check names>"; the exit
// status is nonzero on failure.
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;
std::string g_failed_names;

void Check(bool ok, const char* what) {
  g_checks++;
  if (ok) {
    printf("[proc_test] ok   %s\n", what);
  } else {
    printf("[proc_test] FAIL %s (errno=%d)\n", what, errno);
    g_failures++;
    if (!g_failed_names.empty()) g_failed_names += "; ";
    g_failed_names += what;
  }
  fflush(stdout);
}

volatile sig_atomic_t g_count = 0;
volatile sig_atomic_t g_last_code = 0;
volatile sig_atomic_t g_last_signo = 0;
volatile pid_t g_handler_tid = 0;
volatile uintptr_t g_last_addr = 0;
volatile uint64_t g_sigval = 0;
volatile uintptr_t g_handler_sp = 0;
sigjmp_buf g_jmp;

void InfoHandler(int sig, siginfo_t* si, void* uc) {
  g_count = g_count + 1;
  g_last_signo = sig;
  g_last_code = si->si_code;
  g_last_addr = reinterpret_cast<uintptr_t>(si->si_addr);
  g_sigval = reinterpret_cast<uint64_t>(si->si_value.sival_ptr);
  g_handler_tid = static_cast<pid_t>(syscall(SYS_gettid));
  int local = 0;
  g_handler_sp = reinterpret_cast<uintptr_t>(&local);
  (void)uc;
}

void JumpHandler(int sig, siginfo_t* si, void*) {
  g_last_signo = sig;
  g_last_code = si->si_code;
  g_last_addr = reinterpret_cast<uintptr_t>(si->si_addr);
  siglongjmp(g_jmp, 1);
}

void Install(int sig, void (*h)(int, siginfo_t*, void*), int flags) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = h;
  sa.sa_flags = SA_SIGINFO | flags;
  sigemptyset(&sa.sa_mask);
  sigaction(sig, &sa, nullptr);
}

void Reset(int sig) { signal(sig, SIG_DFL); }

void TestHandlerAndFpu() {
  Install(SIGUSR1, InfoHandler, 0);
  g_count = 0;
  volatile double x = 1.5;
  double acc = 0;
  for (int i = 0; i < 1000; ++i) {
    acc += x * i;
    if (i == 500) raise(SIGUSR1);
  }
  Check(g_count == 1 && g_last_signo == SIGUSR1 && g_last_code == SI_TKILL,
        "SA_SIGINFO handler runs with si_code SI_TKILL");
  Check(acc == 1.5 * 999 * 1000 / 2, "FPU state preserved across handler");
  Reset(SIGUSR1);
}

void NoDeferHandler(int, siginfo_t*, void*) {
  g_count = g_count + 1;
  if (g_count == 1) raise(SIGUSR2);
}

void TestNodeferAndMask() {
  g_count = 0;
  Install(SIGUSR2, NoDeferHandler, SA_NODEFER);
  raise(SIGUSR2);
  Check(g_count == 2, "SA_NODEFER allows nested delivery");
  g_count = 0;
  Install(SIGUSR2, NoDeferHandler, 0);
  raise(SIGUSR2);
  Check(g_count == 2, "deferred signal delivered after handler returns");
  sigset_t cur;
  sigprocmask(SIG_SETMASK, nullptr, &cur);
  Check(!sigismember(&cur, SIGUSR2), "rt_sigreturn restores the signal mask");
  Reset(SIGUSR2);
}

void TestFaults() {
  Install(SIGSEGV, JumpHandler, SA_NODEFER);
  volatile uintptr_t bad_addr = 16;
  if (sigsetjmp(g_jmp, 1) == 0) {
    *reinterpret_cast<volatile int*>(bad_addr) = 1;
  }
  Check(g_last_signo == SIGSEGV && g_last_code == SEGV_MAPERR && g_last_addr == 16,
        "SIGSEGV handler gets SEGV_MAPERR and si_addr");
  void* ro = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (sigsetjmp(g_jmp, 1) == 0) {
    *reinterpret_cast<volatile int*>(ro) = 1;
  }
  Check(g_last_signo == SIGSEGV && g_last_code == SEGV_ACCERR,
        "write to PROT_READ page gives SEGV_ACCERR");
  munmap(ro, 4096);
  Reset(SIGSEGV);
#if defined(__x86_64__)
  Install(SIGFPE, JumpHandler, SA_NODEFER);
  volatile int zero = 0;
  if (sigsetjmp(g_jmp, 1) == 0) {
    volatile int r = 7 / zero;
    (void)r;
  }
  Check(g_last_signo == SIGFPE && g_last_code == FPE_INTDIV, "integer divide gives FPE_INTDIV");
  Reset(SIGFPE);
#endif
  Install(SIGILL, JumpHandler, SA_NODEFER);
  if (sigsetjmp(g_jmp, 1) == 0) {
#if defined(__x86_64__)
    __asm__ volatile("ud2");
#elif defined(__aarch64__)
    __asm__ volatile(".inst 0x00000000");
#endif
  }
  Check(g_last_signo == SIGILL && g_last_code == ILL_ILLOPN, "ud2 gives SIGILL/ILL_ILLOPN");
  Reset(SIGILL);
}

void TestAltstack() {
  static char altstack[65536];
  stack_t ss;
  ss.ss_sp = altstack;
  ss.ss_size = sizeof(altstack);
  ss.ss_flags = 0;
  Check(sigaltstack(&ss, nullptr) == 0, "sigaltstack set");
  Install(SIGUSR1, InfoHandler, SA_ONSTACK);
  raise(SIGUSR1);
  const uintptr_t lo = reinterpret_cast<uintptr_t>(altstack);
  Check(g_handler_sp >= lo && g_handler_sp < lo + sizeof(altstack),
        "SA_ONSTACK handler runs on the alternate stack");
  ss.ss_flags = SS_DISABLE;
  sigaltstack(&ss, nullptr);
  Reset(SIGUSR1);
}

void TestWaitStatuses() {
  pid_t c = fork();
  if (c == 0) _exit(3);
  int st = 0;
  Check(waitpid(c, &st, 0) == c && WIFEXITED(st) && WEXITSTATUS(st) == 3, "exit(3) status");

  c = fork();
  if (c == 0) {
    for (;;) pause();
  }
  kill(c, SIGTERM);
  Check(waitpid(c, &st, 0) == c && WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM,
        "SIGTERM-killed child reports WTERMSIG");

  c = fork();
  if (c == 0) {
    for (;;) {
    }
  }
  usleep(20000);
  kill(c, SIGSTOP);
  Check(waitpid(c, &st, WUNTRACED) == c && WIFSTOPPED(st) && WSTOPSIG(st) == SIGSTOP,
        "SIGSTOP reported by WUNTRACED");
  kill(c, SIGCONT);
  Check(waitpid(c, &st, WCONTINUED) == c && WIFCONTINUED(st), "SIGCONT reported by WCONTINUED");
  kill(c, SIGKILL);
  siginfo_t si;
  memset(&si, 0, sizeof(si));
  Check(waitid(P_PID, c, &si, WEXITED) == 0 && si.si_code == CLD_KILLED &&
            si.si_status == SIGKILL,
        "waitid reports CLD_KILLED for a spinning child");

  c = fork();
  if (c == 0) {
    volatile int* p = nullptr;
    *p = 1;
    _exit(0);
  }
  Check(waitpid(c, &st, 0) == c && WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV,
        "unhandled SIGSEGV kills child with WTERMSIG=SIGSEGV");
}

void AlarmHandler(int, siginfo_t*, void*) { g_count = g_count + 1; }

void TestInterruptions() {
  Install(SIGALRM, AlarmHandler, 0);
  g_count = 0;
  struct timespec req = {5, 0}, rem = {0, 0};
  alarm(1);
  int r = nanosleep(&req, &rem);
  Check(r == -1 && errno == EINTR && rem.tv_sec >= 3 && g_count == 1,
        "nanosleep interrupted by SIGALRM with rem");

  pid_t c = fork();
  if (c == 0) {
    sleep(2);
    _exit(0);
  }
  struct itimerval it = {{0, 0}, {0, 300000}};
  setitimer(ITIMER_REAL, &it, nullptr);
  int st;
  r = waitpid(c, &st, 0);
  Check(r == -1 && errno == EINTR, "waitpid without SA_RESTART returns EINTR");
  Install(SIGALRM, AlarmHandler, SA_RESTART);
  setitimer(ITIMER_REAL, &it, nullptr);
  r = waitpid(c, &st, 0);
  Check(r == c, "waitpid with SA_RESTART is restarted");

  g_count = 0;
  struct itimerval per = {{0, 50000}, {0, 50000}};
  setitimer(ITIMER_REAL, &per, nullptr);
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  do {
    clock_gettime(CLOCK_MONOTONIC, &t1);
  } while ((t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec) < 400000000LL);
  struct itimerval off = {{0, 0}, {0, 0}};
  setitimer(ITIMER_REAL, &off, nullptr);
  Check(g_count >= 4, "periodic ITIMER_REAL delivers SIGALRM repeatedly");
  Reset(SIGALRM);
}

void TestSigsuspend() {
  Install(SIGUSR1, InfoHandler, 0);
  sigset_t block, old, empty;
  sigemptyset(&block);
  sigaddset(&block, SIGUSR1);
  sigprocmask(SIG_BLOCK, &block, &old);
  g_count = 0;
  pid_t parent = getpid();
  pid_t c = fork();
  if (c == 0) {
    usleep(50000);
    kill(parent, SIGUSR1);
    _exit(0);
  }
  sigemptyset(&empty);
  int r = sigsuspend(&empty);
  Check(r == -1 && errno == EINTR && g_count == 1, "sigsuspend returns EINTR after handler");
  sigset_t now;
  sigprocmask(SIG_SETMASK, nullptr, &now);
  Check(sigismember(&now, SIGUSR1), "sigsuspend restores the original mask");
  waitpid(c, nullptr, 0);
  sigprocmask(SIG_SETMASK, &old, nullptr);
  Reset(SIGUSR1);
}

void TestTimerCreate() {
  Install(SIGRTMIN, InfoHandler, 0);
  g_count = 0;
  struct sigevent sev;
  memset(&sev, 0, sizeof(sev));
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGRTMIN;
  sev.sigev_value.sival_ptr = reinterpret_cast<void*>(0x1234);
  timer_t tid;
  Check(timer_create(CLOCK_MONOTONIC, &sev, &tid) == 0, "timer_create");
  struct itimerspec its = {{0, 0}, {0, 50000000}};
  timer_settime(tid, 0, &its, nullptr);
  struct timespec req = {1, 0};
  nanosleep(&req, nullptr);
  Check(g_count == 1 && g_last_code == SI_TIMER && g_sigval == 0x1234,
        "POSIX timer delivers SI_TIMER with sigval");
  timer_delete(tid);
  Reset(SIGRTMIN);
}

volatile int g_thread_ready = 0;
volatile pid_t g_thread_tid = 0;

void* SleeperThread(void*) {
  g_thread_tid = static_cast<pid_t>(syscall(SYS_gettid));
  g_thread_ready = 1;
  struct timespec req = {0, 200000000};
  nanosleep(&req, nullptr);
  return nullptr;
}

void TestThreads() {
  Install(SIGUSR2, InfoHandler, 0);
  g_handler_tid = 0;
  pthread_t th;
  pthread_create(&th, nullptr, SleeperThread, nullptr);
  while (!g_thread_ready) {
  }
  syscall(SYS_tgkill, getpid(), g_thread_tid, SIGUSR2);
  pthread_join(th, nullptr);
  Check(g_handler_tid == g_thread_tid, "tgkill delivers to the target thread");
  Reset(SIGUSR2);
}

uint32_t g_futex_word = 0;
volatile int g_waiters_started = 0;

void* FutexWaiter(void*) {
  __atomic_add_fetch(&g_waiters_started, 1, __ATOMIC_SEQ_CST);
  syscall(SYS_futex, &g_futex_word, FUTEX_WAIT_PRIVATE, 0, nullptr, nullptr, 0);
  return nullptr;
}

void TestFutex() {
  pthread_t th[3];
  for (auto& t : th) pthread_create(&t, nullptr, FutexWaiter, nullptr);
  while (g_waiters_started < 3) {
  }
  usleep(100000);  // let the waiters block
  long woken = syscall(SYS_futex, &g_futex_word, FUTEX_WAKE_PRIVATE, 2, nullptr, nullptr, 0);
  Check(woken == 2, "FUTEX_WAKE returns the number of waiters woken");
  usleep(50000);
  woken = syscall(SYS_futex, &g_futex_word, FUTEX_WAKE_PRIVATE, 10, nullptr, nullptr, 0);
  Check(woken == 1, "FUTEX_WAKE wakes the remaining waiter");
  for (auto& t : th) pthread_join(t, nullptr);
  struct timespec ts = {0, 20000000};
  long r = syscall(SYS_futex, &g_futex_word, FUTEX_WAIT_PRIVATE, 0, &ts, nullptr, 0);
  Check(r == -1 && errno == ETIMEDOUT, "FUTEX_WAIT times out");
  r = syscall(SYS_futex, &g_futex_word, FUTEX_LOCK_PI, 0, nullptr, nullptr, 0);
  Check(r == -1 && errno == ENOSYS, "FUTEX_LOCK_PI reports ENOSYS");
}

void TestTimeAndUsage() {
  struct timespec rt;
  clock_gettime(CLOCK_REALTIME, &rt);
  Check(rt.tv_sec > 1577836800, "CLOCK_REALTIME is after 2020-01-01 (RTC seeded)");
  struct timespec c0, c1;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c0);
  volatile uint64_t spin = 0;
  for (uint64_t i = 0; i < 50000000; ++i) spin += i;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c1);
  Check((c1.tv_sec - c0.tv_sec) * 1000000000LL + (c1.tv_nsec - c0.tv_nsec) > 1000000,
        "CLOCK_PROCESS_CPUTIME_ID advances");
  struct rusage ru;
  Check(getrusage(RUSAGE_SELF, &ru) == 0 && (ru.ru_utime.tv_sec > 0 || ru.ru_utime.tv_usec > 0),
        "getrusage reports CPU time");
  struct sysinfo si;
  Check(sysinfo(&si) == 0 && si.totalram > 0 && si.freeram > 0 && si.freeram <= si.totalram,
        "sysinfo reports memory");
  Check(getpgid(999999) == -1 && errno == ESRCH, "getpgid(unknown) is ESRCH");
  Check(syscall(SYS_rseq, nullptr, 0, 0, 0) == -1 && errno == ENOSYS, "rseq is ENOSYS");
  Check(syscall(SYS_membarrier, 0, 0) >= 0, "membarrier QUERY");
  Check(syscall(SYS_membarrier, 8 /* PRIVATE_EXPEDITED */, 0) == -1 && errno == EPERM,
        "membarrier PRIVATE_EXPEDITED unregistered is EPERM");
  syscall(SYS_membarrier, 16, 0);
  Check(syscall(SYS_membarrier, 8, 0) == 0, "membarrier PRIVATE_EXPEDITED after register");
  errno = 0;
  setpriority(PRIO_PROCESS, 0, 5);
  Check(getpriority(PRIO_PROCESS, 0) == 5 && errno == 0, "setpriority/getpriority round trip");
  char name[32] = {};
  prctl(PR_SET_NAME, "a_very_long_thread_name_here");
  prctl(PR_GET_NAME, name);
  Check(strlen(name) == 15, "PR_SET_NAME truncated to 15 bytes");
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  TestHandlerAndFpu();
  TestNodeferAndMask();
  TestFaults();
  TestAltstack();
  TestWaitStatuses();
  TestInterruptions();
  TestSigsuspend();
  TestTimerCreate();
  TestThreads();
  TestFutex();
  TestTimeAndUsage();
  if (g_failures == 0) {
    printf("[PROC_TEST: PASS %d/%d]\n", g_checks, g_checks);
  } else {
    printf("[PROC_TEST: FAIL %d/%d] %s\n", g_failures, g_checks, g_failed_names.c_str());
  }
  return g_failures == 0 ? 0 : 1;
}
