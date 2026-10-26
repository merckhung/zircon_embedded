# Rebuilding Chromium for the Zircon Linux ABI shim

The source revision is pinned in `REVISION` (Chromium 158.0.8107.0). The patch
filename retains its historical `x64` suffix; the same source patch and GN
configuration are used for x64 and ARM64. Both browser bundles were rebuilt and
pass the secure-page/renderer QEMU check. See `../../docs/REBUILD.md`.

## Checkout and build

Install Chromium's documented Linux build dependencies and depot_tools. Keep
the checkout outside this project to avoid publishing source caches and local
build metadata. From an empty external build directory:

```sh
fetch --nohooks chromium
cd src
git checkout 7cc9d0d833549df75d31f9049130b05e3d1c9f84
gclient sync -D --revision src@7cc9d0d833549df75d31f9049130b05e3d1c9f84
```

From this project directory, build each architecture:

```sh
python3 chromium/build.py --checkout /path/to/chromium/src --arch x64 --jobs 8
python3 chromium/build.py --checkout /path/to/chromium/src --arch arm64 --jobs 8
```

The script checks HEAD against `REVISION`, checks/applies the patch, substitutes
`target_cpu`, generates GN files with unused arguments treated as errors, and
builds `chrome` and `chrome_crashpad_handler` using the checkout's Ninja.
Outputs are in `out/ZirconLinux_x64` and `out/ZirconLinux_arm64`.
Build and packaging helpers share an architecture-specific lock. Packaging
refuses an active build before inspecting Ninja outputs or depfiles.
On resuming an interrupted build, the helper removes empty compiler outputs
under that lock so Ninja recompiles them rather than linking invalid files.

## Configuration and patch scope

The browser uses Aura/Views and Ozone X11, bundled BoringSSL and Chrome Root
Store, with ANGLE linked statically into Chrome and SwiftShader/Vulkan libraries
bundled. The pinned build emits infrastructure-only EGL/GLES shared stubs; these
are excluded from the runtime bundle. The current guest launch uses CPU page
painting with GPU paths disabled; WebGL/Vulkan runtime support is not established
by these checks. GLib, GTK, Qt, D-Bus, udev, CUPS, ALSA,
PulseAudio and NSS integrations are disabled in `args.gn`. The source patch
adds guards and fallback implementations for optional integrations and adapts
X11 selection/drag and EGL display handling. These changes need validation by
building and booting both targets, including actual page rendering.

Changes which previously suppressed inotify startup, invented memory sizes,
invented Crashpad metadata or ignored crash-handler startup failure have been
removed. Failures in those upstream paths must be addressed in the shim if
runtime verification exposes them.

## Packaging requirements

After each build completes, run:

```sh
python3 chromium/package_runtime.py --checkout /path/to/chromium/src \
  --arch x64 --package-cache /path/to/private/debian-packages
python3 chromium/package_browser.py --checkout /path/to/chromium/src \
  --arch x64 --staging-dir /path/to/private/packaging \
  --deny-file /path/to/private/identities.txt
```

Repeat with `--arch arm64`. Staging must be outside this project and on the
same filesystem as the bundle so directory renames can preserve the prior
bundle. The helper refuses incomplete Ninja targets, validates ELF architecture
and runtime hashes, generates Chromium component notices, strips binaries,
records `build-manifest.json`, and audits the staged bundle before installing it.
Runtime packaging selects versions from the build sysroot but extracts executable
libraries from SHA-256-pinned Debian packages. It rejects sysroot link-only stubs.

Replace each architecture's complete bundle together: `chrome`, crash handler,
SwiftShader/Vulkan shared libraries and Vulkan ICD, ICU/V8 data, resource packs
and `locales/en-US.pak`. Strip executable debug data with the checkout's
`llvm-strip`; scan the resulting payloads for private build paths and identities.
Keep Chromium's license and generated bundled-component notices alongside the
binaries. Runtime libraries must have a pinned redistributable source and
corresponding license/source instructions. Record artifact hashes and GN args.

Both browser bundles have been rebuilt and replaced. Both QEMU checks pass with
secure Yahoo HTTP 200 documents, fresh host renderer challenges and inspected
QMP captures. Compilation alone is not a QEMU PASS.
