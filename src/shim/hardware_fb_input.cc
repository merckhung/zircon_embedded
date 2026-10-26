#include "src/shim/fd_ops.h"
#include "src/shim/net_inet.h"
#include "src/shim/tty.h"
#include "src/shim/vfs.h"

namespace {

FbHardwareInfo g_fb_info;
std::mutex g_input_mtx;
std::condition_variable g_input_cv;
std::deque<LinuxInputEvent> g_input_events;
std::atomic<uint64_t> g_input_counter{0};
bool g_input_available = false;

// Port I/O helpers (used after zx_ioports_request enables IOPB bits)
#if defined(__x86_64__)
inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %b0, %w1" : : "a"(val), "Nd"(port));
}
inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
  return ret;
}
inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %w0, %w1" : : "a"(val), "Nd"(port));
}
inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ volatile("inw %w1, %w0" : "=a"(ret) : "Nd"(port));
  return ret;
}
inline void outl(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %w1" : : "a"(val), "Nd"(port));
}
inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %w1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
#else
inline void outb(uint16_t, uint8_t) {}
inline uint8_t inb(uint16_t) { return 0xFF; }
inline void outw(uint16_t, uint16_t) {}
inline uint16_t inw(uint16_t) { return 0xFFFF; }
inline void outl(uint16_t, uint32_t) {}
inline uint32_t inl(uint16_t) { return 0xFFFFFFFF; }
#endif

uint32_t PciConfigRead32(uint8_t bus, uint8_t slot, uint8_t func,
                         uint8_t offset) {
  uint32_t address = (1U << 31) | (static_cast<uint32_t>(bus) << 16) |
                     (static_cast<uint32_t>(slot) << 11) |
                     (static_cast<uint32_t>(func) << 8) | (offset & 0xFC);
  outl(0xCF8, address);
  return inl(0xCFC);
}

void PciConfigWrite32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                      uint32_t val) {
  uint32_t address = (1U << 31) | (static_cast<uint32_t>(bus) << 16) |
                     (static_cast<uint32_t>(slot) << 11) |
                     (static_cast<uint32_t>(func) << 8) | (offset & 0xFC);
  outl(0xCF8, address);
  outl(0xCFC, val);
}

void BochsVbeWrite(uint16_t index, uint16_t value) {
  outw(0x01CE, index);
  outw(0x01CF, value);
}

uint16_t BochsVbeRead(uint16_t index) {
  outw(0x01CE, index);
  return inw(0x01CF);
}

struct PciDisplay {
  bool found = false;
  uint8_t bus = 0, slot = 0, func = 0;
  uint64_t bar0 = 0;
  uint64_t bar0_size = 0;
};

#if defined(__x86_64__)
// Find the Bochs/QEMU standard VGA (PCI 1234:1111) and size its BAR0.
PciDisplay FindBochsVga() {
  PciDisplay d;
  for (int bus = 0; bus < 256 && !d.found; ++bus) {
    for (uint8_t slot = 0; slot < 32 && !d.found; ++slot) {
      uint32_t id0 = PciConfigRead32(static_cast<uint8_t>(bus), slot, 0, 0x00);
      if ((id0 & 0xFFFF) == 0xFFFF) continue;
      uint32_t hdr = PciConfigRead32(static_cast<uint8_t>(bus), slot, 0, 0x0C);
      int nfunc = ((hdr >> 16) & 0x80) ? 8 : 1;
      for (uint8_t func = 0; func < nfunc; ++func) {
        uint32_t id = PciConfigRead32(static_cast<uint8_t>(bus), slot, func, 0);
        if (id != 0x11111234) continue;  // vendor 0x1234, device 0x1111
        d.found = true;
        d.bus = static_cast<uint8_t>(bus);
        d.slot = slot;
        d.func = func;
        break;
      }
    }
  }
  if (!d.found) return d;

  // Size BAR0 the standard way: disable decoding, write all-ones, read back.
  uint32_t cmd = PciConfigRead32(d.bus, d.slot, d.func, 0x04);
  PciConfigWrite32(d.bus, d.slot, d.func, 0x04, cmd & ~0x3U);
  uint32_t lo = PciConfigRead32(d.bus, d.slot, d.func, 0x10);
  bool is64 = (lo & 0x6) == 0x4;
  uint32_t hi = is64 ? PciConfigRead32(d.bus, d.slot, d.func, 0x14) : 0;
  PciConfigWrite32(d.bus, d.slot, d.func, 0x10, 0xFFFFFFFF);
  uint32_t lo_mask = PciConfigRead32(d.bus, d.slot, d.func, 0x10);
  uint32_t hi_mask = 0xFFFFFFFF;
  if (is64) {
    PciConfigWrite32(d.bus, d.slot, d.func, 0x14, 0xFFFFFFFF);
    hi_mask = PciConfigRead32(d.bus, d.slot, d.func, 0x14);
    PciConfigWrite32(d.bus, d.slot, d.func, 0x14, hi);
  }
  PciConfigWrite32(d.bus, d.slot, d.func, 0x10, lo);
  // Enable memory space + I/O space decoding.
  PciConfigWrite32(d.bus, d.slot, d.func, 0x04, cmd | 0x3);

  if (lo & 0x1) {  // I/O BAR: not a framebuffer
    d.found = false;
    return d;
  }
  uint64_t mask = (static_cast<uint64_t>(hi_mask) << 32) | (lo_mask & ~0xFULL);
  if (!is64) mask |= 0xFFFFFFFF00000000ULL;
  d.bar0 = (static_cast<uint64_t>(hi) << 32) | (lo & ~0xFULL);
  d.bar0_size = mask ? (~mask + 1) : 0;
  if (d.bar0 == 0 || d.bar0_size == 0) d.found = false;
  return d;
}
#endif

