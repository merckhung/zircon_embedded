#ifndef ZIRCON_EMBEDDED_SRC_SHIM_SHIM_COMMON_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_SHIM_COMMON_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zircon/process.h>
#include <zircon/processargs.h>
#include <zircon/syscalls-next.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/syscalls/object.h>
#include <zircon/syscalls/resource.h>
#include <zircon/threads.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Global Zircon handles initialized at startup
extern zx_handle_t g_debuglog;
extern zx_handle_t g_bootfs_vmo;
extern zx_handle_t g_bootdata_vmo;
extern zx_handle_t g_vdso_vmo;
extern zx_handle_t g_mmio_rsrc;
extern zx_handle_t g_ioport_rsrc;
extern zx_handle_t g_irq_rsrc;
extern zx_handle_t g_sys_rsrc;
extern zx_handle_t g_vmex_rsrc;
extern zx_handle_t g_debug_rsrc;

void ShimLog(const char* s);
void ShimLogf(const char* fmt, ...);

// Linux errno constants (negative values returned by syscalls)
constexpr int64_t kEPERM = -1;
constexpr int64_t kENOENT = -2;
constexpr int64_t kESRCH = -3;
constexpr int64_t kEINTR = -4;
constexpr int64_t kEIO = -5;
constexpr int64_t kENXIO = -6;
constexpr int64_t kE2BIG = -7;
constexpr int64_t kENOEXEC = -8;
constexpr int64_t kEBADF = -9;
constexpr int64_t kECHILD = -10;
constexpr int64_t kEAGAIN = -11;
constexpr int64_t kENOMEM = -12;
constexpr int64_t kEACCES = -13;
constexpr int64_t kEFAULT = -14;
constexpr int64_t kEBUSY = -16;
constexpr int64_t kEEXIST = -17;
constexpr int64_t kEXDEV = -18;
constexpr int64_t kENODEV = -19;
constexpr int64_t kENOTDIR = -20;
constexpr int64_t kEISDIR = -21;
constexpr int64_t kEINVAL = -22;
constexpr int64_t kENFILE = -23;
constexpr int64_t kEMFILE = -24;
constexpr int64_t kENOTTY = -25;
constexpr int64_t kENOSPC = -28;
constexpr int64_t kESPIPE = -29;
constexpr int64_t kEROFS = -30;
constexpr int64_t kEPIPE = -32;
constexpr int64_t kERANGE = -34;
constexpr int64_t kENOSYS = -38;
constexpr int64_t kENOTEMPTY = -39;
constexpr int64_t kELOOP = -40;
constexpr int64_t kENOTSOCK = -88;
constexpr int64_t kEOPNOTSUPP = -95;
constexpr int64_t kEAFNOSUPPORT = -97;
constexpr int64_t kEADDRINUSE = -98;
constexpr int64_t kECONNRESET = -104;
constexpr int64_t kEISCONN = -106;
constexpr int64_t kENOTCONN = -107;
constexpr int64_t kETIMEDOUT = -110;
constexpr int64_t kECONNREFUSED = -111;

// Kernel-internal restart codes (never visible to Linux userspace). A blocking
// syscall interrupted by a signal returns one of these; the return-to-user
// path in signals.cc converts it to -EINTR or restarts the syscall exactly as
// Linux does (SA_RESTART, handler/no handler, restart_syscall(2)).
constexpr int64_t kERESTARTSYS = -512;
constexpr int64_t kERESTARTNOINTR = -513;
constexpr int64_t kERESTARTNOHAND = -514;
constexpr int64_t kERESTART_RESTARTBLOCK = -516;

