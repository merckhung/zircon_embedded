#!/bin/sh
# /init for the 3rd rootfs: Xfce4 Desktop + Prebuilt Chromium on Zircon Microkernel
#
# Every status line is derived from observed state:
#   * x11_probe queries the X server (window tree, WM_CLASS, map state,
#     _NET_WM_PID, EWMH WM check window, WM_S0 owner, input focus).
#   * [XFCE4_PROC] lines come from /proc/<pid>/{stat,cmdline,comm} of the
#     PIDs we launched.
#   * Network interface eth0 (Realtek RTL8139 PCI DMA) is verified via
#     /proc/net/dev, /proc/net/route, and a live TCP/TLS/HTTP connection
#     to www.yahoo.com by Chromium.
# On any failure "[CHROMIUM_ROOTFS: FAILED] <reason>" is printed with logs.

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

LOGS="/tmp/xvfb.log /tmp/xfwm4.log /tmp/xfdesktop.log /tmp/xfce4-panel.log /tmp/chromium.log"

fail() {
  echo "[CHROMIUM_ROOTFS: FAILED] $*"
  for f in $LOGS; do
    if [ -s "$f" ]; then
      echo "----- tail $f -----"
      tail -n 40 "$f"
    fi
  done
  /bin/x11_probe list 2>&1
  exec /bin/sh
}

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

echo "================================================================"
echo "[chromium_rootfs] Booting 64-bit Xfce4 + Chromium on Zircon Microkernel"
echo "================================================================"
uname -a

/bin/zx_test || fail "zx_test failed"

echo "[chromium_rootfs] Verifying RTL8139 eth0 network interface..."
cat /proc/net/dev
cat /proc/net/route
grep -q "eth0:" /proc/net/dev || fail "eth0 not present in /proc/net/dev"

mkdir -p /tmp/.X11-unix /tmp/.ICE-unix /run/dbus /var/run/dbus /var/lib/dbus \
         /tmp/chrome-profile \
         "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" \
  || fail "cannot create runtime directories"
chmod 1777 /tmp /tmp/.X11-unix /tmp/.ICE-unix
chmod 0700 "$XDG_RUNTIME_DIR"

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
echo "[chromium_rootfs] machine-id: $MACHINE_ID"

echo "[chromium_rootfs] Updating gdk-pixbuf loaders, MIME database, and GLib schemas..."
if [ -x /usr/bin/update-mime-database ] && [ -d /usr/share/mime ]; then
  PKGSYSTEM_ENABLE_FSYNC=0 /usr/bin/update-mime-database /usr/share/mime
fi
if [ -x /usr/bin/gdk-pixbuf-query-loaders ]; then
  /usr/bin/gdk-pixbuf-query-loaders --update-cache
fi
if [ -x /usr/bin/glib-compile-schemas ] && [ -d /usr/share/glib-2.0/schemas ]; then
  /usr/bin/glib-compile-schemas /usr/share/glib-2.0/schemas
fi
if [ -x /usr/bin/fc-cache ]; then
  /usr/bin/fc-cache -f >/dev/null 2>&1 || true
fi

echo "[chromium_rootfs] Starting D-Bus system and session daemons..."
dbus-daemon --config-file=/etc/dbus-1/system-zircon.conf --fork \
  || fail "system dbus-daemon failed"
dbus-daemon --config-file=/etc/dbus-1/session-zircon.conf --fork \
  || fail "session dbus-daemon failed"
export DBUS_SESSION_BUS_ADDRESS="unix:path=/tmp/dbus-session.sock"

if [ -x /usr/lib/xfce4/xfconf/xfconfd ]; then
  /usr/lib/xfce4/xfconf/xfconfd >/tmp/xfconfd.log 2>&1 &
fi

FB_GEOM=$(/bin/x11_fb_bridge --print-geometry) || fail "cannot query /dev/fb0 geometry"
echo "[chromium_rootfs] /dev/fb0 geometry: $FB_GEOM"

echo "[chromium_rootfs] Starting Xvfb :0 (${FB_GEOM}x24, -fbdir /tmp)..."
Xvfb :0 -screen 0 "${FB_GEOM}x24" -fbdir /tmp -ac -nolisten tcp -noreset >/tmp/xvfb.log 2>&1 &
XVFB_PID=$!

i=0
until [ -e /tmp/Xvfb_screen0 ] && [ -e /tmp/.X11-unix/X0 ]; do
  i=$((i + 1))
  [ $i -gt 150 ] && fail "Xvfb did not create its socket/screen file in 15s"
  proc_check Xvfb $XVFB_PID >/dev/null || fail "Xvfb exited"
  usleep 100000
