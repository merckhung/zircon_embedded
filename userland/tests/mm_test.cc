// Exercises the shim's Linux memory syscalls (mmap/munmap/mprotect/mremap/
// madvise/mincore, fork copy-on-write, SysV shm, memfd, execve failure paths).
// Prints "MMTEST PASS <name>" or "MMTEST FAIL <name> ..." per check, then a
// summary computed from the results:
//   [MM_TEST: PASS n/n]           when every check passed (exit status 0)
//   [MM_TEST: FAIL k/n] a,b,...   listing the failed checks (exit status 1)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static int g_checks = 0;
static int g_fails = 0;
static std::string g_failed_names;
#define CHECK(name, cond, ...)                                  \
  do {                                                          \
    g_checks++;                                                 \
    if (cond) {                                                 \
      printf("MMTEST PASS %s\n", name);                         \
    } else {                                                    \
      g_fails++;                                                \
      if (!g_failed_names.empty()) g_failed_names += ",";       \
      g_failed_names += name;                                   \
      printf("MMTEST FAIL %s errno=%d: ", name, errno);         \
      printf(__VA_ARGS__);                                      \
      printf("\n");                                             \
    }                                                           \
    fflush(stdout);                                             \
  } while (0)

// Runs fn in a child; returns the raw wait status.
static int RunChild(void (*fn)(void*), void* arg) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid < 0) return -1;  // not a SIGSEGV status, so dependent checks fail
  if (pid == 0) {
    fn(arg);
    _exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return status;
}
static bool KilledBySegv(int st) {
  return WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV;
}

static void WriteByte(void* p) { *reinterpret_cast<volatile char*>(p) = 1; }
static void ReadByte(void* p) {
  volatile char c = *reinterpret_cast<volatile char*>(p);
  (void)c;
}
static void CallCode(void* p) { reinterpret_cast<void (*)()>(p)(); }

