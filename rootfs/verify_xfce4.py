#!/usr/bin/env python3
"""End-to-end QEMU verification + QMP screenshots of //rootfs:xfce4_system.

Usage: verify_xfce4.py <qemu> <boot_shim> <zbi> [--screenshot-dir DIR]
                       [--timeout SECONDS]

Screenshots go to --screenshot-dir, or to
$BUILD_WORKSPACE_DIRECTORY/docs/screenshots when run via `bazel run`.

PASS requires evidence observed on the serial console, all derived from real
guest state (see rootfs/init_xfce4.sh and userland/x11_probe.cc):
  * [ZX_TEST: PASS] from zx_test's own checks.
  * [X11_WM] name="Xfwm4": EWMH check window + WM_S0 owner queried from X.
  * [X11_WINDOW] lines: viewable xfdesktop (covering the screen), viewable
    xfce4-panel and a viewable, WM-managed xfce4-terminal, each with a live
    _NET_WM_PID process whose PID matches the PID /init launched.
  * [XFCE4_PROC] lines: launched processes alive per /proc/<pid>/stat.
  * [X11_FOCUS] OK for xfce4-terminal and [X11_INJECT] ok from the bridge.
  * XFCE_TERM_RESULT_42 stdin_tty=yes ttyname=/dev/pts/N: written to the
    serial port by the shell inside xfce4-terminal after the command line
    `echo XFCE_TERM_RESULT_$((6*7)) stdin_tty=$([ -t 0 ] && echo yes ||
    echo no) ttyname=$(tty) | tee /dev/ttyS0` was typed into the terminal via
    X11 XTEST (typed ahead while the preceding `uname ...` line still ran).
    None of "XFCE_TERM_RESULT_42", "stdin_tty=yes" or a /dev/pts path appears
    in any input or script; they are the shell's computed output, proving the
    keyboard -> X11 -> xfce4-terminal -> PTY -> /bin/sh round-trip.
  * PS2_KBD_RESULT_63: afterwards the verifier types
    `echo PS2_KBD_RESULT_$((7*9)) > /dev/ttyS0` on QEMU's emulated PS/2
    keyboard (QMP send-key). This exercises the hardware input path
    i8042 -> shim /dev/input/event0 -> x11_fb_bridge -> XTEST -> terminal.
Pixel statistics of the screenshots are only a secondary sanity check.
"""

import argparse
import json
import os
import re
import select
import socket
import struct
import subprocess
import sys
import time
import zlib

PS2_MARKER_RE = re.compile(rb"PS2_KBD_RESULT_63\b")
# Typed with the emulated PS/2 keyboard (QMP send-key) after the XTEST test.
PS2_COMMAND = "echo PS2_KBD_RESULT_$((7*9)) > /dev/ttyS0\n"
TERM_MARKER_RE = re.compile(rb"XFCE_TERM_RESULT_42 stdin_tty=yes ttyname=(/dev/pts/\d+)")
WINDOW_RE = re.compile(
    rb"\[X11_WINDOW\] id=(0x[0-9a-f]+) class=(\S+) map=(\w+) managed=(\w+) "
    rb"frame=0x[0-9a-f]+ geom=(\d+)x(\d+)\+(-?\d+)\+(-?\d+) pid=(\S+) "
    rb"proc_state=(\S) proc_argv0=(\S+) proc_comm=(\S+)"
)
PROC_RE = re.compile(
    rb"\[XFCE4_PROC\] name=(\S+) pid=(\d+) state=(\S+) argv0=(\S*) comm=(\S*)"
)
DESKTOP_RE = re.compile(rb"\[X11_PROBE\] DESKTOP_MAPPED .*screen=(\d+)x(\d+)")
WM_RE = re.compile(rb'\[X11_WM\] check_window=0x[0-9a-f]+ name="([^"]*)"')
FOCUS_RE = re.compile(rb"\[X11_FOCUS\] OK focus=(0x[0-9a-f]+) target=(0x[0-9a-f]+) class=xfce4-terminal/")
INJECT_RE = re.compile(rb"\[X11_INJECT\] ok (\d+)")
FAILED_RE = re.compile(rb"\[XFCE4_ROOTFS: FAILED\][^\r\n]*")
LIVE_STATES = (b"R", b"S", b"D")


class QmpError(Exception):
    pass