done

echo "[chromium_rootfs] Starting x11_fb_bridge (Xvfb -> Bochs VBE /dev/fb0, input via XTEST)..."
/bin/x11_fb_bridge /tmp/Xvfb_screen0 &
BRIDGE_PID=$!

echo "[chromium_rootfs] Starting Xfce4 Window Manager (xfwm4)..."
xfwm4 --compositor=off >/tmp/xfwm4.log 2>&1 &
XFWM4_PID=$!

# ARM64 runs under TCG on an x64 host. Synchronize against real WM state;
# starting desktop clients first creates a startup race on slower emulation.
DESKTOP_STARTUP_TIMEOUT=90
[ "$(uname -m)" = aarch64 ] && DESKTOP_STARTUP_TIMEOUT=240
/bin/x11_probe wait-wm "$DESKTOP_STARTUP_TIMEOUT" || fail "window manager did not register"

echo "[chromium_rootfs] Starting Xfce4 Desktop (xfdesktop) & Panel (xfce4-panel)..."
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

echo "[chromium_rootfs] Launching Chromium -> https://www.yahoo.com/ ..."
DBUS_SESSION_BUS_ADDRESS=disabled: \
LD_LIBRARY_PATH=/opt/chromium:/opt/chromium/lib \
  /opt/chromium/chrome \
  --no-sandbox \
  --no-zygote \
  --single-process \
  --in-process-gpu \
  --disable-gpu-sandbox \
  --disable-seccomp-filter-sandbox \
  --disable-setuid-sandbox \
  --disable-crash-reporter \
  --disable-crashpad-for-testing \
  --no-first-run \
  --remote-debugging-port=9222 \
  --no-default-browser-check \
  --test-type \
  --disable-hang-monitor \
  --disable-background-networking \
  --disable-sync \
  --disable-extensions \
  --disable-component-extensions-with-background-pages \
  --disable-component-update \
  --password-store=basic \
  --disable-features=Vulkan,SpareRendererForSitePerProcess,DbusSecretPortal,UseFreedesktopSecretKeyProvider \
  --use-gl=disabled \
  --disable-gpu \
  --disable-gpu-compositing \
  --disable-software-rasterizer \
  --ozone-platform=x11 \
  --lang=en-US \
  --user-data-dir=/tmp/chrome-profile \
  --window-position=20,32 \
  --window-size=984,710 \
  --enable-logging=stderr \
  --v=0 \
  "https://www.yahoo.com/" >/tmp/chromium.log 2>&1 &
CHROME_PID=$!

echo "[chromium_rootfs] Waiting for Chromium X11 window to map..."
i=0
CHROME_MAPPED=0
while [ $i -lt 240 ]; do
  proc_check chrome $CHROME_PID >/dev/null || fail "chrome exited before mapping window"
  if /bin/x11_probe list | grep -E '\[X11_WINDOW\].*(proc_argv0=chrome|class=.*[Cc]hrom).*map=viewable'; then
    CHROME_MAPPED=1
    break
  fi
  i=$((i + 1))
  usleep 250000
done
[ $CHROME_MAPPED -eq 1 ] || fail "Chromium X11 window did not become viewable within 60s"

proc_check chrome $CHROME_PID || fail "chrome not running"
echo "[CHROMIUM_WINDOW: MAPPED]"

# Confirm the actual HTTPS document and evaluate a fresh JavaScript challenge.
# Window titles also appear on error pages, so they cannot establish a load.
i=0
YAHOO_LOADED=0
while [ $i -lt 90 ]; do
  proc_check chrome $CHROME_PID >/dev/null || fail "chrome exited during page load"
  if /bin/chromium_page_probe >/tmp/chromium-probe.log 2>&1; then
    cat /tmp/chromium-probe.log
    echo "[CHROMIUM_YAHOO: LOADED] renderer document and challenge verified"
    YAHOO_LOADED=1
    break
  fi
  i=$((i + 1))
  sleep 2
done
[ $YAHOO_LOADED -eq 1 ] || {
  cat /tmp/chromium-probe.log
  fail "Chromium did not produce a loaded secure Yahoo document"
}

# Allow Blink + software compositor to finish layout and painting of the Yahoo front page
sleep 15
proc_check chrome $CHROME_PID || fail "chrome exited after page load"

if [ -s /tmp/chromium.log ]; then
  echo "----- tail /tmp/chromium.log -----"
  tail -n 30 /tmp/chromium.log
fi

/bin/x11_probe list
grep "eth0:" /proc/net/dev
echo "[CHROMIUM_READY]"

exec /bin/sh
