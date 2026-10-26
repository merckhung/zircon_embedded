# Rebuild and publication status

Both kernels, syscall shims, minimal images
and desktop images have been rebuilt. All four minimal/desktop QEMU targets pass.
The x64 Chromium source build is complete and its bundle has been replaced.
Its first QEMU run exposed link-only sysroot libraries incorrectly packaged as
runtimes. Executable Debian payloads now replace those stubs, including their
complete dependency closure. A subsequent worker-thread fault was traced to
Chromium dynamically loading Alpine's musl-linked Xcursor into glibc. Matching
Debian Xcursor payloads replace that fallback. The x64 QEMU recheck with the normal shim
passes, with a secure Yahoo HTTP 200 document and a fresh renderer nonce/product.
Both browser builds, complete bundle replacements and QEMU checks now pass,
with fresh screenshots inspected. The final x64 recheck with updated probe
diagnostics also passed in 97.9 seconds.

## Current results

| Component | x86_64 | ARM64 |
|---|---|---|
| Kernel/bootstrap build | 3,793 graph edges; 3,551 depfiles checked | 3,512 graph edges; 3,310 depfiles checked |
| Minimal QEMU variant | PASS: 49 memory and 41 process checks, zx_test, interactive shell | PASS: 49 memory and 40 process checks, zx_test, interactive shell |
| Desktop QEMU variant | PASS: X11/WM state, XTEST-to-PTY, QMP PS/2 input | PASS: X11/WM state and XTEST-to-PTY input |
| Chromium | PASS: rebuilt bundle, secure Yahoo HTTP 200 document, fresh renderer challenge and inspected QMP capture | PASS: rebuilt bundle, secure Yahoo HTTP 200 document, fresh renderer challenge and inspected QMP capture |

`rebuild-evidence.json` records artifact/screenshot hashes and the current test
results. Four fresh desktop/terminal captures and both Chromium captures were
inspected. ARM64 has no hardware input device in the current configuration; its
input proof is XTEST, not the x86_64 PS/2 path. Base-target wall times include
Bazel build work; the Chromium time covers guest verification.

## Source and toolchain pins

* Fuchsia source revision: `adb2125e89f5080f5d52b17453f3aeb5d834ca7d`, recovered
  from the exported graph's version-generation command.
* `bazel/toolchains.ensure` pins the corresponding official CIPD toolchains.
  Fetch outside the project with
  `cipd ensure -root /path/to/toolchains -ensure-file bazel/toolchains.ensure`.
  Set `FUCHSIA_DIR=/path/to/toolchains` in ignored `user.bazelrc`.
* Chromium: `chromium/patches/REVISION`, version 158.0.8107.0. A separate clean
  checkout accepts the corrected patch. All pinned Git dependency revisions
  matched, with 168 dependency worktrees checked for tracked source changes;
  gclient-managed submodule revisions were checked independently.
  `chromium/patches/dependencies.json` records Git revisions and CIPD instances.
* Host dependencies include Bazel, Python 3, native g++, ARM64 cross g++, and
  the Linux development headers used by the static diagnostics.

## Repairs validated by the base rebuild

GN forwarding headers could not be copied verbatim to a relocated sysroot.
The exporter now materializes 213 forwarding headers per architecture, checking
the same source/build/toolchain roots as dependency provenance. The unused
`zircon/device/bt-hci.h` alias is omitted because its source is absent from the
pinned Fuchsia tree; no replacement API is invented. The missing LLVM libc
malloc-macro header was restored from its pinned upstream revision, recorded
in `restored-sysroot-headers.json`. Relocated-header smoke tests and the full
shim/image builds pass on both architectures.

ARM64 desktop startup initially timed out before Xfwm4 registered. Startup now
waits for the real EWMH handshake and WM_S0 owner before launching desktop clients.
ARM64 TCG runs have a longer readiness deadline; window, process, terminal and
input assertions remain in place. Both desktop verification targets pass with
this sequencing.

Host reboots interrupted earlier builds and left zero-length linker-cache files.
Those caches were removed; replay progress now has atomic checkpoints. Long
browser builds ran in resumable user services kept outside the publication tree.
All four temporary build/verification service definitions have been removed.
Both browser builds, bundle replacements and QEMU verification have completed.

## Chromium patch and verification gates

Removed changes that suppressed the inotify reader while returning success,
invented physical-memory sizes, supplied invented Crashpad product metadata,
and treated crash-handler startup failure as success. Remaining changes configure
optional desktop integrations and graphics/certificate support. Compilation,
packaging and guest runtime checks pass on both architectures.

