#ifndef ZIRCON_EMBEDDED_SRC_SHIM_FD_OPS_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_FD_OPS_H_

// Internal interfaces between the generic fd layer (syscalls_fs.cc), the
// tty layer (tty.cc), the VFS (vfs.cc) and the IPC objects (syscalls_ipc.cc).

#include <string>

#include "src/shim/shim_common.h"

struct VfsNode;

// Linux poll bits.
constexpr uint32_t kPollIn = 0x0001;
constexpr uint32_t kPollPri = 0x0002;
constexpr uint32_t kPollOut = 0x0004;
constexpr uint32_t kPollErr = 0x0008;
constexpr uint32_t kPollHup = 0x0010;
constexpr uint32_t kPollNval = 0x0020;
constexpr uint32_t kPollRdNorm = 0x0040;
constexpr uint32_t kPollWrNorm = 0x0100;
constexpr uint32_t kPollRdHup = 0x2000;

// ---------------------------------------------------------------------------
// Provided by syscalls_fs.cc
// ---------------------------------------------------------------------------

// The Linux thread whose syscall is executing on this host thread. Set at the
// top of HandleFsSyscall (the first dispatcher every syscall goes through).
extern thread_local LinuxThread* t_current_thread;

// True if the calling Linux thread has a pending signal that is not blocked
// by its signal mask (i.e. a blocking syscall should return -EINTR).
bool CurrentThreadHasUnblockedSignal(LinuxProcess* proc);

// Sleep until NotifyPollWaiters() is called or `max_ms` elapses.
// If `seen_seq` is passed and `g_poll_seq != seen_seq`, returns immediately.
uint64_t CurrentPollSeq();
void WaitForPollEvent(int64_t max_ms, uint64_t seen_seq = UINT64_MAX);

// Allocate a unique inode number for objects that have no VfsNode
// (pipes, sockets, eventfd, ...). Shares the VFS inode number space.
uint64_t AllocAnonInode();

// Install `desc` in the lowest free fd of `proc` (fd_mtx taken internally).
// Returns the fd or -EMFILE.
int InstallFd(LinuxProcess* proc, std::shared_ptr<FileDescription> desc,
              bool cloexec);

// Look up an fd. Returns nullptr if not open.
std::shared_ptr<FileDescription> LookupFd(LinuxProcess* proc, int fd);

// poll/ppoll/select/pselect6/epoll_* (fs_poll.cc). Returns kENOSYS for other
// syscall numbers.
int64_t HandlePollSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                          uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5);
// Activity counter for EPOLLET emulation (fs_poll.cc).
uint64_t FdEdgeCounter(FileDescription* desc);

// ---------------------------------------------------------------------------
// Provided by hardware_fb_input.cc
// ---------------------------------------------------------------------------
// True if a PS/2 (i8042) controller was found and /dev/input/event0 exists.
bool InputDeviceAvailable();
// Monotonic count of input events queued so far (EPOLLET emulation).
uint64_t InputEventCounter();

// ---------------------------------------------------------------------------
// Provided by syscalls_ipc.cc  (AF_UNIX sockets, eventfd, timerfd, signalfd,
// inotify). Blocking functions must honour O_NONBLOCK (desc->flags) and return
// -EINTR when CurrentThreadHasUnblockedSignal(proc).
// ---------------------------------------------------------------------------

// Plain read()/write() on a kUnixSocket description.
ssize_t SocketRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                   size_t count);
ssize_t SocketWrite(LinuxProcess* proc, FileDescription* desc, const void* buf,
                    size_t count);
// Poll bits for kUnixSocket (POLLIN/OUT/HUP/RDHUP/ERR).
uint32_t SocketPollEvents(FileDescription* desc);
// FIONREAD / SIOCINQ value.
int SocketBytesAvailable(FileDescription* desc);
// Called from ~FileDescription for kUnixSocket (last reference dropped):
// mark peer closed, drop listener backlog, unbind abstract names, etc.
void SocketOnDescriptionDestroyed(FileDescription* desc);
// Called by unlink()/rename() over a socket inode path so that the bound
// filesystem name no longer reaches the listener (Linux semantics).
void SocketPathUnlinked(VfsNode* node);

// read()/write()/poll for kEventFd, kTimerFd, kSignalFd, kInotify.
ssize_t IpcRead(LinuxProcess* proc, FileDescription* desc, void* buf,
                size_t count);
ssize_t IpcWrite(LinuxProcess* proc, FileDescription* desc, const void* buf,
                 size_t count);
uint32_t IpcPollEvents(FileDescription* desc);
// Monotonic activity counter used to emulate EPOLLET for the kinds above and
// for kUnixSocket: must change whenever new data/events become available or
// buffer space is released.
uint64_t IpcEdgeCounter(FileDescription* desc);
// FIONREAD for kInotify (bytes of queued events); 0 for others.
int IpcBytesAvailable(FileDescription* desc);

// inotify hooks, called by the VFS layer with Vfs::Mutex() held. The inotify
// implementation must not acquire Vfs::Mutex() while holding its own lock.
//  - InotifyDirEvent: event about entry `name` inside directory `dir`
//    (delivered to watches on `dir`; IN_ISDIR is added when child_is_dir).
//  - InotifySelfEvent: event about `node` itself (delivered without a name to
//    watches on `node`). IN_DELETE_SELF also queues IN_IGNORED and removes
//    the watch.
void InotifyDirEvent(VfsNode* dir, uint32_t mask, const std::string& name,
                     uint32_t cookie, bool child_is_dir);
void InotifySelfEvent(VfsNode* node, uint32_t mask);
uint32_t InotifyNextCookie();

// Called when CLOCK_REALTIME is set (clock_settime/settimeofday) so timerfds
// armed with TFD_TIMER_CANCEL_ON_SET report -ECANCELED. Defined in
// syscalls_ipc.cc.
void TimerfdClockWasSet();

// inotify mask bits
constexpr uint32_t kInAccess = 0x00000001;
constexpr uint32_t kInModify = 0x00000002;
constexpr uint32_t kInAttrib = 0x00000004;
constexpr uint32_t kInCloseWrite = 0x00000008;
constexpr uint32_t kInCloseNoWrite = 0x00000010;
constexpr uint32_t kInOpen = 0x00000020;
constexpr uint32_t kInMovedFrom = 0x00000040;
constexpr uint32_t kInMovedTo = 0x00000080;
constexpr uint32_t kInCreate = 0x00000100;
constexpr uint32_t kInDelete = 0x00000200;
constexpr uint32_t kInDeleteSelf = 0x00000400;
constexpr uint32_t kInMoveSelf = 0x00000800;

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_FD_OPS_H_
