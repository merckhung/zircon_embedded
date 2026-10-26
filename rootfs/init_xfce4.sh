#!/bin/sh
# /init for the Xfce4 rootfs (PID 1 zircon_linux_init execs this script).
#
# Every status line below is derived from observed state:
#   * x11_probe queries the X server (window tree, WM_CLASS, map state,
#     _NET_WM_PID, EWMH WM check window, WM_S0 owner, input focus).
#   * [XFCE4_PROC] lines come from /proc/<pid>/{stat,cmdline,comm} of the
#     PIDs we launched.
#   * The terminal round-trip is proven by typing (X11 XTEST via
#     x11_fb_bridge) a command into xfce4-terminal whose shell computes a value
#     and writes it to the serial console. The computed value never appears in
#     anything this script prints.
# On any failure "[XFCE4_ROOTFS: FAILED] <reason>" is printed with logs.

# PID 1 provides no session environment; export everything the desktop needs.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export HOME=/root
export USER=root
export LOGNAME=root
export SHELL=/bin/sh
export TERM=vt100
export DISPLAY=:0
export XDG_RUNTIME_DIR=/run/user/0
export XDG_CONFIG_HOME=/root/.config
export XDG_CACHE_HOME=/root/.cache
export XDG_DATA_HOME=/root/.local/share
export XDG_CONFIG_DIRS=/etc/xdg
export XDG_DATA_DIRS=/usr/local/share:/usr/share
export XDG_CURRENT_DESKTOP=XFCE
export XDG_SESSION_TYPE=x11
export GDK_BACKEND=x11
export NO_AT_BRIDGE=1
export LIBGL_ALWAYS_SOFTWARE=1

LOGS="/tmp/xvfb.log /tmp/xfwm4.log /tmp/xfdesktop.log /tmp/xfce4-panel.log /tmp/xfce4-terminal.log"

fail() {
  echo "[XFCE4_ROOTFS: FAILED] $*"
  for f in $LOGS; do
    if [ -s "$f" ]; then
      echo "----- tail $f -----"
      tail -n 20 "$f"
    fi
  done
  /bin/x11_probe list 2>&1
  exec /bin/sh
}

