// x11_fb_bridge: mirrors the Xvfb screen (-fbdir XWD file) onto the Bochs VBE
// hardware framebuffer (/dev/fb0) and forwards input into the X server.
//
// Input paths (both go exclusively through the X11 XTEST extension, so every
// keystroke is delivered by the X server to whichever X client has focus):
//   1. Hardware PS/2 keyboard/mouse events read from /dev/input/event0.
//      Linux evdev key codes are forwarded as X keycode = code + 8; this is
//      the XKB "evdev" convention, which is verified against the server's
//      actual keyboard mapping (GetKeyboardMapping) at startup.
//   2. A scripted test hook: the file /tmp/x11_inject.cmd. Writers should
//      create it atomically (write a temp file, then rename()). Each line is:
//        type <text>      type <text> verbatim (printable ASCII)
//        typeline <text>  type <text> followed by Return
//        click <x> <y>    move the pointer to (x, y) and click button 1
//      Characters are mapped to keycodes (+Shift) using the server's keymap.
//      After processing, the bridge writes /tmp/x11_inject.ack (atomically)
//      containing "ok <keys>" or "error <reason>".
//
// Framebuffer geometry comes from FBIOGET_VSCREENINFO/FBIOGET_FSCREENINFO and
// the Xvfb image layout from the XWD file header. Any inconsistency is fatal.
//
// Usage: x11_fb_bridge [--print-geometry] [xwd_path]
// Diagnostics go to stderr; --print-geometry prints "<W>x<H>" on stdout.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input-event-codes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "userland/x11_client.h"

namespace {

constexpr const char* kInjectCmdPath = "/tmp/x11_inject.cmd";
constexpr const char* kInjectAckPath = "/tmp/x11_inject.ack";

struct LinuxInputEvent {
  int64_t tv_sec;
  int64_t tv_usec;
  uint16_t type;
  uint16_t code;
  int32_t value;
};

struct FbInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bpp = 0;
  uint32_t line_length = 0;
  uint32_t smem_len = 0;
  uint32_t red_off = 0, green_off = 0, blue_off = 0;
};

struct XwdLayout {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bytes_per_line = 0;
  uint32_t bits_per_pixel = 0;
  size_t pixel_offset = 0;
};

[[noreturn]] void Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void Fatal(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[x11_fb_bridge] FATAL: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[x11_fb_bridge] ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

void SleepMs(unsigned ms) {
  struct timespec ts = {static_cast<time_t>(ms / 1000),
                        static_cast<long>((ms % 1000) * 1000000L)};
  nanosleep(&ts, nullptr);
}

FbInfo QueryFramebuffer(int fb_fd) {
  struct fb_var_screeninfo var{};
  struct fb_fix_screeninfo fix{};
  if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) != 0) {
    Fatal("FBIOGET_VSCREENINFO failed: %s", strerror(errno));
  }
  if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) != 0) {
    Fatal("FBIOGET_FSCREENINFO failed: %s", strerror(errno));
  }
  FbInfo fb;
  fb.width = var.xres;
  fb.height = var.yres;
  fb.bpp = var.bits_per_pixel;
  fb.line_length = fix.line_length;
  fb.smem_len = fix.smem_len;
  fb.red_off = var.red.offset;
  fb.green_off = var.green.offset;
  fb.blue_off = var.blue.offset;
  if (fb.width == 0 || fb.height == 0) {
    Fatal("framebuffer reports zero geometry (%ux%u)", fb.width, fb.height);
  }
  if (fb.bpp != 32) Fatal("unsupported framebuffer depth %u bpp", fb.bpp);
  if (fb.line_length < fb.width * 4) {
    Fatal("fb_fix_screeninfo.line_length=%u < xres*4=%u (bad ioctl layout?)",
          fb.line_length, fb.width * 4);
  }
  if (fb.smem_len < static_cast<uint64_t>(fb.line_length) * fb.height) {
    Fatal("fb_fix_screeninfo.smem_len=%u < line_length*yres=%llu", fb.smem_len,
          static_cast<unsigned long long>(fb.line_length) * fb.height);
  }
  return fb;
}

