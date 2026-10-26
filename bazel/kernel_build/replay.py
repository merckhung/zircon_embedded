#!/usr/bin/env python3
"""Builds the Zircon kernel image from in-tree sources.

Executes every build edge recorded in zircon/kernel_build_data/graph.json
(exported from the upstream Fuchsia GN/ninja build by
export_fuchsia_kernel_graph.py) against the sources in this repository:

  * the C/C++/assembly compiles, `rustc` crate builds (including the Rust
    kernel crates and `core`/`compiler_builtins` built from the Rust
    toolchain's library sources), archives, links, objcopy steps and in-tree
    generator scripts are all run here, in dependency order, in parallel;
  * nothing from a Fuchsia output directory is used. The only inputs outside
    this repository are the toolchains: clang/lld/llvm-* (FUCHSIA_CLANG_DIR),
    rustc + its library sources (FUCHSIA_RUST_DIR) and a host python3.

The upstream commands use paths relative to `<root>/out/<dir>` with sources
at `../../`. We recreate that layout in --build-root: symlinks to this
repository's top-level source directories and to the toolchains, plus an
`out/x64` directory where every command runs verbatim (minus RBE wrappers,
see cmdclean.py).

After the graph is built, kernel.eng.zbi is assembled -- using the zbi host
tool that was just built from source -- with the same zbi
arguments as upstream //zircon/kernel/image/eng (build/zbi/run_zbi.py +
gen/zircon/kernel/image/eng/eng.zbi.rsp), except that the
`zircon/userboot-test-rust` test binary is not packaged.

Finally, every depfile written by the build is scanned to prove that all
files read came from this repository, the build directory, or the toolchains.
"""

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

ARCH_CONFIGS = {
    "x86_64": {
        "out_rel": "out/x64",
        "data_dir": "zircon/kernel_build_data",
        "bootable": "x64",
        "boot_shim": "kernel.phys32/linux-x86-boot-shim.bin",
        "physload_zbi": "kernel.phys_x64/obj/zircon/kernel/phys/physload.zbi",
        "physboot_package": [("physboot", "kernel.phys_x64/obj/zircon/kernel/phys/physboot.module.bin")],
        "libc_so": "user.libc_x64/libc.so",
        "vdso_so": "user.basic_x64-shared/libzircon.so",
        "zircon_package": [
            ("zircon/userboot", "userboot_rust"),
            ("zircon/vdso", "user.basic_x64-shared/libzircon.so"),
            ("zircon/version-string.txt", "gen/zircon/kernel/version-string.txt"),
            ("zircon/vmzircon/code-patches.bin",
             "kernel_x64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.code-patches.bin"),
        ] + [
            (f"zircon/vmzircon/code-patches/{name}", f"kernel_x64.lk_debug_level_2/obj/zircon/kernel/{sub}/{name}.bin")
            for sub, name in [
                ("arch/x86/retpoline", "__x86_indirect_thunk_basic_r11"),
                ("arch/x86/retpoline", "__x86_indirect_thunk_unsafe_r11"),
                ("arch/x86/user-copy", "_x86_copy_to_or_from_user_movsb"),
                ("arch/x86/user-copy", "_x86_copy_to_or_from_user_movsb_smap"),
                ("arch/x86/user-copy", "_x86_copy_to_or_from_user_movsq"),
                ("arch/x86/user-copy", "_x86_copy_to_or_from_user_movsq_smap"),
                ("lib/libc/string/arch/x86", "memcpy_movsb"),
                ("lib/libc/string/arch/x86", "memcpy_movsq"),
                ("lib/libc/string/arch/x86", "memset_stosb"),
                ("lib/libc/string/arch/x86", "memset_stosq"),
            ]
        ] + [
            ("zircon/vmzircon/image.elf", "kernel_x64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.bin"),
        ],
    },
    "arm64": {
        "out_rel": "out/arm64",
        "data_dir": "zircon/kernel_build_data_arm64",
        "bootable": "arm64",
        "boot_shim": "kernel.phys_arm64/linux-arm64-boot-shim.bin",
        "physload_zbi": "kernel.phys_arm64/obj/zircon/kernel/phys/physload.zbi",
        "physboot_package": [("physboot", "kernel.phys_arm64/obj/zircon/kernel/phys/physboot.module.bin")],
        "libc_so": "user.libc_arm64/libc.so",
        "vdso_so": "user.basic_arm64-shared/libzircon.so",
        "zircon_package": [
            ("zircon/userboot", "userboot_rust"),
            ("zircon/vdso", "user.basic_arm64-shared/libzircon.so"),
            ("zircon/version-string.txt", "gen/zircon/kernel/version-string.txt"),
            ("zircon/vmzircon/code-patches.bin",
             "kernel_arm64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.code-patches.bin"),
            ("zircon/vmzircon/image.elf",
             "kernel_arm64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.bin"),
        ],
    },
}

