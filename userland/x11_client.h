// Minimal raw X11 protocol client (no Xlib) shared by x11_fb_bridge and
// x11_probe. It speaks the core protocol over the local UNIX socket
// /tmp/.X11-unix/X<display> and the XTEST extension for synthetic input.
//
// Every request that can fail reports the failure to the caller; X protocol
// errors (including asynchronous ones for requests without replies) are
// logged to stderr with the request's major opcode and sequence number.

#ifndef ZIRCON_EMBEDDED_USERLAND_X11_CLIENT_H_
#define ZIRCON_EMBEDDED_USERLAND_X11_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace x11 {

constexpr uint32_t kNone = 0;
constexpr uint32_t kAtomWmName = 39;      // predefined atom WM_NAME
constexpr uint32_t kAtomWmClass = 67;     // predefined atom WM_CLASS
constexpr uint32_t kAtomString = 31;      // predefined atom STRING
constexpr uint32_t kAtomWindow = 33;      // predefined atom WINDOW
constexpr uint32_t kAtomCardinal = 6;     // predefined atom CARDINAL

// XTEST FakeInput event types.
constexpr uint8_t kKeyPress = 2;
constexpr uint8_t kKeyRelease = 3;
constexpr uint8_t kButtonPress = 4;
constexpr uint8_t kButtonRelease = 5;
constexpr uint8_t kMotionNotify = 6;

// Keysyms used for non-printable characters.
constexpr uint32_t kXK_Return = 0xff0d;
constexpr uint32_t kXK_Tab = 0xff09;
constexpr uint32_t kXK_BackSpace = 0xff08;
constexpr uint32_t kXK_Shift_L = 0xffe1;

struct Property {
  uint32_t type = 0;
  uint8_t format = 0;
  std::vector<uint8_t> data;  // raw bytes (format/8 bytes per item)
};

struct WindowAttributes {
  uint8_t map_state = 0;  // 0 = Unmapped, 1 = Unviewable, 2 = Viewable
  bool override_redirect = false;
  uint16_t window_class = 0;  // 1 = InputOutput, 2 = InputOnly
};

struct Geometry {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t width = 0;
  uint16_t height = 0;
};

struct TreeInfo {
  uint32_t root = 0;
  uint32_t parent = 0;
  std::vector<uint32_t> children;
};

class Connection {
 public:
  Connection() = default;
  ~Connection();
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Connects to local display :display_num (no authorization). A negative
  // value means "parse $DISPLAY (\":N\"), defaulting to :0". Returns false
  // and logs the reason on failure.
  bool Connect(int display_num = -1);
  bool connected() const { return fd_ >= 0; }
  void Close();

  uint32_t root() const { return root_; }
  uint16_t screen_width() const { return screen_width_; }
  uint16_t screen_height() const { return screen_height_; }
  uint8_t min_keycode() const { return min_keycode_; }
  uint8_t max_keycode() const { return max_keycode_; }

  // Core requests. All return false on transport or protocol error.
  bool InternAtom(const char* name, bool only_if_exists, uint32_t* atom);
  bool GetProperty(uint32_t window, uint32_t property, uint32_t type,
                   Property* out);
  bool QueryTree(uint32_t window, TreeInfo* out);
  bool GetWindowAttributes(uint32_t window, WindowAttributes* out);
  bool GetGeometry(uint32_t window, Geometry* out);
  bool TranslateToRoot(uint32_t window, int16_t* root_x, int16_t* root_y);
  bool GetInputFocus(uint32_t* focus);
  bool SetInputFocus(uint32_t window);
  bool GetSelectionOwner(uint32_t selection, uint32_t* owner);
  bool SendClientMessage(uint32_t destination, uint32_t window,
                         uint32_t message_type, const uint32_t data[5],
                         uint32_t event_mask);
  // Round-trip (GetInputFocus) guaranteeing all earlier requests have been
  // processed by the server. Returns false if any earlier request failed.
  bool Sync();

  // Keyboard mapping (core GetKeyboardMapping).
  bool LoadKeyboardMapping();
  // Returns the keysym at (keycode, column) or 0.
  uint32_t KeysymAt(uint8_t keycode, int column) const;
  // Finds a keycode producing |keysym|; *needs_shift set if column 1.
  bool KeycodeForKeysym(uint32_t keysym, uint8_t* keycode,
                        bool* needs_shift) const;

  // XTEST extension.
  bool InitXTest();
  bool FakeInput(uint8_t type, uint8_t detail, int16_t root_x = 0,
                 int16_t root_y = 0);
  // Types |text| through XTEST key events using the server's keyboard
  // mapping. Supports printable ASCII plus '\n' and '\t'. Returns false (and
  // logs) on the first unmappable character or transport error.
  bool TypeText(const char* text, size_t len, unsigned per_key_delay_us);

  // Convenience helpers built on the requests above.
  std::string GetStringProperty(uint32_t window, uint32_t property);
  // WM_CLASS as "instance/class" (empty if absent).
  std::string GetWmClass(uint32_t window);
  uint32_t GetCardinalProperty(uint32_t window, uint32_t property,
                               bool* ok);
  uint32_t GetWindowProperty(uint32_t window, uint32_t property, bool* ok);

  // Number of X protocol errors observed so far.
  int error_count() const { return error_count_; }

 private:
  bool WriteAll(const void* buf, size_t len);
  bool ReadExact(void* buf, size_t len);
  // Sends a request and records its sequence number.
  bool SendRequest(const std::vector<uint8_t>& req, uint16_t* seq_out);
  // Waits for the reply to |seq|, consuming events and errors. Errors for
  // |seq| make this return false; errors for earlier requests are logged
  // and counted (and also make this return false if fail_on_async).
  bool ReadReply(uint16_t seq, std::vector<uint8_t>* reply,
                 bool fail_on_async_error);

  int fd_ = -1;
  uint16_t seq_ = 0;
  uint32_t root_ = 0;
  uint16_t screen_width_ = 0;
  uint16_t screen_height_ = 0;
  uint8_t min_keycode_ = 0;
  uint8_t max_keycode_ = 0;
  uint8_t xtest_opcode_ = 0;
  int keysyms_per_keycode_ = 0;
  std::vector<uint32_t> keymap_;
  int error_count_ = 0;
  int unreported_async_errors_ = 0;
};

}  // namespace x11

#endif  // ZIRCON_EMBEDDED_USERLAND_X11_CLIENT_H_
