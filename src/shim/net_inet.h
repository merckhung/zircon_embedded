#ifndef ZIRCON_EMBEDDED_SRC_SHIM_NET_INET_H_
#define ZIRCON_EMBEDDED_SRC_SHIM_NET_INET_H_

#include "src/shim/shim_common.h"

struct InetSocketState;

// Probes PCI for Realtek RTL8139 (10ec:8139), initializes DMA rings via Zircon
// BTI/IOMMU, and starts the background RX/timer thread. Safe to call even if
// no NIC is attached (loopback 127.0.0.1 remains available).
void NetInetInit();

// Returns true if an RTL8139 NIC was discovered and initialized.
bool NetInetHasNic();

// Generates /proc/net/dev and /proc/net/route contents from real NIC/stack state.
std::string NetGenerateProcNetDev();
std::string NetGenerateProcNetRoute();

// Socket lifecycle and syscall entry points for AF_INET (2) and AF_NETLINK (16).
int64_t InetSysSocket(LinuxProcess* proc, int domain, int type_and_flags,
                      int protocol);
int64_t InetSysBind(LinuxProcess* proc, FileDescription* desc,
                    uintptr_t addr_ptr, int addrlen);
int64_t InetSysConnect(LinuxProcess* proc, LinuxThread* thrd,
                       FileDescription* desc, uintptr_t addr_ptr, int addrlen);
int64_t InetSysListen(FileDescription* desc, int backlog);
int64_t InetSysAccept(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t addr_ptr,
                      uintptr_t addrlen_ptr, int flags);
ssize_t InetSocketRead(LinuxProcess* proc, LinuxThread* thrd,
                       FileDescription* desc, void* buf, size_t count);
ssize_t InetSocketWrite(LinuxProcess* proc, LinuxThread* thrd,
                        FileDescription* desc, const void* buf, size_t count);
int64_t InetSysSendto(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t buf_ptr, size_t len,
                      int flags, uintptr_t addr_ptr, int addrlen);
int64_t InetSysRecvfrom(LinuxProcess* proc, LinuxThread* thrd,
                        FileDescription* desc, uintptr_t buf_ptr, size_t len,
                        int flags, uintptr_t addr_ptr, uintptr_t addrlen_ptr);
int64_t InetDoSendmsg(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t msg_ptr, int flags);
int64_t InetDoRecvmsg(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t msg_ptr, int flags);
int64_t InetSysShutdown(FileDescription* desc, int how);
int64_t InetSysGetName(LinuxProcess* proc, FileDescription* desc,
                       uintptr_t addr_ptr, uintptr_t len_ptr, bool peer);
int64_t InetSysSetsockopt(LinuxProcess* proc, FileDescription* desc, int level,
                          int optname, uintptr_t optval, int optlen);
int64_t InetSysGetsockopt(LinuxProcess* proc, FileDescription* desc, int level,
                          int optname, uintptr_t optval, uintptr_t optlen_ptr);
int16_t InetSocketPollEvents(FileDescription* desc, uint64_t* activity);
int InetSocketBytesAvailable(FileDescription* desc);
void InetSocketOnDescriptionDestroyed(FileDescription* desc);
int64_t InetSocketIoctl(LinuxProcess* proc, FileDescription* desc,
                        uint64_t cmd, uint64_t arg);

#endif  // ZIRCON_EMBEDDED_SRC_SHIM_NET_INET_H_
