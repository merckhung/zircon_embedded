// Realtek RTL8139 (10ec:8139) PCI NIC driver, IPv4/ARP/ICMP/UDP/TCP stack,
// and AF_INET / AF_NETLINK (NETLINK_ROUTE) Linux socket implementation on
// Fuchsia Zircon.
#include "src/shim/net_inet.h"

#include <zircon/syscalls/iommu.h>
#include <zircon/syscalls/object.h>
#include <zircon/syscalls/resource.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "src/shim/fd_ops.h"
#include "src/shim/vfs.h"

namespace {

constexpr int kSIGPIPE = 13;

inline bool CopyFromUser(LinuxProcess* proc, void* dst, uintptr_t src,
                         size_t len) {
  (void)proc;
  if (len == 0) return true;
  if (src == 0) return false;
  memcpy(dst, reinterpret_cast<const void*>(src), len);
  return true;
}

inline bool CopyToUser(LinuxProcess* proc, uintptr_t dst, const void* src,
                       size_t len) {
  (void)proc;
  if (len == 0) return true;
  if (dst == 0) return false;
  memcpy(reinterpret_cast<void*>(dst), src, len);
  return true;
}

// --- Register access helpers (x86 I/O ports vs ARM64 MMIO) -----------------

uintptr_t g_rtl_mmio_base = 0;

#if defined(__x86_64__)
inline void Out8(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
inline void Out16(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
inline void Out32(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
inline uint8_t In8(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
inline uint16_t In16(uint16_t port) {
  uint16_t ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
inline uint32_t In32(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
#else
inline void Out8(uint16_t off, uint8_t val) {
  if (g_rtl_mmio_base) {
    *reinterpret_cast<volatile uint8_t*>(g_rtl_mmio_base + (off & 0xFFU)) = val;
    __asm__ volatile("dsb sy" ::: "memory");
  }
}
inline void Out16(uint16_t off, uint16_t val) {
  if (g_rtl_mmio_base) {
    *reinterpret_cast<volatile uint16_t*>(g_rtl_mmio_base + (off & 0xFFU)) = val;
    __asm__ volatile("dsb sy" ::: "memory");
  }
}
inline void Out32(uint16_t off, uint32_t val) {
  if (g_rtl_mmio_base) {
    *reinterpret_cast<volatile uint32_t*>(g_rtl_mmio_base + (off & 0xFFU)) = val;
    __asm__ volatile("dsb sy" ::: "memory");
  }
}
inline uint8_t In8(uint16_t off) {
  if (!g_rtl_mmio_base) return 0xFF;
  return *reinterpret_cast<volatile uint8_t*>(g_rtl_mmio_base + (off & 0xFFU));
}
inline uint16_t In16(uint16_t off) {
  if (!g_rtl_mmio_base) return 0xFFFF;
  return *reinterpret_cast<volatile uint16_t*>(g_rtl_mmio_base + (off & 0xFFU));
}
inline uint32_t In32(uint16_t off) {
  if (!g_rtl_mmio_base) return 0xFFFFFFFF;
  return *reinterpret_cast<volatile uint32_t*>(g_rtl_mmio_base + (off & 0xFFU));
}
#endif

uint32_t PciRead32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t addr = (1U << 31) | (static_cast<uint32_t>(bus) << 16) |
                  (static_cast<uint32_t>(slot) << 11) |
                  (static_cast<uint32_t>(func) << 8) | (offset & 0xFCU);
  Out32(0xCF8, addr);
  return In32(0xCFC);
}

void PciWrite32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                uint32_t val) {
  uint32_t addr = (1U << 31) | (static_cast<uint32_t>(bus) << 16) |
                  (static_cast<uint32_t>(slot) << 11) |
                  (static_cast<uint32_t>(func) << 8) | (offset & 0xFCU);
  Out32(0xCF8, addr);
  Out32(0xCFC, val);
}

// --- Endian helpers --------------------------------------------------------

inline uint16_t Bswap16(uint16_t x) { return __builtin_bswap16(x); }
inline uint32_t Bswap32(uint32_t x) { return __builtin_bswap32(x); }
inline uint16_t Htons(uint16_t x) { return Bswap16(x); }
inline uint16_t Ntohs(uint16_t x) { return Bswap16(x); }
inline uint32_t Htonl(uint32_t x) { return Bswap32(x); }
inline uint32_t Ntohl(uint32_t x) { return Bswap32(x); }

// --- Linux socket / netlink / ioctl constants ------------------------------

constexpr int kAfUnspec = 0;
constexpr int kAfInet = 2;
constexpr int kAfNetlink = 16;

constexpr int kSockStream = 1;
constexpr int kSockDgram = 2;
constexpr int kSockRaw = 3;
constexpr int kSockTypeMask = 0xF;
constexpr int kSockNonblock = 04000;
constexpr int kSockCloexec = 02000000;

constexpr int kIpprotoIcmp = 1;
constexpr int kIpprotoTcp = 6;
constexpr int kIpprotoUdp = 17;

constexpr int kSolSocket = 1;

constexpr int kSoReuseaddr = 2;
constexpr int kSoType = 3;
constexpr int kSoError = 4;
constexpr int kSoBroadcast = 6;
constexpr int kSoSndbuf = 7;
constexpr int kSoRcvbuf = 8;
constexpr int kSoKeepalive = 9;
constexpr int kSoReuseport = 15;
constexpr int kSoRcvlowat = 18;
constexpr int kSoRcvtimeoOld = 20;
constexpr int kSoSndtimeoOld = 21;
constexpr int kSoAcceptconn = 30;
constexpr int kSoSndbufforce = 32;
constexpr int kSoRcvbufforce = 33;
constexpr int kSoProtocol = 38;
constexpr int kSoDomain = 39;
constexpr int kSoRcvtimeoNew = 66;
constexpr int kSoSndtimeoNew = 67;

constexpr int kMsgPeek = 0x2;
constexpr int kMsgDontwait = 0x40;
constexpr int kMsgWaitall = 0x100;
constexpr int kMsgNosignal = 0x4000;

// Linux errno values (negative)
constexpr int64_t kEAGAIN_ = -11;
constexpr int64_t kEFAULT_ = -14;
constexpr int64_t kENODEV_ = -19;
constexpr int64_t kEINVAL_ = -22;
constexpr int64_t kEPIPE_ = -32;
constexpr int64_t kENOTSOCK_ = -88;
constexpr int64_t kEDESTADDRREQ_ = -89;
constexpr int64_t kEMSGSIZE_ = -90;
constexpr int64_t kEPROTONOSUPPORT_ = -93;
constexpr int64_t kEOPNOTSUPP_ = -95;
constexpr int64_t kEAFNOSUPPORT_ = -97;
constexpr int64_t kENETUNREACH_ = -101;
constexpr int64_t kECONNABORTED_ = -103;
constexpr int64_t kECONNRESET_ = -104;
constexpr int64_t kEISCONN_ = -106;
constexpr int64_t kENOTCONN_ = -107;
constexpr int64_t kETIMEDOUT_ = -110;
constexpr int64_t kECONNREFUSED_ = -111;
constexpr int64_t kEALREADY_ = -114;
constexpr int64_t kEINPROGRESS_ = -115;

// Wire headers
#pragma pack(push, 1)
struct EthHdr {
  uint8_t dst[6];
  uint8_t src[6];
  uint16_t ethertype;
};

struct ArpPkt {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t oper;
  uint8_t sha[6];
  uint32_t spa;
  uint8_t tha[6];
  uint32_t tpa;
};

struct Ipv4Hdr {
  uint8_t ver_ihl;
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t proto;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
};

struct IcmpHdr {
  uint8_t type;
  uint8_t code;
  uint16_t check;
  uint16_t id;
  uint16_t seq;
};

struct UdpHdr {
  uint16_t sport;
  uint16_t dport;
  uint16_t len;
  uint16_t check;
};

struct TcpHdr {
  uint16_t sport;
  uint16_t dport;
  uint32_t seq;
  uint32_t ack_seq;
  uint8_t doff_res;
  uint8_t flags;
  uint16_t window;
  uint16_t check;
  uint16_t urg_ptr;
};

struct SockaddrIn {
  uint16_t sin_family;
  uint16_t sin_port;  // network byte order
  uint32_t sin_addr;  // network byte order
  uint8_t sin_zero[8];
};

struct SockaddrNl {
  uint16_t nl_family;
  uint16_t nl_pad;
  uint32_t nl_pid;
  uint32_t nl_groups;
};

struct NlMsgHdr {
  uint32_t nlmsg_len;
  uint16_t nlmsg_type;
  uint16_t nlmsg_flags;
  uint32_t nlmsg_seq;
  uint32_t nlmsg_pid;
};

struct IfInfoMsg {
  uint8_t ifi_family;
  uint8_t ifi_pad;
  uint16_t ifi_type;
  int32_t ifi_index;
  uint32_t ifi_flags;
  uint32_t ifi_change;
};

struct IfAddrMsg {
  uint8_t ifa_family;
  uint8_t ifa_prefixlen;
  uint8_t ifa_flags;
  uint8_t ifa_scope;
  uint32_t ifa_index;
};

struct RtAttr {
  uint16_t rta_len;
  uint16_t rta_type;
};
#pragma pack(pop)

constexpr uint8_t kTcpFin = 0x01;
constexpr uint8_t kTcpSyn = 0x02;
constexpr uint8_t kTcpRst = 0x04;
constexpr uint8_t kTcpPsh = 0x08;
constexpr uint8_t kTcpAck = 0x10;

// --- RTL8139 hardware & network stack state --------------------------------

struct Rtl8139Device {
  bool active = false;
  uint16_t io_base = 0;
  uint8_t mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
  zx_handle_t bti = ZX_HANDLE_INVALID;
  zx_handle_t dma_vmo = ZX_HANDLE_INVALID;
  zx_handle_t pmt = ZX_HANDLE_INVALID;
  zx_paddr_t dma_phys = 0;
  uint8_t* dma_virt = nullptr;
  uint8_t* rx_ring = nullptr;
  zx_paddr_t rx_phys = 0;
  uint8_t* tx_bufs[4] = {};
  zx_paddr_t tx_phys[4] = {};
  uint32_t rx_offset = 0;
  uint32_t tx_cur = 0;
  std::mutex hw_mtx;
  std::deque<std::vector<uint8_t>> tx_queue;
  std::condition_variable tx_cv;

  std::atomic<uint64_t> rx_bytes{0};
  std::atomic<uint64_t> rx_packets{0};
  std::atomic<uint64_t> tx_bytes{0};
  std::atomic<uint64_t> tx_packets{0};
};

Rtl8139Device g_nic;

// Host-byte-order IPv4 config (QEMU SLIRP defaults)
uint32_t g_local_ip = 0x0A00020FU;    // 10.0.2.15
uint32_t g_netmask = 0xFFFFFF00U;     // 255.255.255.0
uint32_t g_gateway = 0x0A000202U;     // 10.0.2.2
uint32_t g_dns_server = 0x0A000203U;  // 10.0.2.3
constexpr uint32_t kLoopbackIp = 0x7F000001U;  // 127.0.0.1

struct ArpEntry {
  uint32_t ip_host = 0;
  uint8_t mac[6] = {};
  bool valid = false;
};

constexpr size_t kArpTableSize = 32;
ArpEntry g_arp_table[kArpTableSize];
std::condition_variable g_arp_cv;

std::mutex g_net_mtx;
std::atomic<uint16_t> g_ip_id{1};
std::atomic<uint16_t> g_ephemeral_port{49152};
std::atomic<uint32_t> g_iss{0x13572468U};
std::atomic<uint32_t> g_next_nl_pid{1000};

// --- Checksums -------------------------------------------------------------

uint16_t IpChecksum(const void* data, size_t len) {
  const auto* p = static_cast<const uint8_t*>(data);
  uint32_t sum = 0;
  while (len > 1) {
    uint16_t w = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    sum += w;
    p += 2;
    len -= 2;
  }
  if (len == 1) {
    sum += static_cast<uint16_t>(p[0]) << 8;
  }
  while (sum >> 16) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return Htons(static_cast<uint16_t>(~sum & 0xFFFFU));
}

uint16_t L4Checksum(uint32_t saddr_net, uint32_t daddr_net, uint8_t proto,
                    const void* l4_data, size_t l4_len) {
  uint32_t sum = 0;
  uint32_t s_host = Ntohl(saddr_net);
  uint32_t d_host = Ntohl(daddr_net);
  sum += (s_host >> 16) & 0xFFFFU;
  sum += s_host & 0xFFFFU;
  sum += (d_host >> 16) & 0xFFFFU;
  sum += d_host & 0xFFFFU;
  sum += proto;
  sum += static_cast<uint32_t>(l4_len);

  const auto* p = static_cast<const uint8_t*>(l4_data);
  size_t rem = l4_len;
  while (rem > 1) {
    uint16_t w = (static_cast<uint16_t>(p[0]) << 8) | p[1];
    sum += w;
    p += 2;
    rem -= 2;
  }
  if (rem == 1) {
    sum += static_cast<uint16_t>(p[0]) << 8;
  }
  while (sum >> 16) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  uint16_t res = static_cast<uint16_t>(~sum & 0xFFFFU);
  if (proto == kIpprotoUdp && res == 0) res = 0xFFFFU;
  return Htons(res);
}

// --- RTL8139 TX ------------------------------------------------------------

bool Rtl8139SendFrame(const void* frame, size_t len) {
  if (!g_nic.active || len == 0 || len > 1792) return false;
  const auto* p = static_cast<const uint8_t*>(frame);
  {
    std::lock_guard<std::mutex> hw_lock(g_nic.hw_mtx);
    if (g_nic.tx_queue.size() >= 1024) return false;
    g_nic.tx_queue.emplace_back(p, p + len);
  }
  g_nic.tx_cv.notify_one();
  return true;
}

void TxNicWorker() {
  for (;;) {
    std::vector<uint8_t> pkt;
    {
      std::unique_lock<std::mutex> hw_lock(g_nic.hw_mtx);
      g_nic.tx_cv.wait(hw_lock, [] { return !g_nic.tx_queue.empty(); });
      pkt = std::move(g_nic.tx_queue.front());
      g_nic.tx_queue.pop_front();

      uint32_t slot = g_nic.tx_cur & 3U;
      uint16_t tsd_reg = static_cast<uint16_t>(g_nic.io_base + 0x10 + slot * 4);
      uint16_t tsad_reg = static_cast<uint16_t>(g_nic.io_base + 0x20 + slot * 4);

      size_t len = pkt.size();
      size_t tx_len = std::max<size_t>(len, 60);
      memset(g_nic.tx_bufs[slot], 0, tx_len);
      memcpy(g_nic.tx_bufs[slot], pkt.data(), len);

      Out32(tsad_reg, static_cast<uint32_t>(g_nic.tx_phys[slot]));
      // Bit 13 = 0 starts DMA transmission; bits 0..12 = length.
      Out32(tsd_reg, static_cast<uint32_t>(tx_len & 0x1FFFU));

      for (int i = 0; i < 10000; ++i) {
        uint32_t tsd = In32(tsd_reg);
        if (tsd & (1U << 15)) break;  // TOK
      }
      g_nic.tx_cur = (slot + 1) & 3U;
      g_nic.tx_packets.fetch_add(1);
      g_nic.tx_bytes.fetch_add(len);
    }
  }
}

// --- ARP -------------------------------------------------------------------

bool LookupArpLocked(uint32_t ip_host, uint8_t out_mac[6]) {
  if (ip_host == 0xFFFFFFFFU || (ip_host & 0xFFU) == 0xFFU) {
    memset(out_mac, 0xFF, 6);
    return true;
  }
  for (const auto& e : g_arp_table) {
    if (e.valid && e.ip_host == ip_host) {
      memcpy(out_mac, e.mac, 6);
      return true;
    }
  }
  return false;
}

void UpdateArpLocked(uint32_t ip_host, const uint8_t mac[6]) {
  if (ip_host == 0) return;
  size_t free_idx = 0;
  for (size_t i = 0; i < kArpTableSize; ++i) {
    if (g_arp_table[i].valid && g_arp_table[i].ip_host == ip_host) {
      memcpy(g_arp_table[i].mac, mac, 6);
      g_arp_cv.notify_all();
      return;
    }
    if (!g_arp_table[i].valid) free_idx = i;
  }
  g_arp_table[free_idx].valid = true;
  g_arp_table[free_idx].ip_host = ip_host;
  memcpy(g_arp_table[free_idx].mac, mac, 6);
  g_arp_cv.notify_all();
}

void SendArpRequest(uint32_t target_ip_host) {
  if (!g_nic.active) return;
  uint8_t frame[60] = {};
  auto* eth = reinterpret_cast<EthHdr*>(frame);
  memset(eth->dst, 0xFF, 6);
  memcpy(eth->src, g_nic.mac, 6);
  eth->ethertype = Htons(0x0806);

  auto* arp = reinterpret_cast<ArpPkt*>(frame + sizeof(EthHdr));
  arp->htype = Htons(1);
  arp->ptype = Htons(0x0800);
  arp->hlen = 6;
  arp->plen = 4;
  arp->oper = Htons(1);  // request
  memcpy(arp->sha, g_nic.mac, 6);
  arp->spa = Htonl(g_local_ip);
  memset(arp->tha, 0x00, 6);
  arp->tpa = Htonl(target_ip_host);
  Rtl8139SendFrame(frame, sizeof(EthHdr) + sizeof(ArpPkt));
}

bool ResolveNextHopMac(std::unique_lock<std::mutex>& net_lock,
                       uint32_t dst_ip_host, uint8_t out_mac[6]) {
  uint32_t next_hop = dst_ip_host;
  if ((dst_ip_host & g_netmask) != (g_local_ip & g_netmask)) {
    next_hop = g_gateway;
  }
  if (LookupArpLocked(next_hop, out_mac)) return true;

  for (int attempt = 0; attempt < 5; ++attempt) {
    net_lock.unlock();
    SendArpRequest(next_hop);
    net_lock.lock();
    if (g_arp_cv.wait_for(net_lock, std::chrono::milliseconds(150), [&]() {
          return LookupArpLocked(next_hop, out_mac);
        })) {
      return true;
    }
  }
  // Fallback if QEMU SLIRP is reachable at standard SLIRP gateway MAC
  // 52:55:0a:00:02:02 when ARP reply was delayed.
  if (next_hop == g_gateway) {
    const uint8_t slirp_gw[6] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    memcpy(out_mac, slirp_gw, 6);
    UpdateArpLocked(next_hop, slirp_gw);
    return true;
  }
  return false;
}

bool SendIpv4PacketLocked(std::unique_lock<std::mutex>& net_lock,
                          uint32_t dst_ip_host, uint8_t proto,
                          const void* payload, size_t payload_len) {
  if (!g_nic.active || payload_len + sizeof(EthHdr) + sizeof(Ipv4Hdr) > 1518) {
    return false;
  }
  uint8_t dst_mac[6];
  if (!ResolveNextHopMac(net_lock, dst_ip_host, dst_mac)) return false;

  uint8_t frame[1536];
  auto* eth = reinterpret_cast<EthHdr*>(frame);
  memcpy(eth->dst, dst_mac, 6);
  memcpy(eth->src, g_nic.mac, 6);
  eth->ethertype = Htons(0x0800);

  auto* ip = reinterpret_cast<Ipv4Hdr*>(frame + sizeof(EthHdr));
  ip->ver_ihl = 0x45;
  ip->tos = 0;
  ip->tot_len = Htons(static_cast<uint16_t>(sizeof(Ipv4Hdr) + payload_len));
  ip->id = Htons(g_ip_id.fetch_add(1));
  ip->frag_off = Htons(0x4000);  // DF
  ip->ttl = 64;
  ip->proto = proto;
  ip->check = 0;
  ip->saddr = Htonl(g_local_ip);
  ip->daddr = Htonl(dst_ip_host);
  ip->check = IpChecksum(ip, sizeof(Ipv4Hdr));

  memcpy(frame + sizeof(EthHdr) + sizeof(Ipv4Hdr), payload, payload_len);
  size_t total = sizeof(EthHdr) + sizeof(Ipv4Hdr) + payload_len;
  net_lock.unlock();
  bool ok = Rtl8139SendFrame(frame, total);
  net_lock.lock();
  return ok;
}

}  // namespace

// --- Socket state definition -----------------------------------------------

enum class TcpState {
  kClosed = 0,
  kListen,
  kSynSent,
  kEstablished,
  kFinWait1,
  kFinWait2,
  kCloseWait,
  kLastAck,
  kTimeWait,
};

struct InetDatagram {
  std::vector<uint8_t> data;
  uint32_t src_ip_host = 0;
  uint16_t src_port_host = 0;
};

struct TcpUnackedSeg {
  uint32_t seq_start = 0;  // host order
  uint32_t seq_end = 0;    // host order (includes SYN/FIN count + payload)
  uint8_t flags = 0;
  std::vector<uint8_t> payload;
  int64_t last_sent_ns = 0;
  int retries = 0;
};

struct InetSocketState {
  int domain = 2;    // AF_INET (2) or AF_NETLINK (16)
  int type = 1;      // SOCK_STREAM (1), SOCK_DGRAM (2), SOCK_RAW (3)
  int protocol = 0;  // IPPROTO_TCP (6), IPPROTO_UDP (17), NETLINK_ROUTE (0)

  bool bound = false;
  bool connected = false;
  bool listening = false;
  bool closed = false;
  bool rcv_shutdown = false;
  bool send_shutdown = false;

  uint32_t local_ip_host = 0;
  uint16_t local_port_host = 0;
  uint32_t remote_ip_host = 0;
  uint16_t remote_port_host = 0;

  // AF_NETLINK fields
  uint32_t nl_pid = 0;
  uint32_t nl_groups = 0;

  // Socket options
  int so_error = 0;  // positive errno
  int sndbuf = 262144;
  int rcvbuf = 262144;
  int rcvlowat = 1;
  int reuseaddr = 0;
  int reuseport = 0;
  int keepalive = 0;
  int broadcast = 0;
  int tcp_nodelay = 0;
  int64_t rcvtimeo_ns = -1;
  int64_t sndtimeo_ns = -1;
  std::map<uint64_t, int> extra_opts;

  // Datagram queue (UDP, ICMP, AF_NETLINK)
  std::deque<InetDatagram> rx_dgrams;

  // TCP state
  TcpState tcp_state = TcpState::kClosed;
  bool is_loopback = false;
  std::weak_ptr<InetSocketState> lo_peer;
  std::deque<std::shared_ptr<InetSocketState>> accept_queue;
  int max_backlog = 128;

  uint32_t snd_una = 0;
  uint32_t snd_nxt = 0;
  uint32_t rcv_nxt = 0;
  uint16_t snd_wnd = 64240;
  bool peer_fin_received = false;
  bool fin_sent = false;
  int64_t syn_sent_ns = 0;
  int syn_retries = 0;

  std::deque<uint8_t> rx_stream;
  // Out-of-order TCP segments keyed by sequence number (host order)
  std::map<uint32_t, std::vector<uint8_t>> ooo_segments;
  // Unacknowledged outgoing TCP segments for retransmission
  std::deque<TcpUnackedSeg> tx_unacked;

  std::condition_variable cv;
  std::atomic<uint64_t> activity{1};
};

namespace {

std::vector<std::weak_ptr<InetSocketState>> g_inet_sockets;

void BumpSocket(InetSocketState* s) {
  s->activity.fetch_add(1, std::memory_order_relaxed);
  s->cv.notify_all();
  NotifyPollWaiters();
}

uint16_t AllocEphemeralPortLocked(int type) {
  for (int tries = 0; tries < 16384; ++tries) {
    uint16_t p = g_ephemeral_port.fetch_add(1);
    if (p < 49152 || p >= 65530) {
      g_ephemeral_port.store(49152);
      p = 49152 + (tries % 1000);
    }
    bool busy = false;
    for (auto it = g_inet_sockets.begin(); it != g_inet_sockets.end();) {
      auto sp = it->lock();
      if (!sp) {
        it = g_inet_sockets.erase(it);
        continue;
      }
      if (!sp->closed && sp->domain == kAfInet && sp->type == type &&
          sp->bound && sp->local_port_host == p) {
        busy = true;
      }
      ++it;
    }
    if (!busy) return p;
  }
  return 55000;
}

bool IsLocalIp(uint32_t ip_host) {
  return (ip_host >> 24) == 127 || ip_host == g_local_ip || ip_host == 0;
}

// Forward TCP segment on the wire (caller holds g_net_mtx)
void SendRawTcpSegmentLocked(std::unique_lock<std::mutex>& net_lock,
                             uint32_t src_ip_host, uint16_t src_port_host,
                             uint32_t dst_ip_host, uint16_t dst_port_host,
                             uint32_t seq_host, uint32_t ack_host,
                             uint8_t flags, const uint8_t* payload,
                             size_t payload_len) {
  uint8_t seg[1500] = {};
  size_t opt_len = (flags & kTcpSyn) ? 4 : 0;  // MSS option = 1460
  size_t hdr_len = sizeof(TcpHdr) + opt_len;
  if (hdr_len + payload_len > sizeof(seg)) return;

  auto* tcp = reinterpret_cast<TcpHdr*>(seg);
  tcp->sport = Htons(src_port_host);
  tcp->dport = Htons(dst_port_host);
  tcp->seq = Htonl(seq_host);
  tcp->ack_seq = Htonl(ack_host);
  tcp->doff_res = static_cast<uint8_t>((hdr_len / 4) << 4);
  tcp->flags = flags;
  tcp->window = Htons(64240);
  tcp->check = 0;
  tcp->urg_ptr = 0;

  if (opt_len == 4) {
    uint8_t* opt = seg + sizeof(TcpHdr);
    opt[0] = 2;  // Kind: MSS
    opt[1] = 4;  // Length: 4
    opt[2] = (1460 >> 8) & 0xFF;
    opt[3] = 1460 & 0xFF;
  }
  if (payload_len > 0 && payload != nullptr) {
    memcpy(seg + hdr_len, payload, payload_len);
  }
  tcp->check = L4Checksum(Htonl(src_ip_host), Htonl(dst_ip_host), kIpprotoTcp,
                          seg, hdr_len + payload_len);
  SendIpv4PacketLocked(net_lock, dst_ip_host, kIpprotoTcp, seg,
                       hdr_len + payload_len);
}

void SendTcpSegmentLocked(std::unique_lock<std::mutex>& net_lock,
                          InetSocketState* s, uint8_t flags,
                          const uint8_t* payload, size_t payload_len,
                          bool queue_retransmit) {
  uint32_t seq = s->snd_nxt;
  uint32_t seq_advance = static_cast<uint32_t>(payload_len);
  if (flags & kTcpSyn) seq_advance += 1;
  if (flags & kTcpFin) seq_advance += 1;
  s->snd_nxt += seq_advance;

  if (queue_retransmit && seq_advance > 0) {
    TcpUnackedSeg unacked;
    unacked.seq_start = seq;
    unacked.seq_end = seq + seq_advance;
    unacked.flags = flags;
    if (payload_len > 0 && payload != nullptr) {
      unacked.payload.assign(payload, payload + payload_len);
    }
    unacked.last_sent_ns = static_cast<int64_t>(zx_clock_get_monotonic());
    unacked.retries = 0;
    s->tx_unacked.push_back(std::move(unacked));
  }

  SendRawTcpSegmentLocked(net_lock, s->local_ip_host, s->local_port_host,
                          s->remote_ip_host, s->remote_port_host, seq,
                          s->rcv_nxt, flags, payload, payload_len);
}

// Helper for sequence number comparison (a <= b in mod 2^32 space)
inline bool SeqLeq(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) <= 0;
}
inline bool SeqLt(uint32_t a, uint32_t b) {
  return static_cast<int32_t>(a - b) < 0;
}

// --- Incoming packet processing (caller holds g_net_mtx) -------------------

void HandleRxArpLocked(std::unique_lock<std::mutex>& net_lock,
                       const uint8_t* pkt, size_t len) {
  if (len < sizeof(ArpPkt)) return;
  const auto* arp = reinterpret_cast<const ArpPkt*>(pkt);
  if (Ntohs(arp->htype) != 1 || Ntohs(arp->ptype) != 0x0800) return;

  uint32_t spa_host = Ntohl(arp->spa);
  uint32_t tpa_host = Ntohl(arp->tpa);
  UpdateArpLocked(spa_host, arp->sha);

  if (Ntohs(arp->oper) == 1 && tpa_host == g_local_ip) {
    uint8_t reply[60] = {};
    auto* eth = reinterpret_cast<EthHdr*>(reply);
    memcpy(eth->dst, arp->sha, 6);
    memcpy(eth->src, g_nic.mac, 6);
    eth->ethertype = Htons(0x0806);

    auto* r = reinterpret_cast<ArpPkt*>(reply + sizeof(EthHdr));
    r->htype = Htons(1);
    r->ptype = Htons(0x0800);
    r->hlen = 6;
    r->plen = 4;
    r->oper = Htons(2);  // reply
    memcpy(r->sha, g_nic.mac, 6);
    r->spa = Htonl(g_local_ip);
    memcpy(r->tha, arp->sha, 6);
    r->tpa = arp->spa;

    net_lock.unlock();
    Rtl8139SendFrame(reply, sizeof(EthHdr) + sizeof(ArpPkt));
    net_lock.lock();
  }
}

void HandleRxUdpLocked(uint32_t src_ip_host, uint32_t dst_ip_host,
                       const uint8_t* l4, size_t l4_len) {
  if (l4_len < sizeof(UdpHdr)) return;
  const auto* udp = reinterpret_cast<const UdpHdr*>(l4);
  uint16_t sport = Ntohs(udp->sport);
  uint16_t dport = Ntohs(udp->dport);
  uint16_t ulen = Ntohs(udp->len);
  if (ulen < sizeof(UdpHdr) || ulen > l4_len) return;

  const uint8_t* data = l4 + sizeof(UdpHdr);
  size_t data_len = ulen - sizeof(UdpHdr);

  for (auto it = g_inet_sockets.begin(); it != g_inet_sockets.end();) {
    auto s = it->lock();
    if (!s) {
      it = g_inet_sockets.erase(it);
      continue;
    }
    if (!s->closed && s->domain == kAfInet && s->type == kSockDgram &&
        s->bound && s->local_port_host == dport &&
        (s->local_ip_host == 0 || s->local_ip_host == dst_ip_host ||
         (s->local_ip_host == kLoopbackIp && src_ip_host == g_dns_server))) {
      if (s->rx_dgrams.size() < 256) {
        InetDatagram dg;
        dg.data.assign(data, data + data_len);
        dg.src_ip_host = src_ip_host;
        dg.src_port_host = sport;
        s->rx_dgrams.push_back(std::move(dg));
        BumpSocket(s.get());
      }
    }
    ++it;
  }
}

void HandleRxIcmpLocked(std::unique_lock<std::mutex>& net_lock,
                        uint32_t src_ip_host, const uint8_t* l4,
                        size_t l4_len) {
  if (l4_len < sizeof(IcmpHdr)) return;
  const auto* icmp = reinterpret_cast<const IcmpHdr*>(l4);
  if (icmp->type == 8 && icmp->code == 0) {
    // Echo request -> send Echo reply
    std::vector<uint8_t> reply(l4, l4 + l4_len);
    auto* r = reinterpret_cast<IcmpHdr*>(reply.data());
    r->type = 0;
    r->check = 0;
    r->check = IpChecksum(reply.data(), reply.size());
    SendIpv4PacketLocked(net_lock, src_ip_host, kIpprotoIcmp, reply.data(),
                         reply.size());
    return;
  }
  // Deliver to any raw/ping ICMP sockets
  for (auto it = g_inet_sockets.begin(); it != g_inet_sockets.end();) {
    auto s = it->lock();
    if (!s) {
      it = g_inet_sockets.erase(it);
      continue;
    }
    if (!s->closed && s->domain == kAfInet && s->protocol == kIpprotoIcmp) {
      if (s->rx_dgrams.size() < 64) {
        InetDatagram dg;
        dg.data.assign(l4, l4 + l4_len);
        dg.src_ip_host = src_ip_host;
        dg.src_port_host = 0;
        s->rx_dgrams.push_back(std::move(dg));
        BumpSocket(s.get());
      }
    }
    ++it;
  }
}

void HandleRxTcpLocked(std::unique_lock<std::mutex>& net_lock,
                       uint32_t src_ip_host, uint32_t dst_ip_host,
                       const uint8_t* l4, size_t l4_len) {
  if (l4_len < sizeof(TcpHdr)) return;
  const auto* tcp = reinterpret_cast<const TcpHdr*>(l4);
  uint16_t sport = Ntohs(tcp->sport);
  uint16_t dport = Ntohs(tcp->dport);
  uint32_t seq = Ntohl(tcp->seq);
  uint32_t ack = Ntohl(tcp->ack_seq);
  size_t hdr_len = static_cast<size_t>((tcp->doff_res >> 4) * 4);
  uint8_t flags = tcp->flags;
  if (hdr_len < sizeof(TcpHdr) || hdr_len > l4_len) return;

  const uint8_t* payload = l4 + hdr_len;
  size_t payload_len = l4_len - hdr_len;

  std::shared_ptr<InetSocketState> match;
  for (auto it = g_inet_sockets.begin(); it != g_inet_sockets.end();) {
    auto s = it->lock();
    if (!s) {
      it = g_inet_sockets.erase(it);
      continue;
    }
    if (!s->closed && s->domain == kAfInet && s->type == kSockStream &&
        s->local_port_host == dport && s->remote_port_host == sport &&
        s->remote_ip_host == src_ip_host) {
      match = s;
      break;
    }
    ++it;
  }

  if (!match) {
    if (!(flags & kTcpRst)) {
      uint32_t rst_seq = (flags & kTcpAck) ? ack : 0;
      uint32_t rst_ack =
          seq + static_cast<uint32_t>(payload_len) + ((flags & kTcpSyn) ? 1 : 0);
      uint8_t rst_flags =
          (flags & kTcpAck) ? kTcpRst : static_cast<uint8_t>(kTcpRst | kTcpAck);
      SendRawTcpSegmentLocked(net_lock, dst_ip_host, dport, src_ip_host, sport,
                              rst_seq, rst_ack, rst_flags, nullptr, 0);
    }
    return;
  }

  InetSocketState* s = match.get();
  if (flags & kTcpRst) {
    s->so_error = (s->tcp_state == TcpState::kSynSent)
                      ? static_cast<int>(-kECONNREFUSED_)
                      : static_cast<int>(-kECONNRESET_);
    s->tcp_state = TcpState::kClosed;
    s->connected = false;
    s->peer_fin_received = true;
    s->tx_unacked.clear();
    BumpSocket(s);
    return;
  }

  // Process ACK against tx_unacked
  if (flags & kTcpAck) {
    if (SeqLt(s->snd_una, ack) && SeqLeq(ack, s->snd_nxt)) {
      s->snd_una = ack;
      while (!s->tx_unacked.empty() &&
             SeqLeq(s->tx_unacked.front().seq_end, ack)) {
        s->tx_unacked.pop_front();
      }
    }
    s->snd_wnd = Ntohs(tcp->window);
  }

  if (s->tcp_state == TcpState::kSynSent) {
    if ((flags & (kTcpSyn | kTcpAck)) == (kTcpSyn | kTcpAck)) {
      s->rcv_nxt = seq + 1;
      s->snd_una = ack;
      s->tx_unacked.clear();
      s->tcp_state = TcpState::kEstablished;
      s->connected = true;
      s->so_error = 0;
      SendTcpSegmentLocked(net_lock, s, kTcpAck, nullptr, 0, false);
      BumpSocket(s);
    } else if (flags & kTcpSyn) {
      s->rcv_nxt = seq + 1;
      s->tcp_state = TcpState::kEstablished;
      s->connected = true;
      s->so_error = 0;
      SendTcpSegmentLocked(net_lock, s, kTcpAck, nullptr, 0, false);
      BumpSocket(s);
    }
    return;
  }

  bool send_ack = false;
  bool woke = false;

  if (payload_len > 0) {
    if (seq == s->rcv_nxt) {
      // In-order segment
      s->rx_stream.insert(s->rx_stream.end(), payload, payload + payload_len);
      s->rcv_nxt += static_cast<uint32_t>(payload_len);
      // Drain any contiguous out-of-order segments
      while (!s->ooo_segments.empty()) {
        auto it = s->ooo_segments.begin();
        if (SeqLeq(it->first + static_cast<uint32_t>(it->second.size()),
                   s->rcv_nxt)) {
          s->ooo_segments.erase(it);
        } else if (SeqLeq(it->first, s->rcv_nxt)) {
          uint32_t overlap = s->rcv_nxt - it->first;
          s->rx_stream.insert(s->rx_stream.end(),
                              it->second.begin() + overlap, it->second.end());
          s->rcv_nxt += static_cast<uint32_t>(it->second.size()) - overlap;
          s->ooo_segments.erase(it);
        } else {
          break;
        }
      }
      send_ack = true;
      woke = true;
    } else if (SeqLt(seq, s->rcv_nxt)) {
      // Partial overlap or duplicate segment
      uint32_t end_seq = seq + static_cast<uint32_t>(payload_len);
      if (SeqLt(s->rcv_nxt, end_seq)) {
        uint32_t offset = s->rcv_nxt - seq;
        s->rx_stream.insert(s->rx_stream.end(), payload + offset,
                            payload + payload_len);
        s->rcv_nxt = end_seq;
        woke = true;
      }
      send_ack = true;
    } else if (SeqLt(s->rcv_nxt, seq)) {
      // Future out-of-order segment: buffer it and send duplicate ACK so peer
      // can fast-retransmit the missing segment!
      if (s->ooo_segments.size() < 128) {
        s->ooo_segments[seq].assign(payload, payload + payload_len);
      }
      send_ack = true;
    }
  }

  if (flags & kTcpFin) {
    uint32_t fin_seq = seq + static_cast<uint32_t>(payload_len);
    if (fin_seq == s->rcv_nxt && !s->peer_fin_received) {
      s->rcv_nxt += 1;
      s->peer_fin_received = true;
      if (s->tcp_state == TcpState::kEstablished) {
        s->tcp_state = TcpState::kCloseWait;
      } else if (s->tcp_state == TcpState::kFinWait1 ||
                 s->tcp_state == TcpState::kFinWait2) {
        s->tcp_state = TcpState::kTimeWait;
      }
      send_ack = true;
      woke = true;
    } else if (s->peer_fin_received) {
      send_ack = true;
    }
  }

  if (s->tcp_state == TcpState::kLastAck && (flags & kTcpAck) &&
      s->tx_unacked.empty()) {
    s->tcp_state = TcpState::kClosed;
    woke = true;
  } else if (s->tcp_state == TcpState::kFinWait1 && (flags & kTcpAck) &&
             s->tx_unacked.empty()) {
    s->tcp_state = TcpState::kFinWait2;
    woke = true;
  }

  if (send_ack) {
    SendTcpSegmentLocked(net_lock, s, kTcpAck, nullptr, 0, false);
  }
  if (woke) {
    BumpSocket(s);
  }
}

void HandleRxFrameLocked(std::unique_lock<std::mutex>& net_lock,
                         const uint8_t* frame, size_t len) {
  if (len < sizeof(EthHdr)) return;
  const auto* eth = reinterpret_cast<const EthHdr*>(frame);
  uint16_t etype = Ntohs(eth->ethertype);
  const uint8_t* payload = frame + sizeof(EthHdr);
  size_t payload_len = len - sizeof(EthHdr);

  if (etype == 0x0806) {
    HandleRxArpLocked(net_lock, payload, payload_len);
    return;
  }
  if (etype != 0x0800 || payload_len < sizeof(Ipv4Hdr)) return;

  const auto* ip = reinterpret_cast<const Ipv4Hdr*>(payload);
  if ((ip->ver_ihl >> 4) != 4) return;
  size_t ihl = static_cast<size_t>((ip->ver_ihl & 0x0FU) * 4);
  uint16_t tot_len = Ntohs(ip->tot_len);
  if (ihl < sizeof(Ipv4Hdr) || tot_len < ihl || tot_len > payload_len) return;

  uint32_t saddr_host = Ntohl(ip->saddr);
  uint32_t daddr_host = Ntohl(ip->daddr);
  UpdateArpLocked(saddr_host, eth->src);

  const uint8_t* l4 = payload + ihl;
  size_t l4_len = tot_len - ihl;
  if (ip->proto == kIpprotoUdp) {
    HandleRxUdpLocked(saddr_host, daddr_host, l4, l4_len);
  } else if (ip->proto == kIpprotoTcp) {
    HandleRxTcpLocked(net_lock, saddr_host, daddr_host, l4, l4_len);
  } else if (ip->proto == kIpprotoIcmp) {
    HandleRxIcmpLocked(net_lock, saddr_host, l4, l4_len);
  }
}

void PollNicAndTimers() {
  constexpr uint32_t kRxBufLen = 8192;
  for (;;) {
    bool had_work = false;
    while (g_nic.active) {
      uint8_t frame_copy[2048];
      uint16_t pkt_len = 0;
      {
        std::lock_guard<std::mutex> hw_lock(g_nic.hw_mtx);
        uint16_t isr = In16(g_nic.io_base + 0x3E);
        if (isr != 0) Out16(g_nic.io_base + 0x3E, isr);

        uint8_t cmd = In8(g_nic.io_base + 0x37);
        if ((cmd & 0x01) != 0) break;  // BUFE: RX buffer empty

        uint32_t ring_off = g_nic.rx_offset % kRxBufLen;
        const uint8_t* hdr = g_nic.rx_ring + ring_off;
        uint16_t rx_status =
            static_cast<uint16_t>(hdr[0]) | (static_cast<uint16_t>(hdr[1]) << 8);
        uint16_t rx_size =
            static_cast<uint16_t>(hdr[2]) | (static_cast<uint16_t>(hdr[3]) << 8);

        if ((rx_status & 0x01) == 0 || rx_size < 18 || rx_size > 1792 ||
            (rx_status & 0x1E) != 0) {
          // Reset RX ring pointer and re-enable RX on corrupted header
          g_nic.rx_offset = 0;
          Out32(g_nic.io_base + 0x30, static_cast<uint32_t>(g_nic.rx_phys));
          Out16(g_nic.io_base + 0x38, 0xFFF0U);
          Out8(g_nic.io_base + 0x37, 0x0C);
          Out32(g_nic.io_base + 0x44, 0x0000008FU);
          Out16(g_nic.io_base + 0x3E, 0xFFFFU);
          break;
        }
        pkt_len = static_cast<uint16_t>(rx_size - 4);  // strip 4-byte CRC
        memcpy(frame_copy, hdr + 4, pkt_len);

        g_nic.rx_offset = ((ring_off + rx_size + 4 + 3) & ~3U) % kRxBufLen;
        uint16_t capr = static_cast<uint16_t>((g_nic.rx_offset - 16) & 0xFFFFU);
        Out16(g_nic.io_base + 0x38, capr);
        g_nic.rx_packets.fetch_add(1);
        g_nic.rx_bytes.fetch_add(pkt_len);
      }

      if (pkt_len > 0) {
        had_work = true;
        std::unique_lock<std::mutex> net_lock(g_net_mtx);
        HandleRxFrameLocked(net_lock, frame_copy, pkt_len);
      }
    }

    // Check TCP retransmission timers
    int64_t now_ns = static_cast<int64_t>(zx_clock_get_monotonic());
    {
      std::unique_lock<std::mutex> net_lock(g_net_mtx);
      for (auto it = g_inet_sockets.begin(); it != g_inet_sockets.end();) {
        auto s = it->lock();
        if (!s) {
          it = g_inet_sockets.erase(it);
          continue;
        }
        if (!s->closed && !s->is_loopback && s->domain == kAfInet &&
            s->type == kSockStream && !s->tx_unacked.empty()) {
          auto& seg = s->tx_unacked.front();
          int64_t rto_ns = (seg.flags & kTcpSyn) ? 800000000LL : 350000000LL;
          if (now_ns - seg.last_sent_ns >= rto_ns) {
            if (seg.retries >= 8) {
              s->so_error = static_cast<int>(-kETIMEDOUT_);
              s->tcp_state = TcpState::kClosed;
              s->connected = false;
              s->peer_fin_received = true;
              s->tx_unacked.clear();
              BumpSocket(s.get());
            } else {
              seg.retries++;
              seg.last_sent_ns = now_ns;
              uint32_t seq_start = seg.seq_start;
              uint8_t flags = seg.flags;
              std::vector<uint8_t> payload_copy = seg.payload;
              SendRawTcpSegmentLocked(net_lock, s->local_ip_host,
                                      s->local_port_host, s->remote_ip_host,
                                      s->remote_port_host, seq_start,
                                      s->rcv_nxt, flags, payload_copy.data(),
                                      payload_copy.size());
            }
          }
        }
        ++it;
      }
    }

    if (!had_work) {
      zx_nanosleep(zx_deadline_after(ZX_MSEC(1)));
    }
  }
}

// --- Netlink (NETLINK_ROUTE) helpers ---------------------------------------

void AppendNetlinkAttr(std::vector<uint8_t>* buf, uint16_t type,
                       const void* data, size_t len) {
  uint16_t rta_len = static_cast<uint16_t>(sizeof(RtAttr) + len);
  size_t aligned = (rta_len + 3U) & ~3U;
  size_t old_sz = buf->size();
  buf->resize(old_sz + aligned, 0);
  auto* rta = reinterpret_cast<RtAttr*>(buf->data() + old_sz);
  rta->rta_len = rta_len;
  rta->rta_type = type;
  if (len > 0) memcpy(buf->data() + old_sz + sizeof(RtAttr), data, len);
}

void EnqueueNetlinkLinkMsg(std::vector<uint8_t>* pkt, uint32_t seq,
                           uint32_t pid, int ifindex, const char* name,
                           uint16_t ifi_type, uint32_t ifi_flags,
                           const uint8_t mac[6], uint32_t mtu) {
  std::vector<uint8_t> msg(sizeof(NlMsgHdr) + sizeof(IfInfoMsg), 0);
  auto* ifi = reinterpret_cast<IfInfoMsg*>(msg.data() + sizeof(NlMsgHdr));
  ifi->ifi_family = kAfUnspec;
  ifi->ifi_type = ifi_type;
  ifi->ifi_index = ifindex;
  ifi->ifi_flags = ifi_flags;
  ifi->ifi_change = 0xFFFFFFFFU;

  AppendNetlinkAttr(&msg, 3 /* IFLA_IFNAME */, name, strlen(name) + 1);
  AppendNetlinkAttr(&msg, 4 /* IFLA_MTU */, &mtu, sizeof(mtu));
  if (mac != nullptr) {
    AppendNetlinkAttr(&msg, 1 /* IFLA_ADDRESS */, mac, 6);
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    AppendNetlinkAttr(&msg, 2 /* IFLA_BROADCAST */, bcast, 6);
  }

  auto* nlh = reinterpret_cast<NlMsgHdr*>(msg.data());
  nlh->nlmsg_len = static_cast<uint32_t>(msg.size());
  nlh->nlmsg_type = 16;   // RTM_NEWLINK
  nlh->nlmsg_flags = 2;   // NLM_F_MULTI
  nlh->nlmsg_seq = seq;
  nlh->nlmsg_pid = pid;

  pkt->insert(pkt->end(), msg.begin(), msg.end());
}

void EnqueueNetlinkAddrMsg(std::vector<uint8_t>* pkt, uint32_t seq,
                           uint32_t pid, uint32_t ifindex, const char* label,
                           uint32_t ip_host, uint8_t prefixlen, uint8_t scope) {
  std::vector<uint8_t> msg(sizeof(NlMsgHdr) + sizeof(IfAddrMsg), 0);
  auto* ifa = reinterpret_cast<IfAddrMsg*>(msg.data() + sizeof(NlMsgHdr));
  ifa->ifa_family = kAfInet;
  ifa->ifa_prefixlen = prefixlen;
  ifa->ifa_flags = 0x80;  // IFA_F_PERMANENT
  ifa->ifa_scope = scope;
  ifa->ifa_index = ifindex;

  uint32_t ip_net = Htonl(ip_host);
  AppendNetlinkAttr(&msg, 1 /* IFA_ADDRESS */, &ip_net, 4);
  AppendNetlinkAttr(&msg, 2 /* IFA_LOCAL */, &ip_net, 4);
  AppendNetlinkAttr(&msg, 3 /* IFA_LABEL */, label, strlen(label) + 1);

  auto* nlh = reinterpret_cast<NlMsgHdr*>(msg.data());
  nlh->nlmsg_len = static_cast<uint32_t>(msg.size());
  nlh->nlmsg_type = 20;  // RTM_NEWADDR
  nlh->nlmsg_flags = 2;  // NLM_F_MULTI
  nlh->nlmsg_seq = seq;
  nlh->nlmsg_pid = pid;

  pkt->insert(pkt->end(), msg.begin(), msg.end());
}

void HandleNetlinkRequestLocked(InetSocketState* s, const void* buf,
                                size_t len) {
  if (len < sizeof(NlMsgHdr)) return;
  const auto* req = static_cast<const NlMsgHdr*>(buf);
  uint32_t seq = req->nlmsg_seq;
  uint32_t pid = s->nl_pid ? s->nl_pid : 1;

  if (req->nlmsg_type == 18 /* RTM_GETLINK */) {
    std::vector<uint8_t> pkt;
    uint8_t zero_mac[6] = {};
    // IFF_UP(0x1) | IFF_LOOPBACK(0x8) | IFF_RUNNING(0x40) | IFF_LOWER_UP(0x10000)
    EnqueueNetlinkLinkMsg(&pkt, seq, pid, 1, "lo", 772 /* ARPHRD_LOOPBACK */,
                          0x10049U, zero_mac, 65536);
    // IFF_UP(0x1) | IFF_BROADCAST(0x2) | IFF_RUNNING(0x40) | IFF_MULTICAST(0x1000) | IFF_LOWER_UP(0x10000)
    EnqueueNetlinkLinkMsg(&pkt, seq, pid, 2, "eth0", 1 /* ARPHRD_ETHER */,
                          0x11043U, g_nic.mac, 1500);
    InetDatagram dg;
    dg.data = std::move(pkt);
    s->rx_dgrams.push_back(std::move(dg));
  } else if (req->nlmsg_type == 22 /* RTM_GETADDR */) {
    std::vector<uint8_t> pkt;
    EnqueueNetlinkAddrMsg(&pkt, seq, pid, 1, "lo", kLoopbackIp, 8,
                          254 /* RT_SCOPE_HOST */);
    EnqueueNetlinkAddrMsg(&pkt, seq, pid, 2, "eth0", g_local_ip, 24,
                          0 /* RT_SCOPE_UNIVERSE */);
    InetDatagram dg;
    dg.data = std::move(pkt);
    s->rx_dgrams.push_back(std::move(dg));
  }

  // Always terminate a multi-part dump with NLMSG_DONE (type 3)
  std::vector<uint8_t> done_pkt(sizeof(NlMsgHdr) + sizeof(int32_t), 0);
  auto* dnlh = reinterpret_cast<NlMsgHdr*>(done_pkt.data());
  dnlh->nlmsg_len = static_cast<uint32_t>(done_pkt.size());
  dnlh->nlmsg_type = 3;  // NLMSG_DONE
  dnlh->nlmsg_flags = 2;  // NLM_F_MULTI
  dnlh->nlmsg_seq = seq;
  dnlh->nlmsg_pid = pid;
  InetDatagram done_dg;
  done_dg.data = std::move(done_pkt);
  s->rx_dgrams.push_back(std::move(done_dg));
  BumpSocket(s);
}

}  // namespace

// --- Public initialization and /proc helpers -------------------------------

void NetInetInit() {
  static bool initialized = false;
  if (initialized) return;
  initialized = true;

  if (g_sys_rsrc == ZX_HANDLE_INVALID) {
    return;
  }

  uint16_t io_base = 0;
#if defined(__x86_64__)
  if (g_ioport_rsrc == ZX_HANDLE_INVALID) return;
  bool found = false;
  uint8_t nic_bus = 0, nic_slot = 0, nic_func = 0;
  for (uint16_t slot = 0; slot < 32 && !found; ++slot) {
    for (uint8_t func = 0; func < 8; ++func) {
      uint32_t id = PciRead32(0, static_cast<uint8_t>(slot), func, 0x00);
      if (id == 0xFFFFFFFFU) {
        if (func == 0) break;
        continue;
      }
      if (id == 0x813910ECU) {
        uint32_t bar0 = PciRead32(0, static_cast<uint8_t>(slot), func, 0x10);
        if ((bar0 & 0x01U) != 0) {
          io_base = static_cast<uint16_t>(bar0 & ~0x3U);
          nic_bus = 0;
          nic_slot = static_cast<uint8_t>(slot);
          nic_func = func;
          found = true;
          break;
        }
      }
      if (func == 0) {
        uint32_t hdr = PciRead32(0, static_cast<uint8_t>(slot), 0, 0x0C);
        if (((hdr >> 16) & 0x80U) == 0) break;
      }
    }
  }

  if (!found || io_base == 0) {
    ShimLogf("[net] No RTL8139 PCI NIC found; loopback 127.0.0.1 only.\n");
    return;
  }

  if (zx_ioports_request(g_ioport_rsrc, io_base, 256) != ZX_OK) {
    ShimLogf("[net] Failed to request RTL8139 I/O ports at 0x%x\n", io_base);
    return;
  }

  // Enable PCI I/O Space (bit 0) and Bus Master (bit 2)
  uint32_t cmd = PciRead32(nic_bus, nic_slot, nic_func, 0x04);
  PciWrite32(nic_bus, nic_slot, nic_func, 0x04, cmd | 0x05U);
#elif defined(__aarch64__)
  if (g_mmio_rsrc == ZX_HANDLE_INVALID) return;
  const uint64_t kEcamPhys = 0x3f000000ULL;
  const size_t kEcamSize = 0x100000ULL;
  zx_handle_t ecam_rsrc = ZX_HANDLE_INVALID;
  const char ecam_name[] = "rtl_pcie_ecam";
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, kEcamPhys, kEcamSize,
                         ecam_name, sizeof(ecam_name), &ecam_rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t ecam_vmo = ZX_HANDLE_INVALID;
  zx_status_t ecam_st =
      zx_vmo_create_physical(ecam_rsrc, kEcamPhys, kEcamSize, &ecam_vmo);
  zx_handle_close(ecam_rsrc);
  if (ecam_st != ZX_OK) return;
  uintptr_t ecam_base = 0;
  ecam_st = zx_vmar_map(zx_vmar_root_self(),
                        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0, ecam_vmo, 0,
                        kEcamSize, &ecam_base);
  zx_handle_close(ecam_vmo);
  if (ecam_st != ZX_OK) return;

  int found_slot = -1;
  for (int slot = 0; slot < 32; ++slot) {
    volatile uint32_t* dev_cfg = reinterpret_cast<volatile uint32_t*>(
        ecam_base + (static_cast<uintptr_t>(slot) << 15));
    if (dev_cfg[0] == 0x813910ECU) {
      found_slot = slot;
      break;
    }
  }
  if (found_slot < 0) {
    zx_vmar_unmap(zx_vmar_root_self(), ecam_base, kEcamSize);
    ShimLogf("[net] No RTL8139 PCI NIC found on ECAM; loopback 127.0.0.1 only.\n");
    return;
  }

  volatile uint32_t* dev_cfg = reinterpret_cast<volatile uint32_t*>(
      ecam_base + (static_cast<uintptr_t>(found_slot) << 15));
  // BAR1 (offset 0x14, index 5) is RTL8139 MMIO BAR (256 bytes).
  const uint64_t kRtlMmioPhys = 0x12000000ULL;
  const size_t kRtlMmioSize = 4096;
  dev_cfg[5] = static_cast<uint32_t>(kRtlMmioPhys);
  // Enable Memory Space (bit 1) and Bus Master (bit 2).
  uint32_t cmd = dev_cfg[1];
  dev_cfg[1] = cmd | 0x0006U;
  zx_vmar_unmap(zx_vmar_root_self(), ecam_base, kEcamSize);

  zx_handle_t mmio_rsrc = ZX_HANDLE_INVALID;
  const char mmio_name[] = "rtl8139_mmio";
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, kRtlMmioPhys,
                         kRtlMmioSize, mmio_name, sizeof(mmio_name),
                         &mmio_rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t mmio_vmo = ZX_HANDLE_INVALID;
  if (zx_vmo_create_physical(mmio_rsrc, kRtlMmioPhys, kRtlMmioSize,
                             &mmio_vmo) != ZX_OK) {
    zx_handle_close(mmio_rsrc);
    return;
  }
  zx_handle_close(mmio_rsrc);
  zx_vmo_set_cache_policy(mmio_vmo, ZX_CACHE_POLICY_UNCACHED_DEVICE);
  if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                  mmio_vmo, 0, kRtlMmioSize, &g_rtl_mmio_base) != ZX_OK) {
    zx_handle_close(mmio_vmo);
    return;
  }
  zx_handle_close(mmio_vmo);
#endif

  // Allocate physically contiguous DMA memory via Zircon Stub IOMMU + BTI
  zx_handle_t iommu_rsrc = ZX_HANDLE_INVALID;
  zx_status_t st =
      zx_resource_create(g_sys_rsrc, ZX_RSRC_KIND_SYSTEM,
                         ZX_RSRC_SYSTEM_IOMMU_BASE, 1, "iommu", 5, &iommu_rsrc);
  zx_handle_t iommu = ZX_HANDLE_INVALID;
  if (st == ZX_OK) {
    zx_iommu_desc_stub_t desc{};
    st = zx_iommu_create(iommu_rsrc, ZX_IOMMU_TYPE_STUB, &desc, sizeof(desc),
                         &iommu);
  }
  if (st == ZX_OK) {
    st = zx_bti_create(iommu, 0, 0, &g_nic.bti);
  }
  constexpr size_t kDmaSize = 128 * 1024;
  if (st == ZX_OK) {
    st = zx_vmo_create_contiguous(g_nic.bti, kDmaSize, 0, &g_nic.dma_vmo);
  }
  if (st == ZX_OK) {
    st = zx_bti_pin(g_nic.bti,
                    ZX_BTI_PERM_READ | ZX_BTI_PERM_WRITE | ZX_BTI_CONTIGUOUS,
                    g_nic.dma_vmo, 0, kDmaSize, &g_nic.dma_phys, 1, &g_nic.pmt);
  }
  uintptr_t map_addr = 0;
  if (st == ZX_OK) {
    st = zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                     g_nic.dma_vmo, 0, kDmaSize, &map_addr);
  }
  if (st != ZX_OK || map_addr == 0) {
    ShimLogf("[net] Failed to allocate contiguous DMA for RTL8139: %d\n", st);
    return;
  }