STATE_FILE = ".replay_state.json"
SYSROOT_DIR = "gen/zircon/public/sysroot"
ZBI_TOOL = "host_x64/zbi"


def load_graph(path):
    """Loads graph.json.gz and decodes interned path indices."""
    import gzip
    with gzip.open(path, "rt") as f:
        g = json.load(f)
    paths = g["paths"]
    edges = []
    for e in g["edges"]:
        edges.append({
            "cmd": e["cmd"],
            "outputs": [paths[i] for i in e["out"]],
            "inputs": [paths[i] for i in e["in"]],
            "deps": [paths[i] for i in e["dep"]],
            "rspfile": e.get("rsp"),
            "depfile": e.get("depfile"),
        })
    g["edges"] = edges
    return g


def log(msg):
    print(f"[kernel-replay] {msg}", flush=True)


def ensure_symlink(link, target):
    if os.path.islink(link):
        if os.readlink(link) == target:
            return
        os.unlink(link)
    elif os.path.exists(link):
        raise RuntimeError(f"{link} exists and is not a symlink")
    os.makedirs(os.path.dirname(link), exist_ok=True)
    os.symlink(target, link)


def setup_root(ws, root, clang_root, rust_root, bindgen_root, host_sysroot, python, out_rel):
    os.makedirs(os.path.join(root, out_rel), exist_ok=True)
    for name in sorted(os.listdir(ws)):
        if name.startswith(".") or name.startswith("bazel-") or name in ("out", "prebuilt", "external"):
            continue
        ensure_symlink(os.path.join(root, name), os.path.join(ws, name))
    ensure_symlink(os.path.join(root, "prebuilt/third_party/clang/linux-x64"), clang_root)
    ensure_symlink(os.path.join(root, "prebuilt/third_party/rust/linux-x64"), rust_root)
    ensure_symlink(os.path.join(root, "prebuilt/third_party/rust_bindgen/linux-x64"), bindgen_root)
    ensure_symlink(os.path.join(root, "prebuilt/third_party/sysroot/linux"), host_sysroot)
    for name in ("python3.13", "python3"):
        ensure_symlink(os.path.join(root, "prebuilt/third_party/python3/linux-x64/bin", name), python)


def sync_vendored(src_root, out_dir):
    n = 0
    for dirpath, _, files in os.walk(src_root):
        for f in files:
            s = os.path.join(dirpath, f)
            rel = os.path.relpath(s, src_root)
            d = os.path.join(out_dir, rel)
            if os.path.exists(d):
                with open(s, "rb") as a, open(d, "rb") as b:
                    if a.read() == b.read():
                        continue
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copy2(s, d)
            n += 1
    return n


def mtime(path):
    try:
        return os.stat(path).st_mtime_ns
    except OSError:
        return None