def qmp_command(f, sock, cmd):
    sock.sendall((json.dumps(cmd) + "\r\n").encode("utf-8"))
    while True:
        line = f.readline()
        if not line:
            raise QmpError(f"QMP connection closed while waiting for {cmd['execute']}")
        msg = json.loads(line)
        if "event" in msg:
            continue
        if "error" in msg:
            raise QmpError(f"{cmd['execute']} failed: {msg['error']}")
        if "return" in msg:
            return msg["return"]


def qmp_screendump(qmp_sock_path, ppm_path):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(10.0)
    try:
        s.connect(qmp_sock_path)
        f = s.makefile("r", encoding="utf-8")
        greeting = json.loads(f.readline())
        if "QMP" not in greeting:
            raise QmpError(f"unexpected QMP greeting: {greeting}")
        qmp_command(f, s, {"execute": "qmp_capabilities"})
        if os.path.exists(ppm_path):
            os.unlink(ppm_path)
        qmp_command(f, s, {"execute": "screendump", "arguments": {"filename": ppm_path}})
    finally:
        s.close()
    if not os.path.exists(ppm_path) or os.path.getsize(ppm_path) == 0:
        raise QmpError(f"screendump reported success but {ppm_path} is missing/empty")


QCODE_UNSHIFTED = {" ": "spc", "\n": "ret", "-": "minus", "=": "equal",
                   "/": "slash", ".": "dot", ",": "comma", ";": "semicolon",
                   "'": "apostrophe", "`": "grave_accent", "[": "bracket_left",
                   "]": "bracket_right", "\\": "backslash"}
QCODE_SHIFTED = {"!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6",
                 "&": "7", "*": "8", "(": "9", ")": "0", "_": "minus",
                 "+": "equal", "?": "slash", ">": "dot", "<": "comma",
                 ":": "semicolon", '"': "apostrophe", "~": "grave_accent",
                 "{": "bracket_left", "}": "bracket_right", "|": "backslash"}


def char_to_qcodes(c):
    if c.isascii() and (c.islower() or c.isdigit()):
        return [c]
    if c.isascii() and c.isupper():
        return ["shift", c.lower()]
    if c in QCODE_UNSHIFTED:
        return [QCODE_UNSHIFTED[c]]
    if c in QCODE_SHIFTED:
        return ["shift", QCODE_SHIFTED[c]]
    raise ValueError(f"no PS/2 qcode mapping for {c!r}")


