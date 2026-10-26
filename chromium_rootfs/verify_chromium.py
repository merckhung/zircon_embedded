#!/usr/bin/env python3
"""End-to-end QEMU verification + QMP screenshot of //chromium_rootfs:xfce4_chromium_system.

Usage: verify_chromium.py <qemu> <boot_shim> <zbi> [--screenshot-dir DIR]
                          [--timeout SECONDS]

Boots Zircon + Linux Shim + Xfce4 Desktop + Prebuilt Chromium on QEMU with
4096 MB RAM and a Realtek RTL8139 PCI NIC (`-netdev user,id=net0 -device rtl8139,netdev=net0`),
verifies that the Xfce4 desktop and Chromium window are viewable on the X11
server, verifies real network packets on `eth0` in `/proc/net/dev`, and
captures `docs/screenshots/xfce4_chromium_yahoo.png`.
"""

import argparse
import json
import os
import re
import select
import secrets
import socket
import struct
import subprocess
import sys
import time
import zlib

WINDOW_RE = re.compile(
    rb"\[X11_WINDOW\] id=(0x[0-9a-f]+) class=(.+?) map=(\w+) managed=(\w+) "
    rb"frame=0x[0-9a-f]+ geom=(\d+)x(\d+)\+(-?\d+)\+(-?\d+) pid=(\S+) "
    rb"proc_state=(\S) proc_argv0=(\S+) proc_comm=(\S+) name=\"([^\"]*)\""
)
PROC_RE = re.compile(
    rb"\[XFCE4_PROC\] name=(\S+) pid=(\d+) state=(\S+) argv0=(\S*) comm=(\S*)"
)
DESKTOP_RE = re.compile(rb"\[X11_PROBE\] DESKTOP_MAPPED .*screen=(\d+)x(\d+)")
WM_RE = re.compile(rb'\[X11_WM\] check_window=0x[0-9a-f]+ name="([^"]*)"')
NET_RE = re.compile(rb"eth0:\s*(\d+)\s+(\d+)\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+(\d+)\s+(\d+)")
FAILED_RE = re.compile(rb"\[CHROMIUM_ROOTFS: FAILED\][^\r\n]*")
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


def read_ppm(ppm_path):
    with open(ppm_path, "rb") as f:
        data = f.read()
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
    print(f"\n[verify_chromium] Captured {png_path} ({label}): {w}x{h}, "
          f"non_black={frac:.1%}, sampled_colors={ncolors}", flush=True)
    return {"w": w, "h": h, "non_black": frac, "colors": ncolors}


def latest_windows(output):
    wins = {}
    for m in WINDOW_RE.finditer(output):
        cls = m.group(2).decode()
        d = {
            "id": m.group(1).decode(),
            "class": cls,
            "instance": cls.split("/")[0],
            "map": m.group(3).decode(),
            "managed": m.group(4) == b"yes",
            "w": int(m.group(5)),
            "h": int(m.group(6)),
            "pid": m.group(9).decode(),
            "proc_state": m.group(10),
            "proc_argv0": m.group(11).decode(),
            "proc_comm": m.group(12).decode(),
            "name": m.group(13).decode(errors="replace"),
        }
        if d["map"] == "viewable":
            wins[d["instance"]] = d
            if d["proc_argv0"] and d["proc_argv0"] != "-":
                wins[d["proc_argv0"]] = d
            if "chrom" in cls.lower():
                wins["chrome"] = d
    return wins


def page_observations(output):
    observations = []
    for line in output.splitlines():
        if line.startswith(b"[CHROMIUM_PAGE] "):
            try:
                record = json.loads(line[len(b"[CHROMIUM_PAGE] "):])
                if isinstance(record, dict):
                    observations.append(record)
            except (ValueError, UnicodeError):
                continue
    return observations