  g_nic.io_base = io_base;
  g_nic.dma_virt = reinterpret_cast<uint8_t*>(map_addr);
  memset(g_nic.dma_virt, 0, kDmaSize);
  g_nic.rx_ring = g_nic.dma_virt;
  g_nic.rx_phys = g_nic.dma_phys;
  for (int i = 0; i < 4; ++i) {
    size_t off = 0x12000 + static_cast<size_t>(i) * 0x800;
    g_nic.tx_bufs[i] = g_nic.dma_virt + off;
    g_nic.tx_phys[i] = g_nic.dma_phys + off;
  }

  // Power on & software reset RTL8139
  Out8(io_base + 0x52, 0x00);
  Out8(io_base + 0x37, 0x10);
  for (int i = 0; i < 10000; ++i) {
    if ((In8(io_base + 0x37) & 0x10) == 0) break;
  }

  for (int i = 0; i < 6; ++i) {
    g_nic.mac[i] = In8(io_base + i);
  }

  Out32(io_base + 0x08, 0xFFFFFFFFU);  // MAR0..3: accept all multicast
  Out32(io_base + 0x0C, 0xFFFFFFFFU);  // MAR4..7: accept all multicast
  Out32(io_base + 0x30, static_cast<uint32_t>(g_nic.rx_phys));  // RBSTART
  Out16(io_base + 0x3C, 0x0000);  // IMR (polling mode)
  Out16(io_base + 0x3E, 0xFFFF);  // ISR clear
  Out8(io_base + 0x37, 0x0C);     // CR: RE | TE (must be set before RCR/TCR)
  // RCR: AAP|APM|AM|AB|WRAP(bit 7), RBLEN=0 (8KB + 16 + 1500 wrap margin)
  Out32(io_base + 0x44, 0x0000008FU);
  Out32(io_base + 0x40, 0x03000700U);  // TCR
  Out16(io_base + 0x38, 0xFFF0U);      // CAPR = -16