void PushInputEventLocked(uint16_t type, uint16_t code, int32_t value) {
  uint64_t now_ns = zx_clock_get_monotonic();
  LinuxInputEvent ev{};
  ev.tv_sec = static_cast<int64_t>(now_ns / 1000000000ULL);
  ev.tv_usec = static_cast<int64_t>((now_ns / 1000ULL) % 1000000ULL);
  ev.type = type;
  ev.code = code;
  ev.value = value;
  g_input_events.push_back(ev);
  g_input_counter++;
}

// UART receive: feed bytes into the console tty's line discipline.
void SerialRxWorker() {
  if (g_debug_rsrc == ZX_HANDLE_INVALID) return;
  auto console = GetConsoleTty();
  while (true) {
    char raw[64];
    size_t actual = 0;
    zx_status_t st = zx_debug_read(g_debug_rsrc, raw, sizeof(raw), &actual);
    if (st != ZX_OK || actual == 0) {
      zx_nanosleep(zx_deadline_after(ZX_MSEC(10)));
      continue;
    }
    TtyInput(console, reinterpret_cast<const uint8_t*>(raw), actual);
  }
}

void Ps2WaitInputClear() {
  for (int i = 0; i < 10000; ++i) {
    if ((inb(0x64) & 0x02) == 0) return;
  }
}

void Ps2WaitOutputReady() {
  for (int i = 0; i < 10000; ++i) {
    if ((inb(0x64) & 0x01) != 0) return;
  }
}

void InitPs2Controller() {
  // Drain any stale bytes sitting in the i8042 output buffer.
  for (int i = 0; i < 64 && (inb(0x64) & 0x01) != 0; ++i) {
    (void)inb(0x60);
  }
  // Enable keyboard (0xAE) and auxiliary mouse (0xA8) ports on i8042.
  Ps2WaitInputClear();
  outb(0x64, 0xAE);
  Ps2WaitInputClear();
  outb(0x64, 0xA8);
  // Read command byte, enable both clocks (clear 0x30), keep translation (0x40).
  Ps2WaitInputClear();
  outb(0x64, 0x20);
  Ps2WaitOutputReady();
  uint8_t status = (inb(0x60) | 0x03) & ~0x30;
  Ps2WaitInputClear();
  outb(0x64, 0x60);
  Ps2WaitInputClear();
  outb(0x60, status);
  // Set mouse defaults & enable packet streaming.
  auto mouse_write = [](uint8_t b) {
    Ps2WaitInputClear();
    outb(0x64, 0xD4);
    Ps2WaitInputClear();
    outb(0x60, b);
    Ps2WaitOutputReady();
    (void)inb(0x60);
  };
  mouse_write(0xF6);
  mouse_write(0xF4);
  // Drain any leftover ACK bytes.
  for (int i = 0; i < 16 && (inb(0x64) & 0x01) != 0; ++i) {
    (void)inb(0x60);
  }
}

struct VmPortRegs {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
};

