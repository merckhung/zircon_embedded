#!/usr/bin/env python3
"""Package executable Debian runtimes matching Chromium's pinned link sysroot."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent


def needed_libraries(path):
    output = subprocess.check_output(["readelf", "-Wd", str(path)], text=True)
    return re.findall(r"Shared library: \[([^\]]+)\]", output)


def validate_runtime_elf(path, arch):
    data = path.read_bytes()
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("invalid runtime ELF: " + path.name)
    if struct.unpack_from("<H", data, 18)[0] != (62 if arch == "x64" else 183):
        raise ValueError("wrong runtime architecture: " + path.name)
    entry = struct.unpack_from("<Q", data, 24)[0]
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    executable = []
    if size != 56 or offset + size * count > len(data):
        raise ValueError("invalid runtime program headers: " + path.name)
    for index in range(count):
        kind, flags, file_offset, address, _, file_size, _, _ = struct.unpack_from(
            "<IIQQQQQQ", data, offset + size * index
        )
        if kind == 1 and flags & 1 and file_size:
            if file_offset + file_size > len(data):
                raise ValueError("truncated executable segment: " + path.name)
            executable.append((address, address + file_size))
    if not executable or (
        entry and not any(start <= entry < end for start, end in executable)
    ):
        raise ValueError(
            "runtime has no executable code at its entry; link-time stubs cannot run: "
            + path.name
        )


def fields(path):
    result = {}
    for line in path.read_text().splitlines():
        if ": " in line and not line.startswith(" "):
            key, value = line.split(": ", 1)
            result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--arch", choices=("x64", "arm64"), required=True)
    parser.add_argument("--package-cache", type=Path, required=True)
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    cache = args.package_cache.resolve()
    if cache.is_relative_to(ROOT.parent):
        parser.error("package cache must be outside the publication tree")
    cache.mkdir(parents=True, exist_ok=True)
    pins = {
        row["package"]: row
        for row in json.loads((ROOT / "runtime-packages.json").read_text())
        if row["architecture"] == args.arch
    }
    arch = "amd64" if args.arch == "x64" else "arm64"
    config = json.loads(
        (checkout / "build/linux/sysroot_scripts/sysroots.json").read_text()
    )["bullseye_" + arch]
    source_root = checkout / "build/linux" / config["SysrootDir"]
    expected_url = config["URL"] + "/" + config["Sha256Sum"]
    stamp = source_root / ".stamp"
    if not stamp.exists() or stamp.read_text().strip() != expected_url:
        parser.error("run install-sysroot.py for this architecture before packaging")
    destination = ROOT / ("prebuilt" if args.arch == "x64" else "prebuilt_arm64")
    names = sorted(p.name for p in (destination / "lib").iterdir() if ".so" in p.name)
    # Chromium's X11 cursor loader opens this by name at runtime, outside the
    # DT_NEEDED graph. Keep its glibc ABI separate from the Alpine desktop.
    names = sorted(set(names) | {'libXcursor.so.1'})
    by_name = {}
    for directory, _, files in os.walk(source_root):
        if "/debian/" in directory:
            continue
        for name in files:
            if ".so" in name:
                path = (Path(directory) / name).resolve()
                if path.is_relative_to(source_root) and path.is_file():
                    by_name[name] = path
    # Sysroot stubs retain DT_NEEDED metadata, so use it to select the entire
    # runtime dependency closure before extracting executable package payloads.
    pending = list(names)
    selected = set()
    while pending:
        name = pending.pop()
        if name in selected:
            continue
        source = by_name.get(name)
        if source is None:
            parser.error("runtime dependency missing from build sysroot: " + name)
        selected.add(name)
        pending.extend(needed_libraries(source))
    names = sorted(selected)
    owners = {}
    for sums in (source_root / "debian").glob("*/DEBIAN/md5sums"):
        for line in sums.read_text().splitlines():
            pieces = line.split(None, 1)
            if len(pieces) == 2:
                owners[pieces[1]] = sums.parent / "control"
    urls = (
        (
            checkout
            / "build/linux/sysroot_scripts/generated_package_lists"
            / ("bullseye." + arch)
        )
        .read_text()
        .splitlines()
    )
    entries = []
    # Validate the full selection before replacing any existing library.
    for name in names:
        source = by_name.get(name)
        if source is None:
            parser.error("missing runtime library: " + name)
        relative = source.relative_to(source_root).as_posix()
        control = owners.get(relative)
        if control is None:
            parser.error("no Debian package owner for " + relative)
        package = fields(control)
        version = package["Version"].split(":")[-1]
        suffix = "/" + package["Package"] + "_" + version + "_" + arch + ".deb"
        matches = [url for url in urls if url.endswith(suffix)]
        if len(matches) != 1:
            parser.error("no unique package URL for " + package["Package"])
        entries.append(
            {
                "file": "lib/" + name,
                "sysroot_file": relative,
                "package": package["Package"],
                "version": package["Version"],
                "source_package": package.get("Source", package["Package"]),
                "package_url": matches[0],
            }
        )
    with tempfile.TemporaryDirectory(prefix="runtime-extract-", dir=cache) as temporary:
        extracted = {}
        for entry in entries:
            package = entry["package"]
            pin = pins[package]
            if pin["version"] != entry["version"] or pin["url"] != entry["package_url"]:
                parser.error(
                    "runtime package pin differs from link sysroot: " + package
                )
            if package not in extracted:
                archive = cache / pin["url"].rsplit("/", 1)[-1]
                if not archive.exists():
                    data = urllib.request.urlopen(pin["url"], timeout=60).read()
                    if hashlib.sha256(data).hexdigest() != pin["sha256"]:
                        parser.error("downloaded package checksum mismatch: " + package)
                    pending = archive.with_suffix(".tmp")
                    pending.write_bytes(data)
                    pending.replace(archive)
                if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
                    parser.error("cached package checksum mismatch: " + package)
                package_root = Path(temporary) / package
                subprocess.run(
                    ["dpkg-deb", "-x", str(archive), str(package_root)], check=True
                )
                extracted[package] = package_root
            source = (extracted[package] / entry["sysroot_file"]).resolve()
            if not source.is_relative_to(extracted[package]) or not source.is_file():
                parser.error(
                    "runtime payload missing from Debian package: " + entry["file"]
                )
            validate_runtime_elf(source, args.arch)
            entry["package_file"] = entry.pop("sysroot_file")
            entry["sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
            entry["package_sha256"] = pin["sha256"]
        # Replace only after every original Debian payload passes validation.
        for entry in entries:
            shutil.copyfile(
                extracted[entry["package"]] / entry["package_file"],
                destination / entry["file"],
            )
            (destination / entry["file"]).chmod(0o755)
    manifest = {
        "chromium_revision": (ROOT / "patches/REVISION").read_text().strip(),
        "architecture": args.arch,
        "build_sysroot_url": expected_url,
        "build_sysroot_sha256": config["Sha256Sum"],
        "runtime_source": "unmodified Debian package payloads; build sysroot supplies version selection only",
        "libraries": entries,
    }
    (destination / "runtime-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )
    print(f"Replaced {len(entries)} runtime libraries for {args.arch}")


if __name__ == "__main__":
    main()
