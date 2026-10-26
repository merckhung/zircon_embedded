#!/usr/bin/env python3
"""Builds a POSIX ustar rootfs.tar from Alpine x86_64 or ARM64 .apk packages and custom binaries."""

import argparse
import io
import os
import tarfile

BUSYBOX_APPLETS = [
    "sh", "ash", "ls", "cat", "echo", "mkdir", "rm", "rmdir", "cp", "mv",
    "ln", "chmod", "chown", "ps", "kill", "sleep", "usleep", "uname", "id",
    "pwd", "env", "grep", "egrep", "fgrep", "sed", "awk", "head", "tail",
    "wc", "sort", "uniq", "tr", "cut", "find", "xargs", "tar", "gzip",
    "gunzip", "zcat", "df", "du", "free", "uptime", "dmesg", "mount",
    "umount", "hostname", "date", "touch", "stat", "readlink", "realpath",
    "dirname", "basename", "expr", "test", "[", "[[", "true", "false",
    "which", "nohup", "setsid", "tty", "clear", "reset", "printf", "seq",
    "tee", "yes", "whoami", "logname", "md5sum", "sha256sum", "hexdump",
    "od", "strings", "cmp", "diff", "patch", "vi", "less", "more",
]

DBUS_SESSION_CONF = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <keep_umask/>
  <listen>unix:path=/tmp/dbus-session.sock</listen>
  <standard_session_servicedirs />
  <servicedir>/usr/share/dbus-1/services</servicedir>
  <auth>EXTERNAL</auth>
  <auth>ANONYMOUS</auth>
  <allow_anonymous/>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
    <allow user="*"/>
  </policy>
</busconfig>
"""

DBUS_SYSTEM_CONF = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>system</type>
  <user>root</user>
  <listen>unix:path=/run/dbus/system_bus_socket</listen>
  <auth>EXTERNAL</auth>
  <auth>ANONYMOUS</auth>
  <allow_anonymous/>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
    <allow user="*"/>
  </policy>
</busconfig>
"""

XFCE4_TERMINALRC = """[Configuration]
ColorForeground=#e6edf3
ColorBackground=#0d1117
ColorCursor=#58a6ff
FontName=Monospace 11
MiscAlwaysShowTabs=FALSE
MiscBell=FALSE
MiscBordersDefault=TRUE
MiscCursorBlinks=FALSE
MiscDefaultGeometry=84x24
MiscMenubarDefault=TRUE
MiscToolbarDefault=FALSE
ScrollingBar=SCROLLBAR_RIGHT
TitleInitial=Zircon Embedded Terminal (Linux Shim on Zircon)
"""


def add_bytes_file(out_tar, path, data_bytes, mode=0o644):
    ti = tarfile.TarInfo(name=path.lstrip("/"))
    ti.type = tarfile.REGTYPE
    ti.mode = mode
    ti.uid = 0
    ti.gid = 0
    ti.size = len(data_bytes)
    ti.mtime = 0
    out_tar.addfile(ti, io.BytesIO(data_bytes))


def add_dir(out_tar, path, mode=0o755):
    ti = tarfile.TarInfo(name=path.lstrip("/"))
    ti.type = tarfile.DIRTYPE
    ti.mode = mode
    ti.uid = 0
    ti.gid = 0
    ti.mtime = 0
    out_tar.addfile(ti)


# Directories every rootfs needs at boot. The shim's VFS only provides the
# kernel-ish mount points (/dev, /dev/pts, /dev/shm, /dev/input, /proc, /sys,
# /tmp); everything else must come from the rootfs image (or /init).
BASE_DIRS = [
    ("bin", 0o755),
    ("sbin", 0o755),
    ("lib", 0o755),
    ("usr", 0o755),
    ("usr/bin", 0o755),
    ("usr/sbin", 0o755),
    ("usr/lib", 0o755),
    ("usr/share", 0o755),
    ("tmp", 0o1777),
    ("run", 0o755),
    ("var", 0o755),
    ("var/run", 0o755),
    ("var/lib", 0o755),
    ("var/log", 0o755),
    ("root", 0o700),
    ("etc", 0o755),
]

XFCE4_DIRS = [
    ("tmp/.X11-unix", 0o1777),
    ("tmp/.ICE-unix", 0o1777),
    ("run/dbus", 0o755),
    ("var/run/dbus", 0o755),
    ("var/lib/dbus", 0o755),
    ("root/.config", 0o700),
    ("root/.cache", 0o700),
]