#if defined(__x86_64__)
inline VmPortRegs VmPortCall(uint32_t cmd, uint32_t arg) {
  VmPortRegs r;
  r.eax = 0x564D5868U;  // VMMOUSE_MAGIC ("VMXh")
  r.ebx = arg;
  r.ecx = cmd;
  r.edx = 0x5658U;      // VMMOUSE_PORT
  __asm__ volatile("inl %%dx, %%eax"
                   : "+a"(r.eax), "+b"(r.ebx), "+c"(r.ecx), "+d"(r.edx)
                   :
                   : "memory");
  return r;
}
#else
inline VmPortRegs VmPortCall(uint32_t, uint32_t) {
  return VmPortRegs{};
}
#endif

bool InitVmMouse() {
  if (g_ioport_rsrc == ZX_HANDLE_INVALID ||
      zx_ioports_request(g_ioport_rsrc, 0x5658, 4) != ZX_OK) {
    return false;
  }
  // Probe VMPORT presence via VMMOUSE_GETVERSION (10).
  VmPortRegs ver = VmPortCall(10, 0);
  if (ver.ebx != 0x564D5868U) {
    return false;
  }
  // Enable vmmouse: send READ_ID (0x45414552) via VMMOUSE_COMMAND (41),
  // read 1 word via VMMOUSE_DATA (39) and verify VMMOUSE_VERSION_ID (0x3442554a),
  // then request absolute pointer mode (0x53424152).
  VmPortCall(41, 0x45414552U);
  VmPortRegs id = VmPortCall(39, 1);
  if (id.eax != 0x3442554AU) {
    return false;
  }
  VmPortCall(41, 0x53424152U);
  ShimLogf("[zircon-init] Enabled QEMU vmmouse absolute pointer (port 0x5658)\n");
  return true;
}

