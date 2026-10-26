"""Starlark build rules for Zircon x86_64 binaries, Linux static userland binaries, and ZBI images."""

load(
    "@zircon_toolchains//:paths.bzl",
    "FUCHSIA_BINDGEN_DIR",
    "FUCHSIA_CLANG_DIR",
    "FUCHSIA_CLANG_PP",
    "FUCHSIA_HOST_SYSROOT_DIR",
    "FUCHSIA_QEMU_AARCH64",
    "FUCHSIA_RUST_DIR",
)

def _zircon_cc_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    arch = ctx.attr.arch
    target = "aarch64-unknown-fuchsia" if arch == "arm64" else "x86_64-unknown-fuchsia"
    if ctx.attr.sysroot:
        sysroot_files = ctx.files.sysroot
    elif arch == "arm64":
        sysroot_files = ctx.files._sysroot_arm64
    else:
        sysroot_files = ctx.files._sysroot_x64
    sysroot_dir = sysroot_files[0].path

    args = [
        FUCHSIA_CLANG_PP,
        "--target=" + target,
        "--sysroot=" + sysroot_dir,
        "-ffuchsia-api-level=4293918720",
        "-O2",
        "-std=c++20",
        "-fno-exceptions",
        "-fno-rtti",
        "-static-libstdc++",
        "-Wall",
        "-Wno-unused-function",
        "-I",
        ".",
    ]
    for inc in ctx.attr.includes:
        if inc == ".":
            args.extend(["-I", ctx.label.package if ctx.label.package else "."])
        else:
            args.extend(["-I", (ctx.label.package + "/" + inc) if ctx.label.package else inc])
    args.extend(ctx.attr.copts)
    for src in ctx.files.srcs:
        if src.extension in ["cc", "cpp", "c", "S", "s"]:
            args.append(src.path)
    args.extend(["-lzircon", "-o", out.path])
    args.extend(ctx.attr.linkopts)

    ctx.actions.run(
        executable = "/bin/bash",
        arguments = ["-c", " ".join(["'" + a + "'" for a in args])],
        inputs = depset(ctx.files.srcs + ctx.files.hdrs + sysroot_files),
        outputs = [out],
        use_default_shell_env = True,
        execution_requirements = {"no-sandbox": "1"},
        mnemonic = "ZirconCcBinary",
        progress_message = "Compiling Zircon %s C++ binary %%{label}" % ctx.attr.arch,
    )
    return [DefaultInfo(files = depset([out]), executable = out)]

zircon_cc_binary = rule(
    implementation = _zircon_cc_binary_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "arm64"]),
        "srcs": attr.label_list(allow_files = [".cc", ".cpp", ".c", ".S", ".s", ".h"]),
        "hdrs": attr.label_list(allow_files = [".h", ".hpp"]),
        "includes": attr.string_list(default = ["."]),
        "copts": attr.string_list(default = []),
        "linkopts": attr.string_list(default = []),
        "sysroot": attr.label(default = None),
        "_sysroot_x64": attr.label(default = "//zircon:sysroot"),
        "_sysroot_arm64": attr.label(default = "//zircon:sysroot_arm64"),
    },
    executable = True,
)

def _linux_static_cc_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    compiler = "/usr/bin/aarch64-linux-gnu-g++" if ctx.attr.arch == "arm64" else "/usr/bin/g++"
    args = [
        compiler,
        "-O2",
        "-std=c++20",
        "-static",
        "-fno-exceptions",
        "-fno-rtti",
        "-Wall",
        "-Wno-unused-function",
        "-I",
        ".",
    ]
    args.extend(ctx.attr.copts)
    for src in ctx.files.srcs:
        if src.extension in ["cc", "cpp", "c", "S", "s"]:
            args.append(src.path)
    args.extend(["-o", out.path])
    args.extend(ctx.attr.linkopts)

    ctx.actions.run(
        executable = "/bin/bash",
        arguments = ["-c", " ".join(["'" + a + "'" for a in args])],
        inputs = depset(ctx.files.srcs + ctx.files.hdrs),
        outputs = [out],
        use_default_shell_env = True,
        execution_requirements = {"no-sandbox": "1"},
        mnemonic = "LinuxStaticCcBinary",
        progress_message = "Compiling static Linux %s C++ binary %%{label}" % ctx.attr.arch,
    )
    return [DefaultInfo(files = depset([out]), executable = out)]