A title containing the destination hostname and NIC traffic could accept an
error page. The new `chromium_page_probe` uses guest-loopback DevTools to inspect
the actual secure Yahoo document, successful HTTP status, readiness and visible
text. After guest readiness the host sends an unpredictable nonce and factors;
the renderer must return that nonce and the correct JavaScript product before
verification captures its screenshot. Eight host-side evidence-gate tests pass,
including stale/echoed input, wrong answers, error documents and HTTP 429.
The probe compiles for both architectures. The first rebuilt ARM64 run loaded
a secure Yahoo HTTP 200 document and rendered the page, but its later fresh host
challenge failed. The probe used a three-second socket deadline. ARM64 now allows
30 seconds for socket I/O and 120 seconds for the host challenge; the x64 limits
remain three and 25 seconds. Socket failures now report their error code. The
document, nonce and arithmetic assertions are unchanged. The ARM64 rerun passed
in 193.3 seconds, including the fresh host challenge and an inspected capture.
The current launch disables GPU and crash-reporter paths;
WebGL/Vulkan and crash-reporting runtime support are not claimed.

Both runtime-library sets now come from unmodified, pinned Debian binary packages
matching the build-sysroot versions. Chromium's sysroot libraries have executable
sections removed for linking and cannot run. The first fresh x64 boot caught that
packaging error; runtime packaging now validates executable load segments and
interpreter entry coverage. Versions, package URLs and hashes are recorded in
each bundle's `runtime-manifest.json` and `chromium/runtime-packages.json`. Chromium,
project and runtime notices are packaged in the Chromium images. See
`LICENSING.md` for license scope and corresponding-source information.

Each architecture now has 28 executable runtime libraries, with their recursive
DT_NEEDED dependencies and explicitly loaded Xcursor/NSS libraries included.
Browser packaging rejects missing dependencies and excludes upstream shared
EGL/GLES infrastructure stubs because ANGLE is statically linked into Chrome.
The x64 bundle reports Chromium 158.0.8107.0 with its own loader and libraries on
the host, including with Debian Xcursor preloaded. The subsequent x64 QEMU run
passed; temporary fault tracing was removed before that run.

## Publication privacy

Real identities are kept in an external private deny list, not embedded or
obfuscated in the published audit tools. Run:

```sh
python3 tools/audit_publication.py --deny-file /path/to/private/identities.txt
python3 tools/audit_source_archives.py \
  --deny-file /path/to/private/identities.txt \
  --report docs/source-archive-privacy.json
```

The checks cover configured tokens in plain, UTF-16 and UTF-32 form, home paths,
compressed build graphs, nested source tar payloads and archive metadata. Invalid
upstream tar regression fixtures are scanned as raw bytes and identified in the
archive report. These checks do not replace final binary and screenshot review.
Six privacy-test cases pass, including encoded identities, private filenames,
missing publication scope and false-positive controls for public URLs.

A recursive source audit found four upstream Go trace fixtures with workstation
home prefixes. Their 74 prefixes were neutralized with equal-length replacements.
An independent comparison found 102,104 unchanged file contents, only the four
fixture changes, and one added modification notice. Copyright/license contents
and compiler/runtime source contents were preserved. The published GCC descriptor
is explicitly unsigned, with the original signed descriptor retained for provenance.
All corresponding-source hashes and `dpkg-source` extraction passed. Details and
reproduction are in `chromium/runtime_sources/README.md`; the final archive audit
reports no configured-token findings.

Git LFS attributes cover large browser executables and corresponding-source
archives. Install Git LFS and run `git lfs pull` after cloning the finished repo.
Private logs, caches and toolchain configuration remain outside Git.

## Publication verification

All six QEMU targets pass. Both Chromium bundles were rebuilt from the pinned
patched source and replaced together with their assets, runtime libraries and
notices. Six fresh QMP screenshots were inspected. Kernel artifacts, images,
browser payloads and corresponding-source archives have recorded SHA-256 values.

The final privacy check covered 12,477 staged files twice: first the exact Git
blob bytes, then the six LFS payloads after checking each pointer, size and hash.
Both scans reported zero findings. The four source archives match the hashes
in the recursive metadata/payload audit. All four temporary build/verification
services were removed. Private build logs, caches, identities and configuration
are excluded from publication.

The initial publication uses one root commit.
No remote or push is part of this preparation. Git LFS payloads must accompany
any later GitHub publication.
