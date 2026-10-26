#ifndef ZIRCON_EMBEDDED_SRC_SHIM_FS_INTERNAL_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_FS_INTERNAL_H_

// Private helpers shared by syscalls_fs.cc (fd core, open, read/write, stat,
// locks) and fs_syscalls.cc (the HandleFsSyscall dispatcher).

#include "src/shim/fd_ops.h"
#include "src/shim/vfs.h"

constexpr int64_t kENAMETOOLONG = -36;
constexpr int64_t kEFBIG = -27;
constexpr int64_t kEOVERFLOW = -75;
constexpr int64_t kEDEADLK = -35;
constexpr int64_t kENOLCK = -37;
constexpr uint32_t kLinuxO_PATH = 010000000;
constexpr uint32_t kLinuxO_TMPFILE = 020000000;
constexpr uint32_t kLinuxO_ASYNC = 020000;
constexpr uint32_t kLinuxO_DIRECT = 040000;
constexpr uint32_t kLinuxO_NOATIME = 01000000;
constexpr uint32_t kLinuxO_LARGEFILE = 0100000;

// st_dev identifiers of the shim's filesystems.
constexpr uint64_t kDevRootfs = 1;    // 0:1  in-memory root filesystem
constexpr uint64_t kDevProcfs = 4;    // 0:4  synthesized /proc (and /sys files)
constexpr uint64_t kDevSockfs = 8;    // 0:8
constexpr uint64_t kDevPipefs = 12;   // 0:12
constexpr uint64_t kDevAnonfs = 13;   // 0:13 anon_inode (eventfd, epoll, ...)

int64_t FdLimit(LinuxProcess* proc);
// Allocate the lowest free fd >= min_fd (proc->fd_mtx held). -EMFILE if none.
int AllocateFdLocked(LinuxProcess* proc, int min_fd,
                     std::shared_ptr<FileDescription> desc, bool cloexec);
// Remove fd from the table (fd_mtx held) and drop POSIX record locks that
// the process holds on the file. Returns the description (to be released
// after fd_mtx is dropped) or nullptr if fd was not open.
std::shared_ptr<FileDescription> DetachFdLocked(LinuxProcess* proc, int fd);

// Resolve `raw` relative to dirfd into a normalized absolute path.
int64_t ResolveAt(LinuxProcess* proc, int dirfd, const std::string& raw,
                  std::string* out);
// Expand /proc/self, /proc/<pid>/{cwd,exe,root} and intermediate
// /proc/<pid>/fd/<n> symlinks.
std::string ExpandProcLinks(LinuxProcess* proc, const std::string& norm,
                            bool follow_final);
// If `norm` is /proc/<pid>/fd/<n>, return that description.
std::shared_ptr<FileDescription> ProcFdTarget(LinuxProcess* proc,
                                              const std::string& norm);

int64_t OpenPathCommon(LinuxProcess* proc, int dirfd, const char* user_path,
                       uint32_t flags, uint32_t mode);

// Permission check against the process's fs credentials. mask: 4=r 2=w 1=x.
bool MayAccess(LinuxProcess* proc, VfsNode* node, int mask,
               bool use_real_ids = false);
bool IsOwnerOrRoot(LinuxProcess* proc, VfsNode* node);

void FillStatFromNode(VfsNode* node, LinuxStat64* st);
void FillStatFromDesc(LinuxProcess* proc, FileDescription* desc,
                      LinuxStat64* st);
// stat(2) on a normalized path (handles /proc). Returns 0 or -errno.
int64_t StatPath(LinuxProcess* proc, const std::string& norm, bool follow,
                 LinuxStat64* st);

// Record locks (fcntl F_GETLK/F_SETLK/F_SETLKW and OFD variants) and flock.
int64_t FcntlLock(LinuxProcess* proc, FileDescription* desc, int cmd,
                  void* user_flock);
int64_t FlockOp(LinuxProcess* proc, FileDescription* desc, int op);

bool IsSeekable(FileDescription* desc);
// pread/pwrite at an explicit offset.
ssize_t PreadDesc(LinuxProcess* proc, FileDescription* desc, void* buf,
                  size_t count, uint64_t pos);
ssize_t PwriteDesc(LinuxProcess* proc, FileDescription* desc, const void* buf,
                   size_t count, uint64_t pos);

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_FS_INTERNAL_H_