  g_nic.active = true;
  ShimLogf("[net] RTL8139 initialized at ioport=0x%x dma_phys=0x%llx "
           "MAC=%02x:%02x:%02x:%02x:%02x:%02x IP=10.0.2.15/24 GW=10.0.2.2\n",
           io_base, static_cast<unsigned long long>(g_nic.dma_phys),
           g_nic.mac[0], g_nic.mac[1], g_nic.mac[2], g_nic.mac[3], g_nic.mac[4],
           g_nic.mac[5]);

  std::thread(TxNicWorker).detach();
  std::thread(PollNicAndTimers).detach();
  SendArpRequest(g_gateway);
  SendArpRequest(g_dns_server);
}

bool NetInetHasNic() { return g_nic.active; }

std::string NetGenerateProcNetDev() {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "Inter-|   Receive                                                |"
           "  Transmit\n"
           " face |bytes    packets errs drop fifo frame compressed multicast|"
           "bytes    packets errs drop fifo colls carrier compressed\n"
           "    lo:       0       0    0    0    0     0          0         0 "
           "       0       0    0    0    0     0       0          0\n"
           "  eth0: %7llu %7llu    0    0    0     0          0         0 "
           "%8llu %7llu    0    0    0     0       0          0\n",
           static_cast<unsigned long long>(g_nic.rx_bytes.load()),
           static_cast<unsigned long long>(g_nic.rx_packets.load()),
           static_cast<unsigned long long>(g_nic.tx_bytes.load()),
           static_cast<unsigned long long>(g_nic.tx_packets.load()));
  return buf;
}

