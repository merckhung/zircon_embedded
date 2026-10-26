// x11_probe: inspects a running X server over the raw X11 protocol and
// reports what is really there, so boot-time verification is derived from
// observed state rather than from fixed sleeps.
//
// Usage:
//   x11_probe list
//       Print every client window (any window carrying WM_CLASS).
//   x11_probe wait-wm <timeout_s>
//       Wait for the real EWMH check-window handshake and WM_S0 ownership.
//   x11_probe wait-desktop <timeout_s>
//       Wait until an EWMH window manager is running (root
//       _NET_SUPPORTING_WM_CHECK -> check window that points to itself, has a
//       _NET_WM_NAME, and WM_S0 has an owner), and viewable xfdesktop
//       (covering the screen) and xfce4-panel client windows exist whose
//       _NET_WM_PID processes are alive. Also waits (best effort, reported)
//       for xfdesktop to publish its backdrop pixmap (_XROOTPMAP_ID).
//   x11_probe wait-window <wm_class_instance> <timeout_s>
//       Wait until a viewable, WM-managed (reparented) client window with the
//       given WM_CLASS instance exists.
//   x11_probe focus <wm_class_instance> <timeout_s>
//       Ask the WM to activate the window (_NET_ACTIVE_WINDOW), fall back to
//       SetInputFocus, and succeed only once GetInputFocus reports that
//       window (or a descendant of it).
//
// Every reported line is prefixed with [X11_...]; exit status is 0 only if
// the requested condition was observed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "userland/x11_client.h"

namespace {

struct ClientWindow {
  uint32_t id = 0;
  uint32_t parent = 0;
  uint32_t toplevel = 0;  // ancestor that is a direct child of the root
  std::string wm_class;   // "instance/class"
  std::string name;
  uint8_t map_state = 0;
  bool override_redirect = false;
  int16_t x = 0, y = 0;
  uint16_t width = 0, height = 0;
  uint32_t pid = 0;
  bool has_pid = false;
  char proc_state = '?';  // from /proc/<pid>/stat; '-' if missing
  std::string proc_comm;
  std::string proc_argv0;  // basename of argv[0] from /proc/<pid>/cmdline

  bool viewable() const { return map_state == 2; }
  bool managed(uint32_t root) const { return parent != root; }
  std::string instance() const {
    size_t slash = wm_class.find('/');
    return slash == std::string::npos ? wm_class : wm_class.substr(0, slash);
  }
  bool process_alive() const {
    return has_pid && proc_state != '-' && proc_state != 'Z' &&
           proc_state != 'X' && proc_state != '?';
  }
};

struct Atoms {
  uint32_t net_wm_name = 0;
  uint32_t net_wm_pid = 0;
  uint32_t net_supporting_wm_check = 0;
  uint32_t net_active_window = 0;
  uint32_t wm_s0 = 0;
  uint32_t xrootpmap_id = 0;
  uint32_t pixmap = 0;
};

double Now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

void SleepMs(unsigned ms) {
  struct timespec ts = {static_cast<time_t>(ms / 1000),
                        static_cast<long>((ms % 1000) * 1000000L)};
  nanosleep(&ts, nullptr);
}

bool ReadSmallFile(const std::string& path, std::string* out) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  char buf[512];
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  out->assign(buf, n);
  return true;
}

void FillProcInfo(ClientWindow* w) {
  if (!w->has_pid) return;
  std::string stat;
  if (!ReadSmallFile("/proc/" + std::to_string(w->pid) + "/stat", &stat)) {
    w->proc_state = '-';
    return;
  }
  size_t rp = stat.rfind(')');
  w->proc_state = (rp != std::string::npos && rp + 2 < stat.size())
                      ? stat[rp + 2]
                      : '?';
  std::string comm;
  if (ReadSmallFile("/proc/" + std::to_string(w->pid) + "/comm", &comm)) {
    while (!comm.empty() && (comm.back() == '\n' || comm.back() == '\0')) {
      comm.pop_back();
    }
    w->proc_comm = comm;
  }
  std::string cmdline;
  if (ReadSmallFile("/proc/" + std::to_string(w->pid) + "/cmdline",
                    &cmdline)) {
    std::string argv0 = cmdline.substr(0, cmdline.find('\0'));
    size_t slash = argv0.rfind('/');
    w->proc_argv0 =
        slash == std::string::npos ? argv0 : argv0.substr(slash + 1);
  }
}

std::string Sanitize(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += (static_cast<unsigned char>(c) < 0x20) ? '?' : c;
  }
  return out;
}

