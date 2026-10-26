#!/usr/bin/env python3
"""Scan publishable files, including gzip data and local LFS payloads.

Run before the initial commit. Findings report paths and categories without
printing matched private data. Upstream attribution is retained.
"""
import argparse
import gzip
import os
from pathlib import Path
import re
import socket
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument(
        "--deny-file",
        type=Path,
        required=True,
        help="Private UTF-8 file with one forbidden identity/token per line; keep outside the repository",
    )
    args = parser.parse_args()
    root = args.root.resolve()
    if not root.is_dir():
        parser.error("publication root is not a directory")
    patterns = {
        "local home path": rb"/(?:home|Users)/[a-zA-Z0-9_.@-]+/",
        "private workstation path": rb"/usr/loca[l]?/google/",
    }
    deny_file = args.deny_file.resolve()
    if deny_file.is_relative_to(root):
        parser.error("identity deny file must be outside the publication tree")
    private_tokens = [
        line.strip().encode()
        for line in deny_file.read_text().splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]
    if not private_tokens:
        parser.error("identity deny file contains no tokens")
    hostname = socket.gethostname().encode()
    if len(hostname) > 3:
        # Plain words can also be legitimate project/domain terminology.
        prefix = rb"(?:@|(?:build[ -]host|hostname)[=: ]+)"
        if any(c in hostname for c in b"0123456789.-"):
            prefix = rb"(?<![a-z0-9_.-])"
        patterns["current build hostname"] = (
            prefix + re.escape(hostname) + rb"(?![a-z0-9_.-])"
        )
    # Fixed-string prefilters keep large ELF scans linear and inexpensive.
    prefilters = {
        "local home path": (b"/home/", b"/users/"),
        "private workstation path": (
            b"/usr/" + b"loca/google/",
            b"/usr/" + b"local/google/",
        ),
        "current build hostname": (hostname.lower(),),
    }
    for number, token in enumerate(private_tokens, 1):
        category = f"private token {number}"
        patterns[category] = re.escape(token)
        prefilters[category] = (token.lower(),)
    wide_tokens = [
        (encoding, token.decode().encode(encoding).lower())
        for token in private_tokens
        for encoding in ("utf-16le", "utf-16be", "utf-32le", "utf-32be")
    ]
    compiled = {k: re.compile(v, re.I) for k, v in patterns.items()}
    if (root / ".git").exists():
        names = subprocess.check_output(
            ["git", "-C", str(root), "ls-files", "-z"]
        ).split(b"\0")
        paths = [root / os.fsdecode(n) for n in names if n]
    else:
        paths = []
        for directory, dirs, files in os.walk(root):
            paths.extend(
                Path(directory) / d
                for d in dirs
                if d not in (".git", "__pycache__")
                and not d.startswith("bazel-")
                and (Path(directory) / d).is_symlink()
            )
            dirs[:] = [
                d
                for d in dirs
                if d not in (".git", "__pycache__")
                and not d.startswith("bazel-")
                and not (Path(directory) / d).is_symlink()
            ]
            paths.extend(
                Path(directory) / f
                for f in files
                if f != "user.bazelrc" and not f.endswith(".pyc")
            )
    if not paths:
        parser.error("no publishable files found; stage files first when using Git")
    failures = []
    total = 0
    for path in sorted(paths):
        if path.is_symlink():
            failures.append((str(path.relative_to(root)), "unreviewed symlink"))
            continue
        data = path.read_bytes()
        if path.suffix == ".gz":
            data += gzip.decompress(data)
        data = os.fsencode(path.relative_to(root)) + b"\0" + data
        lowered = data.lower()
        for encoding, token in wide_tokens:
            if token in lowered:
                failures.append(
                    (
                        str(path.relative_to(root)),
                        f"private token encoded as {encoding}",
                    )
                )
        for category, pattern in compiled.items():
            candidate = lowered
            if category == "local home path":
                # Upstream browser URLs, verified in the bundled Chromium.
                candidate = candidate.replace(
                    b"https://www.quad9.net/home/privacy/", b""
                )
                candidate = candidate.replace(b"/cast/chromecast/home/wallpaper/", b"")
                candidate = candidate.replace(b"/v1/users/me/", b"")
            found = False
            for token in prefilters[category]:
                start = 0
                while not found:
                    pos = candidate.find(token, start)
                    if pos < 0:
                        break
                    window = candidate[max(0, pos - 128) : pos + len(token) + 256]
                    if category == "local home path":
                        window = re.sub(rb"https?://[!-~]+", b"", window)
                    found = bool(pattern.search(window))
                    start = pos + len(token)
                if found:
                    failures.append((str(path.relative_to(root)), category))
                    break
        total += 1
    for name, category in failures:
        for token in private_tokens:
            name = re.sub(re.escape(token.decode()), "[redacted]", name, flags=re.I)
        name = re.sub(r"/(?:home|Users)/[^/]+/", "/[home-path]/", name, flags=re.I)
        print(f"FAIL {name}: {category}")
    print(f"Scanned {total} files; {len(failures)} findings")
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
