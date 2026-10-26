#include "userland/x11_client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

namespace x11 {
namespace {

void Put8(std::vector<uint8_t>* v, uint8_t x) { v->push_back(x); }
void Put16(std::vector<uint8_t>* v, uint16_t x) {
  v->push_back(static_cast<uint8_t>(x));
  v->push_back(static_cast<uint8_t>(x >> 8));
}
void Put32(std::vector<uint8_t>* v, uint32_t x) {
  for (int i = 0; i < 4; ++i) v->push_back(static_cast<uint8_t>(x >> (8 * i)));
}
void PadTo4(std::vector<uint8_t>* v) {
  while (v->size() % 4) v->push_back(0);
}
uint16_t Get16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t Get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
// Fills in the request length field (in 4-byte units) at offset 2.
void FinishRequest(std::vector<uint8_t>* v) {
  PadTo4(v);
  uint16_t words = static_cast<uint16_t>(v->size() / 4);
  (*v)[2] = static_cast<uint8_t>(words);
  (*v)[3] = static_cast<uint8_t>(words >> 8);
}
std::vector<uint8_t> NewRequest(uint8_t opcode, uint8_t data) {
  std::vector<uint8_t> v;
  Put8(&v, opcode);
  Put8(&v, data);
  Put16(&v, 0);  // length placeholder
  return v;
}

void SleepUs(unsigned us) {
  struct timespec ts = {static_cast<time_t>(us / 1000000),
                        static_cast<long>((us % 1000000) * 1000)};
  nanosleep(&ts, nullptr);
}

}  // namespace

Connection::~Connection() { Close(); }

void Connection::Close() {
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
}

bool Connection::WriteAll(const void* buf, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = write(fd_, p, len);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) {
      fprintf(stderr, "[x11] write to X server failed: %s\n",
              n < 0 ? strerror(errno) : "short write");
      Close();
      return false;
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
  return true;
}

bool Connection::ReadExact(void* buf, size_t len) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = read(fd_, p, len);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) {
      fprintf(stderr, "[x11] read from X server failed: %s\n",
              n < 0 ? strerror(errno) : "connection closed");
      Close();
      return false;
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
  return true;
}

bool Connection::Connect(int display_num) {
  if (fd_ >= 0) return true;
  if (display_num < 0) {
    const char* d = getenv("DISPLAY");
    display_num = (d && d[0] == ':') ? atoi(d + 1) : 0;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    fprintf(stderr, "[x11] socket(AF_UNIX) failed: %s\n", strerror(errno));
    return false;
  }
  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d",
           display_num);
  if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    fprintf(stderr, "[x11] connect(%s) failed: %s\n", addr.sun_path,
            strerror(errno));
    close(fd);
    return false;
  }
  fd_ = fd;
  seq_ = 0;

  // Connection setup: little-endian, protocol 11.0, no authorization.
  uint8_t setup_req[12] = {'l', 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  if (!WriteAll(setup_req, sizeof(setup_req))) return false;
  uint8_t hdr[8];
  if (!ReadExact(hdr, sizeof(hdr))) return false;
  size_t extra_len = static_cast<size_t>(Get16(hdr + 6)) * 4;
  std::vector<uint8_t> extra(extra_len);
  if (extra_len && !ReadExact(extra.data(), extra_len)) return false;
  if (hdr[0] != 1) {
    std::string reason(reinterpret_cast<const char*>(extra.data()),
                       hdr[0] == 0 ? std::min<size_t>(hdr[1], extra_len) : 0);
    fprintf(stderr, "[x11] connection setup refused (status=%u): %s\n",
            hdr[0], reason.c_str());
    Close();
    return false;
  }
  if (extra_len < 40) {
    fprintf(stderr, "[x11] setup reply too short (%zu bytes)\n", extra_len);
    Close();
    return false;
  }
  uint16_t vendor_len = Get16(&extra[16]);
  uint8_t num_screens = extra[20];
  uint8_t num_formats = extra[21];
  min_keycode_ = extra[26];
  max_keycode_ = extra[27];
  size_t screen_off = 32 + ((vendor_len + 3u) & ~3u) + 8u * num_formats;
  if (num_screens < 1 || screen_off + 40 > extra_len) {
    fprintf(stderr, "[x11] setup reply has no usable screen\n");
    Close();
    return false;
  }
  root_ = Get32(&extra[screen_off]);
  screen_width_ = Get16(&extra[screen_off + 20]);
  screen_height_ = Get16(&extra[screen_off + 22]);
  return true;
}