void HardwareInputWorker() {
  InitPs2Controller();
  const bool vmmouse_active = InitVmMouse();

  uint8_t mouse_pkt[3] = {0, 0, 0};
  int mouse_idx = 0;
  uint8_t prev_buttons = 0;
  int32_t prev_abs_x = -1;
  int32_t prev_abs_y = -1;
  bool e0_prefix = false;

  while (true) {
    bool had_input = false;

    if (vmmouse_active) {
      int32_t pending_abs_x = prev_abs_x;
      int32_t pending_abs_y = prev_abs_y;
      bool has_pending_abs = false;
      for (int pkt = 0; pkt < 64; ++pkt) {
        VmPortRegs st = VmPortCall(40 /* VMMOUSE_STATUS */, 0);
        if (st.eax == 0xFFFF0000U || (st.eax & 0xFFFFU) < 4) break;
        VmPortRegs d = VmPortCall(39 /* VMMOUSE_DATA */, 4);
        uint8_t btns = 0;
        if (d.eax & 0x20U) btns |= 1;  // Left
        if (d.eax & 0x10U) btns |= 2;  // Right
        if (d.eax & 0x08U) btns |= 4;  // Middle

        if (d.eax & 0x00010000U) {
          // Relative packet fallback
          std::lock_guard<std::mutex> lock(g_input_mtx);
          int32_t dx = static_cast<int32_t>(d.ebx);
          int32_t dy = static_cast<int32_t>(d.ecx);
          if (dx != 0) PushInputEventLocked(0x02, 0x00, dx);
          if (dy != 0) PushInputEventLocked(0x02, 0x01, dy);
          if ((btns & 1) != (prev_buttons & 1)) {
            PushInputEventLocked(0x01, 0x110, (btns & 1) ? 1 : 0);
          }
          if ((btns & 2) != (prev_buttons & 2)) {
            PushInputEventLocked(0x01, 0x111, (btns & 2) ? 1 : 0);
          }
          if ((btns & 4) != (prev_buttons & 4)) {
            PushInputEventLocked(0x01, 0x112, (btns & 4) ? 1 : 0);
          }
          PushInputEventLocked(0x00, 0x00, 0);
          prev_buttons = btns;
          had_input = true;
        } else {
          // Absolute coordinates in 0..65535. Coalesce pure motion packets
          // within a single poll tick, flushing immediately if buttons change.
          pending_abs_x = static_cast<int32_t>(d.ebx & 0xFFFFU);
          pending_abs_y = static_cast<int32_t>(d.ecx & 0xFFFFU);
          has_pending_abs = true;
          if (btns != prev_buttons) {
            std::lock_guard<std::mutex> lock(g_input_mtx);
            if (pending_abs_x != prev_abs_x) {
              PushInputEventLocked(0x03, 0x00, pending_abs_x);
              prev_abs_x = pending_abs_x;
            }
            if (pending_abs_y != prev_abs_y) {
              PushInputEventLocked(0x03, 0x01, pending_abs_y);
              prev_abs_y = pending_abs_y;
            }
            if ((btns & 1) != (prev_buttons & 1)) {
              PushInputEventLocked(0x01, 0x110, (btns & 1) ? 1 : 0);
            }
            if ((btns & 2) != (prev_buttons & 2)) {
              PushInputEventLocked(0x01, 0x111, (btns & 2) ? 1 : 0);
            }
            if ((btns & 4) != (prev_buttons & 4)) {
              PushInputEventLocked(0x01, 0x112, (btns & 4) ? 1 : 0);
            }
            PushInputEventLocked(0x00, 0x00, 0);
            prev_buttons = btns;
            has_pending_abs = false;
            had_input = true;
          }
        }
      }
      if (has_pending_abs &&
          (pending_abs_x != prev_abs_x || pending_abs_y != prev_abs_y)) {
        std::lock_guard<std::mutex> lock(g_input_mtx);
        if (pending_abs_x != prev_abs_x) {
          PushInputEventLocked(0x03, 0x00, pending_abs_x);
          prev_abs_x = pending_abs_x;
        }
        if (pending_abs_y != prev_abs_y) {
          PushInputEventLocked(0x03, 0x01, pending_abs_y);
          prev_abs_y = pending_abs_y;
        }
        PushInputEventLocked(0x00, 0x00, 0);
        had_input = true;
      }
    }

    for (int iter = 0; iter < 128; ++iter) {
      uint8_t st = inb(0x64);
      if ((st & 0x01) == 0) break;
      uint8_t data = inb(0x60);

      std::lock_guard<std::mutex> lock(g_input_mtx);
      if (st & 0x20) {
        if (vmmouse_active) {
          // When vmmouse is active, i8042 aux bytes are just IRQ/FIFO pokes;
          // actual mouse packets were already drained from VMPORT 0x5658 above.
          continue;
        }
        // PS/2 Aux (Mouse) byte
        if (mouse_idx == 0 && (data & 0x08) == 0) {
          continue;
        }
        mouse_pkt[mouse_idx++] = data;
        if (mouse_idx == 3) {
          mouse_idx = 0;
          int dx = static_cast<int>(mouse_pkt[1]) -
                   ((mouse_pkt[0] & 0x10) ? 256 : 0);
          int dy = -(static_cast<int>(mouse_pkt[2]) -
                     ((mouse_pkt[0] & 0x20) ? 256 : 0));
          uint8_t btns = mouse_pkt[0] & 0x07;

          if (dx != 0) PushInputEventLocked(0x02, 0x00, dx);  // EV_REL, REL_X
          if (dy != 0) PushInputEventLocked(0x02, 0x01, dy);  // EV_REL, REL_Y
          if ((btns & 1) != (prev_buttons & 1)) {
            PushInputEventLocked(0x01, 0x110, (btns & 1) ? 1 : 0);  // BTN_LEFT
          }
          if ((btns & 2) != (prev_buttons & 2)) {
            PushInputEventLocked(0x01, 0x111, (btns & 2) ? 1 : 0);  // BTN_RIGHT
          }
          if ((btns & 4) != (prev_buttons & 4)) {
            PushInputEventLocked(0x01, 0x112, (btns & 4) ? 1 : 0);  // BTN_MIDDLE
          }
          PushInputEventLocked(0x00, 0x00, 0);  // EV_SYN
          prev_buttons = btns;
          had_input = true;
        }
      } else {
        // PS/2 Keyboard Set 1 scancode (maps 1:1 to Linux KEY_* for 1..88)
        if (data == 0xE0) {
          e0_prefix = true;
          continue;
        }
        bool release = (data & 0x80) != 0;
        uint16_t code = data & 0x7F;
        if (e0_prefix) {
          e0_prefix = false;
          if (code == 0x48) code = 103;       // KEY_UP
          else if (code == 0x50) code = 108;  // KEY_DOWN
          else if (code == 0x4B) code = 105;  // KEY_LEFT
          else if (code == 0x4D) code = 106;  // KEY_RIGHT
          else if (code == 0x1C) code = 96;   // KEY_KPENTER
          else if (code == 0x53) code = 111;  // KEY_DELETE
        }
        if (code > 0) {
          PushInputEventLocked(0x01, code, release ? 0 : 1);  // EV_KEY
          PushInputEventLocked(0x00, 0x00, 0);                // EV_SYN
          had_input = true;
        }
      }
    }

    if (had_input) {
      g_input_cv.notify_all();
      NotifyPollWaiters();
    }

    zx_nanosleep(zx_deadline_after(ZX_MSEC(4)));
  }
}