std::string NetGenerateProcNetRoute() {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\t"
           "MTU\tWindow\tIRTT\n"
           "eth0\t%08X\t00000000\t0001\t0\t0\t0\t%08X\t0\t0\t0\n"
           "eth0\t00000000\t%08X\t0003\t0\t0\t0\t00000000\t0\t0\t0\n",
           Htonl(g_local_ip & g_netmask), Htonl(g_netmask), Htonl(g_gateway));
  return buf;
}

// --- Socket syscalls -------------------------------------------------------

int64_t InetSysSocket(LinuxProcess* proc, int domain, int type_and_flags,
                      int protocol) {
  if (domain != kAfInet && domain != kAfNetlink) return kEAFNOSUPPORT_;
  int type = type_and_flags & kSockTypeMask;
  int extra = type_and_flags & ~kSockTypeMask;
  if ((extra & ~(kSockNonblock | kSockCloexec)) != 0) return kEINVAL_;

  if (domain == kAfNetlink) {
    if (type != kSockRaw && type != kSockDgram) return kEPROTONOSUPPORT_;
  } else {
    if (type == kSockStream) {
      if (protocol != 0 && protocol != kIpprotoTcp) return kEPROTONOSUPPORT_;
      protocol = kIpprotoTcp;
    } else if (type == kSockDgram) {
      if (protocol != 0 && protocol != kIpprotoUdp && protocol != kIpprotoIcmp) {
        return kEPROTONOSUPPORT_;
      }
      if (protocol == 0) protocol = kIpprotoUdp;
    } else if (type == kSockRaw) {
      if (protocol == 0) protocol = kIpprotoIcmp;
    } else {
      return kEAFNOSUPPORT_;
    }
  }

  auto state = std::make_shared<InetSocketState>();
  state->domain = domain;
  state->type = type;
  state->protocol = protocol;
  if (domain == kAfNetlink) {
    state->nl_pid = g_next_nl_pid.fetch_add(1);
  }

  {
    std::lock_guard<std::mutex> lock(g_net_mtx);
    g_inet_sockets.push_back(state);
  }

  auto desc = std::make_shared<FileDescription>();
  desc->kind = FdKind::kUnixSocket;
  desc->flags = kLinuxO_RDWR | ((extra & kSockNonblock) ? kLinuxO_NONBLOCK : 0);
  desc->inet_sock = state;
  desc->anon_ino = Vfs::AllocInode();
  desc->create_time_ns = ShimRealtimeNs();
  return InstallFd(proc, desc, (extra & kSockCloexec) != 0);
}