def add_symlink(out_tar, path, target):
    ti = tarfile.TarInfo(name=path.lstrip("/"))
    ti.type = tarfile.SYMTYPE
    ti.mode = 0o777
    ti.uid = 0
    ti.gid = 0
    ti.linkname = target
    ti.mtime = 0
    out_tar.addfile(ti)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--init", required=True)
    parser.add_argument("--extra-bin", action="append", default=[])
    parser.add_argument("--extra-file", action="append", default=[])
    parser.add_argument("--extra-symlink", action="append", default=[])
    parser.add_argument("--xfce4-config", action="store_true")
    parser.add_argument("apks", nargs="*")
    args = parser.parse_args()

    seen_paths = set()

    with tarfile.open(args.output, "w", format=tarfile.GNU_FORMAT) as out_tar:
        for d, mode in BASE_DIRS + (XFCE4_DIRS if args.xfce4_config else []):
            add_dir(out_tar, d, mode)
            seen_paths.add(d)

        for apk_path in args.apks:
            with tarfile.open(apk_path, "r:gz") as apk:
                for member in apk.getmembers():
                    name = member.name.lstrip("./")
                    if not name or name.startswith(".SIGN") or name in (
                        ".PKGINFO",
                        ".trigger",
                        ".post-install",
                        ".pre-install",
                        ".post-upgrade",
                        ".pre-upgrade",
                    ):
                        continue
                    member.name = name
                    member.uid = 0
                    member.gid = 0
                    member.uname = "root"
                    member.gname = "root"
                    if member.isfile():
                        fobj = apk.extractfile(member)
                        out_tar.addfile(member, fobj)
                    elif member.issym() or member.islnk() or member.isdir():
                        if member.islnk():
                            member.linkname = member.linkname.lstrip("./")
                        out_tar.addfile(member)
                    seen_paths.add(name)

        # Populate BusyBox applet symlinks if /bin/busybox is present
        if "bin/busybox" in seen_paths:
            for applet in BUSYBOX_APPLETS:
                bin_path = f"bin/{applet}"
                usr_bin_path = f"usr/bin/{applet}"
                if bin_path not in seen_paths and usr_bin_path not in seen_paths:
                    add_symlink(out_tar, bin_path, "/bin/busybox")
                    seen_paths.add(bin_path)

        # Install /init script
        with open(args.init, "rb") as f:
            init_bytes = f.read()
        add_bytes_file(out_tar, "init", init_bytes, mode=0o755)
        seen_paths.add("init")

        # Install extra binaries (--extra-bin dest=src)
        def ensure_parent_dirs(dest_path):
            parts = dest_path.split("/")[:-1]
            cur = ""
            for p in parts:
                if not p:
                    continue
                cur = f"{cur}/{p}" if cur else p
                if cur not in seen_paths:
                    add_dir(out_tar, cur, mode=0o755)
                    seen_paths.add(cur)

        for item in args.extra_bin:
            dest, src = item.split("=", 1)
            dest = dest.lstrip("/")
            ensure_parent_dirs(dest)
            with open(src, "rb") as f:
                b = f.read()
            add_bytes_file(out_tar, dest, b, mode=0o755)
            seen_paths.add(dest)

        for item in args.extra_file:
            dest, src = item.split("=", 1)
            dest = dest.lstrip("/")
            ensure_parent_dirs(dest)
            with open(src, "rb") as f:
                b = f.read()
            add_bytes_file(out_tar, dest, b, mode=0o644)
            seen_paths.add(dest)

        for item in args.extra_symlink:
            dest, target = item.split("=", 1)
            dest = dest.lstrip("/")
            ensure_parent_dirs(dest)
            add_symlink(out_tar, dest, target)
            seen_paths.add(dest)

        # Install /etc essentials
        add_bytes_file(
            out_tar,
            "etc/passwd",
            b"root:x:0:0:root:/root:/bin/sh\n"
            b"messagebus:x:100:100:messagebus:/var/run/dbus:/sbin/nologin\n"
            b"nobody:x:65534:65534:nobody:/:/sbin/nologin\n",
            mode=0o644,
        )
        add_bytes_file(
            out_tar,
            "etc/group",
            b"root:x:0:root\n"
            b"messagebus:x:100:\n"
            b"video:x:27:root\n"
            b"input:x:999:root\n"
            b"nogroup:x:65534:\n",
            mode=0o644,
        )
        add_bytes_file(out_tar, "etc/hostname", b"zircon-embedded\n", mode=0o644)
        add_bytes_file(
            out_tar,
            "etc/hosts",
            b"127.0.0.1\tlocalhost zircon-embedded\n",
            mode=0o644,
        )
        add_bytes_file(
            out_tar,
            "etc/nsswitch.conf",
            b"passwd: files\ngroup: files\nhosts: files dns\n",
            mode=0o644,
        )
        add_bytes_file(
            out_tar,
            "etc/resolv.conf",
            b"nameserver 10.0.2.3\n",
            mode=0o644,
        )
        # NOTE: /etc/machine-id is intentionally NOT shipped: it must be unique
        # per boot/instance, so /init generates it (dbus-uuidgen or
        # /dev/urandom).

        if args.xfce4_config:
            add_bytes_file(
                out_tar,
                "etc/dbus-1/session-zircon.conf",
                DBUS_SESSION_CONF.encode("utf-8"),
                mode=0o644,
            )
            add_bytes_file(
                out_tar,
                "etc/dbus-1/system-zircon.conf",
                DBUS_SYSTEM_CONF.encode("utf-8"),
                mode=0o644,
            )
            add_bytes_file(
                out_tar,
                "root/.config/xfce4/terminal/terminalrc",
                XFCE4_TERMINALRC.encode("utf-8"),
                mode=0o644,
            )


if __name__ == "__main__":
    main()