// Linux open flags
constexpr uint32_t kLinuxO_ACCMODE = 00000003;
constexpr uint32_t kLinuxO_RDONLY = 00000000;
constexpr uint32_t kLinuxO_WRONLY = 00000001;
constexpr uint32_t kLinuxO_RDWR = 00000002;
constexpr uint32_t kLinuxO_CREAT = 00000100;
constexpr uint32_t kLinuxO_EXCL = 00000200;
constexpr uint32_t kLinuxO_NOCTTY = 00000400;
constexpr uint32_t kLinuxO_TRUNC = 00001000;
constexpr uint32_t kLinuxO_APPEND = 00002000;
constexpr uint32_t kLinuxO_NONBLOCK = 00004000;
#if defined(__aarch64__)
constexpr uint32_t kLinuxO_DIRECTORY = 00040000;
constexpr uint32_t kLinuxO_NOFOLLOW = 00100000;
#else
constexpr uint32_t kLinuxO_DIRECTORY = 00200000;
constexpr uint32_t kLinuxO_NOFOLLOW = 00400000;
#endif
constexpr uint32_t kLinuxO_CLOEXEC = 02000000;
constexpr int kLinuxAT_FDCWD = -100;
constexpr int kLinuxAT_SYMLINK_NOFOLLOW = 0x100;
constexpr int kLinuxAT_REMOVEDIR = 0x200;
constexpr int kLinuxAT_EMPTY_PATH = 0x1000;

#if defined(__aarch64__)
// Linux AArch64 (asm-generic) struct stat (128 bytes)
struct LinuxStat64 {
  uint64_t st_dev;
  uint64_t st_ino;
  uint32_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint64_t st_rdev;
  uint64_t __pad1;
  int64_t st_size;
  int32_t st_blksize;
  int32_t __pad2;
  int64_t st_blocks;
  uint64_t st_atime_sec;
  uint64_t st_atime_nsec;
  uint64_t st_mtime_sec;
  uint64_t st_mtime_nsec;
  uint64_t st_ctime_sec;
  uint64_t st_ctime_nsec;
  uint32_t __unused4;
  uint32_t __unused5;
};
static_assert(sizeof(LinuxStat64) == 128, "AArch64 struct stat must be 128 bytes");
#else
// Linux x86_64 struct stat (144 bytes)
struct LinuxStat64 {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_nlink;
  uint32_t st_mode;
  uint32_t st_uid;
  uint32_t st_gid;
  uint32_t __pad0;
  uint64_t st_rdev;
  int64_t st_size;
  int64_t st_blksize;
  int64_t st_blocks;
  uint64_t st_atime_sec;
  uint64_t st_atime_nsec;
  uint64_t st_mtime_sec;
  uint64_t st_mtime_nsec;
  uint64_t st_ctime_sec;
  uint64_t st_ctime_nsec;
  int64_t __unused[3];
};
static_assert(sizeof(LinuxStat64) == 144, "x86_64 struct stat must be 144 bytes");
#endif

// Linux statx (256 bytes)
struct LinuxStatxTimestamp {
  int64_t tv_sec;
  uint32_t tv_nsec;
  int32_t __reserved;
};

struct LinuxStatx {
  uint32_t stx_mask;
  uint32_t stx_blksize;
  uint64_t stx_attributes;
  uint32_t stx_nlink;
  uint32_t stx_uid;
  uint32_t stx_gid;
  uint16_t stx_mode;
  uint16_t __spare0[1];
  uint64_t stx_ino;
  uint64_t stx_size;
  uint64_t stx_blocks;
  uint64_t stx_attributes_mask;
  LinuxStatxTimestamp stx_atime;
  LinuxStatxTimestamp stx_btime;
  LinuxStatxTimestamp stx_ctime;
  LinuxStatxTimestamp stx_mtime;
  uint32_t stx_rdev_major;
  uint32_t stx_rdev_minor;
  uint32_t stx_dev_major;
  uint32_t stx_dev_minor;
  uint64_t stx_mnt_id;
  uint64_t __spare2;
  uint64_t __spare3[12];
};

struct LinuxIovec {
  void* iov_base;
  size_t iov_len;
};

struct LinuxPollFd {
  int32_t fd;
  int16_t events;
  int16_t revents;
};

#if defined(__aarch64__)
struct LinuxEpollEvent {
  uint32_t events;
  uint32_t __pad;
  uint64_t data;
};
static_assert(sizeof(LinuxEpollEvent) == 16, "AArch64 struct epoll_event must be 16 bytes");
#else
struct __attribute__((packed)) LinuxEpollEvent {
  uint32_t events;
  uint64_t data;
};
static_assert(sizeof(LinuxEpollEvent) == 12, "x86_64 struct epoll_event must be 12 bytes");
#endif