bool Connection::SendRequest(const std::vector<uint8_t>& req,
                             uint16_t* seq_out) {
  if (fd_ < 0) {
    fprintf(stderr, "[x11] request on closed connection\n");
    return false;
  }
  if (!WriteAll(req.data(), req.size())) return false;
  ++seq_;
  if (seq_out) *seq_out = seq_;
  return true;
}

bool Connection::ReadReply(uint16_t seq, std::vector<uint8_t>* reply,
                           bool fail_on_async_error) {
  bool async_error = false;
  while (true) {
    uint8_t pkt[32];
    if (!ReadExact(pkt, sizeof(pkt))) return false;
    uint16_t pkt_seq = Get16(pkt + 2);
    if (pkt[0] == 0) {  // Error
      ++error_count_;
      fprintf(stderr,
              "[x11] X protocol error code=%u seq=%u major=%u minor=%u "
              "resource=0x%x%s\n",
              pkt[1], pkt_seq, pkt[10], Get16(pkt + 8), Get32(pkt + 4),
              pkt_seq == seq ? "" : " (earlier request)");
      if (pkt_seq == seq) return false;
      async_error = true;
      continue;
    }
    if (pkt[0] == 1) {  // Reply
      size_t extra = static_cast<size_t>(Get32(pkt + 4)) * 4;
      std::vector<uint8_t> buf(32 + extra);
      memcpy(buf.data(), pkt, 32);
      if (extra && !ReadExact(buf.data() + 32, extra)) return false;
      if (pkt_seq != seq) {
        fprintf(stderr, "[x11] unexpected reply seq=%u (waiting for %u)\n",
                pkt_seq, seq);
        continue;
      }
      if (reply) *reply = std::move(buf);
      return !(fail_on_async_error && async_error);
    }
    // Event (including GenericEvent with extra data): skip.
    if ((pkt[0] & 0x7f) == 35) {
      size_t extra = static_cast<size_t>(Get32(pkt + 4)) * 4;
      std::vector<uint8_t> skip(extra);
      if (extra && !ReadExact(skip.data(), extra)) return false;
    }
  }
}

bool Connection::Sync() {
  auto req = NewRequest(43, 0);  // GetInputFocus
  FinishRequest(&req);
  uint16_t seq;
  if (!SendRequest(req, &seq)) return false;
  return ReadReply(seq, nullptr, /*fail_on_async_error=*/true);
}

bool Connection::InternAtom(const char* name, bool only_if_exists,
                            uint32_t* atom) {
  size_t n = strlen(name);
  auto req = NewRequest(16, only_if_exists ? 1 : 0);
  Put16(&req, static_cast<uint16_t>(n));
  Put16(&req, 0);
  req.insert(req.end(), name, name + n);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  *atom = Get32(&rep[8]);
  return true;
}

bool Connection::GetProperty(uint32_t window, uint32_t property,
                             uint32_t type, Property* out) {
  auto req = NewRequest(20, 0);
  Put32(&req, window);
  Put32(&req, property);
  Put32(&req, type);
  Put32(&req, 0);       // long-offset
  Put32(&req, 16384);   // long-length (64 KiB)
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  out->format = rep[1];
  out->type = Get32(&rep[8]);
  uint32_t items = Get32(&rep[16]);
  size_t bytes = static_cast<size_t>(items) * (out->format / 8);
  if (32 + bytes > rep.size()) bytes = rep.size() - 32;
  out->data.assign(rep.begin() + 32, rep.begin() + 32 + bytes);
  return true;
}

bool Connection::QueryTree(uint32_t window, TreeInfo* out) {
  auto req = NewRequest(15, 0);
  Put32(&req, window);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  out->root = Get32(&rep[8]);
  out->parent = Get32(&rep[12]);
  uint16_t n = Get16(&rep[16]);
  out->children.clear();
  for (uint16_t i = 0; i < n && 32 + 4u * (i + 1) <= rep.size(); ++i) {
    out->children.push_back(Get32(&rep[32 + 4 * i]));
  }
  return true;
}

bool Connection::GetWindowAttributes(uint32_t window, WindowAttributes* out) {
  auto req = NewRequest(3, 0);
  Put32(&req, window);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  if (rep.size() < 44) return false;
  out->window_class = Get16(&rep[12]);
  out->map_state = rep[26];
  out->override_redirect = rep[27] != 0;
  return true;
}

bool Connection::GetGeometry(uint32_t window, Geometry* out) {
  auto req = NewRequest(14, 0);
  Put32(&req, window);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  out->x = static_cast<int16_t>(Get16(&rep[12]));
  out->y = static_cast<int16_t>(Get16(&rep[14]));
  out->width = Get16(&rep[16]);
  out->height = Get16(&rep[18]);
  return true;
}

