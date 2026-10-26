"""Command normalization shared by the kernel graph exporter and replayer.

The Fuchsia GN/ninja build wraps every tool invocation in remote-execution
(RBE) and hygiene wrappers. Those wrappers only decide *where* a command runs
and add post-checks; with `--exec_strategy=local` they execute the inner
command unchanged. This module strips exactly those wrappers and nothing
else, so the replayed command line is the real compiler/linker/tool command
that the upstream build runs.

Stripped:
  * `python3 ../../build/rbe/<x>_remote_wrapper.py [wrapper flags] -- CMD`
      -> `CMD`
  * `python3 ../../build/rbe/output_leak_scanner.py [flags] -- CMD` -> `CMD`
  * `python3 ../../build/tracer/hermetic_inputs_action.py [flags] -- CMD`
      -> `CMD` (an input-tracing check around the real action)
  * `--remote-*` flags that `rustc_remote_wrapper.py` consumes from the
    rustc command line (they are not rustc flags).
  * trailing `&& verify_depfile.py ...` and `&& restat_cacher.py ...`
    (build-hygiene checks that do not produce build outputs).

Replaced:
  * `&& buildidtool -stamp X.build-id.stamp ...` (a prebuilt host tool) by
    the in-tree `bazel/kernel_build/buildid_stamp.py` with the same
    arguments: later steps read the build-ID stamp it writes.
"""

import re

_WRAPPER_RE = re.compile(
    r"build/(?:rbe/(?:[a-z_]+_remote_wrapper|output_leak_scanner)|tracer/(?:hermetic_inputs_action|restat_cacher))\.py(?:\s|$)"
)
_BUILDIDTOOL = "../../prebuilt/tools/buildidtool/linux-x64/buildidtool"
_BUILDID_STAMP = ("../../prebuilt/third_party/python3/linux-x64/bin/python3.13 -S "
                  "../../bazel/kernel_build/buildid_stamp.py")
_DROP_SEGMENT_RE = re.compile(
    r"(?:build/toolchain/verify_depfile\.py|"
    r"build/tracer/restat_cacher\.py)"
)
_REMOTE_FLAG_RE = re.compile(r"(?:(?<=\s)|^)--remote-[^\s]*")


def _split_and(cmd):
    """Splits a shell command on top-level ' && ' (outside quotes)."""
    parts = []
    cur = []
    i = 0
    quote = None
    n = len(cmd)
    while i < n:
        c = cmd[i]
        if quote:
            cur.append(c)
            if c == "\\" and i + 1 < n:
                cur.append(cmd[i + 1])
                i += 2
                continue
            if c == quote:
                quote = None
            i += 1
            continue
        if c in ("'", '"'):
            quote = c
            cur.append(c)
            i += 1
            continue
        if c == "\\" and i + 1 < n:
            cur.append(c)
            cur.append(cmd[i + 1])
            i += 2
            continue
        if cmd.startswith(" && ", i):
            parts.append("".join(cur).strip())
            cur = []
            i += 4
            continue
        cur.append(c)
        i += 1
    if cur:
        parts.append("".join(cur).strip())
    return [p for p in parts if p]


def _unwrap(segment):
    """Returns the inner command of a wrapper invocation (repeatedly)."""
    while " -- " in segment and _WRAPPER_RE.search(segment.split(" -- ", 1)[0]):
        segment = segment.split(" -- ", 1)[1].strip()
    return segment


def _program(segment):
    """Returns the program (or python script) a shell segment runs."""
    toks = segment.split()
    i = 0
    # Skip leading VAR=value environment assignments.
    while i < len(toks) and re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", toks[i]):
        i += 1
    if i >= len(toks):
        return ""
    prog = toks[i]
    if re.search(r"python3(\.\d+)?$", prog):
        for t in toks[i + 1:]:
            if t.endswith(".py") or t.endswith(".pyz"):
                return t
    return prog


def clean_command(cmd):
    """Returns the real tool command for an upstream ninja command line."""
    out = []
    for seg in _split_and(cmd):
        seg = _unwrap(seg)
        if _DROP_SEGMENT_RE.search(_program(seg)):
            continue
        if _program(seg) == _BUILDIDTOOL:
            # buildidtool's stamp output is consumed by later steps; produce
            # it with an equivalent in-tree script (see buildid_stamp.py).
            out.append(_BUILDID_STAMP + seg[len(_BUILDIDTOOL):])
            continue
        seg = _REMOTE_FLAG_RE.sub("", seg)
        seg = seg.strip()
        if seg:
            out.append(seg)
    return " && ".join(out)