struct LinuxTermios {
  uint32_t c_iflag;
  uint32_t c_oflag;
  uint32_t c_cflag;
  uint32_t c_lflag;
  uint8_t c_line;
  uint8_t c_cc[19];
};

struct LinuxWinsize {
  uint16_t ws_row;
  uint16_t ws_col;
  uint16_t ws_xpixel;
  uint16_t ws_ypixel;
};

struct LinuxSigaction {
  uint64_t sa_handler;
  uint64_t sa_flags;
  uint64_t sa_restorer;
  uint64_t sa_mask;
};

struct LinuxInputEvent {
  int64_t tv_sec;
  int64_t tv_usec;
  uint16_t type;
  uint16_t code;
  int32_t value;
};

// Linux x86_64 siginfo_t (128 bytes). The union starts at offset 16.
struct LinuxSiginfo {
  int32_t si_signo = 0;
  int32_t si_errno = 0;
  int32_t si_code = 0;
  int32_t pad0 = 0;
  uint8_t fields[112] = {};
};
static_assert(sizeof(LinuxSiginfo) == 128, "siginfo_t must be 128 bytes");

struct VfsNode;
struct AddressSpaceState;  // src/shim/mem_space.h
struct LinuxProcess;
struct LinuxThread;

// Pipe state
struct PipeState {
  std::mutex mtx;
  std::deque<uint8_t> buf;
  int readers = 1;  // open read-end descriptions (decremented on destruction)
  int writers = 1;  // open write-end descriptions (decremented on destruction)
  size_t capacity = 65536;     // F_GETPIPE_SZ / F_SETPIPE_SZ
  uint64_t total_written = 0;  // monotonic counters (edge-triggered epoll)
  uint64_t total_read = 0;
};

// PTY state (/dev/ptmx <-> /dev/pts/<N>)
struct PtyPair {
  int index = 0;
  std::mutex mtx;
  std::deque<uint8_t> master_to_slave;
  std::deque<uint8_t> slave_to_master;
  std::string canon_line;
  int master_count = 1;
  int slave_count = 0;
  int locked = 0;
  int fg_pgrp = 0;
  bool packet_mode = false;
  LinuxTermios termios{};
  LinuxWinsize winsize{24, 80, 800, 600};
  // Canonical-mode line boundaries inside master_to_slave: lengths of
  // completed lines in order (a zero-length entry is a VEOF on an empty line).
  std::deque<size_t> canon_lines;
  bool lnext_pending = false;  // previous input byte was VLNEXT
  bool is_console = false;     // UART-backed system console (no master side)
  bool slave_opened = false;   // a slave has been opened at least once
  bool stopped = false;        // output suspended (VSTOP / TCOOFF)
  bool exclusive = false;      // TIOCEXCL
  uint8_t pkt_status = 0;      // pending TIOCPKT control byte
  int session = 0;             // session that owns this tty as ctty (0 = none)
  uint64_t m2s_total = 0;      // monotonic counters (edge-triggered epoll)
  uint64_t s2m_total = 0;
};

// AF_UNIX Socket state
struct FileDescription;

struct UnixSocketPacket {
  std::vector<uint8_t> data;
  std::vector<std::shared_ptr<FileDescription>> scm_fds;
  // --- syscalls_ipc.cc (guarded by its global socket mutex) ---
  size_t offset = 0;        // SOCK_STREAM: bytes of `data` already consumed
  std::string sender_name;  // sender's sun_path bytes at send time ("" = unnamed)
  int32_t cred_pid = 0;     // SCM_CREDENTIALS attached by the sender
  uint32_t cred_uid = 0;
  uint32_t cred_gid = 0;
  // Socket whose send buffer (SO_SNDBUF accounting) these bytes are charged to.
  std::weak_ptr<struct UnixSocketState> sender;
};

struct UnixSocketState {
  std::mutex mtx;
  int domain = 1;  // AF_UNIX
  int type = 1;    // SOCK_STREAM
  std::string bound_path;
  bool listening = false;
  bool connected = false;
  bool peer_closed = false;
  int owner_pid = 1;
  int peer_pid = 1;
  std::weak_ptr<UnixSocketState> peer;
  std::deque<std::shared_ptr<UnixSocketState>> backlog;
  std::deque<uint8_t> rx_stream;
  std::deque<UnixSocketPacket> rx_packets;
  std::deque<std::shared_ptr<FileDescription>> rx_fds;