int64_t InetSysBind(LinuxProcess* proc, FileDescription* desc,
                    uintptr_t addr_ptr, int addrlen) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;

  if (s->domain == kAfNetlink) {
    if (addrlen < static_cast<int>(sizeof(SockaddrNl))) return kEINVAL_;
    SockaddrNl nl{};
    if (!CopyFromUser(proc, &nl, addr_ptr, sizeof(nl))) return kEFAULT_;
    if (nl.nl_family != kAfNetlink) return kEAFNOSUPPORT_;
    std::lock_guard<std::mutex> lock(g_net_mtx);
    if (nl.nl_pid != 0) s->nl_pid = nl.nl_pid;
    s->nl_groups = nl.nl_groups;
    s->bound = true;
    return 0;
  }

  if (addrlen < static_cast<int>(sizeof(SockaddrIn))) return kEINVAL_;
  SockaddrIn sin{};
  if (!CopyFromUser(proc, &sin, addr_ptr, sizeof(sin))) return kEFAULT_;
  if (sin.sin_family != kAfInet && sin.sin_family != kAfUnspec) {
    return kEAFNOSUPPORT_;
  }

  uint32_t ip_host = Ntohl(sin.sin_addr);
  uint16_t port_host = Ntohs(sin.sin_port);

  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (port_host == 0) {
    port_host = AllocEphemeralPortLocked(s->type);
  }
  s->local_ip_host = ip_host;
  s->local_port_host = port_host;
  s->bound = true;
  return 0;
}

