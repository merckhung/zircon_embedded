#!/usr/bin/env python3
"""Exports the upstream Fuchsia ninja build graph for the Zircon kernel image.

This is a *provenance* tool, run by a maintainer against a configured Fuchsia
checkout (`fx set ... && fx build kernel.eng.zbi`). It is NOT run by the Bazel
build. It produces the vendored build description that
`bazel/kernel_build/replay.py` executes from in-tree sources:

  zircon/kernel_build_data/graph.json.gz  Every build edge needed to produce the
                                        kernel artifacts: the real compiler /
                                        rustc / linker / objcopy / ar / script
                                        command (RBE wrappers stripped, see
                                        cmdclean.py), its outputs, inputs and
                                        depfile-discovered dependencies.
  zircon/kernel_build_data/out/...      Files the replay needs in the output
                                        directory that ninja edges in the
                                        replayed set do not produce:
                                          - GN-time generated files (response
                                            files, target specs, config json),
                                          - outputs of host-tool code
                                            generators that are not replayed
                                            (fidlc / zither / hwreg header
                                            generators run as host binaries).
  <source files>                        Any source file (C/C++/asm/Rust/linker
                                        script/python) the edges read that is
                                        missing from this tree is copied in from
                                        the Fuchsia checkout at the same path.

Edges that are deliberately NOT replayed (and therefore have their consumed
outputs vendored) are reported in graph.json["skipped"] with the reason.
"""

import argparse
import collections
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cmdclean  # noqa: E402

X64_KERNEL_TARGETS = [
    # Boot shim QEMU loads as a Linux bzImage.
    "kernel.phys32/linux-x86-boot-shim.bin",
    # Kernel package contents (see gen/zircon/kernel/image/eng/*.fini).
    "kernel.phys_x64/obj/zircon/kernel/phys/physload.zbi",
    "kernel.phys_x64/obj/zircon/kernel/phys/physboot.module.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.code-patches.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/retpoline/__x86_indirect_thunk_basic_r11.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/retpoline/__x86_indirect_thunk_unsafe_r11.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/user-copy/_x86_copy_to_or_from_user_movsb.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/user-copy/_x86_copy_to_or_from_user_movsb_smap.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/user-copy/_x86_copy_to_or_from_user_movsq.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/arch/x86/user-copy/_x86_copy_to_or_from_user_movsq_smap.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/lib/libc/string/arch/x86/memcpy_movsb.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/lib/libc/string/arch/x86/memcpy_movsq.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/lib/libc/string/arch/x86/memset_stosb.bin",
    "kernel_x64.lk_debug_level_2/obj/zircon/kernel/lib/libc/string/arch/x86/memset_stosq.bin",
    "user.basic_x64-shared/libzircon.so",
    "userboot_rust",
    "gen/zircon/kernel/version-string.txt",
    # Fuchsia C library used by the PID 1 shim (zircon_linux_init), and the
    # sysroot (headers + link stubs) the shim is compiled against.
    "user.libc_x64/libc.so",
    "gen/zircon/public/sysroot/lib/libc.so",
    "gen/zircon/public/sysroot/lib/Scrt1.o",
    "gen/zircon/public/sysroot/lib/libzircon.so",
    # Host tool that packs ZBI images (kernel.eng.zbi and the system images).
    "host_x64/zbi",
]

ARM64_KERNEL_TARGETS = [
    # Boot shim QEMU loads as a Linux ARM64 image.
    "kernel.phys_arm64/linux-arm64-boot-shim.bin",
    # Kernel package contents (see gen/zircon/kernel/image/eng/*.fini).
    "kernel.phys_arm64/obj/zircon/kernel/phys/physload.zbi",
    "kernel.phys_arm64/obj/zircon/kernel/phys/physboot.module.bin",
    "kernel_arm64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.bin",
    "kernel_arm64.lk_debug_level_2/obj/zircon/kernel/bin/vmzircon.with-tests.code-patches.bin",
    "user.basic_arm64-shared/libzircon.so",
    "userboot_rust",
    "gen/zircon/kernel/version-string.txt",
    # Fuchsia C library used by the PID 1 shim (zircon_linux_init), and the
    # sysroot (headers + link stubs) the shim is compiled against.
    "user.libc_arm64/libc.so",
    "gen/zircon/public/sysroot/lib/libc.so",
    "gen/zircon/public/sysroot/lib/Scrt1.o",
    "gen/zircon/public/sysroot/lib/libzircon.so",
    # Host tool that packs ZBI images.
    "host_x64/zbi",
]