linux_static_cc_binary = rule(
    implementation = _linux_static_cc_binary_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "arm64"]),
        "srcs": attr.label_list(allow_files = [".cc", ".cpp", ".c", ".S", ".s", ".h"]),
        "hdrs": attr.label_list(allow_files = [".h", ".hpp"]),
        "copts": attr.string_list(default = []),
        "linkopts": attr.string_list(default = []),
    },
    executable = True,
)

def _zircon_zbi_image_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + ".zbi")
    arch = ctx.attr.arch
    zbi_tool = ctx.file.zbi_tool if ctx.file.zbi_tool else ctx.file._zbi_tool
    physload = ctx.file.physload if ctx.file.physload else (
        ctx.file._physload_arm64 if arch == "arm64" else ctx.file._physload_x64)
    kernel_zbi = ctx.file.kernel_zbi if ctx.file.kernel_zbi else (
        ctx.file._kernel_zbi_arm64 if arch == "arm64" else ctx.file._kernel_zbi_x64)
    libc_so = ctx.file.libc_so if ctx.file.libc_so else (
        ctx.file._libc_so_arm64 if arch == "arm64" else ctx.file._libc_so_x64)
    init_bin = ctx.file.init_bin
    rootfs_tar = ctx.file.rootfs_tar

    args = [
        zbi_tool.path,
        "-o",
        out.path,
        physload.path,
        kernel_zbi.path,
        "--files",
        "--entry=bin/zircon_linux_init=" + init_bin.path,
        "--entry=lib/ld.so.1=" + libc_so.path,
        "--entry=data/rootfs.tar=" + rootfs_tar.path,
        "--type=cmdline",
        "--entry=userboot.next=bin/zircon_linux_init",
    ]
    if arch == "x86_64":
        args.append("--entry=kernel.serial=legacy")
    args.extend([
        "--entry=kernel.vdso.always_use_next=true",
        "--entry=kernel.halt-on-panic=true",
    ])
    for entry in ctx.attr.cmdline_entries:
        args.append("--entry=" + entry)

    ctx.actions.run(
        executable = zbi_tool.path,
        arguments = args[1:],
        inputs = depset([zbi_tool, physload, kernel_zbi, libc_so, init_bin, rootfs_tar]),
        outputs = [out],
        mnemonic = "ZirconZbiImage",
        progress_message = "Packaging Zircon ZBI image %{label}",
    )
    return [DefaultInfo(files = depset([out]))]

zircon_zbi_image = rule(
    implementation = _zircon_zbi_image_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "arm64"]),
        "init_bin": attr.label(mandatory = True, allow_single_file = True),
        "rootfs_tar": attr.label(mandatory = True, allow_single_file = True),
        "cmdline_entries": attr.string_list(default = []),
        "zbi_tool": attr.label(default = None, allow_single_file = True),
        "physload": attr.label(default = None, allow_single_file = True),
        "kernel_zbi": attr.label(default = None, allow_single_file = True),
        "libc_so": attr.label(default = None, allow_single_file = True),
        "_zbi_tool": attr.label(default = "//zircon:zbi_tool", allow_single_file = True),
        "_physload_x64": attr.label(default = "//zircon:physload", allow_single_file = True),
        "_physload_arm64": attr.label(default = "//zircon:physload_arm64", allow_single_file = True),
        "_kernel_zbi_x64": attr.label(default = "//zircon:kernel_eng_zbi", allow_single_file = True),
        "_kernel_zbi_arm64": attr.label(default = "//zircon:kernel_eng_zbi_arm64", allow_single_file = True),
        "_libc_so_x64": attr.label(default = "//zircon:libc_so", allow_single_file = True),
        "_libc_so_arm64": attr.label(default = "//zircon:libc_so_arm64", allow_single_file = True),
    },
)

def _zircon_qemu_runner_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + ".sh")
    script = ctx.file.script
    arch = ctx.attr.arch
    boot_shim = ctx.file.boot_shim if ctx.file.boot_shim else (
        ctx.file._boot_shim_arm64 if arch == "arm64" else ctx.file._boot_shim_x64)
    zbi = ctx.file.zbi_image
    content = """#!/bin/bash
set -e
exec python3 "{script}" "{qemu}" "{shim}" "{zbi}" "$@"
""".format(
        script = script.short_path,
        qemu = ctx.attr.qemu_bin,
        shim = boot_shim.short_path,
        zbi = zbi.short_path,
    )
    ctx.actions.write(out, content, is_executable = True)
    runfiles = ctx.runfiles(files = [script, boot_shim, zbi])
    return [DefaultInfo(executable = out, runfiles = runfiles)]