  // --- syscalls_ipc.cc. Every UnixSocketState field (and its packets) is
  // guarded by the single socket mutex in syscalls_ipc.cc; the legacy members
  // mtx, bound_path, peer_closed, owner_pid, peer_pid, rx_stream and rx_fds
  // above are not used. `connected` = has a peer (stream: established; dgram:
  // default destination set); rx_packets holds stream segments and datagrams.
  int protocol = 0;               // SO_PROTOCOL (Linux reports 0 for AF_UNIX)
  std::string local_name;         // own address: sun_path bytes ("" = unnamed)
  std::string peer_name;          // peer's address captured at connect time
  VfsNode* bound_node = nullptr;  // socket inode created by bind(path)
  bool dead = false;              // released (last description closed)
  bool rcv_shutdown = false;      // Linux RCV_SHUTDOWN
  bool send_shutdown = false;     // Linux SEND_SHUTDOWN
  int max_backlog = 0;            // listen() backlog
  int so_error = 0;               // pending SO_ERROR (positive errno)
  bool passcred = false;          // SO_PASSCRED
  bool passsec = false;           // SO_PASSSEC
  int sndbuf = 212992;            // SO_SNDBUF (bytes, already doubled)
  int rcvbuf = 212992;            // SO_RCVBUF
  int rcvlowat = 1;               // SO_RCVLOWAT
  int peek_off = -1;              // SO_PEEK_OFF (-1 = disabled)
  int64_t rcvtimeo_ns = -1;       // SO_RCVTIMEO (-1 = none, 0 = don't wait)
  int64_t sndtimeo_ns = -1;       // SO_SNDTIMEO
  int linger_onoff = 0;           // SO_LINGER (stored; no effect on AF_UNIX)
  int linger_secs = 0;
  std::map<int, int> plain_opts;  // stored-only SOL_SOCKET ints (SO_KEEPALIVE...)
  bool has_peer_cred = false;     // SO_PEERCRED / SO_PEERGROUPS
  int32_t peer_cred_pid = 0;
  uint32_t peer_cred_uid = 0;
  uint32_t peer_cred_gid = 0;
  std::vector<uint32_t> peer_groups;
  size_t wmem = 0;  // bytes sent by this socket still queued at a receiver
  std::atomic<uint64_t> activity{0};  // EPOLLET edge counter
};

// Epoll interest entry. Holds only a weak reference so that a watched
// description is released (and its peers see EOF/HUP) when the last fd
// referring to it is closed, exactly like Linux.
struct EpollEntry {
  int fd = -1;
  std::weak_ptr<FileDescription> desc;
  uint32_t events = 0;
  uint64_t data = 0;
  bool disabled = false;       // EPOLLONESHOT fired; re-armed by EPOLL_CTL_MOD
  uint32_t last_reported = 0;  // EPOLLET bookkeeping
  uint64_t last_counter = 0;   // EPOLLET activity counter at last report
};

struct EpollState {
  std::mutex mtx;
  // Keyed by (fd, description) like Linux's (fd, struct file) pair.
  std::map<std::pair<int, FileDescription*>, EpollEntry> entries;
};

enum class FdKind {
  kVfsFile,
  kSerialConsole,
  kDevNull,
  kDevZero,
  kDevRandom,
  kDevFb0,
  kDevInputEvent0,
  kPipeRead,
  kPipeWrite,
  kUnixSocket,
  kEpoll,
  kEventFd,
  kTimerFd,
  kSignalFd,
  kInotify,
  kPtyMaster,
  kPtySlave,
  kProcDynamic,
  kDevFull,
};

// IPC object state (eventfd/timerfd/signalfd/inotify), defined in
// syscalls_ipc.cc.
struct IpcObjectState;
struct InetSocketState;

void NotifyPollWaiters();

struct FileDescription {
  FdKind kind = FdKind::kVfsFile;
  uint32_t flags = 0;
  std::mutex mtx;
  VfsNode* node = nullptr;
  std::string path;
  size_t offset = 0;
  std::string dynamic_content;

