#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {

bool DumpFile(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("[zx_test] ERROR: failed to open %s\n", path);
    return false;
  }
  printf("--- %s ---\n", path);
  char buf[1024];
  ssize_t n = 0;
  size_t total = 0;
  while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
    buf[n] = '\0';
    fputs(buf, stdout);
    total += static_cast<size_t>(n);
  }
  close(fd);
  if (n < 0) {
    printf("[zx_test] ERROR: read(%s) failed\n", path);
    return false;
  }
  if (total == 0) {
    printf("[zx_test] ERROR: %s is empty\n", path);
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  printf("============================================================\n");
  printf("[zx_test] Verifying 64-bit Linux Binary on Zircon Microkernel\n");
  printf("============================================================\n");

  struct utsname uts{};
  if (uname(&uts) != 0) {
    printf("[zx_test] FAIL: uname() failed\n");
    return 1;
  }
  printf("[zx_test] uname: %s %s %s %s (%s)\n", uts.sysname, uts.nodename,
         uts.release, uts.version, uts.machine);
#if defined(__x86_64__)
  if (strcmp(uts.sysname, "Linux") != 0 || strcmp(uts.machine, "x86_64") != 0) {
#elif defined(__aarch64__)
  if (strcmp(uts.sysname, "Linux") != 0 || strcmp(uts.machine, "aarch64") != 0) {
#else
  if (strcmp(uts.sysname, "Linux") != 0) {
#endif
    printf("[zx_test] FAIL: unexpected uname sysname/machine\n");
    return 1;
  }

  if (!DumpFile("/proc/zircon/info")) return 1;
  if (!DumpFile("/proc/zircon/processes")) return 1;
  if (!DumpFile("/proc/zircon/vmos")) return 1;

  // Verify anonymous mmap + write/read
  void* p = mmap(nullptr, 65536, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) {
    printf("[zx_test] FAIL: mmap() failed\n");
    return 1;
  }
  // Anonymous memory must start zeroed; write a pattern across every page and
  // read it back.
  uint8_t* bytes = static_cast<uint8_t*>(p);
  for (size_t i = 0; i < 65536; ++i) {
    if (bytes[i] != 0) {
      printf("[zx_test] FAIL: fresh anonymous mapping not zeroed at %zu\n", i);
      return 1;
    }
  }
  for (size_t i = 0; i < 65536; ++i) bytes[i] = static_cast<uint8_t>(i * 7 + 3);
  for (size_t i = 0; i < 65536; ++i) {
    if (bytes[i] != static_cast<uint8_t>(i * 7 + 3)) {
      printf("[zx_test] FAIL: mmap readback mismatch at %zu\n", i);
      return 1;
    }
  }
  strcpy(static_cast<char*>(p), "Zircon VMO-backed Linux mmap OK");
  printf("[zx_test] mmap check: %s\n", static_cast<char*>(p));
  if (munmap(p, 65536) != 0) {
    printf("[zx_test] FAIL: munmap() failed\n");
    return 1;
  }

  // Verify pipe + fork + waitpid across isolated Zircon shared processes
  int pipefd[2] = {-1, -1};
  if (pipe(pipefd) != 0) {
    printf("[zx_test] FAIL: pipe() failed\n");
    return 1;
  }

  pid_t child = fork();
  if (child < 0) {
    printf("[zx_test] FAIL: fork() failed\n");
    return 1;
  }
  if (child == 0) {
    close(pipefd[0]);
    char msg[128];
    int len = snprintf(msg, sizeof(msg),
                       "Hello from forked Linux child PID=%d (PPID=%d)!",
                       getpid(), getppid());
    if (write(pipefd[1], msg, static_cast<size_t>(len)) != len) _exit(1);
    close(pipefd[1]);
    _exit(42);
  }

  close(pipefd[1]);
  char rx[128] = {};
  ssize_t r = read(pipefd[0], rx, sizeof(rx) - 1);
  close(pipefd[0]);

  int status = 0;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 42 || r <= 0) {
    printf("[zx_test] FAIL: child waitpid status=0x%x r=%zd\n", status, r);
    return 1;
  }
  char expected[128];
  snprintf(expected, sizeof(expected),
           "Hello from forked Linux child PID=%d (PPID=%d)!", child, getpid());
  if (strcmp(rx, expected) != 0) {
    printf("[zx_test] FAIL: pipe payload '%s' != expected '%s'\n", rx,
           expected);
    return 1;
  }
  printf("[zx_test] fork+pipe check: '%s' (exit code=%d)\n", rx,
         WEXITSTATUS(status));

  struct timespec ts{};
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0 ||
      (ts.tv_sec == 0 && ts.tv_nsec == 0)) {
    printf("[zx_test] FAIL: clock_gettime(CLOCK_MONOTONIC) failed\n");
    return 1;
  }
  printf("[zx_test] monotonic uptime: %lld.%03lld s\n",
         static_cast<long long>(ts.tv_sec),
         static_cast<long long>(ts.tv_nsec / 1000000));
  printf("[ZX_TEST: PASS] All Zircon + Linux Shim checks succeeded!\n");
  fflush(stdout);
  return 0;
}