int64_t InetSysConnect(LinuxProcess* proc, LinuxThread* thrd,
                       FileDescription* desc, uintptr_t addr_ptr, int addrlen) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;

  if (s->domain == kAfNetlink) {
    std::lock_guard<std::mutex> lock(g_net_mtx);
    s->connected = true;
    return 0;
  }

  if (addrlen < static_cast<int>(sizeof(SockaddrIn))) return kEINVAL_;
  SockaddrIn sin{};
  if (!CopyFromUser(proc, &sin, addr_ptr, sizeof(sin))) return kEFAULT_;
  if (sin.sin_family == kAfUnspec) {
    std::lock_guard<std::mutex> lock(g_net_mtx);
    s->connected = false;
    s->remote_ip_host = 0;
    s->remote_port_host = 0;
    return 0;
  }
  if (sin.sin_family != kAfInet) return kEAFNOSUPPORT_;

  uint32_t dst_ip = Ntohl(sin.sin_addr);
  uint16_t dst_port = Ntohs(sin.sin_port);
  if (dst_ip == 0) dst_ip = kLoopbackIp;

  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  if (s->type == kSockDgram || s->type == kSockRaw) {
    if (!s->bound) {
      s->local_port_host = AllocEphemeralPortLocked(s->type);
      s->bound = true;
    }
    if (s->local_ip_host == 0) {
      s->local_ip_host = ((dst_ip >> 24) == 127) ? kLoopbackIp : g_local_ip;
    }
    // Redirect loopback DNS (127.0.0.1:53 / 127.0.0.53:53) to QEMU SLIRP DNS
    // 10.0.2.3:53 when no local UDP listener is bound to 53.
    if ((dst_ip >> 24) == 127 && dst_port == 53) {
      dst_ip = g_dns_server;
      s->local_ip_host = g_local_ip;
    }
    s->remote_ip_host = dst_ip;
    s->remote_port_host = dst_port;
    s->connected = true;
    BumpSocket(s.get());
    return 0;
  }

  // SOCK_STREAM (TCP)
  if (s->tcp_state == TcpState::kEstablished) return kEISCONN_;
  if (s->tcp_state == TcpState::kSynSent) return kEALREADY_;
  if (dst_port == 0) return kECONNREFUSED_;

  if (!s->bound) {
    s->local_port_host = AllocEphemeralPortLocked(kSockStream);
    s->bound = true;
  }

  // Check if connecting to a local listener (127.x.x.x or 10.0.2.15)
  if (IsLocalIp(dst_ip)) {
    std::shared_ptr<InetSocketState> listener;
    for (auto& w : g_inet_sockets) {
      auto cand = w.lock();
      if (cand && !cand->closed && cand->domain == kAfInet &&
          cand->type == kSockStream && cand->listening &&
          cand->local_port_host == dst_port &&
          (cand->local_ip_host == 0 || cand->local_ip_host == dst_ip)) {
        listener = cand;
        break;
      }
    }
    if (!listener) {
      if (dst_port == 53 && g_nic.active) {
        dst_ip = g_dns_server;
      } else {
        s->so_error = static_cast<int>(-kECONNREFUSED_);
        return kECONNREFUSED_;
      }
    } else {
      auto srv = std::make_shared<InetSocketState>();
      srv->domain = kAfInet;
      srv->type = kSockStream;
      srv->protocol = kIpprotoTcp;
      srv->bound = true;
      srv->connected = true;
      srv->is_loopback = true;
      srv->tcp_state = TcpState::kEstablished;
      srv->local_ip_host = dst_ip;
      srv->local_port_host = dst_port;
      srv->remote_ip_host = kLoopbackIp;
      srv->remote_port_host = s->local_port_host;

      s->local_ip_host = kLoopbackIp;
      s->remote_ip_host = dst_ip;
      s->remote_port_host = dst_port;
      s->connected = true;
      s->is_loopback = true;
      s->tcp_state = TcpState::kEstablished;
      s->so_error = 0;

      s->lo_peer = srv;
      srv->lo_peer = s;
      g_inet_sockets.push_back(srv);
      listener->accept_queue.push_back(srv);
      BumpSocket(listener.get());
      BumpSocket(s.get());
      return 0;
    }
  }

  if (!g_nic.active) {
    s->so_error = static_cast<int>(-kENETUNREACH_);
    return kENETUNREACH_;
  }

  s->local_ip_host = g_local_ip;
  s->remote_ip_host = dst_ip;
  s->remote_port_host = dst_port;
  s->is_loopback = false;
  uint32_t iss = g_iss.fetch_add(64000);
  s->snd_una = iss;
  s->snd_nxt = iss;
  s->rcv_nxt = 0;
  s->so_error = 0;
  s->peer_fin_received = false;
  s->fin_sent = false;
  s->tx_unacked.clear();
  s->ooo_segments.clear();
  s->tcp_state = TcpState::kSynSent;
  s->syn_sent_ns = static_cast<int64_t>(zx_clock_get_monotonic());
  s->syn_retries = 0;

  SendTcpSegmentLocked(net_lock, s.get(), kTcpSyn, nullptr, 0, true);

  if ((desc->flags & kLinuxO_NONBLOCK) != 0) {
    return kEINPROGRESS_;
  }

  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(10000);
  while (s->tcp_state == TcpState::kSynSent && !s->closed) {
    if (thrd && ShimSignalPending(proc, thrd)) return kEINTR;
    if (s->cv.wait_until(net_lock, deadline) == std::cv_status::timeout) {
      s->tcp_state = TcpState::kClosed;
      s->tx_unacked.clear();
      s->so_error = static_cast<int>(-kETIMEDOUT_);
      return kETIMEDOUT_;
    }
  }
  if (s->tcp_state == TcpState::kEstablished) return 0;
  int err = s->so_error ? -s->so_error : static_cast<int>(kECONNREFUSED_);
  return err;
}

int64_t InetSysListen(FileDescription* desc, int backlog) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  if (s->type != kSockStream) return kEOPNOTSUPP_;
  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (!s->bound) {
    s->local_port_host = AllocEphemeralPortLocked(kSockStream);
    s->bound = true;
  }
  s->listening = true;
  s->tcp_state = TcpState::kListen;
  s->max_backlog = std::max(backlog, 1);
  return 0;
}

int64_t InetSysAccept(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t addr_ptr,
                      uintptr_t addrlen_ptr, int flags) {
  if ((flags & ~(kSockNonblock | kSockCloexec)) != 0) return kEINVAL_;
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  if (s->type != kSockStream) return kEOPNOTSUPP_;

  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  if (!s->listening) return kEINVAL_;

  while (s->accept_queue.empty() && !s->closed) {
    if ((desc->flags & kLinuxO_NONBLOCK) != 0) return kEAGAIN_;
    if (thrd && ShimSignalPending(proc, thrd)) return kEINTR;
    s->cv.wait_for(net_lock, std::chrono::milliseconds(50));
  }
  if (s->accept_queue.empty()) return kECONNABORTED_;

  auto child = s->accept_queue.front();
  s->accept_queue.pop_front();
  uint32_t peer_ip = child->remote_ip_host;
  uint16_t peer_port = child->remote_port_host;
  net_lock.unlock();

  if (addr_ptr != 0) {
    if (addrlen_ptr == 0) return kEFAULT_;
    int ulen = 0;
    if (!CopyFromUser(proc, &ulen, addrlen_ptr, sizeof(ulen)) || ulen < 0) {
      return kEFAULT_;
    }
    SockaddrIn sin{};
    sin.sin_family = kAfInet;
    sin.sin_port = Htons(peer_port);
    sin.sin_addr = Htonl(peer_ip);
    size_t copy_n = std::min<size_t>(static_cast<size_t>(ulen), sizeof(sin));
    if (copy_n > 0 && !CopyToUser(proc, addr_ptr, &sin, copy_n)) return kEFAULT_;
    int actual = sizeof(sin);
    if (!CopyToUser(proc, addrlen_ptr, &actual, sizeof(actual))) return kEFAULT_;
  }

  auto cdesc = std::make_shared<FileDescription>();
  cdesc->kind = FdKind::kUnixSocket;
  cdesc->flags = kLinuxO_RDWR | ((flags & kSockNonblock) ? kLinuxO_NONBLOCK : 0);
  cdesc->inet_sock = child;
  cdesc->anon_ino = Vfs::AllocInode();
  cdesc->create_time_ns = ShimRealtimeNs();
  return InstallFd(proc, cdesc, (flags & kSockCloexec) != 0);
}

namespace {

ssize_t DoInetRecv(LinuxProcess* proc, LinuxThread* thrd, FileDescription* desc,
                   void* buf, size_t count, int flags, uint32_t* out_ip_host,
                   uint16_t* out_port_host) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  bool nonblock =
      ((desc->flags & kLinuxO_NONBLOCK) != 0) || ((flags & kMsgDontwait) != 0);
  bool peek = (flags & kMsgPeek) != 0;
  bool waitall = (flags & kMsgWaitall) != 0 && !peek && !nonblock;

  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  int64_t timeo_ns = s->rcvtimeo_ns;
  auto deadline = (timeo_ns > 0)
                      ? (std::chrono::steady_clock::now() +
                         std::chrono::nanoseconds(timeo_ns))
                      : std::chrono::steady_clock::time_point::max();

  if (s->domain == kAfNetlink || s->type == kSockDgram || s->type == kSockRaw) {
    while (s->rx_dgrams.empty()) {
      if (s->rcv_shutdown || s->closed) return 0;
      if (nonblock || timeo_ns == 0) return kEAGAIN_;
      if (thrd && ShimSignalPending(proc, thrd)) return kEINTR;
      if (timeo_ns > 0) {
        if (s->cv.wait_until(net_lock, deadline) == std::cv_status::timeout &&
            s->rx_dgrams.empty()) {
          return kEAGAIN_;
        }
      } else {
        s->cv.wait_for(net_lock, std::chrono::milliseconds(50));
      }
    }
    const auto& dg = s->rx_dgrams.front();
    if (out_ip_host) *out_ip_host = dg.src_ip_host;
    if (out_port_host) *out_port_host = dg.src_port_host;
    size_t n = std::min(count, dg.data.size());
    if (n > 0) memcpy(buf, dg.data.data(), n);
    if (!peek) {
      s->rx_dgrams.pop_front();
      BumpSocket(s.get());
    }
    return static_cast<ssize_t>(n);
  }

  // SOCK_STREAM (TCP)
  if (s->tcp_state == TcpState::kClosed && !s->connected &&
      s->rx_stream.empty() && !s->peer_fin_received) {
    if (s->so_error != 0) {
      int err = -s->so_error;
      s->so_error = 0;
      return err;
    }
    return kENOTCONN_;
  }
  if (count == 0) return 0;

  size_t copied = 0;
  auto* dst = static_cast<uint8_t*>(buf);
  for (;;) {
    while (s->rx_stream.empty()) {
      if (s->so_error != 0 && copied == 0) {
        int err = -s->so_error;
        s->so_error = 0;
        return err;
      }
      if (s->peer_fin_received || s->rcv_shutdown || s->closed ||
          s->tcp_state == TcpState::kClosed) {
        return static_cast<ssize_t>(copied);
      }
      if (copied > 0 && !waitall) return static_cast<ssize_t>(copied);
      if (nonblock || timeo_ns == 0) {
        return copied > 0 ? static_cast<ssize_t>(copied) : kEAGAIN_;
      }
      if (thrd && ShimSignalPending(proc, thrd)) {
        return copied > 0 ? static_cast<ssize_t>(copied) : kEINTR;
      }
      if (timeo_ns > 0) {
        if (s->cv.wait_until(net_lock, deadline) == std::cv_status::timeout &&
            s->rx_stream.empty()) {
          return copied > 0 ? static_cast<ssize_t>(copied) : kEAGAIN_;
        }
      } else {
        s->cv.wait_for(net_lock, std::chrono::milliseconds(20));
      }
    }

    size_t take = std::min(count - copied, s->rx_stream.size());
    std::copy_n(s->rx_stream.begin(), take, dst + copied);
    if (!peek) {
      s->rx_stream.erase(s->rx_stream.begin(),
                         s->rx_stream.begin() + static_cast<ptrdiff_t>(take));
      BumpSocket(s.get());
    }
    copied += take;
    if (copied >= count || peek || !waitall) break;
  }
  if (out_ip_host) *out_ip_host = s->remote_ip_host;
  if (out_port_host) *out_port_host = s->remote_port_host;
  return static_cast<ssize_t>(copied);
}

ssize_t DoInetSend(LinuxProcess* proc, LinuxThread* thrd, FileDescription* desc,
                   const void* buf, size_t count, int flags,
                   bool has_dest_addr, uint32_t dst_ip_host,
                   uint16_t dst_port_host) {
  (void)proc;
  (void)thrd;
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;

  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  if (s->send_shutdown) {
    if ((flags & kMsgNosignal) == 0) QueueSignalToThread(proc, thrd, kSIGPIPE);
    return kEPIPE_;
  }

  if (s->domain == kAfNetlink) {
    HandleNetlinkRequestLocked(s.get(), buf, count);
    return static_cast<ssize_t>(count);
  }

  if (s->type == kSockDgram || s->type == kSockRaw) {
    uint32_t dip = has_dest_addr ? dst_ip_host : s->remote_ip_host;
    uint16_t dport = has_dest_addr ? dst_port_host : s->remote_port_host;
    if (!has_dest_addr && !s->connected) return kEDESTADDRREQ_;
    if (dip == 0) dip = kLoopbackIp;

    if (!s->bound) {
      s->local_port_host = AllocEphemeralPortLocked(s->type);
      s->bound = true;
    }
    if (s->local_ip_host == 0) {
      s->local_ip_host = ((dip >> 24) == 127) ? kLoopbackIp : g_local_ip;
    }

    // Check local UDP sockets first
    if (IsLocalIp(dip)) {
      bool delivered = false;
      for (auto& w : g_inet_sockets) {
        auto rx = w.lock();
        if (rx && !rx->closed && rx->domain == kAfInet &&
            rx->type == kSockDgram && rx->bound &&
            rx->local_port_host == dport &&
            (rx->local_ip_host == 0 || rx->local_ip_host == dip)) {
          InetDatagram dg;
          dg.data.assign(static_cast<const uint8_t*>(buf),
                         static_cast<const uint8_t*>(buf) + count);
          dg.src_ip_host = kLoopbackIp;
          dg.src_port_host = s->local_port_host;
          rx->rx_dgrams.push_back(std::move(dg));
          BumpSocket(rx.get());
          delivered = true;
        }
      }
      if (delivered) return static_cast<ssize_t>(count);
      if (dport == 53 && g_nic.active) {
        dip = g_dns_server;
      } else {
        return static_cast<ssize_t>(count);
      }
    }

    if (!g_nic.active) return kENETUNREACH_;
    if (s->protocol == kIpprotoIcmp) {
      SendIpv4PacketLocked(net_lock, dip, kIpprotoIcmp, buf, count);
      return static_cast<ssize_t>(count);
    }

    if (count > 1472) return kEMSGSIZE_;
    std::vector<uint8_t> pkt(sizeof(UdpHdr) + count);
    auto* udp = reinterpret_cast<UdpHdr*>(pkt.data());
    udp->sport = Htons(s->local_port_host);
    udp->dport = Htons(dport);
    udp->len = Htons(static_cast<uint16_t>(pkt.size()));
    udp->check = 0;
    if (count > 0) memcpy(pkt.data() + sizeof(UdpHdr), buf, count);
    udp->check = L4Checksum(Htonl(g_local_ip), Htonl(dip), kIpprotoUdp,
                            pkt.data(), pkt.size());
    SendIpv4PacketLocked(net_lock, dip, kIpprotoUdp, pkt.data(), pkt.size());
    return static_cast<ssize_t>(count);
  }

  // SOCK_STREAM (TCP)
  if (s->tcp_state == TcpState::kSynSent) return kEAGAIN_;
  if (s->tcp_state != TcpState::kEstablished &&
      s->tcp_state != TcpState::kCloseWait) {
    if (s->so_error != 0) {
      int err = -s->so_error;
      s->so_error = 0;
      return err;
    }
    if ((flags & kMsgNosignal) == 0) QueueSignalToThread(proc, thrd, kSIGPIPE);
    return kEPIPE_;
  }
  if (count == 0) return 0;

  if (s->is_loopback) {
    auto peer = s->lo_peer.lock();
    if (!peer || peer->closed || peer->rcv_shutdown) {
      if ((flags & kMsgNosignal) == 0) QueueSignalToThread(proc, thrd, kSIGPIPE);
      return kEPIPE_;
    }
    const auto* src = static_cast<const uint8_t*>(buf);
    peer->rx_stream.insert(peer->rx_stream.end(), src, src + count);
    BumpSocket(peer.get());
    return static_cast<ssize_t>(count);
  }

  const auto* src = static_cast<const uint8_t*>(buf);
  size_t off = 0;
  constexpr size_t kMss = 1400;
  while (off < count) {
    size_t chunk = std::min(count - off, kMss);
    SendTcpSegmentLocked(net_lock, s.get(), kTcpAck | kTcpPsh, src + off, chunk,
                         true);
    off += chunk;
  }
  return static_cast<ssize_t>(count);
}

}  // namespace