void InitBochsDisplay() {
#if defined(__x86_64__)
  if (g_ioport_rsrc == ZX_HANDLE_INVALID || g_mmio_rsrc == ZX_HANDLE_INVALID) {
    return;
  }
  if (zx_ioports_request(g_ioport_rsrc, 0xCF8, 8) != ZX_OK ||
      zx_ioports_request(g_ioport_rsrc, 0x01CE, 4) != ZX_OK) {
    return;
  }
  PciDisplay pci = FindBochsVga();
  if (!pci.found) {
    ShimLogf("[zircon-init] No Bochs/QEMU std VGA (PCI 1234:1111): no /dev/fb0\n");
    return;
  }
  uint16_t vbe_id = BochsVbeRead(0x00);  // VBE_DISPI_INDEX_ID
  if (vbe_id < 0xB0C0 || vbe_id > 0xB0CF) {
    ShimLogf("[zircon-init] PCI 1234:1111 has no Bochs VBE interface (id=0x%04x)\n",
             vbe_id);
    return;
  }
  // Video memory reported by the adapter (in 64 KiB units).
  uint64_t vram = static_cast<uint64_t>(BochsVbeRead(0x0A)) * 65536ULL;
  uint64_t fb_mem = pci.bar0_size;
  if (vram != 0 && vram < fb_mem) fb_mem = vram;

  const uint32_t kWidth = 1024, kHeight = 768, kBpp = 32;
  if (static_cast<uint64_t>(kWidth) * kHeight * 4 > fb_mem) {
    ShimLogf("[zircon-init] Bochs VGA memory too small (%llu bytes)\n",
             static_cast<unsigned long long>(fb_mem));
    return;
  }
  BochsVbeWrite(0x04, 0x00);         // VBE_DISPI_INDEX_ENABLE = DISABLED
  BochsVbeWrite(0x01, kWidth);       // XRES
  BochsVbeWrite(0x02, kHeight);      // YRES
  BochsVbeWrite(0x03, kBpp);         // BPP
  BochsVbeWrite(0x06, kWidth);       // VIRT_WIDTH
  BochsVbeWrite(0x07, kHeight);      // VIRT_HEIGHT
  BochsVbeWrite(0x08, 0);            // X_OFFSET
  BochsVbeWrite(0x09, 0);            // Y_OFFSET
  BochsVbeWrite(0x04, 0x01 | 0x40);  // ENABLED | LFB_ENABLED
  // Read back the mode the adapter actually accepted.
  uint32_t width = BochsVbeRead(0x01), height = BochsVbeRead(0x02);
  uint32_t bpp = BochsVbeRead(0x03), virt_w = BochsVbeRead(0x06);
  if (bpp != 32 || width == 0 || height == 0) {
    ShimLogf("[zircon-init] Bochs VBE rejected %ux%ux32 mode\n", kWidth, kHeight);
    return;
  }
  uint32_t stride = (virt_w ? virt_w : width) * 4;

  size_t map_size = (fb_mem + 4095ULL) & ~4095ULL;
  zx_handle_t bar_rsrc = ZX_HANDLE_INVALID;
  const char rsrc_name[] = "bochs_vbe_fb";
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, pci.bar0, map_size,
                         rsrc_name, sizeof(rsrc_name), &bar_rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t phys_vmo = ZX_HANDLE_INVALID;
  zx_status_t st = zx_vmo_create_physical(bar_rsrc, pci.bar0, map_size, &phys_vmo);
  zx_handle_close(bar_rsrc);
  if (st != ZX_OK) return;
  zx_vmo_set_cache_policy(phys_vmo, ZX_CACHE_POLICY_WRITE_COMBINING);
  uintptr_t mapped = 0;
  if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                  phys_vmo, 0, map_size, &mapped) != ZX_OK) {
    zx_handle_close(phys_vmo);
    return;
  }
  g_fb_info.active = true;
  g_fb_info.phys_addr = pci.bar0;
  g_fb_info.width = width;
  g_fb_info.height = height;
  g_fb_info.stride_bytes = stride;
  g_fb_info.size_bytes = stride * height;
  g_fb_info.vram_bytes = map_size;
  g_fb_info.phys_vmo = phys_vmo;
  g_fb_info.mapped_fb = reinterpret_cast<uint32_t*>(mapped);

  // Paint an initial dark-blue gradient (cosmetic boot background).
  for (uint32_t y = 0; y < height; ++y) {
    uint32_t r = 18 + (y * 16) / height;
    uint32_t g = 34 + (y * 36) / height;
    uint32_t b = 64 + (y * 64) / height;
    uint32_t px = (0xFFU << 24) | (r << 16) | (g << 8) | b;
    uint32_t* row = g_fb_info.mapped_fb + y * (stride / 4);
    for (uint32_t x = 0; x < width; ++x) row[x] = px;
  }
  Vfs::CreateDevice("/dev/fb0", 0660, (29ULL << 8) | 0);
  ShimLogf(
      "[zircon-init] Bochs VBE %ux%ux32 LFB at BAR0=0x%llx (%llu KiB, PCI "
      "%02x:%02x.%x, VBE ID=0x%04x)\n",
      width, height, static_cast<unsigned long long>(pci.bar0),
      static_cast<unsigned long long>(map_size / 1024), pci.bus, pci.slot,
      pci.func, vbe_id);