class Replayer:
    def __init__(self, graph, out_dir, jobs, verbose):
        self.edges = graph["edges"]
        self.out_dir = out_dir
        self.jobs = jobs
        self.verbose = verbose
        self.producer = {}
        for i, e in enumerate(self.edges):
            for o in e["outputs"]:
                self.producer[o] = i
        self.deps = []
        for e in self.edges:
            d = set()
            for p in e["inputs"] + e.get("deps", []):
                j = self.producer.get(p)
                if j is not None:
                    d.add(j)
            self.deps.append(d)
        state_path = os.path.join(out_dir, STATE_FILE)
        self.state_path = state_path
        try:
            with open(state_path) as f:
                self.state = json.load(f)
        except (OSError, ValueError):
            self.state = {}
        self.lock = threading.Lock()
        self.env = self._make_env()

    def key(self, i):
        return self.edges[i]["outputs"][0]

    def chash(self, i):
        """Fingerprint of the command, its rspfile and which inputs exist.

        Including the set of absent inputs makes an edge re-run when a file it
        could not see before appears (mtimes alone cannot detect that).
        """
        e = self.edges[i]
        rsp = e.get("rspfile") or {}
        missing = [p for p in e["inputs"] + e.get("deps", [])
                   if mtime(os.path.join(self.out_dir, p)) is None]
        env = "\0".join(f"{k}={v}" for k, v in sorted(self.env.items()))
        return hashlib.sha1("\0".join([env, e["cmd"], rsp.get("content", "")] + missing).encode()).hexdigest()

    def up_to_date(self, i, rebuilt):
        e = self.edges[i]
        if self.state.get(self.key(i)) != self.chash(i):
            return False
        if any(d in rebuilt for d in self.deps[i]):
            return False
        out_times = [mtime(os.path.join(self.out_dir, o)) for o in e["outputs"]]
        if any(t is None for t in out_times):
            return False
        oldest = min(out_times)
        for p in e["inputs"] + e.get("deps", []):
            t = mtime(os.path.join(self.out_dir, p))
            if t is not None and t > oldest:
                return False
        return True

    def run_edge(self, i):
        e = self.edges[i]
        for o in e["outputs"]:
            os.makedirs(os.path.dirname(os.path.join(self.out_dir, o)) or self.out_dir, exist_ok=True)
        if e.get("depfile"):
            # Like ninja: create the depfile's directory.
            os.makedirs(os.path.dirname(os.path.join(self.out_dir, e["depfile"])), exist_ok=True)
        rsp = e.get("rspfile")
        if rsp:
            # Like ninja: write the rule's response file before running.
            rp = os.path.join(self.out_dir, rsp["path"])
            os.makedirs(os.path.dirname(rp), exist_ok=True)
            with open(rp, "w") as f:
                f.write(rsp["content"])
        t0 = time.time()
        proc = subprocess.run(["/bin/bash", "-c", e["cmd"]], cwd=self.out_dir,
                              capture_output=True, text=True, env=self.env)
        return proc.returncode, proc.stdout, proc.stderr, time.time() - t0

    def _make_env(self):
        return {
            "PATH": "/usr/bin:/bin",
            "HOME": os.environ.get("HOME", "/tmp"),
            "LANG": "C.UTF-8",
            # zircon/kernel/git-version-string.sh appends "-dirty" if
            # `git status` in the checkout root reports changes. The build
            # root is not a git checkout; stop git from discovering whatever
            # repository happens to enclose the build directory, so the
            # version string is deterministic: "git-<upstream Fuchsia revision
            # the kernel sources were exported from>".
            "GIT_CEILING_DIRECTORIES": os.path.dirname(os.path.dirname(os.path.dirname(self.out_dir))),
        }

    def save_state(self):
        # Keep completed command fingerprints across interrupted builds.
        temporary = self.state_path + ".tmp"
        with open(temporary, "w") as f:
            json.dump(self.state, f)
        os.replace(temporary, self.state_path)

    def run(self):
        n = len(self.edges)
        remaining = {i: set(self.deps[i]) for i in range(n)}
        dependents = [[] for _ in range(n)]
        for i in range(n):
            for d in self.deps[i]:
                dependents[d].append(i)
        ready = [i for i in range(n) if not remaining[i]]
        done = set()
        rebuilt = set()
        failed = None
        ran = 0
        t_start = time.time()
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            running = {}
            while (ready or running) and failed is None:
                while ready and len(running) < self.jobs * 2:
                    i = ready.pop()
                    if self.up_to_date(i, rebuilt):
                        done.add(i)
                        for j in dependents[i]:
                            remaining[j].discard(i)
                            if not remaining[j]:
                                ready.append(j)
                        continue
                    running[pool.submit(self.run_edge, i)] = (i, self.chash(i))
                if not running:
                    continue
                finished, _ = concurrent.futures.wait(running, return_when=concurrent.futures.FIRST_COMPLETED)
                for fut in finished:
                    i, fingerprint = running.pop(fut)
                    rc, so, se, dt = fut.result()
                    ran += 1
                    if rc != 0:
                        failed = (i, rc, so, se)
                        break
                    self.state[self.key(i)] = fingerprint
                    rebuilt.add(i)
                    done.add(i)
                    if ran % 250 == 0:
                        self.save_state()
                    if self.verbose or ran % 250 == 0:
                        log(f"{len(done)}/{n} edges ({ran} executed, {time.time() - t_start:.0f}s)")
                    for j in dependents[i]:
                        remaining[j].discard(i)
                        if not remaining[j]:
                            ready.append(j)
        self.save_state()
        if failed:
            i, rc, so, se = failed
            log(f"FAILED (exit {rc}) building {self.edges[i]['outputs']}")
            log("command: " + self.edges[i]["cmd"][:4000])
            sys.stderr.write((so + se)[-6000:] + "\n")
            return False
        if len(done) != n:
            log(f"ERROR: dependency cycle or unscheduled edges ({n - len(done)} left)")
            return False
        log(f"graph complete: {n} edges, {ran} executed in {time.time() - t_start:.1f}s")
        return True