// Parses the big-endian XWDFileHeader (X11 XWD file version 7).
bool ParseXwd(const uint8_t* map, size_t size, const FbInfo& fb,
              XwdLayout* out, std::string* err) {
  if (size < 100) {
    *err = "file shorter than XWD header";
    return false;
  }
  uint32_t h[25];
  for (int i = 0; i < 25; ++i) {
    uint32_t v;
    memcpy(&v, map + 4 * i, 4);
    h[i] = ntohl(v);
  }
  const uint32_t header_size = h[0], file_version = h[1], pixmap_format = h[2],
                 width = h[4], height = h[5], byte_order = h[7],
                 bits_per_pixel = h[11], bytes_per_line = h[12],
                 red_mask = h[14], green_mask = h[15], blue_mask = h[16],
                 ncolors = h[19];
  char buf[256];
  if (file_version != 7 || header_size < 100 || pixmap_format != 2) {
    snprintf(buf, sizeof(buf),
             "unexpected XWD header (version=%u header_size=%u format=%u)",
             file_version, header_size, pixmap_format);
    *err = buf;
    return false;
  }
  if (bits_per_pixel != 32 || byte_order != 0 /* LSBFirst */) {
    snprintf(buf, sizeof(buf), "unsupported XWD pixel layout bpp=%u order=%u",
             bits_per_pixel, byte_order);
    *err = buf;
    return false;
  }
  if (red_mask != (0xffu << fb.red_off) ||
      green_mask != (0xffu << fb.green_off) ||
      blue_mask != (0xffu << fb.blue_off)) {
    snprintf(buf, sizeof(buf),
             "XWD masks r=%08x g=%08x b=%08x do not match fb offsets %u/%u/%u",
             red_mask, green_mask, blue_mask, fb.red_off, fb.green_off,
             fb.blue_off);
    *err = buf;
    return false;
  }
  if (bytes_per_line < width * 4) {
    snprintf(buf, sizeof(buf), "XWD bytes_per_line=%u < width*4=%u",
             bytes_per_line, width * 4);
    *err = buf;
    return false;
  }
  size_t pixel_offset =
      static_cast<size_t>(header_size) + static_cast<size_t>(ncolors) * 12;
  if (pixel_offset + static_cast<size_t>(bytes_per_line) * height > size) {
    snprintf(buf, sizeof(buf),
             "XWD file too small: offset %zu + %u*%u > size %zu", pixel_offset,
             bytes_per_line, height, size);
    *err = buf;
    return false;
  }
  out->width = width;
  out->height = height;
  out->bytes_per_line = bytes_per_line;
  out->bits_per_pixel = bits_per_pixel;
  out->pixel_offset = pixel_offset;
  return true;
}

void DrawCursor(uint8_t* frame, const FbInfo& fb, int cx, int cy) {
  static const char* kArrow[12] = {
      "X           ", "XX          ", "X.X         ", "X..X        ",
      "X...X       ", "X....X      ", "X.....X     ", "X......X    ",
      "X...XXXXX   ", "X..X        ", "X.X         ", "XX          ",
  };
  for (int dy = 0; dy < 12; ++dy) {
    int y = cy + dy;
    if (y < 0 || y >= static_cast<int>(fb.height)) continue;
    uint32_t* row = reinterpret_cast<uint32_t*>(frame + y * fb.line_length);
    for (int dx = 0; dx < 12; ++dx) {
      int x = cx + dx;
      if (x < 0 || x >= static_cast<int>(fb.width)) continue;
      char c = kArrow[dy][dx];
      if (c == 'X') row[x] = 0xFF000000U;
      if (c == '.') row[x] = 0xFFFFFFFFU;
    }
  }
}

// Verifies that the X server's keymap follows the evdev convention
// (X keycode = Linux key code + 8) for a representative set of keys.
bool VerifyEvdevKeymap(const x11::Connection& x) {
  struct Probe {
    uint16_t linux_code;
    uint32_t keysym;
    const char* name;
  } probes[] = {
      {KEY_A, 'a', "a"},         {KEY_Z, 'z', "z"},
      {KEY_1, '1', "1"},         {KEY_0, '0', "0"},
      {KEY_SPACE, ' ', "space"}, {KEY_SLASH, '/', "slash"},
      {KEY_MINUS, '-', "minus"}, {KEY_ENTER, x11::kXK_Return, "Return"},
      {KEY_LEFTSHIFT, x11::kXK_Shift_L, "Shift_L"},
  };
  bool ok = true;
  for (const auto& p : probes) {
    uint32_t got = x.KeysymAt(static_cast<uint8_t>(p.linux_code + 8), 0);
    if (got != p.keysym) {
      Log("ERROR: keycode %u (evdev %u) maps to keysym 0x%x, expected %s "
          "(0x%x)",
          p.linux_code + 8, p.linux_code, got, p.name, p.keysym);
      ok = false;
    }
  }
  return ok;
}