# proc_check <name> <pid>: report the real state of a launched process.
proc_check() {
  _name=$1
  _pid=$2
  _state=-
  _comm=-
  _argv0=-
  if _stat=$(cat "/proc/$_pid/stat" 2>/dev/null); then
    _rest=${_stat##*) }
    _state=${_rest%% *}
    _comm=$(cat "/proc/$_pid/comm" 2>/dev/null)
    _argv0=$(tr '\0' '\n' < "/proc/$_pid/cmdline" 2>/dev/null | head -n 1)
    _argv0=${_argv0##*/}
  fi
  echo "[XFCE4_PROC] name=$_name pid=$_pid state=$_state argv0=$_argv0 comm=$_comm"
  case "$_state" in
    R|S|D) return 0 ;;
  esac
  return 1
}

# find_child <ppid>: print the PID of a process whose parent is <ppid>.
find_child() {
  for _d in /proc/[0-9]*; do
    _stat=$(cat "$_d/stat" 2>/dev/null) || continue
    _rest=${_stat##*) }
    _ppid=$(echo "$_rest" | cut -d' ' -f2)
    if [ "$_ppid" = "$1" ]; then
      echo "${_d#/proc/}"
      return 0
    fi
  done
  return 1
}

# inject <line>...: hand an XTEST script to x11_fb_bridge and wait for its ack.
inject() {
  rm -f /tmp/x11_inject.ack
  printf '%s\n' "$@" > /tmp/x11_inject.cmd.tmp || return 1
  mv /tmp/x11_inject.cmd.tmp /tmp/x11_inject.cmd || return 1
  _i=0
  while [ ! -e /tmp/x11_inject.ack ]; do
    _i=$((_i + 1))
    [ $_i -gt 300 ] && { echo "[X11_INJECT] no ack from x11_fb_bridge"; return 1; }
    usleep 100000
  done
  _ack=$(cat /tmp/x11_inject.ack)
  echo "[X11_INJECT] $_ack"
  case "$_ack" in
    ok*) return 0 ;;
  esac
  return 1
}

echo "================================================================"
echo "[xfce4_rootfs] Booting 64-bit Xfce4 Desktop on Zircon Microkernel"
echo "================================================================"
uname -a

/bin/zx_test || fail "zx_test failed"

mkdir -p /tmp/.X11-unix /tmp/.ICE-unix /run/dbus /var/run/dbus /var/lib/dbus \
         "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" \
  || fail "cannot create runtime directories"
chmod 1777 /tmp /tmp/.X11-unix /tmp/.ICE-unix
chmod 0700 "$XDG_RUNTIME_DIR"

# Per-boot machine id (D-Bus and GLib require one).
if [ -x /usr/bin/dbus-uuidgen ]; then
  rm -f /etc/machine-id
  dbus-uuidgen --ensure=/etc/machine-id || fail "dbus-uuidgen failed"
else
  od -An -N16 -tx1 /dev/urandom | tr -d ' \n' > /etc/machine-id
  echo >> /etc/machine-id
fi
ln -sf /etc/machine-id /var/lib/dbus/machine-id
MACHINE_ID=$(cat /etc/machine-id)
[ ${#MACHINE_ID} -eq 32 ] || fail "invalid machine-id '$MACHINE_ID'"
echo "[xfce4_rootfs] machine-id: $MACHINE_ID"

echo "[xfce4_rootfs] Updating gdk-pixbuf loaders, MIME database, and GLib schemas..."
if [ -x /usr/bin/update-mime-database ] && [ -d /usr/share/mime ]; then
  PKGSYSTEM_ENABLE_FSYNC=0 /usr/bin/update-mime-database /usr/share/mime
fi
if [ -x /usr/bin/gdk-pixbuf-query-loaders ]; then
  /usr/bin/gdk-pixbuf-query-loaders --update-cache
fi
if [ -x /usr/bin/glib-compile-schemas ] && [ -d /usr/share/glib-2.0/schemas ]; then
  /usr/bin/glib-compile-schemas /usr/share/glib-2.0/schemas
fi

echo "[xfce4_rootfs] Starting D-Bus system and session daemons..."
dbus-daemon --config-file=/etc/dbus-1/system-zircon.conf --fork \
  || fail "system dbus-daemon failed"
dbus-daemon --config-file=/etc/dbus-1/session-zircon.conf --fork \
  || fail "session dbus-daemon failed"
export DBUS_SESSION_BUS_ADDRESS="unix:path=/tmp/dbus-session.sock"

if [ -x /usr/lib/xfce4/xfconf/xfconfd ]; then
  /usr/lib/xfce4/xfconf/xfconfd >/tmp/xfconfd.log 2>&1 &
fi

# Size the X screen from the real framebuffer geometry (FBIOGET_VSCREENINFO).
FB_GEOM=$(/bin/x11_fb_bridge --print-geometry) || fail "cannot query /dev/fb0 geometry"
echo "[xfce4_rootfs] /dev/fb0 geometry: $FB_GEOM"

echo "[xfce4_rootfs] Starting Xvfb :0 (${FB_GEOM}x24, -fbdir /tmp)..."
Xvfb :0 -screen 0 "${FB_GEOM}x24" -fbdir /tmp -ac -nolisten tcp -noreset >/tmp/xvfb.log 2>&1 &
XVFB_PID=$!

i=0
until [ -e /tmp/Xvfb_screen0 ] && [ -e /tmp/.X11-unix/X0 ]; do
  i=$((i + 1))
  [ $i -gt 150 ] && fail "Xvfb did not create its socket/screen file in 15s"
  proc_check Xvfb $XVFB_PID >/dev/null || fail "Xvfb exited"
  usleep 100000
done

echo "[xfce4_rootfs] Starting x11_fb_bridge (Xvfb -> Bochs VBE /dev/fb0, input via XTEST)..."
# Logs go straight to the serial console so failures are visible.
/bin/x11_fb_bridge /tmp/Xvfb_screen0 &
BRIDGE_PID=$!

echo "[xfce4_rootfs] Starting Xfce4 Window Manager (xfwm4)..."
xfwm4 --compositor=off >/tmp/xfwm4.log 2>&1 &
XFWM4_PID=$!

# ARM64 runs under TCG on an x64 host. Synchronize against real WM state;
# starting desktop clients first creates a startup race on slower emulation.
DESKTOP_STARTUP_TIMEOUT=90
[ "$(uname -m)" = aarch64 ] && DESKTOP_STARTUP_TIMEOUT=240
/bin/x11_probe wait-wm "$DESKTOP_STARTUP_TIMEOUT" || fail "window manager did not register"

echo "[xfce4_rootfs] Starting Xfce4 Desktop (xfdesktop) & Panel (xfce4-panel)..."
xfdesktop >/tmp/xfdesktop.log 2>&1 &
XFDESKTOP_PID=$!
xfce4-panel --disable-wm-check --sm-client-disable >/tmp/xfce4-panel.log 2>&1 &
PANEL_PID=$!

/bin/x11_probe wait-desktop "$DESKTOP_STARTUP_TIMEOUT" || fail "Xfce4 desktop windows did not appear"
proc_check Xvfb $XVFB_PID || fail "Xvfb not running"
proc_check x11_fb_bridge $BRIDGE_PID || fail "x11_fb_bridge not running"
proc_check xfwm4 $XFWM4_PID || fail "xfwm4 not running"
proc_check xfdesktop $XFDESKTOP_PID || fail "xfdesktop not running"
proc_check xfce4-panel $PANEL_PID || fail "xfce4-panel not running"
echo "[XFCE4_DESKTOP: READY]"

# Leave the desktop alone briefly so the verifier can take the desktop-only
# screenshot before the terminal window opens.
sleep 5

echo "[xfce4_rootfs] Starting Xfce4 Terminal (xfce4-terminal)..."
xfce4-terminal --disable-server --title="Zircon Embedded Terminal (Linux Shim on Zircon $(uname -m))" --geometry=80x22+100+80 >/tmp/xfce4-terminal.log 2>&1 &
TERMINAL_PID=$!

/bin/x11_probe wait-window xfce4-terminal 60 || fail "xfce4-terminal window did not appear"
proc_check xfce4-terminal $TERMINAL_PID || fail "xfce4-terminal not running"

# Wait until the terminal has spawned its shell on the PTY.
i=0
until SHELL_PID=$(find_child $TERMINAL_PID); do
  i=$((i + 1))
  [ $i -gt 300 ] && fail "xfce4-terminal did not spawn a shell"
  usleep 100000
done
proc_check terminal-shell $SHELL_PID || fail "terminal shell not running"

# Give the shell a moment to reach its interactive prompt (line editing on).
sleep 2
/bin/x11_probe focus xfce4-terminal 10 || fail "could not focus xfce4-terminal"

# Type into the focused terminal through X11 (XTEST key events). Two command
# lines are typed back-to-back, so the second is typed ahead while the first
# is still running (exercising PTY canonical-mode buffering). The first shows
# uname + /proc/zircon/info in the terminal; the second makes the terminal's
# shell compute a value and send it to the serial console together with
# whether its stdin is a tty and its tty name. The verifier requires both.
inject \
  'typeline uname -a && head -n 5 /proc/zircon/info' \
  'typeline echo XFCE_TERM_RESULT_$((6*7)) stdin_tty=$([ -t 0 ] && echo yes || echo no) ttyname=$(tty) | tee /dev/ttyS0' \
  || fail "XTEST keystroke injection failed"

echo "[xfce4_rootfs] Keystrokes delivered to X server; serial console shell follows."
exec /bin/sh