def check_provenance(out_dir, allowed_roots):
    """Scans depfiles to prove every input came from allowed roots."""
    allowed = [os.path.realpath(r) + os.sep for r in allowed_roots]
    bad = {}
    nfiles = 0
    memo = {}
    for dirpath, _, files in os.walk(out_dir):
        for f in files:
            if not f.endswith(".d"):
                continue
            path = os.path.join(dirpath, f)
            try:
                with open(path, errors="replace") as fh:
                    text = fh.read().replace("\\\n", " ")
            except OSError:
                continue
            nfiles += 1
            for line in text.splitlines():
                if ":" not in line:
                    continue
                for dep in line.split(":", 1)[1].split():
                    if dep in ("\\",):
                        continue
                    key = os.path.join(out_dir, dep)
                    full = memo.get(key)
                    if full is None:
                        full = os.path.realpath(key)
                        memo[key] = full
                    if not any(full.startswith(a) for a in allowed):
                        bad.setdefault(full, path)
    return nfiles, bad


def assemble_zbi(zbi_tool, out_dir, output, cfg):
    gen = os.path.join(out_dir, "gen/zembedded")
    os.makedirs(gen, exist_ok=True)

    def fini(name, entries):
        p = os.path.join(gen, name)
        with open(p, "w") as f:
            for dst, src in entries:
                f.write(f"{dst}={src}\n")
        return os.path.relpath(p, out_dir)

    zircon_fini = fini("zircon.fini", cfg["zircon_package"])
    physboot_fini = fini("physboot.package.fini", cfg["physboot_package"])
    cmd = [
        zbi_tool,
        # Same arguments as upstream gen/zircon/kernel/image/eng/eng.zbi.rsp
        "--prefix=", "--files", "--compressed",
        "--type=cmdline",
        "--entry=kernel.enable-debugging-syscalls=true",
        "--entry=kernel.enable-serial-syscalls=true",
        "--prefix=", "--files", "--compressed", "--compressed=max", "--files-type=kernel",
        "--files", zircon_fini,
        "--type=container", cfg["physload_zbi"],
        "--prefix=", "--files", "--compressed", "--compressed=max", "--files-type=kernel",
        "--files", physboot_fini,
        # run_zbi.py trailing arguments from //zircon/kernel/image/eng
        "--output", os.path.abspath(output), f"--bootable={cfg['bootable']}", "--compressed=zstd.max",
    ]
    subprocess.run(cmd, cwd=out_dir, check=True)


