# Licensing and bundled components

Original project code and build/verification scripts are distributed under the
BSD 2-Clause terms in `LICENSE`. Existing upstream copyright notices and licenses
remain in effect for imported code. The top-level license does not replace them.

* Fuchsia userspace, SDK and support code: BSD-style terms; the original root
  license is preserved in `docs/licenses/FUCHSIA-LICENSE` and patent notice in
  `PATENTS`.
* Zircon kernel and scripts: MIT-style terms in `zircon/kernel/LICENSE` and
  `zircon/scripts/LICENSE`; system code: `zircon/system/LICENSE`.
* Other Zircon components: see `zircon/LICENSE` and component license files.
* BoringSSL, LLVM libc, Scudo/GWP-ASan, LZ4, Zstandard, RapidJSON and Rust crates:
  license texts are retained in their respective source directories. For split
  include/source trees, the license in the component's `src` directory applies
  to both. `docs/upstream-license-provenance.json` records upstream URLs and
  SHA-256 hashes for restored license files.
* Chromium and its bundled dependencies: each browser bundle includes `LICENSE`
  and generated `THIRD_PARTY_NOTICES.html`; these are also packaged in the
  Chromium rootfs image.
* The glibc runtime libraries and Alpine packages retain their own component
  licenses. Binary redistribution also requires the corresponding source and
  notices required by those licenses, including GPL/LGPL components.

Both browser bundles include runtime licenses and manifests for the pinned Debian
packages selected to match Chromium's build sysroot. The runtime binaries are
unmodified Debian payloads; the build sysroot's link-only stubs are not runtimes.
`chromium/runtime_sources/` retains glibc/GCC corresponding
source archives, hashes, Debian patches and the Chromium sysroot modification
recipe. See its README for unpacking and library replacement instructions.

Both browser bundles have been rebuilt and verified in QEMU. The final
staged-content audit covers the notices, corresponding-source archives and
actual LFS payloads; results are recorded in `rebuild-evidence.json`.
