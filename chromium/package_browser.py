#!/usr/bin/env python3
"""Validate, stage and audit a complete freshly built Chromium bundle."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

from build import acquire_build_lock
from package_runtime import needed_libraries, validate_runtime_elf

ROOT = Path(__file__).resolve().parent
BINARIES = (
    "chrome",
    "chrome_crashpad_handler",
    "libvk_swiftshader.so",
    "libvulkan.so.1",
)
ASSETS = (
    "vk_swiftshader_icd.json",
    "icudtl.dat",
    "v8_context_snapshot.bin",
    "resources.pak",
    "chrome_100_percent.pak",
    "chrome_200_percent.pak",
    "locales/en-US.pak",
)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command, checkout):
    return subprocess.check_output(command, cwd=checkout, text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--arch", choices=("x64", "arm64"), required=True)
    parser.add_argument("--staging-dir", type=Path, required=True)
    parser.add_argument("--deny-file", type=Path, required=True)
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    build_lock = acquire_build_lock(checkout, args.arch)
    staging = args.staging_dir.resolve()
    if staging.is_relative_to(ROOT.parent):
        parser.error("staging directory must be outside the publication tree")
    staging.mkdir(parents=True, exist_ok=True)
    if staging.stat().st_dev != ROOT.stat().st_dev:
        parser.error("staging and bundle directories must be on the same filesystem")
    revision = (ROOT / "patches/REVISION").read_text().strip()
    if run(["git", "rev-parse", "HEAD"], checkout) != revision:
        parser.error("checkout revision does not match patches/REVISION")
    patch = ROOT / "patches/chromium_zircon_linux_x64.patch"
    run(["git", "apply", "--reverse", "--check", str(patch)], checkout)
    source_diff = subprocess.check_output(
        ["git", "diff", "--ignore-submodules=all"], cwd=checkout
    )
    if source_diff != patch.read_bytes():
        parser.error("tracked source changes differ from the checked-in patch")
    output = checkout / "out" / ("ZirconLinux_" + args.arch)
    angle_flags = output / "gen/ui/gl/buildflags.h"
    if (
        "#define BUILDFLAG_INTERNAL_USE_STATIC_ANGLE() (1)"
        not in angle_flags.read_text()
    ):
        parser.error(
            "this bundle layout requires the pinned static ANGLE configuration"
        )
    expected_args = (
        (ROOT / "patches/args.gn")
        .read_text()
        .replace('target_cpu = "x64"', f'target_cpu = "{args.arch}"')
    )
    if (output / "args.gn").read_text() != expected_args:
        parser.error("generated GN args differ from the checked-in configuration")
    dry_run = run(
        [
            str(checkout / "third_party/ninja/ninja"),
            "-C",
            str(output),
            "-n",
            "chrome",
            "chrome_crashpad_handler",
        ],
        checkout,
    )
    if "ninja: no work to do." not in dry_run:
        parser.error("Chromium build is incomplete; finish both build targets first")
    for name in BINARIES + ASSETS:
        source = output / name
        if not source.is_file() or source.stat().st_size == 0:
            parser.error("missing or empty build output: " + name)
    destination = ROOT / ("prebuilt" if args.arch == "x64" else "prebuilt_arm64")
    backup = staging / (destination.name + "-previous")
    if backup.exists():
        parser.error(
            "previous bundle backup already exists; preserve or relocate it first"
        )
    with tempfile.TemporaryDirectory(
        prefix="chromium-" + args.arch + "-", dir=staging
    ) as tmp:
        bundle = Path(tmp) / destination.name
        shutil.copytree(destination, bundle)
        # Static ANGLE lives in chrome. Upstream emits these dummy libraries
        # only for build infrastructure; they provide no EGL/GLES API.
        for name in ("libEGL.so", "libGLESv2.so"):
            (bundle / name).unlink(missing_ok=True)
        for name in BINARIES + ASSETS:
            target = bundle / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(output / name, target)
            target.chmod(0o755 if name in BINARIES else 0o644)
        strip = checkout / "third_party/llvm-build/Release+Asserts/bin/llvm-strip"
        machine = 62 if args.arch == "x64" else 183
        for name in BINARIES:
            target = bundle / name
            with target.open("rb") as stream:
                header = stream.read(20)
            if (
                header[:6] != b"\x7fELF\x02\x01"
                or struct.unpack("<H", header[18:20])[0] != machine
            ):
                raise RuntimeError("wrong ELF architecture: " + name)
            run([str(strip), "--strip-all", str(target)], checkout)
        shutil.copyfile(checkout / "LICENSE", bundle / "LICENSE")
        shutil.copyfile(ROOT.parent / "LICENSE", bundle / "PROJECT_LICENSE.txt")
        run(
            [
                sys.executable,
                "tools/licenses/licenses.py",
                "credits",
                "--target-os",
                "linux",
                "--gn-out-dir",
                str(output),
                "--gn-target",
                "//chrome:chrome",
                str(bundle / "THIRD_PARTY_NOTICES.html"),
            ],
            checkout,
        )
        runtime = json.loads((bundle / "runtime-manifest.json").read_text())
        if (
            runtime["architecture"] != args.arch
            or runtime["chromium_revision"] != revision
        ):
            raise RuntimeError("runtime library provenance does not match this build")
        for entry in runtime["libraries"]:
            validate_runtime_elf(bundle / entry["file"], args.arch)
            if digest(bundle / entry["file"]) != entry["sha256"]:
                raise RuntimeError(
                    "runtime library differs from manifest: " + entry["file"]
                )
        supplied = set(BINARIES) | {
            Path(entry["file"]).name for entry in runtime["libraries"]
        }
        if not {'libXcursor.so.1', 'libnss_dns.so.2', 'libnss_files.so.2'} <= supplied:
            raise RuntimeError('missing explicitly loaded X11 cursor or glibc NSS runtime library')
        for path in [bundle / name for name in BINARIES] + [
            bundle / entry["file"] for entry in runtime["libraries"]
        ]:
            missing = set(needed_libraries(path)) - supplied
            if missing:
                raise RuntimeError(
                    "missing runtime dependencies of "
                    + path.name
                    + ": "
                    + ", ".join(sorted(missing))
                )
        artifacts = [
            {
                "file": path.relative_to(bundle).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": digest(path),
            }
            for path in sorted(bundle.rglob("*"))
            if path.is_file() and path.name != "build-manifest.json"
        ]
        provenance = {
            "chromium_revision": revision,
            "architecture": args.arch,
            "patch_sha256": digest(patch),
            "gn_args": expected_args,
            "dependencies_sha256": digest(ROOT / "patches/dependencies.json"),
            "strip": "checkout llvm-strip --strip-all",
            "angle_linkage": "static in chrome; upstream infrastructure-only shared stubs excluded",
            "angle_buildflags_sha256": digest(angle_flags),
            "artifacts": artifacts,
        }
        (bundle / "build-manifest.json").write_text(
            json.dumps(provenance, indent=2) + "\n"
        )
        subprocess.run(
            [
                sys.executable,
                str(ROOT.parent / "tools/audit_publication.py"),
                "--root",
                str(bundle),
                "--deny-file",
                str(args.deny_file.resolve()),
            ],
            check=True,
        )
        # Keep the prior bundle outside the repository and restore it on failure.
        destination.rename(backup)
        try:
            bundle.rename(destination)
        except BaseException:
            backup.rename(destination)
            raise
    print(
        "Installed complete " + args.arch + " bundle; prior bundle preserved externally"
    )


if __name__ == "__main__":
    main()