ssize_t InetSocketRead(LinuxProcess* proc, LinuxThread* thrd,
                       FileDescription* desc, void* buf, size_t count) {
  return DoInetRecv(proc, thrd, desc, buf, count, 0, nullptr, nullptr);
}

ssize_t InetSocketWrite(LinuxProcess* proc, LinuxThread* thrd,
                        FileDescription* desc, const void* buf, size_t count) {
  return DoInetSend(proc, thrd, desc, buf, count, 0, false, 0, 0);
}

int64_t InetSysSendto(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t buf_ptr, size_t len,
                      int flags, uintptr_t addr_ptr, int addrlen) {
  bool has_dest = false;
  uint32_t dst_ip = 0;
  uint16_t dst_port = 0;
  if (addr_ptr != 0 && addrlen >= static_cast<int>(sizeof(SockaddrIn)) &&
      desc->inet_sock && desc->inet_sock->domain == kAfInet) {
    SockaddrIn sin{};
    if (!CopyFromUser(proc, &sin, addr_ptr, sizeof(sin))) return kEFAULT_;
    if (sin.sin_family != kAfInet && sin.sin_family != kAfUnspec) {
      return kEAFNOSUPPORT_;
    }
    has_dest = true;
    dst_ip = Ntohl(sin.sin_addr);
    dst_port = Ntohs(sin.sin_port);
  }
  std::vector<uint8_t> kbuf(len);
  if (len > 0 && !CopyFromUser(proc, kbuf.data(), buf_ptr, len)) return kEFAULT_;
  return DoInetSend(proc, thrd, desc, kbuf.data(), len, flags, has_dest, dst_ip,
                    dst_port);
}

int64_t InetSysRecvfrom(LinuxProcess* proc, LinuxThread* thrd,
                        FileDescription* desc, uintptr_t buf_ptr, size_t len,
                        int flags, uintptr_t addr_ptr, uintptr_t addrlen_ptr) {
  std::vector<uint8_t> kbuf(len);
  uint32_t src_ip = 0;
  uint16_t src_port = 0;
  ssize_t r = DoInetRecv(proc, thrd, desc, kbuf.data(), len, flags, &src_ip,
                         &src_port);
  if (r < 0) return r;
  if (r > 0 && !CopyToUser(proc, buf_ptr, kbuf.data(), static_cast<size_t>(r))) {
    return kEFAULT_;
  }
  if (addr_ptr != 0 && desc->inet_sock) {
    if (addrlen_ptr == 0) return kEFAULT_;
    int ulen = 0;
    if (!CopyFromUser(proc, &ulen, addrlen_ptr, sizeof(ulen)) || ulen < 0) {
      return kEFAULT_;
    }
    if (desc->inet_sock->domain == kAfNetlink) {
      SockaddrNl nl{};
      nl.nl_family = kAfNetlink;
      nl.nl_pid = 0;  // from kernel
      nl.nl_groups = 0;
      size_t copy_n = std::min<size_t>(static_cast<size_t>(ulen), sizeof(nl));
      if (copy_n > 0 && !CopyToUser(proc, addr_ptr, &nl, copy_n)) return kEFAULT_;
      int actual = sizeof(nl);
      if (!CopyToUser(proc, addrlen_ptr, &actual, sizeof(actual))) return kEFAULT_;
    } else {
      SockaddrIn sin{};
      sin.sin_family = kAfInet;
      sin.sin_port = Htons(src_port);
      sin.sin_addr = Htonl(src_ip);
      size_t copy_n = std::min<size_t>(static_cast<size_t>(ulen), sizeof(sin));
      if (copy_n > 0 && !CopyToUser(proc, addr_ptr, &sin, copy_n)) return kEFAULT_;
      int actual = sizeof(sin);
      if (!CopyToUser(proc, addrlen_ptr, &actual, sizeof(actual))) return kEFAULT_;
    }
  }
  return r;
}

namespace {

struct LinuxMsghdr {
  uintptr_t msg_name;
  uint32_t msg_namelen;
  uint32_t pad0;
  uintptr_t msg_iov;
  uint64_t msg_iovlen;
  uintptr_t msg_control;
  uint64_t msg_controllen;
  int32_t msg_flags;
  int32_t pad1;
};

}  // namespace

int64_t InetDoSendmsg(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t msg_ptr, int flags) {
  LinuxMsghdr mh{};
  if (!CopyFromUser(proc, &mh, msg_ptr, sizeof(mh))) return kEFAULT_;
  if (mh.msg_iovlen > 1024) return kEMSGSIZE_;
  std::vector<LinuxIovec> iovs(mh.msg_iovlen);
  if (mh.msg_iovlen > 0 &&
      !CopyFromUser(proc, iovs.data(), mh.msg_iov,
                    mh.msg_iovlen * sizeof(LinuxIovec))) {
    return kEFAULT_;
  }
  std::vector<uint8_t> payload;
  for (const auto& iv : iovs) {
    if (iv.iov_len == 0) continue;
    size_t old = payload.size();
    payload.resize(old + iv.iov_len);
    if (!CopyFromUser(proc, payload.data() + old,
                      reinterpret_cast<uintptr_t>(iv.iov_base), iv.iov_len)) {
      return kEFAULT_;
    }
  }
  bool has_dest = false;
  uint32_t dst_ip = 0;
  uint16_t dst_port = 0;
  if (mh.msg_name != 0 && mh.msg_namelen >= sizeof(SockaddrIn) &&
      desc->inet_sock && desc->inet_sock->domain == kAfInet) {
    SockaddrIn sin{};
    if (!CopyFromUser(proc, &sin, mh.msg_name, sizeof(sin))) return kEFAULT_;
    has_dest = true;
    dst_ip = Ntohl(sin.sin_addr);
    dst_port = Ntohs(sin.sin_port);
  }
  return DoInetSend(proc, thrd, desc, payload.data(), payload.size(), flags,
                    has_dest, dst_ip, dst_port);
}

int64_t InetDoRecvmsg(LinuxProcess* proc, LinuxThread* thrd,
                      FileDescription* desc, uintptr_t msg_ptr, int flags) {
  LinuxMsghdr mh{};
  if (!CopyFromUser(proc, &mh, msg_ptr, sizeof(mh))) return kEFAULT_;
  if (mh.msg_iovlen > 1024) return kEMSGSIZE_;
  std::vector<LinuxIovec> iovs(mh.msg_iovlen);
  if (mh.msg_iovlen > 0 &&
      !CopyFromUser(proc, iovs.data(), mh.msg_iov,
                    mh.msg_iovlen * sizeof(LinuxIovec))) {
    return kEFAULT_;
  }
  size_t total_cap = 0;
  for (const auto& iv : iovs) total_cap += iv.iov_len;

  std::vector<uint8_t> kbuf(total_cap);
  uint32_t src_ip = 0;
  uint16_t src_port = 0;
  ssize_t r = DoInetRecv(proc, thrd, desc, kbuf.data(), total_cap, flags,
                         &src_ip, &src_port);
  if (r < 0) return r;

  size_t rem = static_cast<size_t>(r);
  size_t off = 0;
  for (const auto& iv : iovs) {
    size_t chunk = std::min(rem, iv.iov_len);
    if (chunk > 0) {
      if (!CopyToUser(proc, reinterpret_cast<uintptr_t>(iv.iov_base),
                      kbuf.data() + off, chunk)) {
        return kEFAULT_;
      }
      off += chunk;
      rem -= chunk;
    }
  }

  if (mh.msg_name != 0 && desc->inet_sock) {
    if (desc->inet_sock->domain == kAfNetlink) {
      SockaddrNl nl{};
      nl.nl_family = kAfNetlink;
      nl.nl_pid = 0;
      nl.nl_groups = 0;
      size_t copy_n = std::min<size_t>(mh.msg_namelen, sizeof(nl));
      if (copy_n > 0 && !CopyToUser(proc, mh.msg_name, &nl, copy_n)) {
        return kEFAULT_;
      }
      mh.msg_namelen = sizeof(nl);
    } else {
      SockaddrIn sin{};
      sin.sin_family = kAfInet;
      sin.sin_port = Htons(src_port);
      sin.sin_addr = Htonl(src_ip);
      size_t copy_n = std::min<size_t>(mh.msg_namelen, sizeof(sin));
      if (copy_n > 0 && !CopyToUser(proc, mh.msg_name, &sin, copy_n)) {
        return kEFAULT_;
      }
      mh.msg_namelen = sizeof(sin);
    }
  }
  mh.msg_controllen = 0;
  mh.msg_flags = 0;
  if (!CopyToUser(proc, msg_ptr, &mh, sizeof(mh))) return kEFAULT_;
  return r;
}

int64_t InetSysShutdown(FileDescription* desc, int how) {
  if (how < 0 || how > 2) return kEINVAL_;
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  if (how == 0 || how == 2) s->rcv_shutdown = true;
  if (how == 1 || how == 2) {
    s->send_shutdown = true;
    if (s->type == kSockStream) {
      if (s->is_loopback) {
        auto peer = s->lo_peer.lock();
        if (peer) {
          peer->peer_fin_received = true;
          BumpSocket(peer.get());
        }
      } else if (!s->fin_sent &&
                 (s->tcp_state == TcpState::kEstablished ||
                  s->tcp_state == TcpState::kCloseWait)) {
        s->fin_sent = true;
        s->tcp_state = (s->tcp_state == TcpState::kEstablished)
                           ? TcpState::kFinWait1
                           : TcpState::kLastAck;
        SendTcpSegmentLocked(net_lock, s.get(), kTcpFin | kTcpAck, nullptr, 0,
                             true);
      }
    }
  }
  BumpSocket(s.get());
  return 0;
}

int64_t InetSysGetName(LinuxProcess* proc, FileDescription* desc,
                       uintptr_t addr_ptr, uintptr_t len_ptr, bool peer) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  int ulen = 0;
  if (!CopyFromUser(proc, &ulen, len_ptr, sizeof(ulen)) || ulen < 0) {
    return kEFAULT_;
  }
  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (s->domain == kAfNetlink) {
    SockaddrNl nl{};
    nl.nl_family = kAfNetlink;
    nl.nl_pid = peer ? 0 : s->nl_pid;
    nl.nl_groups = peer ? 0 : s->nl_groups;
    size_t copy_n = std::min<size_t>(static_cast<size_t>(ulen), sizeof(nl));
    if (copy_n > 0 && !CopyToUser(proc, addr_ptr, &nl, copy_n)) return kEFAULT_;
    int actual = sizeof(nl);
    if (!CopyToUser(proc, len_ptr, &actual, sizeof(actual))) return kEFAULT_;
    return 0;
  }

  if (peer && !s->connected && s->tcp_state != TcpState::kEstablished &&
      s->tcp_state != TcpState::kCloseWait) {
    return kENOTCONN_;
  }
  SockaddrIn sin{};
  sin.sin_family = kAfInet;
  if (peer) {
    sin.sin_addr = Htonl(s->remote_ip_host);
    sin.sin_port = Htons(s->remote_port_host);
  } else {
    uint32_t lip = s->local_ip_host;
    if (lip == 0 && s->connected) {
      lip = ((s->remote_ip_host >> 24) == 127) ? kLoopbackIp : g_local_ip;
    }
    sin.sin_addr = Htonl(lip);
    sin.sin_port = Htons(s->local_port_host);
  }
  size_t copy_n = std::min<size_t>(static_cast<size_t>(ulen), sizeof(sin));
  if (copy_n > 0 && !CopyToUser(proc, addr_ptr, &sin, copy_n)) return kEFAULT_;
  int actual = sizeof(sin);
  if (!CopyToUser(proc, len_ptr, &actual, sizeof(actual))) return kEFAULT_;
  return 0;
}