bool InitAtoms(x11::Connection* x, Atoms* a) {
  return x->InternAtom("_NET_WM_NAME", false, &a->net_wm_name) &&
         x->InternAtom("_NET_WM_PID", false, &a->net_wm_pid) &&
         x->InternAtom("_NET_SUPPORTING_WM_CHECK", false,
                       &a->net_supporting_wm_check) &&
         x->InternAtom("_NET_ACTIVE_WINDOW", false, &a->net_active_window) &&
         x->InternAtom("WM_S0", false, &a->wm_s0) &&
         x->InternAtom("_XROOTPMAP_ID", false, &a->xrootpmap_id) &&
         x->InternAtom("PIXMAP", false, &a->pixmap);
}

// Recursively collects client windows (windows with WM_CLASS). Windows may
// disappear while we walk; such errors are tolerated (BadWindow) and the
// subtree skipped.
void Walk(x11::Connection* x, const Atoms& a, uint32_t win, uint32_t parent,
          uint32_t toplevel, int depth, std::vector<ClientWindow>* out) {
  if (depth > 0) {
    std::string cls = x->GetWmClass(win);
    if (!x->connected()) return;
    if (!cls.empty()) {
      ClientWindow w;
      w.id = win;
      w.parent = parent;
      w.toplevel = toplevel;
      w.wm_class = cls;
      w.name = x->GetStringProperty(win, a.net_wm_name);
      if (w.name.empty()) w.name = x->GetStringProperty(win, x11::kAtomWmName);
      x11::WindowAttributes attrs;
      if (x->GetWindowAttributes(win, &attrs)) {
        w.map_state = attrs.map_state;
        w.override_redirect = attrs.override_redirect;
      }
      x11::Geometry g;
      if (x->GetGeometry(win, &g)) {
        w.width = g.width;
        w.height = g.height;
      }
      x->TranslateToRoot(win, &w.x, &w.y);
      bool ok = false;
      w.pid = x->GetCardinalProperty(win, a.net_wm_pid, &ok);
      w.has_pid = ok;
      FillProcInfo(&w);
      out->push_back(w);
    }
  }
  if (depth >= 4) return;
  x11::TreeInfo tree;
  if (!x->QueryTree(win, &tree)) return;
  for (uint32_t child : tree.children) {
    Walk(x, a, child, win, depth == 0 ? child : toplevel, depth + 1, out);
  }
}

std::vector<ClientWindow> ListClients(x11::Connection* x, const Atoms& a) {
  std::vector<ClientWindow> out;
  Walk(x, a, x->root(), 0, 0, 0, &out);
  return out;
}

void PrintWindow(const ClientWindow& w, uint32_t root) {
  static const char* kMapStates[] = {"unmapped", "unviewable", "viewable"};
  char proc[192];
  if (w.has_pid) {
    snprintf(proc, sizeof(proc), "pid=%u proc_state=%c proc_argv0=%s "
             "proc_comm=%s", w.pid, w.proc_state,
             w.proc_argv0.empty() ? "-" : w.proc_argv0.c_str(),
             w.proc_comm.empty() ? "-" : w.proc_comm.c_str());
  } else {
    snprintf(proc, sizeof(proc),
             "pid=- proc_state=- proc_argv0=- proc_comm=-");
  }
  printf("[X11_WINDOW] id=0x%08x class=%s map=%s managed=%s frame=0x%08x "
         "geom=%ux%u+%d+%d %s name=\"%s\"\n",
         w.id, w.wm_class.c_str(),
         w.map_state < 3 ? kMapStates[w.map_state] : "?",
         w.managed(root) ? "yes" : "no", w.toplevel, w.width, w.height, w.x,
         w.y, proc, Sanitize(w.name).c_str());
}

const ClientWindow* FindViewable(const std::vector<ClientWindow>& wins,
                                 const std::string& instance) {
  for (const auto& w : wins) {
    if (w.instance() == instance && w.viewable()) return &w;
  }
  return nullptr;
}

struct WmInfo {
  bool ok = false;
  uint32_t check_window = 0;
  uint32_t wm_s0_owner = 0;
  std::string name;
  std::string problem;
};

WmInfo ProbeWm(x11::Connection* x, const Atoms& a) {
  WmInfo info;
  bool ok = false;
  info.check_window =
      x->GetWindowProperty(x->root(), a.net_supporting_wm_check, &ok);
  if (!ok || !info.check_window) {
    info.problem = "root has no _NET_SUPPORTING_WM_CHECK";
    return info;
  }
  uint32_t self = x->GetWindowProperty(info.check_window,
                                       a.net_supporting_wm_check, &ok);
  if (!ok || self != info.check_window) {
    info.problem = "_NET_SUPPORTING_WM_CHECK window does not point to itself";
    return info;
  }
  info.name = x->GetStringProperty(info.check_window, a.net_wm_name);
  if (info.name.empty()) {
    info.problem = "WM check window has no _NET_WM_NAME";
    return info;
  }
  if (!x->GetSelectionOwner(a.wm_s0, &info.wm_s0_owner) ||
      !info.wm_s0_owner) {
    info.problem = "WM_S0 selection has no owner";
    return info;
  }
  info.ok = true;
  return info;
}

