# Zircon kernel build data

This directory lets Bazel build the whole Zircon side of the system from the
sources in this repository. `//zircon:kernel_build` produces:

* the Linux/x86 boot shim
* physload, physboot, and the kernel proper (`vmzircon`, both its C++ and its
  Rust crates, including `core`/`compiler_builtins` built from the Rust
  toolchain's library sources)
* the code-patch blobs, the vDSO (`libzircon.so`), and userboot
* the `kernel.eng.zbi` package
* the Fuchsia `libc.so` and sysroot that the PID 1 shim is compiled against
* the `zbi` host tool

## Contents

| Path | What it is |
|---|---|
| `graph.json.gz` | Every build edge needed for those artifacts. Each edge records the real compiler, `rustc`, linker, `ar`, objcopy or in-tree script command line; its outputs, inputs and depfile-discovered dependencies; and any ninja response file it uses. Paths are interned in `paths[]`. |
| `out/...` | Files the replayed edges read from the output directory but that no replayed edge produces. See below. |

The files under `out/` fall into two groups:

* **GN-time files**: response files, Rust target specs, config JSON and the
  sysroot header copies, all written by `gn gen`.
* **Generated sources**: the output of host-built code generators that are
  not replayed (`fidlc`, `fidlgen_*`, `zither`, the hwreg header generators,
  and libc `hdrgen`). This is the usual practice for checked-in generated
  code. The exporter refuses to vendor any ELF file or archive.

## How it was produced

`bazel/kernel_build/export_fuchsia_kernel_graph.py` was run once against a
Fuchsia checkout configured with `fx set` and built with `fx build`. For the
target list, see `KERNEL_TARGETS` in that script. The exporter does four things:

1. It parses the ninja manifests with full variable scoping and evaluates
   every command. Its evaluation is cross-checked against ninja's own
   `compdb-targets` output, and the export aborts on any mismatch.
2. It strips only the RBE/hygiene wrappers (see
   `bazel/kernel_build/cmdclean.py`).
3. It replaces `buildidtool` with the equivalent
   `bazel/kernel_build/buildid_stamp.py`.
4. It copies any source file that is missing from this tree.

## How it is built

`bazel/kernel_build/replay.py` runs the edges in dependency order, with these
inputs and checks:

* **Inputs**: the commands run against this repository's sources. The only
  external inputs are the toolchains, located by `bazel/zircon_repo.bzl`:
  clang/lld, rustc plus its library sources, rust-bindgen, a Linux host
  sysroot (used for host proc-macros and the `zbi` tool), and python3.
* **Provenance check**: after the build, every depfile is scanned. The build
  fails if any input came from outside this repository, the build directory or
  those toolchains.
* **Reproducibility evidence**: the current rebuild records artifact hashes
  and checks depfile provenance. Bitwise equality with a separate upstream
  build has not been revalidated for the current tree; hashes alone do not
  establish that comparison.
* **Exported sysroot**: GN forwarding headers are materialized into real header
  contents before export, so the resulting sysroot can be relocated. Stale GN
  aliases whose source is absent are omitted with a diagnostic. The unused
  `zircon/device/bt-hci.h` alias refers to a header absent from the pinned source
  revision; no replacement API is invented.

One deliberate difference from upstream: `kernel.eng.zbi` does not package
the `userboot-test-rust` test binary.
