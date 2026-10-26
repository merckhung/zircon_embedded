"""Locates the external toolchains used by the build.

This repository rule does not provide any prebuilt Zircon/Fuchsia build
artifacts: the kernel image, vDSO, userboot, libc, sysroot and zbi tool are
all built from this repository's sources (see //zircon:kernel and
bazel/kernel_build/replay.py). It only records where the toolchains live:

  FUCHSIA_CLANG_DIR        clang/lld/llvm-* toolchain root (bin/clang)
  FUCHSIA_RUST_DIR         rustc toolchain root incl. lib/rustlib/src
  FUCHSIA_BINDGEN_DIR      rust-bindgen (bindgen), used by the Rust kernel
  FUCHSIA_HOST_SYSROOT_DIR Linux host sysroot for host proc-macros/tools
  QEMU_DIR                 QEMU (bin/qemu-system-x86_64) used to run images

Each may be set explicitly (e.g. `--repo_env=FUCHSIA_CLANG_DIR=...`);
otherwise it defaults to the standard location inside a Fuchsia checkout
given by FUCHSIA_DIR (`--repo_env=FUCHSIA_DIR=/path/to/fuchsia`, see
user.bazelrc.example).
"""

_TOOLS = [
    # (env var, path inside a Fuchsia checkout, file that must exist)
    ("FUCHSIA_CLANG_DIR", "prebuilt/third_party/clang/linux-x64", "bin/clang"),
    ("FUCHSIA_RUST_DIR", "prebuilt/third_party/rust/linux-x64", "bin/rustc"),
    ("FUCHSIA_BINDGEN_DIR", "prebuilt/third_party/rust_bindgen/linux-x64", "bindgen"),
    ("FUCHSIA_HOST_SYSROOT_DIR", "prebuilt/third_party/sysroot/linux", "usr/include"),
    ("QEMU_DIR", "prebuilt/third_party/qemu/linux-x64", "bin/qemu-system-x86_64"),
]

def _zircon_toolchains_repo_impl(rctx):
    fuchsia_dir = rctx.os.environ.get("FUCHSIA_DIR", "")
    values = {}
    for env, rel, probe in _TOOLS:
        d = rctx.os.environ.get(env, "")
        if not d:
            if not fuchsia_dir:
                fail(("%s is not set and FUCHSIA_DIR is not set. Point FUCHSIA_DIR at a " +
                      "Fuchsia checkout (e.g. --repo_env=FUCHSIA_DIR=... in user.bazelrc) " +
                      "or set %s directly.") % (env, env))
            d = fuchsia_dir + "/" + rel
        if not rctx.path(d + "/" + probe).exists:
            fail("%s=%s does not contain %s" % (env, d, probe))
        values[env] = str(rctx.path(d).realpath)

    rctx.file("paths.bzl", "\n".join([
        "FUCHSIA_CLANG_DIR = \"%s\"" % values["FUCHSIA_CLANG_DIR"],
        "FUCHSIA_CLANG_PP = \"%s/bin/clang++\"" % values["FUCHSIA_CLANG_DIR"],
        "FUCHSIA_RUST_DIR = \"%s\"" % values["FUCHSIA_RUST_DIR"],
        "FUCHSIA_BINDGEN_DIR = \"%s\"" % values["FUCHSIA_BINDGEN_DIR"],
        "FUCHSIA_HOST_SYSROOT_DIR = \"%s\"" % values["FUCHSIA_HOST_SYSROOT_DIR"],
        "FUCHSIA_QEMU_X64 = \"%s/bin/qemu-system-x86_64\"" % values["QEMU_DIR"],
        "FUCHSIA_QEMU_AARCH64 = \"%s/bin/qemu-system-aarch64\"" % values["QEMU_DIR"],
        "",
    ]))
    rctx.file("BUILD.bazel", "exports_files([\"paths.bzl\"])\n")

zircon_toolchains_repo = repository_rule(
    implementation = _zircon_toolchains_repo_impl,
    environ = ["FUCHSIA_DIR"] + [t[0] for t in _TOOLS],
    local = True,
)

def _zircon_ext_impl(mctx):
    zircon_toolchains_repo(name = "zircon_toolchains")

zircon_ext = module_extension(
    implementation = _zircon_ext_impl,
)