bool Connect(x11::Connection* x, Atoms* a) {
  if (!x->Connect()) return false;
  if (!InitAtoms(x, a)) {
    fprintf(stderr, "[X11_PROBE] InternAtom failed\n");
    return false;
  }
  return true;
}

int CmdList() {
  x11::Connection x;
  Atoms a;
  if (!Connect(&x, &a)) return 1;
  auto wins = ListClients(&x, a);
  for (const auto& w : wins) PrintWindow(w, x.root());
  printf("[X11_PROBE] %zu client windows\n", wins.size());
  return x.connected() ? 0 : 1;
}

int CmdWaitWm(double timeout_s) {
  x11::Connection x;
  Atoms a;
  if (!Connect(&x, &a)) return 1;
  double deadline = Now() + timeout_s;
  std::string last_problem;
  while (true) {
    WmInfo wm = ProbeWm(&x, a);
    if (!x.connected()) return 1;
    if (wm.ok) {
      printf("[X11_WM] check_window=0x%08x name=\"%s\" wm_s0_owner=0x%08x\n",
             wm.check_window, Sanitize(wm.name).c_str(), wm.wm_s0_owner);
      return 0;
    }
    if (wm.problem != last_problem) {
      printf("[X11_PROBE] waiting for WM: %s\n", wm.problem.c_str());
      last_problem = wm.problem;
    }
    if (Now() > deadline) {
      printf("[X11_PROBE] WM_TIMEOUT after %.0fs: %s\n", timeout_s, wm.problem.c_str());
      return 1;
    }
    SleepMs(250);
  }
}

int CmdWaitDesktop(double timeout_s) {
  x11::Connection x;
  Atoms a;
  if (!Connect(&x, &a)) return 1;
  double deadline = Now() + timeout_s;
  std::string last_problem;
  std::vector<ClientWindow> wins;
  WmInfo wm;
  const ClientWindow* desktop = nullptr;
  const ClientWindow* panel = nullptr;
  while (true) {
    wm = ProbeWm(&x, a);
    wins = ListClients(&x, a);
    if (!x.connected()) {
      printf("[X11_PROBE] lost connection to X server\n");
      return 1;
    }
    desktop = FindViewable(wins, "xfdesktop");
    panel = FindViewable(wins, "xfce4-panel");
    std::string problem;
    if (!wm.ok) {
      problem = "window manager: " + wm.problem;
    } else if (!desktop) {
      problem = "no viewable xfdesktop window";
    } else if (desktop->width < x.screen_width() ||
               desktop->height < x.screen_height()) {
      problem = "xfdesktop window does not cover the screen";
    } else if (!desktop->process_alive()) {
      problem = "xfdesktop window has no live _NET_WM_PID process";
    } else if (!panel) {
      problem = "no viewable xfce4-panel window";
    } else if (!panel->process_alive()) {
      problem = "xfce4-panel window has no live _NET_WM_PID process";
    }
    if (problem.empty()) break;
    if (problem != last_problem) {
      printf("[X11_PROBE] waiting: %s\n", problem.c_str());
      fflush(stdout);
      last_problem = problem;
    }
    if (Now() > deadline) {
      for (const auto& w : wins) PrintWindow(w, x.root());
      printf("[X11_PROBE] DESKTOP_TIMEOUT after %.0fs: %s\n", timeout_s,
             problem.c_str());
      return 1;
    }
    SleepMs(250);
  }

  // Best effort: wait for xfdesktop to publish the rendered backdrop pixmap.
  uint32_t backdrop = 0;
  while (Now() < deadline) {
    x11::Property p;
    if (x.GetProperty(x.root(), a.xrootpmap_id, a.pixmap, &p) &&
        p.format == 32 && p.data.size() >= 4) {
      memcpy(&backdrop, p.data.data(), 4);
      if (backdrop) break;
    }
    SleepMs(250);
  }

  for (const auto& w : wins) PrintWindow(w, x.root());
  printf("[X11_WM] check_window=0x%08x name=\"%s\" wm_s0_owner=0x%08x\n",
         wm.check_window, Sanitize(wm.name).c_str(), wm.wm_s0_owner);
  printf("[X11_PROBE] DESKTOP_MAPPED wm=\"%s\" desktop=0x%08x panel=0x%08x "
         "backdrop_pixmap=0x%08x screen=%ux%u\n",
         Sanitize(wm.name).c_str(), desktop->id, panel->id, backdrop,
         x.screen_width(), x.screen_height());
  fflush(stdout);
  return 0;
}