#elif defined(__aarch64__)
  if (g_mmio_rsrc == ZX_HANDLE_INVALID) return;
  // Map PCIe ECAM at 0x3f000000 (1 MiB for Bus 0).
  const uint64_t kEcamPhys = 0x3f000000ULL;
  const size_t kEcamSize = 0x100000ULL;
  zx_handle_t ecam_rsrc = ZX_HANDLE_INVALID;
  const char ecam_name[] = "pcie_ecam";
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, kEcamPhys, kEcamSize,
                         ecam_name, sizeof(ecam_name), &ecam_rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t ecam_vmo = ZX_HANDLE_INVALID;
  zx_status_t st = zx_vmo_create_physical(ecam_rsrc, kEcamPhys, kEcamSize, &ecam_vmo);
  zx_handle_close(ecam_rsrc);
  if (st != ZX_OK) return;
  uintptr_t ecam_base = 0;
  st = zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                   ecam_vmo, 0, kEcamSize, &ecam_base);
  zx_handle_close(ecam_vmo);
  if (st != ZX_OK) return;

  // Scan bus 0 slots for Bochs VGA / Bochs Display (PCI 1234:1111).
  int found_slot = -1;
  for (int slot = 0; slot < 32; ++slot) {
    volatile uint32_t* dev_cfg = reinterpret_cast<volatile uint32_t*>(
        ecam_base + (static_cast<uintptr_t>(slot) << 15));
    uint32_t id = dev_cfg[0];  // vendor/device
    if (id == 0x11111234) {
      found_slot = slot;
      break;
    }
  }
  if (found_slot < 0) {
    zx_vmar_unmap(zx_vmar_root_self(), ecam_base, kEcamSize);
    ShimLogf("[zircon-init] No Bochs display found on PCIe ECAM\n");
    return;
  }

  volatile uint32_t* dev_cfg = reinterpret_cast<volatile uint32_t*>(
      ecam_base + (static_cast<uintptr_t>(found_slot) << 15));
  // BAR0 is at offset 0x10 (index 4 of 32-bit words): 16 MiB VRAM.
  // BAR2 is at offset 0x18 (index 6 of 32-bit words): 4 KiB MMIO registers.
  // Assign physical addresses in QEMU virt 32-bit PCI MMIO aperture (0x10000000 - 0x3eff0000).
  const uint64_t kFbPhys = 0x10000000ULL;
  const uint64_t kMmioPhys = 0x11000000ULL;
  dev_cfg[4] = static_cast<uint32_t>(kFbPhys);
  dev_cfg[6] = static_cast<uint32_t>(kMmioPhys);
  // Enable Memory Space (bit 1) and Bus Master (bit 2) in Command register (offset 0x04, index 1).
  uint32_t cmd = dev_cfg[1];
  dev_cfg[1] = cmd | 0x0006U;
  zx_vmar_unmap(zx_vmar_root_self(), ecam_base, kEcamSize);

  const uint32_t kWidth = 1024, kHeight = 768;
  const uint32_t stride = kWidth * 4;
  const size_t map_size = 16 * 1024 * 1024;  // 16 MB VRAM

  // Program Bochs VBE DISPI registers via BAR2 MMIO (offset 0x500).
  uint16_t vbe_id = 0;
  {
    const size_t kMmioSize = 4096;
    zx_handle_t mmio_rsrc = ZX_HANDLE_INVALID;
    const char mmio_name[] = "bochs_display_mmio";
    if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, kMmioPhys, kMmioSize,
                           mmio_name, sizeof(mmio_name), &mmio_rsrc) == ZX_OK) {
      zx_handle_t mmio_vmo = ZX_HANDLE_INVALID;
      if (zx_vmo_create_physical(mmio_rsrc, kMmioPhys, kMmioSize, &mmio_vmo) == ZX_OK) {
        zx_vmo_set_cache_policy(mmio_vmo, ZX_CACHE_POLICY_UNCACHED_DEVICE);
        uintptr_t mmio_base = 0;
        if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                        mmio_vmo, 0, kMmioSize, &mmio_base) == ZX_OK) {
          volatile uint16_t* dispi = reinterpret_cast<volatile uint16_t*>(mmio_base + 0x500);
          vbe_id = dispi[0];  // VBE_DISPI_INDEX_ID
          dispi[4] = 0x00;    // VBE_DISPI_INDEX_ENABLE = disabled
          dispi[1] = kWidth;  // VBE_DISPI_INDEX_XRES
          dispi[2] = kHeight; // VBE_DISPI_INDEX_YRES
          dispi[3] = 32;      // VBE_DISPI_INDEX_BPP
          dispi[6] = kWidth;  // VBE_DISPI_INDEX_VIRT_WIDTH
          dispi[7] = kHeight; // VBE_DISPI_INDEX_VIRT_HEIGHT
          dispi[8] = 0;       // VBE_DISPI_INDEX_X_OFFSET
          dispi[9] = 0;       // VBE_DISPI_INDEX_Y_OFFSET
          dispi[4] = 0x41;    // VBE_DISPI_ENABLED (0x01) | VBE_DISPI_LFB_ENABLED (0x40)
          zx_vmar_unmap(zx_vmar_root_self(), mmio_base, kMmioSize);
        }
        zx_handle_close(mmio_vmo);
      }
      zx_handle_close(mmio_rsrc);
    }
  }

  zx_handle_t bar_rsrc = ZX_HANDLE_INVALID;
  const char rsrc_name[] = "bochs_display_fb";
  if (zx_resource_create(g_mmio_rsrc, ZX_RSRC_KIND_MMIO, kFbPhys, map_size,
                         rsrc_name, sizeof(rsrc_name), &bar_rsrc) != ZX_OK) {
    return;
  }
  zx_handle_t phys_vmo = ZX_HANDLE_INVALID;
  st = zx_vmo_create_physical(bar_rsrc, kFbPhys, map_size, &phys_vmo);
  zx_handle_close(bar_rsrc);
  if (st != ZX_OK) return;
  zx_vmo_set_cache_policy(phys_vmo, ZX_CACHE_POLICY_WRITE_COMBINING);
  uintptr_t mapped = 0;
  if (zx_vmar_map(zx_vmar_root_self(), ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0,
                  phys_vmo, 0, map_size, &mapped) != ZX_OK) {
    zx_handle_close(phys_vmo);
    return;
  }
  g_fb_info.active = true;
  g_fb_info.phys_addr = kFbPhys;
  g_fb_info.width = kWidth;
  g_fb_info.height = kHeight;
  g_fb_info.stride_bytes = stride;
  g_fb_info.size_bytes = stride * kHeight;
  g_fb_info.vram_bytes = map_size;
  g_fb_info.phys_vmo = phys_vmo;
  g_fb_info.mapped_fb = reinterpret_cast<uint32_t*>(mapped);

  // Paint initial dark-blue gradient background.
  for (uint32_t y = 0; y < kHeight; ++y) {
    uint32_t r = 18 + (y * 16) / kHeight;
    uint32_t g = 34 + (y * 36) / kHeight;
    uint32_t b = 64 + (y * 64) / kHeight;
    uint32_t px = (0xFFU << 24) | (r << 16) | (g << 8) | b;
    uint32_t* row = g_fb_info.mapped_fb + y * (stride / 4);
    for (uint32_t x = 0; x < kWidth; ++x) row[x] = px;
  }
  Vfs::CreateDevice("/dev/fb0", 0660, (29ULL << 8) | 0);
  ShimLogf("[zircon-init] Bochs display %ux%ux32 at 0x%llx (slot %d, VBE ID=0x%04x)\n",
           kWidth, kHeight, static_cast<unsigned long long>(kFbPhys), found_slot, vbe_id);