def export_sysroot(source, destination, allowed_roots):
    """Materialize GN forwarding headers before moving the sysroot.

    GN's one-line IWYU forwarding includes are relative to the original output
    directory. Copying those lines verbatim produces a broken exported sysroot.
    Follow only those generated forwarders, retaining the public include layout.
    """
    allowed = [os.path.realpath(root) + os.sep for root in allowed_roots]
    forwarder = re.compile(r'\s*#include "([^"\n]+)"\s*//\s*IWYU pragma: export\s*')
    if os.path.isdir(destination):
        shutil.rmtree(destination)
    shutil.copytree(source, destination, symlinks=False,
                    ignore=shutil.ignore_patterns("*.stamp", "*.json"))
    count = 0
    for directory, _, files in os.walk(source):
        for name in files:
            original = os.path.join(directory, name)
            relative = os.path.relpath(original, source)
            exported = os.path.join(destination, relative)
            if not os.path.isfile(exported):
                continue
            current = os.path.realpath(original)
            seen = set()
            while True:
                if current in seen:
                    raise RuntimeError(f"sysroot forwarding-header cycle: {relative}")
                seen.add(current)
                if not any(current.startswith(root) for root in allowed):
                    raise RuntimeError(f"sysroot header outside declared inputs: {relative}")
                try:
                    with open(current, "rb") as header:
                        data = header.read()
                except FileNotFoundError:
                    # Old GN output can retain an exported-header entry after
                    # its source was deleted. Do not publish a broken alias or
                    # invent a replacement for an API absent from the tree.
                    log(f"omitting stale sysroot forwarder without source: {relative}")
                    os.unlink(exported)
                    data = None
                    break
                try:
                    match = forwarder.fullmatch(data.decode("utf-8"))
                except UnicodeDecodeError:
                    match = None
                if not match:
                    break
                current = os.path.realpath(os.path.join(os.path.dirname(current), match.group(1)))
            if data is not None and len(seen) > 1:
                os.chmod(exported, 0o644)
                with open(exported, "wb") as header:
                    header.write(data)
                count += 1
    return count


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--arch", choices=["x86_64", "arm64"], default="x86_64",
                    help="Target architecture (x86_64 or arm64)")
    ap.add_argument("--data-dir", help="Path to exported kernel_build_data directory")
    ap.add_argument("--workspace", required=True)
    ap.add_argument("--build-root", required=True)
    ap.add_argument("--clang-dir", default=os.environ.get("FUCHSIA_CLANG_DIR", ""),
                    help="clang toolchain root (contains bin/clang)")
    ap.add_argument("--rust-dir", default=os.environ.get("FUCHSIA_RUST_DIR", ""),
                    help="rust toolchain root (contains bin/rustc and lib/rustlib/src)")
    ap.add_argument("--bindgen-dir", default=os.environ.get("FUCHSIA_BINDGEN_DIR", ""),
                    help="rust-bindgen toolchain root (contains bindgen); used by the kernel's "
                         "Rust FFI binding generation")
    ap.add_argument("--host-sysroot-dir", default=os.environ.get("FUCHSIA_HOST_SYSROOT_DIR", ""),
                    help="Linux host sysroot used to link host-side Rust proc-macro crates")
    ap.add_argument("--out-zbi-tool", help="copy of the zbi host tool built from source")
    ap.add_argument("--out-sysroot", help="directory to receive the Fuchsia sysroot "
                    "(headers + link stubs) built from source")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--out-boot-shim")
    ap.add_argument("--out-physload")
    ap.add_argument("--out-kernel-zbi")
    ap.add_argument("--out-libc")
    ap.add_argument("--out-vdso")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    ws = os.path.realpath(args.workspace)
    for name, d, probe in (("clang", args.clang_dir, "bin/clang"), ("rust", args.rust_dir, "bin/rustc"),
                           ("bindgen", args.bindgen_dir, "bindgen"),
                           ("host-sysroot", args.host_sysroot_dir, "usr/include")):
        if not d or not os.path.exists(os.path.join(d, probe)):
            sys.exit(f"[kernel-replay] {name} toolchain not found (got '{d}'); set --{name}-dir")
    clang_root = os.path.realpath(args.clang_dir)
    rust_root = os.path.realpath(args.rust_dir)
    bindgen_root = os.path.realpath(args.bindgen_dir)
    host_sysroot = os.path.realpath(args.host_sysroot_dir)
    cfg = ARCH_CONFIGS[args.arch]
    data_dir = os.path.realpath(args.data_dir) if args.data_dir else os.path.join(ws, cfg["data_dir"])
    root = os.path.realpath(args.build_root)
    out_dir = os.path.join(root, cfg["out_rel"])

    graph = load_graph(os.path.join(data_dir, "graph.json.gz"))
    setup_root(ws, root, clang_root, rust_root, bindgen_root, host_sysroot, os.path.realpath(sys.executable), cfg["out_rel"])
    n = sync_vendored(os.path.join(data_dir, "out"), out_dir)
    log(f"arch {args.arch}; build root {root}; {n} vendored generated files refreshed; {len(graph['edges'])} edges")

    if not Replayer(graph, out_dir, args.jobs, args.verbose).run():
        return 1

    linked = [os.path.realpath(os.path.join(root, n)) for n in os.listdir(root)
              if os.path.islink(os.path.join(root, n))]
    nfiles, bad = check_provenance(out_dir, [ws, root, clang_root, rust_root, bindgen_root,
                                             host_sysroot] + linked)
    if bad:
        log(f"PROVENANCE ERROR: {len(bad)} inputs outside the repository/toolchains:")
        for p, d in list(bad.items())[:40]:
            log(f"   {p}  (from {d})")
        return 1
    log(f"provenance OK: {nfiles} depfiles, all inputs under repository, build dir or toolchains")

    def copy_out(rel, dst):
        if dst:
            os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
            shutil.copyfile(os.path.join(out_dir, rel), dst)
            log(f"{rel} -> {dst} ({os.path.getsize(dst)} bytes)")

    copy_out(cfg["boot_shim"], args.out_boot_shim)
    copy_out(cfg["physload_zbi"], args.out_physload)
    copy_out(cfg["libc_so"], args.out_libc)
    copy_out(cfg["vdso_so"], args.out_vdso)
    copy_out(ZBI_TOOL, args.out_zbi_tool)
    if args.out_zbi_tool:
        os.chmod(args.out_zbi_tool, 0o755)
    if args.out_sysroot:
        src = os.path.join(out_dir, SYSROOT_DIR)
        count = export_sysroot(src, args.out_sysroot,
                               [ws, root, clang_root, rust_root, bindgen_root, host_sysroot] + linked)
        log(f"{SYSROOT_DIR} -> {args.out_sysroot}; {count} forwarding headers materialized")
    if args.out_kernel_zbi:
        assemble_zbi(os.path.join(out_dir, ZBI_TOOL), out_dir, args.out_kernel_zbi, cfg)
        log(f"kernel.eng.zbi -> {args.out_kernel_zbi} ({os.path.getsize(args.out_kernel_zbi)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