  std::shared_ptr<PipeState> pipe;
  std::shared_ptr<UnixSocketState> sock;
  std::shared_ptr<InetSocketState> inet_sock;
  std::shared_ptr<EpollState> epoll;
  std::shared_ptr<PtyPair> pty;

  uint64_t eventfd_val = 0;
  uint64_t timerfd_interval_ns = 0;
  uint64_t timerfd_next_deadline_ns = 0;
  uint64_t signalfd_mask = 0;

  // Keeps an anonymous VfsNode (memfd_create) alive while any description
  // refers to it; such nodes are not linked into the VFS namespace.
  std::shared_ptr<VfsNode> owned_node;

  std::shared_ptr<IpcObjectState> ipc;  // eventfd/timerfd/signalfd/inotify
  uint64_t anon_ino = 0;  // inode number for objects without a VfsNode
  int owner_pid = 0;      // F_SETOWN (positive pid, negative pgid)
  std::string dir_last_name;  // getdents cursor (name of last entry returned)
  uint64_t dir_last_off = 0;  // d_off of that entry
  int64_t create_time_ns = 0;  // wall clock at creation (fstat times for pipes/sockets/anon inodes)

  // Releases pipe/socket/pty/lock state; defined in syscalls_fs.cc.
  ~FileDescription();
};

struct OpenFd {
  std::shared_ptr<FileDescription> desc;
  bool cloexec = false;
};

struct SharedMapping {
  uintptr_t vaddr = 0;
  size_t size = 0;
  uint32_t prot = 0;
  zx_handle_t vmo = ZX_HANDLE_INVALID;
  uint64_t vmo_offset = 0;
  VfsNode* backing_node = nullptr;
  zx_koid_t vmo_koid = ZX_KOID_INVALID;
};

struct LinuxThread {
  int tid = 1;
  LinuxProcess* proc = nullptr;
  // Duplicate of this thread's Zircon handle (for zx_restricted_kick and
  // ZX_INFO_TASK_RUNTIME); guarded by wake_mtx.
  zx_handle_t zx_thread = ZX_HANDLE_INVALID;
  zx_handle_t state_vmo = ZX_HANDLE_INVALID;
  zx_restricted_state_t* state = nullptr;
  uintptr_t state_map_addr = 0;
  uintptr_t clear_child_tid = 0;
  // Blocked signals. Atomic because signal senders read it cross-thread.
  std::atomic<uint64_t> sigmask{0};
  uintptr_t altstack_sp = 0;
  size_t altstack_size = 0;
  int32_t altstack_flags = 2;  // SS_DISABLE
  // Initial register state for a thread that has not started running yet.
  zx_restricted_state_t saved_sig_state{};
  uint64_t orig_x0 = 0;  // original x[0] before syscall return overwrote it (AArch64 restart)
  std::atomic<uint64_t> last_sc{0};
  std::atomic<bool> in_syscall{false};

  // --- Signal, wakeup and exit state (signals.cc, restricted_exec.cc) ---
  std::atomic<uint64_t> pending_signals{0};  // thread-directed pending set
  std::deque<LinuxSiginfo> sigqueue;  // siginfo records; guarded by proc->sig_mtx
  // Set by syscalls that install a temporary mask (sigsuspend, ppoll, ...):
  // on return to user mode the mask is restored (or saved into the frame).
  uint64_t saved_sigmask = 0;
  bool restore_sigmask = false;
  std::atomic<bool> kill_requested{false};  // SYS_exit, exit_group, execve
  int exit_code = 0;                        // status passed to SYS_exit
  std::mutex wake_mtx;  // guards zx_thread and wake_event against close
  zx_handle_t wake_event = ZX_HANDLE_INVALID;  // ZX_USER_SIGNAL_0: re-check
  void* xstate = nullptr;  // user x87/SSE/AVX state while in normal mode
  uintptr_t set_child_tid = 0;  // CLONE_CHILD_SETTID, written by the child
  uintptr_t robust_list_head = 0;
  size_t robust_list_len = 0;
  int restart_kind = 0;  // restart_syscall(2) block
  uint64_t restart_args[6] = {};
  uint64_t fault_trapno = 0;  // last hardware exception (sigcontext)
  uint64_t fault_err = 0;
  uint64_t fault_cr2 = 0;
  // Per-thread name (prctl PR_SET_NAME, max 15 chars). Empty means "same as
  // the process comm". /proc/<pid>/comm shows the main thread's name.
  std::string comm;
};