#endif
}

}  // namespace

void InitHardware() {
  InitBochsDisplay();
  NetInetInit();

  // i8042 PS/2 controller: present if its status port does not float high.
  if (g_ioport_rsrc != ZX_HANDLE_INVALID &&
      zx_ioports_request(g_ioport_rsrc, 0x60, 8) == ZX_OK && inb(0x64) != 0xFF) {
    g_input_available = true;
    Vfs::CreateDevice("/dev/input/event0", 0660, (13ULL << 8) | 64);
    std::thread(HardwareInputWorker).detach();
  }

  std::thread(SerialRxWorker).detach();
}

void SerialWriteBytes(const void* buf, size_t len) {
  const char* p = reinterpret_cast<const char*>(buf);
  size_t rem = len;
  while (rem > 0) {
    size_t chunk = rem > 256 ? 256 : rem;
    zx_debug_write(p, chunk);
    p += chunk;
    rem -= chunk;
  }
}

bool SerialHasInput() {
  return (TtyPollEvents(GetConsoleTty(), false) & kPollIn) != 0;
}

size_t SerialReadInput(void* buf, size_t max_len, bool block) {
  // Kernel-internal reader (no Linux process context): canonical/raw rules of
  // the console tty apply.
  auto console = GetConsoleTty();
  while (true) {
    {
      std::lock_guard<std::mutex> lock(console->mtx);
      if (!console->master_to_slave.empty()) {
        size_t n = std::min(max_len, console->master_to_slave.size());
        uint8_t* dst = static_cast<uint8_t*>(buf);
        for (size_t i = 0; i < n; ++i) {
          dst[i] = console->master_to_slave.front();
          console->master_to_slave.pop_front();
        }
        if (!console->canon_lines.empty()) {
          size_t& l = console->canon_lines.front();
          l = l > n ? l - n : 0;
          if (l == 0) console->canon_lines.pop_front();
        }
        return n;
      }
    }
    if (!block) return 0;
    WaitForPollEvent(10);
  }
}