bool Connection::TranslateToRoot(uint32_t window, int16_t* root_x,
                                 int16_t* root_y) {
  auto req = NewRequest(40, 0);
  Put32(&req, window);
  Put32(&req, root_);
  Put16(&req, 0);
  Put16(&req, 0);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  *root_x = static_cast<int16_t>(Get16(&rep[12]));
  *root_y = static_cast<int16_t>(Get16(&rep[14]));
  return true;
}

bool Connection::GetInputFocus(uint32_t* focus) {
  auto req = NewRequest(43, 0);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  *focus = Get32(&rep[8]);
  return true;
}

bool Connection::SetInputFocus(uint32_t window) {
  auto req = NewRequest(42, 2 /* RevertToParent */);
  Put32(&req, window);
  Put32(&req, 0);  // CurrentTime
  FinishRequest(&req);
  if (!SendRequest(req, nullptr)) return false;
  return Sync();
}

bool Connection::GetSelectionOwner(uint32_t selection, uint32_t* owner) {
  auto req = NewRequest(23, 0);
  Put32(&req, selection);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  *owner = Get32(&rep[8]);
  return true;
}

bool Connection::SendClientMessage(uint32_t destination, uint32_t window,
                                   uint32_t message_type,
                                   const uint32_t data[5],
                                   uint32_t event_mask) {
  auto req = NewRequest(25, 0 /* propagate=False */);
  Put32(&req, destination);
  Put32(&req, event_mask);
  // 32-byte ClientMessage event.
  Put8(&req, 33);
  Put8(&req, 32);  // format
  Put16(&req, 0);
  Put32(&req, window);
  Put32(&req, message_type);
  for (int i = 0; i < 5; ++i) Put32(&req, data[i]);
  FinishRequest(&req);
  if (!SendRequest(req, nullptr)) return false;
  return Sync();
}

bool Connection::LoadKeyboardMapping() {
  int count = max_keycode_ - min_keycode_ + 1;
  if (count <= 0) {
    fprintf(stderr, "[x11] invalid keycode range %u..%u\n", min_keycode_,
            max_keycode_);
    return false;
  }
  auto req = NewRequest(101, 0);
  Put8(&req, min_keycode_);
  Put8(&req, static_cast<uint8_t>(count));
  Put16(&req, 0);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  keysyms_per_keycode_ = rep[1];
  size_t total = static_cast<size_t>(count) * keysyms_per_keycode_;
  if (keysyms_per_keycode_ == 0 || 32 + total * 4 > rep.size()) {
    fprintf(stderr, "[x11] malformed GetKeyboardMapping reply\n");
    return false;
  }
  keymap_.resize(total);
  for (size_t i = 0; i < total; ++i) keymap_[i] = Get32(&rep[32 + 4 * i]);
  return true;
}

uint32_t Connection::KeysymAt(uint8_t keycode, int column) const {
  if (keymap_.empty() || keycode < min_keycode_ || keycode > max_keycode_ ||
      column < 0 || column >= keysyms_per_keycode_) {
    return 0;
  }
  return keymap_[static_cast<size_t>(keycode - min_keycode_) *
                     keysyms_per_keycode_ +
                 column];
}

bool Connection::KeycodeForKeysym(uint32_t keysym, uint8_t* keycode,
                                  bool* needs_shift) const {
  // Prefer an unshifted binding, then a shifted one.
  for (int column = 0; column < 2; ++column) {
    for (int kc = min_keycode_; kc <= max_keycode_; ++kc) {
      if (KeysymAt(static_cast<uint8_t>(kc), column) == keysym) {
        *keycode = static_cast<uint8_t>(kc);
        *needs_shift = (column == 1);
        return true;
      }
    }
  }
  return false;
}

bool Connection::InitXTest() {
  static const char kName[] = "XTEST";
  auto req = NewRequest(98, 0);  // QueryExtension
  Put16(&req, sizeof(kName) - 1);
  Put16(&req, 0);
  req.insert(req.end(), kName, kName + sizeof(kName) - 1);
  FinishRequest(&req);
  uint16_t seq;
  std::vector<uint8_t> rep;
  if (!SendRequest(req, &seq) || !ReadReply(seq, &rep, false)) return false;
  if (!rep[8]) {
    fprintf(stderr, "[x11] XTEST extension not present on X server\n");
    return false;
  }
  xtest_opcode_ = rep[9];
  // XTestGetVersion (minor 0) to confirm the extension answers.
  std::vector<uint8_t> vreq;
  Put8(&vreq, xtest_opcode_);
  Put8(&vreq, 0);
  Put16(&vreq, 0);
  Put8(&vreq, 2);  // client major version
  Put8(&vreq, 0);
  Put16(&vreq, 2);  // client minor version
  FinishRequest(&vreq);
  if (!SendRequest(vreq, &seq) || !ReadReply(seq, &rep, false)) {
    fprintf(stderr, "[x11] XTestGetVersion failed\n");
    return false;
  }
  return true;
}