# Edges whose program is one of these are build-system bookkeeping (no-op
# stamps, lint runs, Bazel hand-off) that produce nothing the kernel artifacts
# consume beyond an empty stamp file.
META_PROGRAMS = re.compile(
    r"(?:build/scripts/no_op\.sh|build/scripts/run_and_stamp\.sh|"
    r"build/bazel/scripts/bazel_ninja_delayed_actions\.py|"
    r"build/go/gen_hermetic_inputs\.py)"
)


def run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, check=True, capture_output=True, text=True, **kw).stdout


class _Scope:
    def __init__(self, parent=None):
        self.parent = parent
        self.vars = {}
        self.rules = {}

    def lookup(self, name):
        s = self
        while s is not None:
            if name in s.vars:
                return s.vars[name]
            s = s.parent
        return ""

    def rule(self, name):
        s = self
        while s is not None:
            if name in s.rules:
                return s.rules[name]
            s = s.parent
        return None


_VARCH = re.compile(r"[a-zA-Z0-9_-]")


def _parse_value(text):
    """Parses a ninja value into [('lit', s) | ('var', name)] tokens."""
    toks, cur, i = [], [], 0
    while i < len(text):
        c = text[i]
        if c != "$":
            cur.append(c)
            i += 1
            continue
        n = text[i + 1] if i + 1 < len(text) else ""
        if n in ("$", " ", ":"):
            cur.append(n)
            i += 2
        elif n == "{":
            j = text.index("}", i)
            toks.append(("lit", "".join(cur)))
            cur = []
            toks.append(("var", text[i + 2:j]))
            i = j + 1
        else:
            j = i + 1
            while j < len(text) and _VARCH.match(text[j]):
                j += 1
            toks.append(("lit", "".join(cur)))
            cur = []
            toks.append(("var", text[i + 1:j]))
            i = j
    toks.append(("lit", "".join(cur)))
    return toks


def _eval(toks, lookup):
    return "".join(v if k == "lit" else lookup(v) for k, v in toks)


def _shell_escape(s):
    if s and all(c.isalnum() or c in "_+-./" for c in s):
        return s
    return "'" + s.replace("'", "'\\''") + "'"


def _split_paths(text):
    """Splits a build-line path list into raw (unevaluated) path strings."""
    toks, cur, i = [], [], 0
    while i < len(text):
        c = text[i]
        if c == "$" and i + 1 < len(text):
            cur.append(text[i:i + 2])
            i += 2
            continue
        if c == " ":
            if cur:
                toks.append("".join(cur))
                cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    if cur:
        toks.append("".join(cur))
    return toks


def _find_unescaped_colon(s):
    i = 0
    while i < len(s):
        if s[i] == "$":
            i += 2
            continue
        if s[i] == ":":
            return i
        i += 1
    return -1