struct LinuxProcess {
  int pid = 1;
  int ppid = 0;
  int pgid = 1;
  int sid = 1;
  uint32_t umask_val = 0022;
  zx_handle_t zx_proc = ZX_HANDLE_INVALID;
  zx_handle_t restricted_vmar = ZX_HANDLE_INVALID;
  uintptr_t vmar_base = 0;
  size_t vmar_len = 0;

  std::string exe_path = "/init";
  std::string comm = "init";
  std::string cmdline;
  std::string cwd = "/";
  std::string last_path;
  std::vector<std::shared_ptr<LinuxThread>> threads;

  uintptr_t brk_start = 0x30000000ULL;
  uintptr_t brk_current = 0x30000000ULL;
  uintptr_t brk_mapped_end = 0x30000000ULL;
  uintptr_t mmap_cursor = 0x40000000ULL;

  std::recursive_mutex fd_mtx;
  std::map<int, OpenFd> fds;

  std::mutex map_mtx;
  std::vector<SharedMapping> shared_mappings;
  // Mapping table of the restricted VMAR (mem_space.h); guarded by map_mtx.
  std::shared_ptr<AddressSpaceState> mm;

  LinuxSigaction sigactions[65]{};  // guarded by sig_mtx
  std::atomic<uint64_t> pending_signals{0};  // process-directed pending set

  // exited: the process is exiting (exit_group, fatal signal, last thread
  // exited); exit_status holds the Linux wait(2) status word.
  std::atomic<bool> exited{false};
  int exit_status = 0;
  bool reaped = false;

  std::mutex vfork_mtx;
  std::condition_variable vfork_cv;
  bool vfork_done = false;

  // --- Signal / job control / exit state (signals.cc, syscalls_proc.cc) ---
  std::mutex sig_mtx;  // guards sigactions, sigqueues, job-control state
  std::deque<LinuxSiginfo> sigqueue;  // siginfo records for pending_signals
  std::condition_variable stop_cv;
  bool stopped = false;  // group stop (SIGSTOP/SIGTSTP/...) in effect
  int stop_sig = 0;
  int wait_report = 0;   // unreported stop/continue for waitpid(2)
  int exit_signal = 17;  // sent to the parent on exit (clone exit_signal)
  std::atomic<bool> zombie{false};  // all threads gone, not yet reaped
  int leader_exit_code = 0;
  int pdeath_signal = 0;
  bool child_subreaper = false;
  int64_t cpu_time_ns = 0;      // own CPU time, captured when it died
  int64_t children_cpu_ns = 0;  // CPU time of reaped descendants
  std::shared_ptr<LinuxThread> vfork_parent;
  int nice = 0;
  uint32_t membarrier_registered = 0;
  uint32_t ruid = 0, euid = 0, suid = 0, fsuid = 0;
  uint32_t rgid = 0, egid = 0, sgid = 0, fsgid = 0;
  std::vector<uint32_t> groups;
  bool rlimits_init = false;
  uint64_t rlimits[16][2] = {};  // [resource][cur, max]
  bool dumpable = true;
  bool no_new_privs = false;
  bool keepcaps = false;
};

// Global process table & synchronization
extern std::recursive_mutex g_proc_mtx;
extern std::map<int, std::shared_ptr<LinuxProcess>> g_processes;
extern std::condition_variable_any g_wait4_cv;
extern std::mutex g_poll_mtx;
extern std::condition_variable g_poll_cv;
extern std::atomic<uint64_t> g_poll_seq;

int AllocateNextPid();
int AllocateNextTid();
void NotifyPollWaiters();

// Hardware FB & Input API (hardware_fb_input.cc)
void InitHardware();
void SerialWriteBytes(const void* buf, size_t len);
bool SerialHasInput();
size_t SerialReadInput(void* buf, size_t max_len, bool block);
LinuxTermios& GetConsoleTermios();

