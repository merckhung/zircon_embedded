# Corresponding runtime sources

This directory retains corresponding sources for glibc 2.31-13+deb11u5 and
GCC 10.2.1-6 (the source of libgcc_s), including Debian's patch archives.
`manifest.json` records upstream URLs, original hashes and published hashes.

The glibc archives and Debian patch archives are unmodified. The GCC original
archive was prepared for publication by replacing workstation home prefixes in
four upstream Go trace fixtures with equal-length neutral source prefixes.
Archive owner metadata was normalized. No compiler/runtime source contents or
copyright/license notices were changed. `publication-redactions.json` identifies
the fixture changes and `publication-source-verification.json` records an
independent comparison with the original source archive.

The original signed GCC source descriptor is retained under
`upstream-descriptors/`. The descriptor at this directory's root is an unsigned
publication derivative with checksums updated for the sanitized archive; it is
not presented as a Debian-signed release. The following extraction commands use
the published descriptors.

To unpack the Debian sources, install Debian development tools and run:

```sh
dpkg-source -x glibc_2.31-13+deb11u5.dsc
dpkg-source -x gcc-10_10.2.1-6.dsc
```

Debian's `debian/rules`, patches and package documentation are included in
those source packages. Runtime libraries come from unmodified Debian Bullseye
binary packages at the versions selected by Chromium's pinned build sysroots.
Chromium removes executable sections from its sysroot libraries to reduce their
size for linking. Those stubs cannot be used as runtime libraries.
See each browser bundle's `runtime-manifest.json` for the exact library,
package version, download URL, package hash and build-sysroot selection hash.
`../runtime-packages.json` pins all downloaded binary packages.

Chromium modifies glibc's default exported symbol versions in its build sysroot;
the packaged runtime libraries retain Debian's original symbol versions and code.
`reversion_glibc.py` and `sysroot_creator.py`, copied from the pinned Chromium
revision, preserve that modification and the sysroot assembly recipe under
`CHROMIUM-LICENSE`. Their other inputs are in the pinned Chromium checkout's
`build/linux/sysroot_scripts/` directory. Recreate that checkout using
`../patches/README.md`.

Runtime copyrights and licenses for every packaged library are retained in
`../../docs/licenses/runtime/`. To replace or relink the LGPL libraries, supply
compatible shared libraries in `chromium/prebuilt/lib/` or
`chromium/prebuilt_arm64/lib/` and rebuild the corresponding Chromium rootfs
with Bazel; the files are packaged as separate dynamically loaded ELF libraries.

To reproduce the GCC publication changes from the pinned upstream archive, keep
the pristine download outside this repository and run from the project root:

```sh
python3 tools/sanitize_runtime_sources.py \
  --upstream-archive /path/to/private/gcc-10_10.2.1.orig.tar.xz \
  --staging-dir /path/to/private/source-work
```

The script verifies the original archive hash, preserves trace string lengths,
retains the original descriptor and updates the published artifact checksums.