def parse_ninja_build_statements(out_dir):
    """Parses the ninja manifests (with variable scoping) into edges.

    Returns {output_path: edge}; each edge has rule, outputs, inputs
    (explicit + implicit + order-only), explicit in/out lists, its own
    bindings and the file scope, so rule variables such as rspfile and
    rspfile_content can be evaluated exactly as ninja does.
    """
    by_output = {}

    def parse_file(fn, scope):
        with open(os.path.join(out_dir, fn), encoding="utf-8", errors="replace") as f:
            data = f.read()
        data = re.sub(r"\$\n[ \t]*", "", data)
        lines = data.split("\n")
        i = 0
        while i < len(lines):
            line = lines[i]
            i += 1
            if not line or line.startswith("#"):
                continue
            block = []
            while i < len(lines) and lines[i].startswith((" ", "\t")):
                if lines[i].strip() and not lines[i].strip().startswith("#"):
                    block.append(lines[i].strip())
                i += 1
            if line.startswith("rule "):
                rb = {}
                for b in block:
                    k, _, v = b.partition("=")
                    rb[k.strip()] = _parse_value(v.strip())
                scope.rules[line.split()[1]] = rb
            elif line.startswith("build "):
                body = line[len("build "):]
                ci = _find_unescaped_colon(body)
                outs_raw, rest = _split_paths(body[:ci]), _split_paths(body[ci + 1:])
                # Ninja canonicalizes paths ("./x" -> "x", "a/../b" -> "b").
                ev = lambda t: os.path.normpath(_eval(_parse_value(t), scope.lookup))
                outs, exp_outs, mode = [], [], "e"
                for t in outs_raw:
                    if t == "|":
                        mode = "i"
                        continue
                    p = ev(t)
                    outs.append(p)
                    if mode == "e":
                        exp_outs.append(p)
                rule = rest[0]
                ins, exp_ins, mode = [], [], "e"
                for t in rest[1:]:
                    if t in ("|", "||"):
                        mode = t
                        continue
                    if t == "|@":
                        mode = "v"
                        continue
                    if mode == "v":
                        continue
                    p = ev(t)
                    ins.append(p)
                    if mode == "e":
                        exp_ins.append(p)
                binds = {}
                for b in block:
                    k, _, v = b.partition("=")
                    binds[k.strip()] = _eval(_parse_value(v.strip()), scope.lookup)
                edge = {"rule": rule, "outputs": outs, "inputs": ins, "exp_in": exp_ins,
                        "exp_out": exp_outs, "binds": binds, "scope": scope}
                for o in outs:
                    by_output[o] = edge
            elif line.startswith("subninja "):
                parse_file(_eval(_parse_value(line.split(None, 1)[1].strip()), scope.lookup),
                           _Scope(scope))
            elif line.startswith("include "):
                parse_file(_eval(_parse_value(line.split(None, 1)[1].strip()), scope.lookup), scope)
            elif line.startswith(("pool ", "default ")):
                continue
            elif "=" in line:
                k, _, v = line.partition("=")
                scope.vars[k.strip()] = _eval(_parse_value(v.strip()), scope.lookup)

    parse_file("build.ninja", _Scope())
    return by_output


def edge_binding(edge, name, _depth=0):
    """Evaluates a variable in an edge's environment (ninja EdgeEnv rules)."""
    if _depth > 50:
        raise ValueError("recursive ninja binding " + name)
    if name == "in":
        return " ".join(_shell_escape(p) for p in edge["exp_in"])
    if name == "in_newline":
        return "\n".join(_shell_escape(p) for p in edge["exp_in"])
    if name == "out":
        return " ".join(_shell_escape(p) for p in edge["exp_out"])
    if name in edge["binds"]:
        return edge["binds"][name]
    rule = edge["scope"].rule(edge["rule"]) or {}
    if name in rule:
        return _eval(rule[name], lambda v: edge_binding(edge, v, _depth + 1))
    return edge["scope"].lookup(name)