def qmp_type_ps2(qmp_sock_path, text):
    """Types text on QEMU's emulated PS/2 keyboard (hardware input path)."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(10.0)
    try:
        s.connect(qmp_sock_path)
        f = s.makefile("r", encoding="utf-8")
        json.loads(f.readline())
        qmp_command(f, s, {"execute": "qmp_capabilities"})
        for c in text:
            keys = [{"type": "qcode", "data": q} for q in char_to_qcodes(c)]
            qmp_command(f, s, {"execute": "send-key",
                               "arguments": {"keys": keys, "hold-time": 30}})
            time.sleep(0.06)
    finally:
        s.close()


def read_ppm(ppm_path):
    with open(ppm_path, "rb") as f:
        data = f.read()
    # P6 header: magic, width, height, maxval separated by whitespace.
    tokens = []
    idx = 0
    while len(tokens) < 4:
        while data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b"#":
            idx = data.index(b"\n", idx) + 1
            continue
        start = idx
        while not data[idx:idx + 1].isspace():
            idx += 1
        tokens.append(data[start:idx])
    idx += 1
    if tokens[0] != b"P6" or tokens[3] != b"255":
        raise ValueError(f"unsupported PPM header {tokens}")
    w, h = int(tokens[1]), int(tokens[2])
    pixels = data[idx:idx + w * h * 3]
    if len(pixels) != w * h * 3:
        raise ValueError("truncated PPM")
    return w, h, pixels


def write_png(png_path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body +
                struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    with open(png_path, "wb") as f:
        f.write(png)


def pixel_stats(w, h, rgb):
    non_black = 0
    colors = set()
    for i in range(0, w * h * 3, 3):
        r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
        if r > 5 or g > 5 or b > 5:
            non_black += 1
        if (i // 3) % 17 == 0:
            colors.add((r >> 3, g >> 3, b >> 3))
    return non_black / float(w * h), len(colors)


def capture(qmp_sock, ppm_path, png_path, label):
    qmp_screendump(qmp_sock, ppm_path)
    w, h, rgb = read_ppm(ppm_path)
    write_png(png_path, w, h, rgb)
    frac, ncolors = pixel_stats(w, h, rgb)
    print(f"\n[verify_xfce4] Captured {png_path} ({label}): {w}x{h}, "
          f"non_black={frac:.1%}, sampled_colors={ncolors}", flush=True)
    return {"w": w, "h": h, "non_black": frac, "colors": ncolors}


def latest_windows(output):
    """Returns {instance: last parsed [X11_WINDOW] dict that is viewable}."""
    wins = {}
    for m in WINDOW_RE.finditer(output):
        cls = m.group(2).decode()
        d = {
            "id": m.group(1).decode(),
            "class": cls,
            "instance": cls.split("/")[0],
            "map": m.group(3).decode(),
            "managed": m.group(4) == b"yes",
            "w": int(m.group(5)), "h": int(m.group(6)),
            "pid": m.group(9).decode(),
            "proc_state": m.group(10),
            "proc_argv0": m.group(11).decode(),
            "proc_comm": m.group(12).decode(),
        }
        if d["map"] == "viewable":
            wins[d["instance"]] = d
    return wins


def evaluate(output, shots, warnings, is_aarch64=False):
    failures = []

    def require(cond, msg):
        if not cond:
            failures.append(msg)
        return cond

    require(b"[ZX_TEST: PASS]" in output, "zx_test did not report [ZX_TEST: PASS]")
    require(b"[x11_fb_bridge] FATAL" not in output, "x11_fb_bridge reported a FATAL error")
    require(b"[x11_fb_bridge] first non-black X11 frame blitted" in output,
            "x11_fb_bridge never blitted a non-black frame to /dev/fb0")

    wm = WM_RE.findall(output)
    require(wm and wm[-1] == b"Xfwm4", f"EWMH window manager is not Xfwm4 (saw {wm})")
    desk = DESKTOP_RE.search(output)
    require(desk is not None, "x11_probe never observed the mapped desktop")

    procs = {}
    for m in PROC_RE.finditer(output):
        procs[m.group(1).decode()] = {
            "pid": m.group(2).decode(), "state": m.group(3),
            "argv0": m.group(4).decode(), "comm": m.group(5).decode()}
    for name in ("Xvfb", "x11_fb_bridge", "xfwm4", "xfdesktop", "xfce4-panel",
                 "xfce4-terminal", "terminal-shell"):
        p = procs.get(name)
        if require(p is not None, f"no [XFCE4_PROC] report for {name}"):
            require(p["state"] in LIVE_STATES,
                    f"{name} (pid {p['pid']}) is not alive: state={p['state'].decode()}")
            expected = "sh" if name == "terminal-shell" else name
            require(p["argv0"] == expected,
                    f"{name} pid {p['pid']} has argv[0] '{p['argv0']}'")
            if p["comm"] != expected[:15]:
                warnings.append(f"{name} pid {p['pid']}: /proc/pid/comm is "
                                f"'{p['comm']}' (expected '{expected[:15]}')")

    wins = latest_windows(output)
    for inst in ("xfdesktop", "xfce4-panel", "xfce4-terminal"):
        w = wins.get(inst)
        if not require(w is not None, f"no viewable {inst} window on the X server"):
            continue
        require(w["proc_state"] in LIVE_STATES and w["proc_argv0"] == inst,
                f"{inst} window's _NET_WM_PID {w['pid']} is not a live {inst} "
                f"process (state={w['proc_state'].decode()} argv0={w['proc_argv0']})")
        launched = procs.get(inst)
        if launched:
            require(w["pid"] == launched["pid"],
                    f"{inst} window belongs to pid {w['pid']}, but /init launched "
                    f"pid {launched['pid']}")
    if desk and "xfdesktop" in wins:
        sw, sh = int(desk.group(1)), int(desk.group(2))
        require(wins["xfdesktop"]["w"] >= sw and wins["xfdesktop"]["h"] >= sh,
                "xfdesktop window does not cover the screen")
    if "xfce4-terminal" in wins:
        require(wins["xfce4-terminal"]["managed"],
                "xfce4-terminal window is not reparented/managed by the WM")

    require(FOCUS_RE.search(output) is not None,
            "xfce4-terminal never received X input focus")
    inj = INJECT_RE.search(output)
    require(inj is not None and int(inj.group(1)) > 0,
            "x11_fb_bridge did not acknowledge the XTEST keystroke injection")
    term = TERM_MARKER_RE.search(output)
    require(term is not None,
            "computed marker 'XFCE_TERM_RESULT_42 stdin_tty=yes "
            "ttyname=/dev/pts/N' never reached "
            "the serial console (keyboard -> X11 -> xfce4-terminal -> PTY -> sh "
            "round-trip not proven)")
    if term is None and b"XFCE_TERM_RESULT_42" in output:
        line = re.search(rb"XFCE_TERM_RESULT_42[^\r\n]*", output).group(0)
        failures.append("terminal shell reported "
                        f"'{line.decode(errors='replace')}' (not a /dev/pts tty)")

    if not is_aarch64:
        require(PS2_MARKER_RE.search(output) is not None,
                "computed marker PS2_KBD_RESULT_63 never reached the serial console "
                "(PS/2 keyboard -> /dev/input/event0 -> x11_fb_bridge -> XTEST -> "
                "xfce4-terminal path not proven)")

    # Secondary sanity checks on the screenshots.
    d, t = shots.get("desktop"), shots.get("terminal")
    if require(d is not None, "desktop screenshot was not captured"):
        require(d["non_black"] > 0.5 and d["colors"] > 20,
                f"desktop screenshot looks blank ({d['non_black']:.1%} lit, "
                f"{d['colors']} colors)")
    if require(t is not None, "terminal screenshot was not captured"):
        require(t["non_black"] > 0.5 and t["colors"] > 20,
                f"terminal screenshot looks blank ({t['non_black']:.1%} lit, "
                f"{t['colors']} colors)")
    return failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("qemu_bin")
    parser.add_argument("boot_shim")
    parser.add_argument("zbi_image")
    parser.add_argument("--screenshot-dir")
    parser.add_argument("--timeout", type=float)
    args = parser.parse_args()

    screenshots_dir = args.screenshot_dir
    if not screenshots_dir:
        ws = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
        if not ws:
            print("[verify_xfce4] ERROR: pass --screenshot-dir or run via "
                  "`bazel run` (BUILD_WORKSPACE_DIRECTORY unset)", file=sys.stderr)
            return 2
        screenshots_dir = os.path.join(ws, "docs", "screenshots")
    os.makedirs(screenshots_dir, exist_ok=True)

    tmpdir = os.environ.get("TEST_TMPDIR") or "/tmp"
    tag = f"zircon_xfce4_{os.getpid()}"
    qmp_sock = os.path.join(tmpdir, f"{tag}_qmp.sock")
    desktop_ppm = os.path.join(tmpdir, f"{tag}_desktop.ppm")
    terminal_ppm = os.path.join(tmpdir, f"{tag}_terminal.ppm")
    is_aarch64 = "aarch64" in os.path.basename(args.qemu_bin)
    if args.timeout is None:
        args.timeout = 900.0 if is_aarch64 else 300.0
    suffix = "_arm64" if is_aarch64 else ""
    desktop_png = os.path.join(screenshots_dir, f"xfce4_desktop{suffix}.png")
    terminal_png = os.path.join(screenshots_dir, f"xfce4_terminal{suffix}.png")

    if is_aarch64:
        cmd = [
            args.qemu_bin,
            "-machine", "virt,highmem=off",
            "-cpu", "cortex-a57",
            "-m", "2048M",
            "-smp", "4",
            "-device", "bochs-display",
            "-display", "none",
            "-serial", "stdio",
            "-no-reboot",
            "-qmp", f"unix:{qmp_sock},server,nowait",
            "-kernel", args.boot_shim,
            "-initrd", args.zbi_image,
            "-append",
            "TERM=dumb kernel.halt-on-panic=true "
            "kernel.vdso.always_use_next=true userboot.next=bin/zircon_linux_init",
        ]
    else:
        cmd = [
            args.qemu_bin,
            "-machine", "q35,smbios-entry-point-type=32",
            "-cpu", "host,migratable=no,+invtsc",
            "-enable-kvm",
            "-m", "2048M",
            "-smp", "4",
            "-vga", "std",
            "-display", "none",
            "-serial", "stdio",
            "-no-reboot",
            "-qmp", f"unix:{qmp_sock},server,nowait",
            "-kernel", args.boot_shim,
            "-initrd", args.zbi_image,
            "-append",
            "TERM=dumb kernel.serial=legacy kernel.halt-on-panic=true "
            "kernel.vdso.always_use_next=true userboot.next=bin/zircon_linux_init",
        ]

    print(f"[verify_xfce4] Launching QEMU: {' '.join(cmd)}", flush=True)
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)

    output = b""
    shots = {}
    errors = []
    start = time.time()
    deadline = start + args.timeout
    desktop_at = None   # time at which to take the desktop screenshot
    terminal_at = None  # time at which to take the terminal screenshot
    fail_seen_at = None
    ps2_sent = None     # time at which PS/2 typing finished
    stop_reason = "timeout"

    try:
        while True:
            now = time.time()
            if now >= deadline:
                stop_reason = f"timeout after {args.timeout:.0f}s"
                break
            rlist, _, _ = select.select([proc.stdout], [], [], 0.2)
            if rlist:
                chunk = os.read(proc.stdout.fileno(), 65536)
                if not chunk:
                    stop_reason = f"QEMU exited (rc={proc.poll()})"
                    break
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
                output += chunk

            if fail_seen_at is None and FAILED_RE.search(output):
                fail_seen_at = now  # keep reading briefly to collect guest logs
            if fail_seen_at is not None and now - fail_seen_at > 5.0:
                stop_reason = "guest reported failure"
                break

            if desktop_at is None and b"[XFCE4_DESKTOP: READY]" in output:
                desktop_at = now + 3.0  # let xfdesktop finish painting icons
            if "desktop" not in shots and desktop_at and now >= desktop_at:
                try:
                    shots["desktop"] = capture(qmp_sock, desktop_ppm, desktop_png, "desktop")
                except (QmpError, OSError, ValueError) as e:
                    errors.append(f"desktop screenshot failed: {e}")
                    shots["desktop"] = None

            # Start the PS/2 stage once the terminal shell has answered at all
            # (the strict marker is evaluated afterwards). On AArch64 virt there
            # is no i8042 PS/2 controller, so we capture the terminal screenshot
            # directly after XFCE_TERM_RESULT_42.
            if ps2_sent is None and b"XFCE_TERM_RESULT_42" in output:
                if is_aarch64:
                    ps2_sent = time.time()
                    terminal_at = time.time() + 3.0
                else:
                    time.sleep(1.0)  # let the shell return to its prompt
                    try:
                        qmp_type_ps2(qmp_sock, PS2_COMMAND)
                    except (QmpError, OSError, ValueError) as e:
                        errors.append(f"PS/2 typing via QMP failed: {e}")
                    ps2_sent = time.time()
            if not is_aarch64 and terminal_at is None and ps2_sent and (
                    PS2_MARKER_RE.search(output) or time.time() - ps2_sent > 30):
                terminal_at = time.time() + 2.0  # let VTE render the output
            if terminal_at and now >= terminal_at:
                try:
                    shots["terminal"] = capture(qmp_sock, terminal_ppm, terminal_png, "terminal")
                except (QmpError, OSError, ValueError) as e:
                    errors.append(f"terminal screenshot failed: {e}")
                    shots["terminal"] = None
                stop_reason = ("terminal round-trips observed"
                               if (is_aarch64 or PS2_MARKER_RE.search(output)) else
                               "PS/2 keyboard marker not seen within 30s")
                break
        if not shots.get("terminal") and proc.poll() is None:
            # Diagnostic screenshot of whatever is on screen at failure time.
            fail_png = os.path.join(tmpdir, f"{tag}_failure.png")
            try:
                capture(qmp_sock, terminal_ppm, fail_png, "failure diagnostics")
            except (QmpError, OSError, ValueError) as e:
                print(f"[verify_xfce4] failure screenshot not captured: {e}", flush=True)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        for p in (qmp_sock, desktop_ppm, terminal_ppm):
            if os.path.exists(p):
                os.unlink(p)

    elapsed = time.time() - start
    print(f"\n[verify_xfce4] Stopped after {elapsed:.1f}s: {stop_reason}", flush=True)
    warnings = []
    failures = errors + evaluate(output, {k: v for k, v in shots.items() if v},
                                 warnings, is_aarch64=is_aarch64)
    for w in warnings:
        print(f"[verify_xfce4] WARNING: {w}", flush=True)
    guest_fail = FAILED_RE.search(output)
    if guest_fail:
        failures.insert(0, "guest: " + guest_fail.group(0).decode(errors="replace"))

    if failures:
        print("\n[verify_xfce4] FAIL:", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    print("\n================================================================")
    print("[verify_xfce4] PASS: Zircon + Linux Shim + Xfce4 Desktop + Terminal OK!")
    print(f"[verify_xfce4]   terminal round-trip: {TERM_MARKER_RE.search(output).group(0).decode()}")
    if not is_aarch64:
        print(f"[verify_xfce4]   PS/2 keyboard path:  {PS2_MARKER_RE.search(output).group(0).decode()}")
    print(f"[verify_xfce4] Screenshots saved to: {screenshots_dir}")
    print("================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