struct FbHardwareInfo {
  bool active = false;
  uint64_t phys_addr = 0;
  uint32_t width = 1024;
  uint32_t height = 768;
  uint32_t stride_bytes = 1024 * 4;
  uint32_t size_bytes = 1024 * 768 * 4;
  zx_handle_t phys_vmo = ZX_HANDLE_INVALID;
  uint32_t* mapped_fb = nullptr;
  uint64_t vram_bytes = 0;  // mappable framebuffer memory (PCI BAR size)
};

const FbHardwareInfo& GetFbHardwareInfo();
void FbWritePixels(size_t offset, const void* src, size_t len);
void FbReadPixels(size_t offset, void* dst, size_t len);
bool InputHasEvents();
size_t InputReadEvents(void* dst, size_t max_bytes, bool block);

// ELF Loader (elf_loader.cc)
int64_t LoadLinuxExecutable(LinuxProcess* proc, const std::string& path,
                            const std::vector<std::string>& args,
                            const std::vector<std::string>& env,
                            uintptr_t* out_entry, uintptr_t* out_sp);

// Address-space lifecycle (mem_space.cc).
// Copies the parent's restricted address space into `child` for fork():
// shared mappings are mapped shared, private mappings become copy-on-write
// snapshots with the same protections, and the mapping table is copied.
bool CloneProcessAddressSpace(LinuxProcess* parent, LinuxProcess* child);
// Unmaps all Linux mappings of an exiting process and drops its SysV shm
// attachments.
void ReleaseProcessAddressSpace(LinuxProcess* proc);

// Restricted execution loop (restricted_exec.cc)
void RunLinuxThreadLoop(std::shared_ptr<LinuxProcess> proc,
                        std::shared_ptr<LinuxThread> thrd);
void SpawnLinuxThreadInProcess(std::shared_ptr<LinuxProcess> proc,
                               std::shared_ptr<LinuxThread> thrd);

// Syscall dispatchers
int64_t DispatchLinuxSyscall(LinuxProcess* proc, LinuxThread* thrd);
int64_t HandleFsSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                        uint64_t a0, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5);
int64_t HandleMemSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                         uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5);
int64_t HandleIpcSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                         uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5);
int64_t HandleProcSyscall(LinuxProcess* proc, LinuxThread* thrd, uint64_t sc,
                          uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5);

// Poll helper shared between syscalls_fs.cc and syscalls_ipc.cc
uint32_t CheckFdPollEvents(FileDescription* desc);
ssize_t ReadFileDescription(LinuxProcess* proc, FileDescription* desc, void* buf, size_t count);
ssize_t WriteFileDescription(LinuxProcess* proc, FileDescription* desc, const void* buf, size_t count);

// Futex helpers (futex.cc)
int64_t SysFutex(LinuxProcess* proc, LinuxThread* thrd, uintptr_t uaddr,
                 int op, uint32_t val, uintptr_t timeout_addr,
                 uintptr_t uaddr2, uint32_t val3);
// Wakes up to max_wake waiters on uaddr; returns the number woken.
int FutexWakeAddress(LinuxProcess* proc, uintptr_t uaddr, int max_wake);

// Signal API (signals.cc). All senders record a siginfo, wake sleepers in
// futex/wait4/nanosleep, zx_restricted_kick running threads, and call
// NotifyPollWaiters().
void QueueSignalToPid(int pid, int sig);  // process-directed, SI_KERNEL
void QueueSignalToProcessGroup(int pgid, int sig);
void QueueSignalToThread(LinuxProcess* proc, LinuxThread* thrd, int sig);
// True if an unblocked signal is pending for thrd, or the thread/process is
// being torn down. Blocking syscalls should then return kERESTARTSYS.
bool ShimSignalPending(LinuxProcess* proc, LinuxThread* thrd);
// Union of the process-directed and thread-directed pending sets.
uint64_t ShimPendingSignalSet(LinuxProcess* proc, LinuxThread* thrd);
// Dequeues the lowest pending signal in `mask` (e.g. for signalfd); returns
// the signal number, or 0 if none is pending.
int ShimDequeueSignal(LinuxProcess* proc, LinuxThread* thrd, uint64_t mask,
                      LinuxSiginfo* out);

// Wall clock (time.cc): Linux CLOCK_REALTIME in ns. Seeded from the CMOS RTC
// at boot; the same clock is installed as libc's UTC reference.
int64_t ShimRealtimeNs();

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_SHIM_COMMON_H_