int CmdWaitWindow(const std::string& instance, double timeout_s) {
  x11::Connection x;
  Atoms a;
  if (!Connect(&x, &a)) return 1;
  double deadline = Now() + timeout_s;
  while (true) {
    auto wins = ListClients(&x, a);
    if (!x.connected()) return 1;
    for (const auto& w : wins) {
      if (w.instance() == instance && w.viewable() && w.managed(x.root()) &&
          w.process_alive()) {
        PrintWindow(w, x.root());
        printf("[X11_PROBE] WINDOW_MAPPED class=%s id=0x%08x\n",
               w.wm_class.c_str(), w.id);
        return 0;
      }
    }
    if (Now() > deadline) {
      for (const auto& w : wins) PrintWindow(w, x.root());
      printf("[X11_PROBE] WINDOW_TIMEOUT: no viewable managed '%s' window\n",
             instance.c_str());
      return 1;
    }
    SleepMs(250);
  }
}

// True if |w| is |target| or a descendant of it.
bool IsSameOrDescendant(x11::Connection* x, uint32_t w, uint32_t target) {
  for (int i = 0; i < 16 && w && w != x->root(); ++i) {
    if (w == target) return true;
    x11::TreeInfo t;
    if (!x->QueryTree(w, &t)) return false;
    w = t.parent;
  }
  return false;
}

int CmdFocus(const std::string& instance, double timeout_s) {
  x11::Connection x;
  Atoms a;
  if (!Connect(&x, &a)) return 1;
  auto wins = ListClients(&x, a);
  const ClientWindow* target = FindViewable(wins, instance);
  if (!target) {
    printf("[X11_FOCUS] FAIL: no viewable '%s' window\n", instance.c_str());
    return 1;
  }
  const uint32_t id = target->id;
  double deadline = Now() + timeout_s;
  bool used_fallback = false;
  // EWMH activation request (source indication 2 = pager/tool).
  const uint32_t data[5] = {2, 0, 0, 0, 0};
  constexpr uint32_t kSubstructureMasks = (1u << 19) | (1u << 20);
  if (!x.SendClientMessage(x.root(), id, a.net_active_window, data,
                           kSubstructureMasks)) {
    printf("[X11_FOCUS] FAIL: sending _NET_ACTIVE_WINDOW failed\n");
    return 1;
  }
  double fallback_at = Now() + 2.0;
  while (true) {
    uint32_t focus = 0;
    if (!x.GetInputFocus(&focus)) {
      printf("[X11_FOCUS] FAIL: GetInputFocus failed\n");
      return 1;
    }
    if (focus > 1 && IsSameOrDescendant(&x, focus, id)) {
      printf("[X11_FOCUS] OK focus=0x%08x target=0x%08x class=%s via=%s\n",
             focus, id, target->wm_class.c_str(),
             used_fallback ? "SetInputFocus" : "_NET_ACTIVE_WINDOW");
      return 0;
    }
    if (!used_fallback && Now() > fallback_at) {
      used_fallback = true;
      printf("[X11_FOCUS] WM did not focus 0x%08x (focus=0x%08x); using "
             "SetInputFocus\n",
             id, focus);
      if (!x.SetInputFocus(id)) {
        printf("[X11_FOCUS] FAIL: SetInputFocus(0x%08x) failed\n", id);
        return 1;
      }
    }
    if (Now() > deadline) {
      printf("[X11_FOCUS] FAIL: focus=0x%08x is not target 0x%08x\n", focus,
             id);
      return 1;
    }
    SleepMs(100);
  }
}

int Usage() {
  fprintf(stderr,
          "usage: x11_probe list | wait-wm <timeout_s> | wait-desktop <timeout_s> | "
          "wait-window <instance> <timeout_s> | focus <instance> "
          "<timeout_s>\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) return Usage();
  std::string cmd = argv[1];
  if (cmd == "list" && argc == 2) return CmdList();
  if (cmd == "wait-wm" && argc == 3) return CmdWaitWm(atof(argv[2]));
  if (cmd == "wait-desktop" && argc == 3) return CmdWaitDesktop(atof(argv[2]));
  if (cmd == "wait-window" && argc == 4) {
    return CmdWaitWindow(argv[2], atof(argv[3]));
  }
  if (cmd == "focus" && argc == 4) return CmdFocus(argv[2], atof(argv[3]));
  return Usage();
}