def renderer_problems(output, challenge):
    nonce, a, b = challenge
    matches = [row for row in page_observations(output) if row.get("nonce") == nonce]
    if not matches:
        return ["no renderer response to the fresh host challenge"]
    row = matches[-1]
    failures = []
    if type(row.get("answer")) not in (int, float) or row["answer"] != a * b:
        failures.append("renderer returned the wrong arithmetic answer")
    for key in ("href", "uri"):
        url = row.get(key, "")
        if not isinstance(url, str) or not url.startswith(("https://www.yahoo.com/", "https://yahoo.com/")):
            failures.append(f"renderer {key} is not the Yahoo HTTPS document")
    if row.get("ready") != "complete" or row.get("secure") is not True:
        failures.append("renderer document is incomplete or not a secure context")
    if type(row.get("http_status")) is not int or not 200 <= row["http_status"] < 300:
        failures.append("renderer navigation did not receive a successful HTTP response")
    title = row.get("title")
    if not isinstance(title, str) or "Yahoo" not in title:
        failures.append("renderer document title does not identify Yahoo")
    if type(row.get("text_length")) is not int or row["text_length"] < 200:
        failures.append("renderer document contains insufficient visible text")
    return failures


def evaluate(output, shots, challenge):
    failures = renderer_problems(output, challenge)

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
            "pid": m.group(2).decode(),
            "state": m.group(3),
            "argv0": m.group(4).decode(),
            "comm": m.group(5).decode(),
        }
    for name in ("Xvfb", "x11_fb_bridge", "xfwm4", "xfdesktop", "xfce4-panel", "chrome"):
        p = procs.get(name)
        if require(p is not None, f"no [XFCE4_PROC] report for {name}"):
            require(p["state"] in LIVE_STATES,
                    f"{name} (pid {p['pid']}) is not alive: state={p['state'].decode()}")

    wins = latest_windows(output)
    for inst in ("xfdesktop", "xfce4-panel", "chrome"):
        w = wins.get(inst)
        require(w is not None, f"no viewable {inst} window on the X server")

    require(b"[CHROMIUM_YAHOO: LOADED]" in output,
            "Chromium did not finish loading www.yahoo.com ([CHROMIUM_YAHOO: LOADED] not seen)")
    chrome_w = wins.get("chrome")
    if chrome_w:
        require("yahoo" in chrome_w["name"].lower(),
                f"Chromium window title does not contain Yahoo: '{chrome_w['name']}'")

    net_matches = list(NET_RE.finditer(output))
    require(len(net_matches) > 0, "no eth0 statistics found in /proc/net/dev")
    if net_matches:
        last_net = net_matches[-1]
        rx_bytes = int(last_net.group(1))
        rx_pkts = int(last_net.group(2))
        tx_bytes = int(last_net.group(3))
        tx_pkts = int(last_net.group(4))
        require(rx_bytes > 50000 and tx_bytes > 5000,
                f"eth0 did not exchange enough HTTPS traffic for www.yahoo.com (rx={rx_bytes}B/{rx_pkts}pkts, tx={tx_bytes}B/{tx_pkts}pkts)")

    c_shot = shots.get("chromium")
    if require(c_shot is not None, "chromium screenshot was not captured"):
        require(c_shot["non_black"] > 0.5 and c_shot["colors"] > 20,
                f"chromium screenshot looks blank ({c_shot['non_black']:.1%} lit, {c_shot['colors']} colors)")

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
            print("[verify_chromium] ERROR: pass --screenshot-dir or run via "
                  "`bazel run` (BUILD_WORKSPACE_DIRECTORY unset)", file=sys.stderr)
            return 2
        screenshots_dir = os.path.join(ws, "docs", "screenshots")
    os.makedirs(screenshots_dir, exist_ok=True)

    tmpdir = os.environ.get("TEST_TMPDIR") or "/tmp"
    tag = f"zircon_chromium_{os.getpid()}"
    qmp_sock = os.path.join(tmpdir, f"{tag}_qmp.sock")
    chromium_ppm = os.path.join(tmpdir, f"{tag}_chromium.ppm")
    is_aarch64 = "aarch64" in os.path.basename(args.qemu_bin)
    if args.timeout is None:
        args.timeout = 900.0 if is_aarch64 else 300.0
    challenge_timeout = 120.0 if is_aarch64 else 25.0
    suffix = "_arm64" if is_aarch64 else ""
    chromium_png = os.path.join(screenshots_dir, f"xfce4_chromium_yahoo{suffix}.png")

    if is_aarch64:
        cmd = [
            args.qemu_bin,
            "-machine", "virt,highmem=off",
            "-cpu", "cortex-a57",
            "-m", "3072M",
            "-smp", "4",
            "-device", "bochs-display",
            "-netdev", "user,id=net0",
            "-device", "rtl8139,netdev=net0",
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
            "-m", "8192M",
            "-smp", "4",
            "-vga", "std",
            "-netdev", "user,id=net0",
            "-device", "rtl8139,netdev=net0",
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

    print(f"[verify_chromium] Launching QEMU: {' '.join(cmd)}", flush=True)
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)

    challenge = (secrets.token_hex(16), secrets.randbelow(60000) + 2, secrets.randbelow(60000) + 2)
    challenge_sent_at = None
    output = b""
    shots = {}
    errors = []
    start = time.time()
    deadline = start + args.timeout
    fail_seen_at = None
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
                fail_seen_at = now
            if fail_seen_at is not None and now - fail_seen_at > 5.0:
                stop_reason = "guest reported failure"
                break

            if b"[CHROMIUM_READY]" in output and challenge_sent_at is None:
                nonce, a, b = challenge
                command = f"/bin/chromium_page_probe {nonce} {a} {b}\n"
                proc.stdin.write(command.encode())
                proc.stdin.flush()
                challenge_sent_at = now
                print("[verify_chromium] Sent fresh renderer challenge", flush=True)

            responded = any(row.get("nonce") == challenge[0] for row in page_observations(output))
            if challenge_sent_at is not None and responded:
                try:
                    shots["chromium"] = capture(qmp_sock, chromium_ppm, chromium_png, "chromium_yahoo")
                except (QmpError, OSError, ValueError) as e:
                    errors.append(f"chromium screenshot failed: {e}")
                    shots["chromium"] = None
                stop_reason = "renderer challenge answered and screenshot captured"
                break
            if challenge_sent_at is not None and now - challenge_sent_at > challenge_timeout:
                errors.append(f"guest did not answer fresh renderer challenge within {challenge_timeout:.0f} seconds")
                stop_reason = "renderer challenge timed out"
                break

        if not shots.get("chromium") and proc.poll() is None:
            fail_png = os.path.join(tmpdir, f"{tag}_failure.png")
            try:
                capture(qmp_sock, chromium_ppm, fail_png, "failure diagnostics")
            except (QmpError, OSError, ValueError) as e:
                print(f"[verify_chromium] failure screenshot not captured: {e}", flush=True)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
        for p in (qmp_sock, chromium_ppm):
            if os.path.exists(p):
                os.unlink(p)

    elapsed = time.time() - start
    print(f"\n[verify_chromium] Stopped after {elapsed:.1f}s: {stop_reason}", flush=True)
    failures = errors + evaluate(output, {k: v for k, v in shots.items() if v}, challenge)
    guest_fail = FAILED_RE.search(output)
    if guest_fail:
        failures.insert(0, "guest: " + guest_fail.group(0).decode(errors="replace"))

    if failures:
        print("\n[verify_chromium] FAIL:", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1

    wins = latest_windows(output)
    chrome_win = wins.get("chrome", {})
    net_matches = list(NET_RE.finditer(output))
    last_net = net_matches[-1]
    print("\n================================================================")
    print("[verify_chromium] PASS: Zircon + Linux Shim + Xfce4 + Chromium OK!")
    print(f"[verify_chromium]   Chromium X11 window: id={chrome_win.get('id')} "
          f"geom={chrome_win.get('w')}x{chrome_win.get('h')} name=\"{chrome_win.get('name')}\"")
    print(f"[verify_chromium]   RTL8139 eth0 traffic: RX={last_net.group(1)}B ({last_net.group(2)} pkts), "
          f"TX={last_net.group(3)}B ({last_net.group(4)} pkts)")
    print(f"[verify_chromium]   Screenshot saved to: {chromium_png}")
    print("================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