bool Connection::FakeInput(uint8_t type, uint8_t detail, int16_t root_x,
                           int16_t root_y) {
  if (!xtest_opcode_) {
    fprintf(stderr, "[x11] FakeInput before InitXTest\n");
    return false;
  }
  std::vector<uint8_t> req;
  Put8(&req, xtest_opcode_);
  Put8(&req, 2);  // X_XTestFakeInput
  Put16(&req, 0);
  Put8(&req, type);
  Put8(&req, detail);
  Put16(&req, 0);
  Put32(&req, 0);  // time = CurrentTime
  Put32(&req, type == kMotionNotify ? root_ : 0);
  for (int i = 0; i < 8; ++i) Put8(&req, 0);
  Put16(&req, static_cast<uint16_t>(root_x));
  Put16(&req, static_cast<uint16_t>(root_y));
  for (int i = 0; i < 8; ++i) Put8(&req, 0);  // pad + deviceid
  FinishRequest(&req);
  return SendRequest(req, nullptr);
}

bool Connection::TypeText(const char* text, size_t len,
                          unsigned per_key_delay_us) {
  if (keymap_.empty() && !LoadKeyboardMapping()) return false;
  uint8_t shift_kc = 0;
  bool dummy = false;
  if (!KeycodeForKeysym(kXK_Shift_L, &shift_kc, &dummy)) {
    fprintf(stderr, "[x11] no keycode for Shift_L in server keymap\n");
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    uint32_t keysym;
    if (c == '\n' || c == '\r') {
      keysym = kXK_Return;
    } else if (c == '\t') {
      keysym = kXK_Tab;
    } else if (c >= 0x20 && c <= 0x7e) {
      keysym = c;  // Latin-1 keysyms equal their code points.
    } else {
      fprintf(stderr, "[x11] cannot type non-ASCII byte 0x%02x\n", c);
      return false;
    }
    uint8_t kc = 0;
    bool shift = false;
    if (!KeycodeForKeysym(keysym, &kc, &shift)) {
      fprintf(stderr, "[x11] keysym 0x%x ('%c') not in server keymap\n",
              keysym, c >= 0x20 ? c : '?');
      return false;
    }
    bool ok = true;
    if (shift) ok = ok && FakeInput(kKeyPress, shift_kc);
    ok = ok && FakeInput(kKeyPress, kc) && FakeInput(kKeyRelease, kc);
    if (shift) ok = ok && FakeInput(kKeyRelease, shift_kc);
    if (!ok) return false;
    if (per_key_delay_us) SleepUs(per_key_delay_us);
  }
  return Sync();
}

std::string Connection::GetStringProperty(uint32_t window,
                                          uint32_t property) {
  Property p;
  if (!GetProperty(window, property, kNone, &p) || p.format != 8) return "";
  return std::string(p.data.begin(), p.data.end());
}

std::string Connection::GetWmClass(uint32_t window) {
  Property p;
  if (!GetProperty(window, kAtomWmClass, kAtomString, &p) || p.format != 8 ||
      p.data.empty()) {
    return "";
  }
  std::string raw(p.data.begin(), p.data.end());
  size_t nul = raw.find('\0');
  if (nul == std::string::npos) return raw;
  std::string instance = raw.substr(0, nul);
  std::string cls = raw.substr(nul + 1);
  while (!cls.empty() && cls.back() == '\0') cls.pop_back();
  return instance + "/" + cls;
}

uint32_t Connection::GetCardinalProperty(uint32_t window, uint32_t property,
                                         bool* ok) {
  Property p;
  *ok = GetProperty(window, property, kAtomCardinal, &p) && p.format == 32 &&
        p.data.size() >= 4;
  return *ok ? Get32(p.data.data()) : 0;
}

uint32_t Connection::GetWindowProperty(uint32_t window, uint32_t property,
                                       bool* ok) {
  Property p;
  *ok = GetProperty(window, property, kAtomWindow, &p) && p.format == 32 &&
        p.data.size() >= 4;
  return *ok ? Get32(p.data.data()) : 0;
}

}  // namespace x11