void WriteAck(const std::string& msg) {
  std::string tmp = std::string(kInjectAckPath) + ".tmp";
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) Fatal("cannot create %s: %s", tmp.c_str(), strerror(errno));
  std::string line = msg + "\n";
  if (write(fd, line.data(), line.size()) != static_cast<ssize_t>(line.size())) {
    Fatal("short write to %s", tmp.c_str());
  }
  close(fd);
  if (rename(tmp.c_str(), kInjectAckPath) != 0) {
    Fatal("rename(%s) failed: %s", kInjectAckPath, strerror(errno));
  }
}

// Executes the scripted injection file. Returns the ack message.
std::string RunInjectScript(x11::Connection* x, const std::string& script,
                            int* mouse_x, int* mouse_y) {
  size_t keys = 0;
  size_t pos = 0;
  while (pos < script.size()) {
    size_t nl = script.find('\n', pos);
    if (nl == std::string::npos) nl = script.size();
    std::string line = script.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;
    if (line.rfind("type ", 0) == 0 || line.rfind("typeline ", 0) == 0) {
      bool newline = line[4] == 'l';
      std::string text = line.substr(newline ? 9 : 5);
      if (newline) text += '\n';
      Log("XTEST typing %zu characters", text.size());
      if (!x->TypeText(text.data(), text.size(), 3000)) {
        return "error XTEST typing failed";
      }
      keys += text.size();
    } else if (line.rfind("click ", 0) == 0) {
      int cx = 0, cy = 0;
      if (sscanf(line.c_str() + 6, "%d %d", &cx, &cy) != 2) {
        return "error malformed click command";
      }
      *mouse_x = cx;
      *mouse_y = cy;
      if (!x->FakeInput(x11::kMotionNotify, 0, static_cast<int16_t>(cx),
                        static_cast<int16_t>(cy)) ||
          !x->FakeInput(x11::kButtonPress, 1) ||
          !x->FakeInput(x11::kButtonRelease, 1) || !x->Sync()) {
        return "error XTEST click failed";
      }
      Log("XTEST click at %d,%d", cx, cy);
    } else {
      return "error unknown command: " + line;
    }
  }
  return "ok " + std::to_string(keys);
}

}  // namespace