int64_t InetSysSetsockopt(LinuxProcess* proc, FileDescription* desc, int level,
                          int optname, uintptr_t optval, int optlen) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  if (optlen < 0) return kEINVAL_;

  int val = 0;
  if (optval != 0 && optlen >= 4) {
    if (!CopyFromUser(proc, &val, optval, 4)) return kEFAULT_;
  }

  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (level == kSolSocket) {
    switch (optname) {
      case kSoReuseaddr:
        s->reuseaddr = val ? 1 : 0;
        return 0;
      case kSoReuseport:
        s->reuseport = val ? 1 : 0;
        return 0;
      case kSoKeepalive:
        s->keepalive = val ? 1 : 0;
        return 0;
      case kSoBroadcast:
        s->broadcast = val ? 1 : 0;
        return 0;
      case kSoSndbuf:
      case kSoSndbufforce:
        s->sndbuf = std::max(val * 2, 4608);
        return 0;
      case kSoRcvbuf:
      case kSoRcvbufforce:
        s->rcvbuf = std::max(val * 2, 2304);
        return 0;
      case kSoRcvlowat:
        s->rcvlowat = std::max(val, 1);
        return 0;
      case kSoRcvtimeoOld:
      case kSoRcvtimeoNew:
      case kSoSndtimeoOld:
      case kSoSndtimeoNew: {
        if (optlen < 16) return kEINVAL_;
        int64_t tv[2] = {};
        if (!CopyFromUser(proc, tv, optval, 16)) return kEFAULT_;
        int64_t ns = (tv[0] == 0 && tv[1] == 0)
                         ? -1
                         : (tv[0] * 1000000000LL + tv[1] * 1000LL);
        if (optname == kSoRcvtimeoOld || optname == kSoRcvtimeoNew) {
          s->rcvtimeo_ns = ns;
        } else {
          s->sndtimeo_ns = ns;
        }
        return 0;
      }
      default:
        s->extra_opts[(static_cast<uint64_t>(level) << 32) |
                      static_cast<uint32_t>(optname)] = val;
        return 0;
    }
  }
  s->extra_opts[(static_cast<uint64_t>(level) << 32) |
                static_cast<uint32_t>(optname)] = val;
  return 0;
}

int64_t InetSysGetsockopt(LinuxProcess* proc, FileDescription* desc, int level,
                          int optname, uintptr_t optval, uintptr_t optlen_ptr) {
  auto s = desc->inet_sock;
  if (!s) return kENOTSOCK_;
  int ulen = 0;
  if (!CopyFromUser(proc, &ulen, optlen_ptr, sizeof(ulen)) || ulen < 0) {
    return kEFAULT_;
  }

  auto write_int = [&](int v) -> int64_t {
    int n = std::min<int>(ulen, sizeof(int));
    if (n > 0 && !CopyToUser(proc, optval, &v, static_cast<size_t>(n))) {
      return kEFAULT_;
    }
    if (!CopyToUser(proc, optlen_ptr, &n, sizeof(n))) return kEFAULT_;
    return 0;
  };

  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (level == kSolSocket) {
    switch (optname) {
      case kSoType:
        return write_int(s->type);
      case kSoDomain:
        return write_int(s->domain);
      case kSoProtocol:
        return write_int(s->protocol);
      case kSoAcceptconn:
        return write_int(s->listening ? 1 : 0);
      case kSoError: {
        int e = s->so_error;
        s->so_error = 0;
        return write_int(e);
      }
      case kSoSndbuf:
        return write_int(s->sndbuf);
      case kSoRcvbuf:
        return write_int(s->rcvbuf);
      case kSoReuseaddr:
        return write_int(s->reuseaddr);
      case kSoReuseport:
        return write_int(s->reuseport);
      case kSoKeepalive:
        return write_int(s->keepalive);
      case kSoBroadcast:
        return write_int(s->broadcast);
      case kSoRcvlowat:
        return write_int(s->rcvlowat);
      default:
        break;
    }
  }
  uint64_t key =
      (static_cast<uint64_t>(level) << 32) | static_cast<uint32_t>(optname);
  auto it = s->extra_opts.find(key);
  return write_int(it != s->extra_opts.end() ? it->second : 0);
}

int16_t InetSocketPollEvents(FileDescription* desc, uint64_t* activity) {
  auto s = desc->inet_sock;
  if (!s) return kPollNval;
  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (activity) *activity = s->activity.load(std::memory_order_relaxed);

  int16_t rev = 0;
  if (s->so_error != 0) rev |= kPollErr;

  if (s->domain == kAfNetlink || s->type == kSockDgram || s->type == kSockRaw) {
    if (!s->rx_dgrams.empty() || s->rcv_shutdown) {
      rev |= kPollIn | kPollRdNorm;
    }
    if (!s->send_shutdown) {
      rev |= kPollOut | kPollWrNorm;
    }
    return rev;
  }

  // SOCK_STREAM (TCP)
  if (s->listening) {
    if (!s->accept_queue.empty()) rev |= kPollIn | kPollRdNorm;
    return rev;
  }
  if (s->tcp_state == TcpState::kSynSent) {
    return rev;
  }
  if (!s->rx_stream.empty() || s->peer_fin_received || s->rcv_shutdown ||
      s->so_error != 0) {
    rev |= kPollIn | kPollRdNorm;
  }
  if (s->peer_fin_received) {
    rev |= kPollRdHup;
  }
  if (s->tcp_state == TcpState::kEstablished ||
      s->tcp_state == TcpState::kCloseWait) {
    if (!s->send_shutdown) rev |= kPollOut | kPollWrNorm;
  } else if (s->tcp_state == TcpState::kClosed) {
    rev |= kPollHup | kPollIn | kPollOut;
  }
  return rev;
}

int InetSocketBytesAvailable(FileDescription* desc) {
  auto s = desc->inet_sock;
  if (!s) return 0;
  std::lock_guard<std::mutex> lock(g_net_mtx);
  if (s->domain == kAfNetlink || s->type == kSockDgram || s->type == kSockRaw) {
    return s->rx_dgrams.empty()
               ? 0
               : static_cast<int>(s->rx_dgrams.front().data.size());
  }
  return static_cast<int>(s->rx_stream.size());
}

void InetSocketOnDescriptionDestroyed(FileDescription* desc) {
  auto s = desc->inet_sock;
  if (!s) return;
  std::unique_lock<std::mutex> net_lock(g_net_mtx);
  if (s->closed) return;
  s->closed = true;
  if (s->type == kSockStream) {
    if (s->is_loopback) {
      auto peer = s->lo_peer.lock();
      if (peer) {
        peer->peer_fin_received = true;
        BumpSocket(peer.get());
      }
    } else if (!s->fin_sent &&
               (s->tcp_state == TcpState::kEstablished ||
                s->tcp_state == TcpState::kCloseWait)) {
      s->fin_sent = true;
      s->tcp_state = (s->tcp_state == TcpState::kEstablished)
                         ? TcpState::kFinWait1
                         : TcpState::kLastAck;
      SendTcpSegmentLocked(net_lock, s.get(), kTcpFin | kTcpAck, nullptr, 0,
                           true);
    } else if (s->tcp_state == TcpState::kSynSent) {
      s->tcp_state = TcpState::kClosed;
      s->tx_unacked.clear();
    }
  }
  BumpSocket(s.get());
}

int64_t InetSocketIoctl(LinuxProcess* proc, FileDescription* desc,
                        uint64_t cmd, uint64_t arg) {
  if (cmd == 0x541B /* FIONREAD */) {
    int avail = InetSocketBytesAvailable(desc);
    return CopyToUser(proc, arg, &avail, sizeof(avail)) ? 0 : kEFAULT_;
  }
  if (cmd == 0x5421 /* FIONBIO */) {
    int nb = 0;
    if (!CopyFromUser(proc, &nb, arg, sizeof(nb))) return kEFAULT_;
    std::lock_guard<std::mutex> lock(desc->mtx);
    if (nb) desc->flags |= kLinuxO_NONBLOCK;
    else desc->flags &= ~kLinuxO_NONBLOCK;
    return 0;
  }

  // Linux struct ifconf / struct ifreq (40 bytes on x86_64)
  if (cmd == 0x8912 /* SIOCGIFCONF */) {
    struct IfConf {
      int32_t ifc_len;
      int32_t pad;
      uintptr_t ifc_buf;
    } ifc{};
    if (!CopyFromUser(proc, &ifc, arg, sizeof(ifc))) return kEFAULT_;
    uint8_t recs[80] = {};
    // Entry 0: lo (127.0.0.1)
    memcpy(recs + 0, "lo", 2);
    auto* sin_lo = reinterpret_cast<SockaddrIn*>(recs + 16);
    sin_lo->sin_family = kAfInet;
    sin_lo->sin_addr = Htonl(kLoopbackIp);
    // Entry 1: eth0 (10.0.2.15)
    memcpy(recs + 40, "eth0", 4);
    auto* sin_eth = reinterpret_cast<SockaddrIn*>(recs + 40 + 16);
    sin_eth->sin_family = kAfInet;
    sin_eth->sin_addr = Htonl(g_local_ip);

    if (ifc.ifc_buf == 0) {
      ifc.ifc_len = 80;
    } else {
      int to_write = std::min<int>(ifc.ifc_len, 80);
      to_write = (to_write / 40) * 40;
      if (to_write > 0 &&
          !CopyToUser(proc, ifc.ifc_buf, recs, static_cast<size_t>(to_write))) {
        return kEFAULT_;
      }
      ifc.ifc_len = to_write;
    }
    return CopyToUser(proc, arg, &ifc, sizeof(ifc)) ? 0 : kEFAULT_;
  }

  if ((cmd >= 0x8910 && cmd <= 0x8938) || cmd == 0x8946 /* SIOCETHTOOL */) {
    uint8_t ifr[40] = {};
    if (!CopyFromUser(proc, ifr, arg, sizeof(ifr))) return kEFAULT_;
    if (cmd == 0x8910 /* SIOCGIFNAME */) {
      int32_t idx = 0;
      memcpy(&idx, ifr + 16, sizeof(idx));
      memset(ifr, 0, 16);
      if (idx == 1) {
        memcpy(ifr, "lo", 2);
      } else if (idx == 2) {
        memcpy(ifr, "eth0", 4);
      } else {
        return kENODEV_;
      }
      return CopyToUser(proc, arg, ifr, sizeof(ifr)) ? 0 : kEFAULT_;
    }

    char ifname[17] = {};
    memcpy(ifname, ifr, 16);
    bool is_lo = (strcmp(ifname, "lo") == 0);
    bool is_eth = (strcmp(ifname, "eth0") == 0 || ifname[0] == '\0');
    if (!is_lo && !is_eth) return kENODEV_;
    if (cmd == 0x8946 /* SIOCETHTOOL */) {
      return is_eth ? 0 : kEOPNOTSUPP_;
    }

    auto* sin = reinterpret_cast<SockaddrIn*>(ifr + 16);
    switch (cmd) {
      case 0x8913: {  // SIOCGIFFLAGS
        int16_t fl = is_lo ? 0x49 /* UP|LOOPBACK|RUNNING */
                           : 0x1043 /* UP|BROADCAST|RUNNING|MULTICAST */;
        memcpy(ifr + 16, &fl, sizeof(fl));
        break;
      }
      case 0x8914:  // SIOCSIFFLAGS
        return 0;
      case 0x8915:  // SIOCGIFADDR
        memset(ifr + 16, 0, 24);
        sin->sin_family = kAfInet;
        sin->sin_addr = Htonl(is_lo ? kLoopbackIp : g_local_ip);
        break;
      case 0x8916:  // SIOCSIFADDR
        if (!is_lo && sin->sin_family == kAfInet) {
          g_local_ip = Ntohl(sin->sin_addr);
        }
        return 0;
      case 0x8917:  // SIOCGIFDSTADDR
      case 0x8919:  // SIOCGIFBRDADDR
        memset(ifr + 16, 0, 24);
        sin->sin_family = kAfInet;
        sin->sin_addr =
            Htonl(is_lo ? kLoopbackIp : (g_local_ip | ~g_netmask));
        break;
      case 0x891B:  // SIOCGIFNETMASK
        memset(ifr + 16, 0, 24);
        sin->sin_family = kAfInet;
        sin->sin_addr = Htonl(is_lo ? 0xFF000000U : g_netmask);
        break;
      case 0x891C:  // SIOCSIFNETMASK
        if (!is_lo && sin->sin_family == kAfInet) {
          g_netmask = Ntohl(sin->sin_addr);
        }
        return 0;
      case 0x8921: {  // SIOCGIFMTU
        int32_t mtu = is_lo ? 65536 : 1500;
        memcpy(ifr + 16, &mtu, sizeof(mtu));
        break;
      }
      case 0x8927: {  // SIOCGIFHWADDR
        memset(ifr + 16, 0, 24);
        uint16_t family = is_lo ? 772 : 1;
        memcpy(ifr + 16, &family, 2);
        if (!is_lo) memcpy(ifr + 18, g_nic.mac, 6);
        break;
      }
      case 0x8933: {  // SIOCGIFINDEX
        int32_t idx = is_lo ? 1 : 2;
        memcpy(ifr + 16, &idx, sizeof(idx));
        break;
      }
      default:
        return kEOPNOTSUPP_;
    }
    return CopyToUser(proc, arg, ifr, sizeof(ifr)) ? 0 : kEFAULT_;
  }

  return kEOPNOTSUPP_;
}