def parse_deps(text, out_dir=None, fuchsia_dir=None):
    deps = {}
    cur = None
    out_prefix = (out_dir.rstrip("/") + "/") if out_dir else None
    fdir_prefix = (fuchsia_dir.rstrip("/") + "/") if fuchsia_dir else None
    for line in text.splitlines():
        if not line.strip():
            continue
        if not line.startswith(" "):
            m = re.match(r"^(.*?): #deps", line)
            cur = m.group(1) if m else None
            if cur is not None:
                deps[cur] = []
        elif cur is not None:
            dep = line.strip()
            if out_prefix and dep.startswith(out_prefix):
                dep = dep[len(out_prefix):]
            elif fdir_prefix and dep.startswith(fdir_prefix):
                dep = "../../" + dep[len(fdir_prefix):]
            deps[cur].append(dep)
    return deps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fuchsia-dir", default=os.environ.get("FUCHSIA_DIR"))
    ap.add_argument("--arch", choices=["x86_64", "arm64"], default="x86_64")
    ap.add_argument("--out-dir", default=None,
                    help="Fuchsia build dir, relative to --fuchsia-dir")
    ap.add_argument("--data-dir", default=None,
                    help="Target data dir in workspace (default: zircon/kernel_build_data[_arm64])")
    ap.add_argument("--workspace", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--report", help="write a JSON report of vendored/skipped items")
    args = ap.parse_args()

    if args.out_dir is None:
        args.out_dir = "out/workbench_eng.arm64-balanced" if args.arch == "arm64" else "out/workbench_eng.x64-balanced"
    kernel_targets = ARM64_KERNEL_TARGETS if args.arch == "arm64" else X64_KERNEL_TARGETS

    fdir = os.path.abspath(args.fuchsia_dir)
    out = os.path.join(fdir, args.out_dir)
    ws = os.path.abspath(args.workspace)
    ninja = os.path.join(fdir, "prebuilt/third_party/ninja/linux-x64/ninja")
    if args.data_dir:
        data_dir = os.path.abspath(args.data_dir)
    else:
        sub = "zircon/kernel_build_data_arm64" if args.arch == "arm64" else "zircon/kernel_build_data"
        data_dir = os.path.join(ws, sub)

    print(f"[export] parsing ninja build statements for {args.arch} from {out} ...", flush=True)
    by_output = parse_ninja_build_statements(out)
    missing_targets = [t for t in kernel_targets if t not in by_output]
    if missing_targets:
        raise SystemExit(f"targets not produced by any ninja edge: {missing_targets}")
    print("[export] ninja -t compdb-targets ...", flush=True)
    compdb = json.loads(run([ninja, "-t", "compdb-targets"] + kernel_targets, out))
    cmd_by_output = {}
    for c in compdb:
        cmd_by_output[c["output"]] = c["command"]
    def classify(clean):
        if not clean:
            return "hygiene-only command"
        gen = "runs a host-built code generator (outputs vendored as generated sources)"
        for seg in clean.split(" && "):
            prog = cmdclean._program(seg)
            toks = seg.split()
            if META_PROGRAMS.search(prog):
                return "build-system bookkeeping"
            if prog.startswith(("host_x64/", "host-tools/", "./host_x64/")):
                return gen
            base = os.path.basename(prog)
            if base in ("gn_run_binary.sh",) or (prog.endswith((".py", ".pyz"))
                                                  and base != "buildid_stamp.py"):
                # Wrapper scripts that execute a host-built binary passed as
                # an argument (gn_run_binary.sh TOOLDIR host_x64/fidlc ...,
                # run_zbi.py --zbi host_x64/zbi, host_x64/.../hdrgen.pyz).
                if any(t.strip("\"'").lstrip("@").split("=")[-1].startswith(("host_x64/", "host-tools/"))
                       for t in toks[1:]):
                    return gen
        return None

    # Walk the graph from the targets. Edges that run host-built generator
    # binaries are not replayed and are not traversed (their consumed outputs
    # are vendored). Everything else, including host proc-macro crates that
    # rustc needs, is replayed from source.
    kept, skipped = [], []
    edge_state = {}
    queue, seen_paths = list(kernel_targets), set()
    mismatched = []
    while queue:
        p = queue.pop()
        if p in seen_paths:
            continue
        seen_paths.add(p)
        e = by_output.get(p)
        if e is None:
            continue
        if e["rule"] == "phony":
            queue.extend(e["inputs"])
            continue
        if id(e) in edge_state:
            continue
        # Evaluate the command with our ninja evaluator and cross-check it
        # against ninja's own expansion (compdb-targets omits edges without
        # explicit inputs, e.g. empty archives; those use ours).
        cmd = edge_binding(e, "command")
        ref = next((cmd_by_output[o] for o in e["outputs"] if o in cmd_by_output), None)
        if ref is not None and ref.strip() != cmd.strip():
            mismatched.append((e["outputs"][0], ref, cmd))
            cmd = ref
        clean = cmdclean.clean_command(cmd)
        reason = classify(clean)
        rec = {"outputs": e["outputs"], "orig": cmd, "cmd": clean, "raw_inputs": e["inputs"]}
        # Ninja writes the rule's response file itself before running.
        depfile = edge_binding(e, "depfile")
        if depfile:
            rec["depfile"] = depfile
        rspfile = edge_binding(e, "rspfile")
        if rspfile:
            rec["rspfile"] = {"path": rspfile, "content": edge_binding(e, "rspfile_content")}
        edge_state[id(e)] = rec
        if reason:
            rec["reason"] = reason
            skipped.append(rec)
        else:
            kept.append(rec)
            queue.extend(e["inputs"])
    if mismatched:
        print(f"[export] ERROR: ninja evaluator disagrees with ninja on {len(mismatched)} commands")
        for o, r, c in mismatched[:3]:
            print("  ", o, "\n    ninja:", r[:300], "\n    ours: ", c[:300])
        return 1
    print("[export] ninja evaluator matches ninja's command expansion for all edges", flush=True)

    def expand(path, seen):
        e = by_output.get(path)
        if e is None or e["rule"] != "phony":
            return [path]
        if path in seen:
            return []
        seen.add(path)
        res = []
        for i in e["inputs"]:
            res.extend(expand(i, seen))
        return res

    for rec in kept:
        rec["inputs"] = sorted(set(p for i in rec.pop("raw_inputs") for p in expand(i, set())))
    for rec in skipped:
        rec.pop("raw_inputs", None)

    kept_outputs = set(o for e in kept for o in e["outputs"])
    print(f"[export] edges: kept={len(kept)} skipped={len(skipped)}", flush=True)
    print("[export] kept by toolchain:", dict(collections.Counter(
        e["outputs"][0].split("/")[0] if "/" in e["outputs"][0] else "." for e in kept)))
    print("[export] skipped by reason:", dict(collections.Counter(s["reason"] for s in skipped)))
    abs_cmds = [e["outputs"][0] for e in kept if fdir in e["cmd"]]
    if abs_cmds:
        print(f"[export] WARNING: {len(abs_cmds)} commands reference the checkout path, e.g. {abs_cmds[:3]}")

    print("[export] ninja -t deps ...", flush=True)
    all_kept_outs = sorted(kept_outputs)
    deps = {}
    for i in range(0, len(all_kept_outs), 2000):
        deps.update(parse_deps(run([ninja, "-t", "deps"] + all_kept_outs[i:i + 2000], out), out, fdir))
    for e in kept:
        d = set()
        for o in e["outputs"]:
            d.update(deps.get(o, []))
        e["deps"] = sorted(d - set(e["inputs"]))
    visited_outputs = set(o for r in kept + skipped for o in r["outputs"])
    unvisited = sorted(set(p for e in kept for p in e["deps"]
                           if p in by_output and by_output[p]["rule"] != "phony"
                           and p not in visited_outputs))
    if unvisited:
        print(f"[export] ERROR: {len(unvisited)} depfile deps are produced by edges not in the "
              f"declared graph closure, e.g. {unvisited[:5]}")
        return 1

    # Classify every path the kept edges read.
    dir_outputs = [o for o in kept_outputs if os.path.isdir(os.path.join(out, o))]

    def under_dir_output(p):
        return any(p.startswith(d + "/") for d in dir_outputs)

    vendor_out = set()
    sources = set()
    prebuilt = collections.Counter()
    for e in kept:
        for p in e["inputs"] + e["deps"]:
            if p.startswith("../../prebuilt/"):
                prebuilt["/".join(p.split("/")[2:6])] += 1
            elif p.startswith("../../"):
                s = p[len("../../"):]
                # Wrapper/hygiene scripts that cmdclean strips are never run.
                if not re.match(r"build/(rbe/|tracer/restat_cacher\.py|"
                                r"toolchain/verify_depfile\.py)", s):
                    sources.add(s)
            elif p not in kept_outputs and not under_dir_output(p):
                vendor_out.add(p)

    binaries = []
    # Argument files referenced as `@file` / `@shell:file` that are neither
    # ninja rspfiles nor edge outputs are GN-time files: vendor them too.
    for e in kept:
        own = (e.get("rspfile") or {}).get("path")
        for m in re.finditer(r"@(?:shell:)?([^\s\"'@]+)", e["cmd"]):
            f = m.group(1)
            if f == own or f in kept_outputs or f.startswith("../../") or under_dir_output(f):
                continue
            if os.path.isfile(os.path.join(out, f)):
                vendor_out.add(f)
    for p in vendor_out:
        f = os.path.join(out, p)
        if os.path.isfile(f):
            with open(f, "rb") as fh:
                head = fh.read(8)
            if head.startswith(b"\x7fELF") or head.startswith(b"!<arch>"):
                binaries.append(p)
    if binaries:
        print(f"[export] ERROR: refusing to vendor compiled binaries: {binaries[:10]}")
        return 1

    missing_src = sorted(s for s in sources if not os.path.exists(os.path.join(ws, s)))
    missing_upstream = [s for s in missing_src if not os.path.exists(os.path.join(fdir, s))]
    vendor_missing = sorted(p for p in vendor_out if not os.path.exists(os.path.join(out, p)))
    print(f"[export] sources referenced={len(sources)} missing-in-tree={len(missing_src)} "
          f"missing-upstream={len(missing_upstream)}")
    print("[export] missing-in-tree by dir:", dict(collections.Counter(
        "/".join(s.split("/")[:3]) for s in missing_src).most_common(40)))
    print(f"[export] out-dir files to vendor={len(vendor_out)} (absent upstream: {len(vendor_missing)})")
    print("[export] vendor by dir:", dict(collections.Counter(
        "/".join(p.split("/")[:3]) for p in vendor_out).most_common(40)))
    print("[export] prebuilt toolchain paths:", dict(prebuilt))
    for p in vendor_missing[:20]:
        print("   absent:", p)
    if args.report:
        skipped_by_out = {o: s for s in skipped for o in s["outputs"]}
        with open(args.report, "w") as f:
            json.dump({
                "vendor_out": {p: (skipped_by_out[p]["reason"] + " :: " + skipped_by_out[p]["cmd"][:300]
                                   if p in skipped_by_out else "GN-time file (no producing edge)")
                               for p in sorted(vendor_out)},
                "skipped": [{"outputs": s["outputs"], "reason": s["reason"], "cmd": s["cmd"][:500]}
                            for s in skipped],
                "missing_src": missing_src,
                "prebuilt_refs": sorted(set(p for e in kept for p in e["inputs"] + e["deps"]
                                            if p.startswith("../../prebuilt/")
                                            and "/clang/" not in p and "/rust/linux" not in p)),
            }, f, indent=1)
    if args.dry_run:
        return 0

    for s in missing_src:
        src = os.path.join(fdir, s)
        if not os.path.exists(src):
            continue
        dst = os.path.join(ws, s)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    vend_root = os.path.join(data_dir, "out")
    if os.path.isdir(vend_root):
        shutil.rmtree(vend_root)
    leaked = []
    for p in sorted(vendor_out):
        src = os.path.join(out, p)
        if not os.path.isfile(src):
            continue
        dst = os.path.join(vend_root, p)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        try:
            with open(dst, "rb") as f:
                if fdir.encode() in f.read():
                    leaked.append(p)
        except OSError:
            pass
    # Compact encoding: every path is interned once in graph["paths"] and
    # edges refer to paths by index. The original (wrapped) commands are not
    # stored; re-run this exporter to see them.
    paths, index = [], {}

    def pid(p):
        if p not in index:
            index[p] = len(paths)
            paths.append(p)
        return index[p]

    enc_edges = []
    for e in kept:
        rec = {"cmd": e["cmd"], "out": [pid(o) for o in e["outputs"]],
               "in": [pid(i) for i in e["inputs"]], "dep": [pid(d) for d in e["deps"]]}
        if "rspfile" in e:
            rec["rsp"] = e["rspfile"]
        if "depfile" in e:
            rec["depfile"] = e["depfile"]
        enc_edges.append(rec)
    graph = {
        "provenance": {
            "arch": args.arch,
            "fuchsia_dir_basename": os.path.basename(fdir),
            "out_dir": args.out_dir,
            "targets": kernel_targets,
            "note": "Generated by bazel/kernel_build/export_fuchsia_kernel_graph.py",
        },
        "prebuilt_toolchain_paths": sorted(prebuilt),
        "paths": paths,
        "edges": enc_edges,
        "skipped": [{"outputs": s["outputs"], "reason": s["reason"]} for s in skipped],
        "vendored_out_files": sorted(p for p in vendor_out if os.path.isfile(os.path.join(out, p))),
    }
    import gzip
    old_json = os.path.join(data_dir, "graph.json")
    if os.path.exists(old_json):
        os.remove(old_json)
    with gzip.open(os.path.join(data_dir, "graph.json.gz"), "wt", compresslevel=9) as f:
        json.dump(graph, f, separators=(",", ":"), sort_keys=True)
    print(f"[export] wrote graph.json.gz; copied {len(missing_src) - len(missing_upstream)} sources; "
          f"vendored {len(graph['vendored_out_files'])} out files")
    if leaked:
        print("[export] WARNING: absolute checkout path found in vendored files:")
        for p in leaked[:50]:
            print("   ", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