LinuxTermios& GetConsoleTermios() { return GetConsoleTty()->termios; }

const FbHardwareInfo& GetFbHardwareInfo() { return g_fb_info; }

void FbWritePixels(size_t offset, const void* src, size_t len) {
  if (!g_fb_info.mapped_fb || offset >= g_fb_info.size_bytes) return;
  size_t avail = g_fb_info.size_bytes - offset;
  if (len > avail) len = avail;
  memcpy(reinterpret_cast<uint8_t*>(g_fb_info.mapped_fb) + offset, src, len);
}

void FbReadPixels(size_t offset, void* dst, size_t len) {
  if (!g_fb_info.mapped_fb || offset >= g_fb_info.size_bytes) return;
  size_t avail = g_fb_info.size_bytes - offset;
  if (len > avail) len = avail;
  memcpy(dst, reinterpret_cast<const uint8_t*>(g_fb_info.mapped_fb) + offset,
         len);
}

bool InputDeviceAvailable() { return g_input_available; }
uint64_t InputEventCounter() { return g_input_counter.load(); }

bool InputHasEvents() {
  std::lock_guard<std::mutex> lock(g_input_mtx);
  return !g_input_events.empty();
}

size_t InputReadEvents(void* dst, size_t max_bytes, bool block) {
  std::unique_lock<std::mutex> lock(g_input_mtx);
  while (g_input_events.empty()) {
    if (!block) return 0;
    g_input_cv.wait(lock);
  }
  uint8_t* out = reinterpret_cast<uint8_t*>(dst);
  size_t written = 0;
  while (written + sizeof(LinuxInputEvent) <= max_bytes &&
         !g_input_events.empty()) {
    LinuxInputEvent ev = g_input_events.front();
    g_input_events.pop_front();
    memcpy(out + written, &ev, sizeof(ev));
    written += sizeof(ev);
  }
  return written;
}