int main(int argc, char** argv) {
  bool print_geometry = false;
  const char* xwd_path = "/tmp/Xvfb_screen0";
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--print-geometry") == 0) {
      print_geometry = true;
    } else {
      xwd_path = argv[i];
    }
  }

  int fb_fd = open("/dev/fb0", O_RDWR);
  if (fb_fd < 0) Fatal("cannot open /dev/fb0: %s", strerror(errno));
  const FbInfo fb = QueryFramebuffer(fb_fd);
  if (print_geometry) {
    printf("%ux%u\n", fb.width, fb.height);
    return 0;
  }
  Log("/dev/fb0: %ux%u %ubpp line_length=%u smem_len=%u", fb.width, fb.height,
      fb.bpp, fb.line_length, fb.smem_len);

  const size_t fb_bytes = static_cast<size_t>(fb.line_length) * fb.height;
  void* fb_map =
      mmap(nullptr, fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
  uint8_t* hw_fb = fb_map == MAP_FAILED ? nullptr : static_cast<uint8_t*>(fb_map);
  if (!hw_fb) {
    Log("mmap(/dev/fb0) failed (%s); falling back to pwrite()",
        strerror(errno));
  }

  int input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
  if (input_fd < 0) {
    Log("cannot open /dev/input/event0: %s (scripted injection only)",
        strerror(errno));
  }

  // Connect to the X server (it may still be starting).
  x11::Connection x;
  for (int attempt = 0; !x.Connect(); ++attempt) {
    if (attempt >= 100) Fatal("could not connect to X server :0");
    SleepMs(100);
  }
  if (!x.InitXTest()) Fatal("XTEST unavailable");
  if (!x.LoadKeyboardMapping()) Fatal("GetKeyboardMapping failed");
  if (!VerifyEvdevKeymap(x)) {
    Fatal("X keymap is not evdev-compatible; cannot forward PS/2 keys");
  }
  Log("connected to X :0 (root=0x%x %ux%u, keycodes %u..%u); verified evdev "
      "keymap; XTEST is the only input path",
      x.root(), x.screen_width(), x.screen_height(), x.min_keycode(),
      x.max_keycode());

  const int pointer_w = x.screen_width();
  const int pointer_h = x.screen_height();
  int mouse_x = pointer_w / 2;
  int mouse_y = pointer_h / 2;
  int prev_mouse_y = -100;

  int xwd_fd = -1;
  const uint8_t* xwd_map = nullptr;
  XwdLayout xwd;
  uint8_t* frame = static_cast<uint8_t*>(malloc(fb_bytes));
  uint8_t* prev_frame = static_cast<uint8_t*>(malloc(fb_bytes));
  if (!frame || !prev_frame) Fatal("out of memory");
  memset(frame, 0, fb_bytes);
  memset(prev_frame, 0xFF, fb_bytes);  // force full blit on first frame
  bool logged_first_frame = false;
  int xwd_wait_ticks = 0;

  while (true) {
    // 0. Map the Xvfb screen file once it has its final size.
    if (!xwd_map) {
      if (xwd_fd < 0) xwd_fd = open(xwd_path, O_RDONLY);
      struct stat st{};
      static std::string last_err = "file not present";
      if (xwd_fd >= 0 && fstat(xwd_fd, &st) == 0 && st.st_size >= 100) {
        size_t size = static_cast<size_t>(st.st_size);
        void* m = mmap(nullptr, size, PROT_READ, MAP_SHARED, xwd_fd, 0);
        if (m == MAP_FAILED) Fatal("mmap(%s) failed: %s", xwd_path, strerror(errno));
        // Xvfb may still be sizing/initializing the file: retry until the
        // header is complete and consistent, then fail loudly.
        if (ParseXwd(static_cast<const uint8_t*>(m), size, fb, &xwd,
                     &last_err)) {
          xwd_map = static_cast<const uint8_t*>(m);
        } else {
          munmap(m, size);
        }
      }
      if (!xwd_map && ++xwd_wait_ticks == 1200) {
        Fatal("%s not usable within ~10s: %s", xwd_path, last_err.c_str());
      }
      if (xwd_map) {
        Log("mapped %s: %ux%u bpp=%u bytes_per_line=%u pixel_offset=%zu",
            xwd_path, xwd.width, xwd.height, xwd.bits_per_pixel,
            xwd.bytes_per_line, xwd.pixel_offset);
        if (xwd.width != fb.width || xwd.height != fb.height) {
          Log("WARNING: X screen %ux%u differs from fb %ux%u; clipping",
              xwd.width, xwd.height, fb.width, fb.height);
        }
      }
    }

    // 1. Hardware PS/2 & vmmouse keyboard/mouse events -> XTEST.
    LinuxInputEvent evs[64];
    bool moved = false;
    bool sent_key_or_btn = false;
    bool ok = true;
    ssize_t n = 0;
    while (ok && input_fd >= 0 && (n = read(input_fd, evs, sizeof(evs))) > 0) {
      size_t count = static_cast<size_t>(n) / sizeof(LinuxInputEvent);
      for (size_t i = 0; i < count && ok; ++i) {
        const auto& ev = evs[i];
        if (ev.type == EV_REL) {
          if (ev.code == REL_X) {
            mouse_x = std::max(0, std::min(pointer_w - 1, mouse_x + ev.value));
            moved = true;
          } else if (ev.code == REL_Y) {
            mouse_y = std::max(0, std::min(pointer_h - 1, mouse_y + ev.value));
            moved = true;
          }
        } else if (ev.type == EV_ABS) {
          if (ev.code == ABS_X) {
            int64_t v = std::max<int64_t>(0, std::min<int64_t>(65535, ev.value));
            mouse_x = static_cast<int>((v * (pointer_w - 1) + 32767) / 65535);
            moved = true;
          } else if (ev.code == ABS_Y) {
            int64_t v = std::max<int64_t>(0, std::min<int64_t>(65535, ev.value));
            mouse_y = static_cast<int>((v * (pointer_h - 1) + 32767) / 65535);
            moved = true;
          }
        } else if (ev.type == EV_KEY) {
          if (ev.code >= BTN_LEFT && ev.code <= BTN_MIDDLE) {
            if (moved) {
              ok = x.FakeInput(x11::kMotionNotify, 0,
                               static_cast<int16_t>(mouse_x),
                               static_cast<int16_t>(mouse_y));
              moved = false;
            }
            uint8_t btn = ev.code == BTN_LEFT ? 1 : ev.code == BTN_RIGHT ? 3 : 2;
            ok = ok && x.FakeInput(
                           ev.value ? x11::kButtonPress : x11::kButtonRelease,
                           btn);
            sent_key_or_btn = true;
          } else if (ev.code > 0 && ev.code + 8 <= x.max_keycode() &&
                     ev.value != 2 /* autorepeat handled by X */) {
            ok = x.FakeInput(ev.value ? x11::kKeyPress : x11::kKeyRelease,
                             static_cast<uint8_t>(ev.code + 8));
            sent_key_or_btn = true;
          }
        }
      }
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
      Fatal("read(/dev/input/event0) failed: %s", strerror(errno));
    }
    if (ok && moved) {
      ok = x.FakeInput(x11::kMotionNotify, 0, static_cast<int16_t>(mouse_x),
                       static_cast<int16_t>(mouse_y));
    }
    if (!ok || (sent_key_or_btn && !x.Sync())) {
      Fatal("forwarding input via XTEST failed");
    }

    // 2. Scripted XTEST injection hook.
    int cmd_fd = open(kInjectCmdPath, O_RDONLY);
    if (cmd_fd >= 0) {
      std::string script;
      char buf[1024];
      ssize_t cn;
      while ((cn = read(cmd_fd, buf, sizeof(buf))) > 0) {
        script.append(buf, static_cast<size_t>(cn));
      }
      close(cmd_fd);
      if (unlink(kInjectCmdPath) != 0) {
        Fatal("unlink(%s) failed: %s", kInjectCmdPath, strerror(errno));
      }
      std::string ack = RunInjectScript(&x, script, &mouse_x, &mouse_y);
      Log("inject result: %s", ack.c_str());
      WriteAck(ack);
      if (!x.connected()) Fatal("lost X server connection");
    }

    // 3. Blit dirty scanlines of Xvfb pixels + software cursor to /dev/fb0.
    if (xwd_map) {
      uint32_t rows = std::min(xwd.height, fb.height);
      size_t row_bytes = static_cast<size_t>(std::min(xwd.width, fb.width)) * 4;
      if (hw_fb) {
        for (uint32_t y = 0; y < rows; ++y) {
          size_t off = static_cast<size_t>(y) * fb.line_length;
          const uint8_t* src_row = xwd_map + xwd.pixel_offset +
                                   static_cast<size_t>(y) * xwd.bytes_per_line;
          bool in_cursor =
              (static_cast<int>(y) >= mouse_y &&
               static_cast<int>(y) < mouse_y + 12) ||
              (static_cast<int>(y) >= prev_mouse_y &&
               static_cast<int>(y) < prev_mouse_y + 12);
          if (!in_cursor && memcmp(src_row, prev_frame + off, row_bytes) == 0) {
            continue;
          }
          memcpy(frame + off, src_row, row_bytes);
        }
        DrawCursor(frame, fb, mouse_x, mouse_y);
        for (uint32_t y = 0; y < rows; ++y) {
          size_t off = static_cast<size_t>(y) * fb.line_length;
          const uint8_t* src_row = xwd_map + xwd.pixel_offset +
                                   static_cast<size_t>(y) * xwd.bytes_per_line;
          bool in_cursor =
              (static_cast<int>(y) >= mouse_y &&
               static_cast<int>(y) < mouse_y + 12) ||
              (static_cast<int>(y) >= prev_mouse_y &&
               static_cast<int>(y) < prev_mouse_y + 12);
          const uint8_t* cand = in_cursor ? (frame + off) : src_row;
          if (memcmp(cand, prev_frame + off, row_bytes) != 0) {
            memcpy(hw_fb + off, cand, row_bytes);
            memcpy(prev_frame + off, cand, row_bytes);
          }
        }
        prev_mouse_y = mouse_y;
      } else {
        for (uint32_t y = 0; y < rows; ++y) {
          memcpy(frame + static_cast<size_t>(y) * fb.line_length,
                 xwd_map + xwd.pixel_offset +
                     static_cast<size_t>(y) * xwd.bytes_per_line,
                 row_bytes);
        }
        DrawCursor(frame, fb, mouse_x, mouse_y);
        if (pwrite(fb_fd, frame, fb_bytes, 0) !=
            static_cast<ssize_t>(fb_bytes)) {
          Fatal("pwrite(/dev/fb0) failed: %s", strerror(errno));
        }
      }
      if (!logged_first_frame) {
        uint64_t lit = 0;
        for (uint32_t y = 0; y < rows; ++y) {
          const uint32_t* row = reinterpret_cast<const uint32_t*>(
              prev_frame + static_cast<size_t>(y) * fb.line_length);
          for (uint32_t xx = 0; xx < fb.width; ++xx) {
            if (row[xx] & 0x00FFFFFFU) lit++;
          }
        }
        if (lit > 1000) {
          logged_first_frame = true;
          Log("first non-black X11 frame blitted to /dev/fb0 (%llu lit "
              "pixels)",
              static_cast<unsigned long long>(lit));
        }
      }
    }

    SleepMs(8);  // ~120 Hz
  }
}