zircon_qemu_runner = rule(
    implementation = _zircon_qemu_runner_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "arm64"]),
        "script": attr.label(mandatory = True, allow_single_file = True),
        "zbi_image": attr.label(mandatory = True, allow_single_file = True),
        "qemu_bin": attr.string(mandatory = True),
        "boot_shim": attr.label(default = None, allow_single_file = True),
        "_boot_shim_x64": attr.label(default = "//zircon:boot_shim", allow_single_file = True),
        "_boot_shim_arm64": attr.label(default = "//zircon:boot_shim_arm64", allow_single_file = True),
    },
    executable = True,
)


def _zircon_kernel_build_impl(ctx):
    """Builds the Zircon kernel image, vDSO, userboot, libc, sysroot and zbi
    tool from this repository's sources by replaying the exported upstream
    build graph (bazel/kernel_build/replay.py)."""
    arch = ctx.attr.arch
    suffix = ("_" + arch) if arch != "x86_64" else ""
    shim_name = "linux-arm64-boot-shim.bin" if arch == "arm64" else "linux-x86-boot-shim.bin"
    outs = {
        "boot_shim": ctx.actions.declare_file(shim_name),
        "physload": ctx.actions.declare_file("physload%s.zbi" % suffix),
        "kernel_zbi": ctx.actions.declare_file("kernel.eng%s.zbi" % suffix),
        "libc": ctx.actions.declare_file("libc%s.so" % suffix),
        "vdso": ctx.actions.declare_file("libzircon%s.so" % suffix),
        "zbi_tool": ctx.actions.declare_file("zbi" + ("_" + arch if arch != "x86_64" else "")),
    }
    sysroot = ctx.actions.declare_directory("sysroot" + suffix)

    suffix = ("_" + arch) if arch != "x86_64" else ""
    build_root = "%s/%s/_kernel_build_root%s" % (ctx.bin_dir.path, ctx.label.package, suffix)
    cmd = " ".join([
        "python3",
        ctx.file._replay.path,
        "--arch " + arch,
        "--workspace .",
        "--build-root \"$PWD/%s\"" % build_root,
        "--clang-dir " + FUCHSIA_CLANG_DIR,
        "--rust-dir " + FUCHSIA_RUST_DIR,
        "--bindgen-dir " + FUCHSIA_BINDGEN_DIR,
        "--host-sysroot-dir " + FUCHSIA_HOST_SYSROOT_DIR,
        "--jobs %d" % ctx.attr.jobs,
        "--out-boot-shim " + outs["boot_shim"].path,
        "--out-physload " + outs["physload"].path,
        "--out-kernel-zbi " + outs["kernel_zbi"].path,
        "--out-libc " + outs["libc"].path,
        "--out-vdso " + outs["vdso"].path,
        "--out-zbi-tool " + outs["zbi_tool"].path,
        "--out-sysroot " + sysroot.path,
    ])
    ctx.actions.run_shell(
        command = cmd,
        inputs = depset(ctx.files.srcs + ctx.files._replay_lib + [ctx.file._replay]),
        outputs = list(outs.values()) + [sysroot],
        use_default_shell_env = True,
        # Runs the absolute-path toolchains and keeps an incremental build
        # directory, so it cannot run in the sandbox or remotely.
        execution_requirements = {"no-sandbox": "1", "local": "1", "no-remote": "1"},
        mnemonic = "ZirconKernelBuild",
        progress_message = "Building Zircon kernel image + libc from source (%{label})",
    )
    groups = {k: depset([v]) for k, v in outs.items()}
    groups["sysroot"] = depset([sysroot])
    return [
        DefaultInfo(files = depset(list(outs.values()) + [sysroot])),
        OutputGroupInfo(**groups),
    ]

zircon_kernel_build = rule(
    implementation = _zircon_kernel_build_impl,
    attrs = {
        "arch": attr.string(default = "x86_64", values = ["x86_64", "arm64"]),
        "srcs": attr.label_list(allow_files = True, doc = "All source/graph files the build reads"),
        "jobs": attr.int(default = 8),
        "_replay": attr.label(default = "//bazel/kernel_build:replay.py", allow_single_file = True),
        "_replay_lib": attr.label(default = "//bazel/kernel_build:lib"),
    },
)