int main() {
  const long pg = 4096;

  // --- mprotect RO really faults; RW works again after mprotect ---
  char* a = (char*)mmap(nullptr, 2 * pg, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  CHECK("mmap_anon", a != MAP_FAILED, "mmap failed");
  a[0] = 42;
  CHECK("mprotect_ro_rc", mprotect(a, pg, PROT_READ) == 0, "mprotect");
  CHECK("mprotect_ro_faults", KilledBySegv(RunChild(WriteByte, a)),
        "write to PROT_READ page did not SIGSEGV");
  CHECK("mprotect_ro_keeps_data", a[0] == 42, "data lost");
  CHECK("mprotect_ro_other_page_rw", (a[pg] = 7, a[pg] == 7), "neighbour");
  CHECK("mprotect_none_faults",
        mprotect(a, pg, PROT_NONE) == 0 && KilledBySegv(RunChild(ReadByte, a)),
        "read of PROT_NONE did not SIGSEGV");
  CHECK("mprotect_rw_restore", mprotect(a, pg, PROT_READ | PROT_WRITE) == 0 &&
                                   a[0] == 42,
        "restore");
  errno = 0;
  CHECK("mprotect_unaligned_einval", mprotect(a + 1, pg, PROT_READ) == -1 &&
                                         errno == EINVAL,
        "expected EINVAL");

  // --- W^X: writable non-exec page cannot be executed until mprotect RX ---
  unsigned char* code = (unsigned char*)mmap(nullptr, pg, PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#if defined(__aarch64__)
  *reinterpret_cast<uint32_t*>(code) = 0xd65f03c0U;  // ret
  __builtin___clear_cache(reinterpret_cast<char*>(code),
                          reinterpret_cast<char*>(code) + 4);
#else
  code[0] = 0xc3;  // ret
#endif
  CHECK("nx_faults", KilledBySegv(RunChild(CallCode, code)),
        "executing RW page did not SIGSEGV");
  CHECK("rx_runs", mprotect(code, pg, PROT_READ | PROT_EXEC) == 0 &&
                       RunChild(CallCode, code) == 0,
        "RX page did not run");

  // --- munmap + mincore ---
  unsigned char vec[2] = {9, 9};
  CHECK("mincore_mapped", mincore(a, 2 * pg, vec) == 0 && vec[0] <= 1 &&
                              vec[1] <= 1,
        "vec=%d,%d", vec[0], vec[1]);
  CHECK("munmap", munmap(a, 2 * pg) == 0, "munmap");
  errno = 0;
  CHECK("mincore_unmapped_enomem", mincore(a, pg, vec) == -1 && errno == ENOMEM,
        "expected ENOMEM");
  errno = 0;
  CHECK("mprotect_unmapped_enomem", mprotect(a, pg, PROT_READ) == -1 &&
                                        errno == ENOMEM,
        "expected ENOMEM");

  // --- MAP_FIXED_NOREPLACE / MAP_FIXED out of range / hint reuse ---
  char* b = (char*)mmap(nullptr, 4 * pg, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  b[0] = 5;
  errno = 0;
  void* r = mmap(b + pg, pg, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS |
                                           MAP_FIXED_NOREPLACE, -1, 0);
  CHECK("noreplace_eexist", r == MAP_FAILED && errno == EEXIST, "r=%p", r);
  errno = 0;
#if defined(__aarch64__)
  r = mmap((void*)0x800000000000ULL, pg, PROT_READ,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
#else
  r = mmap((void*)0x7ff000000000ULL, pg, PROT_READ,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
#endif
  CHECK("fixed_outside_fails", r == MAP_FAILED && errno == ENOMEM, "r=%p", r);
  r = mmap(b + pg, pg, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  CHECK("fixed_replace", r == b + pg && b[pg] == 0 && b[0] == 5, "r=%p", r);
  munmap(b, 4 * pg);
  void* c = mmap(b, 4 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);
  CHECK("hint_reuses_freed_range", c == b, "c=%p b=%p", c, b);
  munmap(c, 4 * pg);

  // --- madvise DONTNEED zeroes private anon ---
  char* d = (char*)mmap(nullptr, 2 * pg, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memset(d, 0xab, 2 * pg);
  CHECK("madv_dontneed_zeroes", madvise(d, pg, MADV_DONTNEED) == 0 &&
                                    d[0] == 0 && d[pg - 1] == 0 &&
                                    (unsigned char)d[pg] == 0xab,
        "d0=%d", d[0]);
  errno = 0;
  CHECK("madv_bad_advice", madvise(d, pg, 12345) == -1 && errno == EINVAL,
        "expected EINVAL");

  // --- mremap: grow (move) keeps data, shrink unmaps tail ---
  char* m = (char*)mremap(d, 2 * pg, 64 * pg, MREMAP_MAYMOVE);
  CHECK("mremap_grow", m != MAP_FAILED && (unsigned char)m[pg] == 0xab &&
                           (m[63 * pg] = 1, true),
        "m=%p", m);
  char* m2 = (char*)mremap(m, 64 * pg, pg, 0);
  CHECK("mremap_shrink", m2 == m, "m2=%p", m2);
  errno = 0;
  CHECK("mremap_shrink_unmapped_tail", mincore(m + pg, pg, vec) == -1 &&
                                           errno == ENOMEM,
        "tail still mapped");
  munmap(m, pg);

  // --- MAP_SHARED anon survives fork & mremap growth keeps sharing ---
  volatile int* sh = (int*)mmap(nullptr, pg, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  *sh = 1;
  fflush(stdout);
  pid_t pid = fork();
  if (pid == 0) {
    *sh = 77;
    _exit(0);
  }
  waitpid(pid, nullptr, 0);
  CHECK("shared_anon_fork", *sh == 77, "*sh=%d", *sh);

  // --- private memory after fork is copy-on-write ---
  int* priv = (int*)mmap(nullptr, pg, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  *priv = 1;
  pid = fork();
  if (pid == 0) {
    *priv = 99;
    _exit(*priv == 99 ? 0 : 1);
  }
  int st = 0;
  waitpid(pid, &st, 0);
  CHECK("private_fork_cow", *priv == 1 && WIFEXITED(st) && WEXITSTATUS(st) == 0,
        "*priv=%d st=%x", *priv, st);
  // Protection is inherited by the child.
  mprotect(priv, pg, PROT_READ);
  CHECK("fork_keeps_prot", KilledBySegv(RunChild(WriteByte, priv)),
        "child could write RO page");

  // --- SysV shm ---
  errno = 0;
  CHECK("shmget_enoent", shmget(0x5151, pg, 0600) == -1 && errno == ENOENT,
        "expected ENOENT");
  int id = shmget(0x5151, 10000, IPC_CREAT | 0600);
  CHECK("shmget_create", id >= 0, "id=%d", id);
  errno = 0;
  CHECK("shmget_excl", shmget(0x5151, pg, IPC_CREAT | IPC_EXCL | 0600) == -1 &&
                           errno == EEXIST,
        "expected EEXIST");
  errno = 0;
  CHECK("shmget_too_big", shmget(0x5151, 20000, 0600) == -1 && errno == EINVAL,
        "expected EINVAL");
  CHECK("shmget_lookup", shmget(0x5151, 100, 0600) == id, "lookup");
  char* s1 = (char*)shmat(id, nullptr, 0);
  char* s2 = (char*)shmat(id, nullptr, SHM_RDONLY);
  struct shmid_ds ds;
  memset(&ds, 0, sizeof(ds));
  CHECK("shm_attach_share", s1 != (char*)-1 && s2 != (char*)-1 &&
                                (s1[100] = 'x', s2[100] == 'x'),
        "s1=%p s2=%p", s1, s2);
  CHECK("shm_stat", shmctl(id, IPC_STAT, &ds) == 0 && ds.shm_nattch == 2 &&
                        ds.shm_segsz == 10000 && ds.shm_cpid == getpid() &&
                        (ds.shm_perm.mode & 0777) == 0600,
        "nattch=%lu segsz=%zu cpid=%d mode=%o", (unsigned long)ds.shm_nattch,
        (size_t)ds.shm_segsz, ds.shm_cpid, ds.shm_perm.mode);
  CHECK("shm_rdonly_faults", KilledBySegv(RunChild(WriteByte, s2)),
        "SHM_RDONLY attach was writable");
  // fork child inherits attachments (nattch +2 while it lives)
  CHECK("shmdt", shmdt(s2) == 0 && shmctl(id, IPC_STAT, &ds) == 0 &&
                     ds.shm_nattch == 1,
        "nattch=%lu", (unsigned long)ds.shm_nattch);
  errno = 0;
  CHECK("shmdt_bad", shmdt(s1 + pg) == -1 && errno == EINVAL, "expected EINVAL");
  CHECK("shm_rmid_deferred", shmctl(id, IPC_RMID, nullptr) == 0 &&
                                 shmctl(id, IPC_STAT, &ds) == 0 &&
                                 (ds.shm_perm.mode & 01000) != 0 &&
                                 ds.shm_perm.__key == IPC_PRIVATE,
        "mode=%o", ds.shm_perm.mode);
  void* s3 = shmat(id, nullptr, 0);
  CHECK("shm_attach_after_rmid", s3 != (void*)-1, "attach");
  // Detach everything: two attachments remain.
  shmdt(s1);
  errno = 0;
  CHECK("shm_still_alive", shmctl(id, IPC_STAT, &ds) == 0 && ds.shm_nattch == 1,
        "nattch=%lu", (unsigned long)ds.shm_nattch);
  // Last detach of a removed segment destroys it.
  errno = 0;
  CHECK("shm_freed_on_last_detach", s3 != (void*)-1 && shmdt(s3) == 0 &&
                                        shmctl(id, IPC_STAT, &ds) == -1 &&
                                        errno == EINVAL,
        "segment still exists");

  // --- memfd ---
  int fd = (int)syscall(SYS_memfd_create, "mmtest", 1 /* MFD_CLOEXEC */);
  CHECK("memfd_create", fd >= 0, "fd=%d", fd);
  CHECK("memfd_cloexec", fcntl(fd, F_GETFD) == FD_CLOEXEC, "flags");
  CHECK("memfd_ftruncate", ftruncate(fd, pg) == 0, "ftruncate");
  char* mf = (char*)mmap(nullptr, pg, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  char buf[8] = {0};
  CHECK("memfd_coherent", mf != MAP_FAILED && (memcpy(mf, "hello", 6), true) &&
                              pread(fd, buf, 6, 0) == 6 &&
                              strcmp(buf, "hello") == 0,
        "buf=%s", buf);
  CHECK("memfd_not_in_devshm", access("/dev/shm/memfd_mmtest", F_OK) != 0,
        "visible");
  close(fd);
  CHECK("memfd_mapping_survives_close", strcmp(mf, "hello") == 0, "lost");
  munmap(mf, pg);

  // --- execve failures leave the process intact ---
  {
    // ELF with a PT_INTERP that does not exist.
    unsigned char elf[0x200];
    memset(elf, 0, sizeof(elf));
    memcpy(elf, "\x7f" "ELF\x02\x01\x01", 7);
    *(uint16_t*)(elf + 16) = 2;      // ET_EXEC
#if defined(__aarch64__)
    *(uint16_t*)(elf + 18) = 183;    // EM_AARCH64
#else
    *(uint16_t*)(elf + 18) = 62;     // EM_X86_64
#endif
    *(uint32_t*)(elf + 20) = 1;
    *(uint64_t*)(elf + 24) = 0x401000;  // entry
    *(uint64_t*)(elf + 32) = 64;        // phoff
    *(uint16_t*)(elf + 52) = 64;
    *(uint16_t*)(elf + 54) = 56;
    *(uint16_t*)(elf + 56) = 2;
    unsigned char* ph = elf + 64;
    *(uint32_t*)(ph + 0) = 3;  // PT_INTERP
    *(uint64_t*)(ph + 8) = 0x180;
    *(uint64_t*)(ph + 32) = 20;
    ph += 56;
    *(uint32_t*)(ph + 0) = 1;  // PT_LOAD
    *(uint32_t*)(ph + 4) = 5;
    *(uint64_t*)(ph + 16) = 0x400000;
    *(uint64_t*)(ph + 32) = sizeof(elf);
    *(uint64_t*)(ph + 40) = sizeof(elf);
    memcpy(elf + 0x180, "/nonexistent/ld.so", 19);
    int efd = open("/tmp/badinterp", O_CREAT | O_TRUNC | O_WRONLY, 0755);
    write(efd, elf, sizeof(elf));
    close(efd);
    char* argv[] = {(char*)"/tmp/badinterp", nullptr};
    char* envp[] = {nullptr};
    errno = 0;
    int rc = execve("/tmp/badinterp", argv, envp);
    CHECK("execve_missing_interp_enoent", rc == -1 && errno == ENOENT,
          "rc=%d", rc);
    std::string big(200 * 1024, 'A');
    char* envp2[] = {(char*)big.c_str(), nullptr};
    char* argv2[] = {(char*)"/bin/true", nullptr};
    errno = 0;
    rc = execve("/bin/true", argv2, envp2);
    CHECK("execve_e2big", rc == -1 && errno == E2BIG, "rc=%d", rc);
    CHECK("alive_after_failed_execve", *sh == 77, "address space damaged");
  }

  if (g_fails == 0) {
    printf("[MM_TEST: PASS %d/%d]\n", g_checks, g_checks);
  } else {
    printf("[MM_TEST: FAIL %d/%d] %s\n", g_fails, g_checks,
           g_failed_names.c_str());
  }
  fflush(stdout);
  return g_fails == 0 ? 0 : 1;
}
