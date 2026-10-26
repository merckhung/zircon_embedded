#!/usr/bin/env python3
"""Writes an ELF build-ID stamp file, replacing upstream's `buildidtool`.

The upstream Fuchsia build appends
    buildidtool -build-id-dir .build-id -stamp OUT.build-id.stamp \
        -entry =BINARY -entry .debug=BINARY
to link steps. Besides populating a `.build-id/xx/yyyy` debug-symbol cache
for debuggers (not needed here, so not reproduced), it writes the binary's
ELF NT_GNU_BUILD_ID as lowercase hex into the stamp file. Later build steps
consume that stamp: e.g. zircon/kernel/phys/kernel-elf-interp.sh embeds
physload's build ID into physboot's PT_INTERP and physboot's into vmzircon's,
which physload/physboot verify at boot.

This script accepts the same command line and produces the same stamp
(empty when the binary has no build ID, e.g. links using --build-id=none).
"""

import struct
import sys


def elf_build_id(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"\x7fELF":
        raise SystemExit(f"{path}: not an ELF file")
    is64 = data[4] == 2
    end = "<" if data[5] == 1 else ">"
    if is64:
        shoff, = struct.unpack_from(end + "Q", data, 0x28)
        shentsize, shnum = struct.unpack_from(end + "HH", data, 0x3A)
        phoff, = struct.unpack_from(end + "Q", data, 0x20)
        phentsize, phnum = struct.unpack_from(end + "HH", data, 0x36)
    else:
        shoff, = struct.unpack_from(end + "I", data, 0x20)
        shentsize, shnum = struct.unpack_from(end + "HH", data, 0x2E)
        phoff, = struct.unpack_from(end + "I", data, 0x1C)
        phentsize, phnum = struct.unpack_from(end + "HH", data, 0x2A)

    def notes(off, size):
        p = off
        while p + 12 <= off + size:
            namesz, descsz, ntype = struct.unpack_from(end + "III", data, p)
            name = data[p + 12:p + 12 + namesz]
            dstart = p + 12 + ((namesz + 3) & ~3)
            desc = data[dstart:dstart + descsz]
            yield name, ntype, desc
            p = dstart + ((descsz + 3) & ~3)

    regions = []
    for i in range(shnum):
        b = shoff + i * shentsize
        if is64:
            stype, = struct.unpack_from(end + "I", data, b + 4)
            off, size = struct.unpack_from(end + "QQ", data, b + 0x18)
        else:
            stype, = struct.unpack_from(end + "I", data, b + 4)
            off, size = struct.unpack_from(end + "II", data, b + 0x10)
        if stype == 7:  # SHT_NOTE
            regions.append((off, size))
    for i in range(phnum):
        b = phoff + i * phentsize
        ptype, = struct.unpack_from(end + "I", data, b)
        if ptype == 4:  # PT_NOTE
            if is64:
                off, = struct.unpack_from(end + "Q", data, b + 8)
                size, = struct.unpack_from(end + "Q", data, b + 0x20)
            else:
                off, = struct.unpack_from(end + "I", data, b + 4)
                size, = struct.unpack_from(end + "I", data, b + 0x10)
            regions.append((off, size))
    for off, size in regions:
        for name, ntype, desc in notes(off, size):
            if name.rstrip(b"\0") == b"GNU" and ntype == 3:  # NT_GNU_BUILD_ID
                return desc.hex()
    # Linked with --build-id=none: buildidtool writes an empty stamp.
    return ""


def main(argv):
    stamp, entries = None, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-stamp", "--stamp"):
            stamp = argv[i + 1]
            i += 2
        elif a in ("-entry", "--entry"):
            entries.append(argv[i + 1])
            i += 2
        elif a in ("-build-id-dir", "--build-id-dir"):
            i += 2
        else:
            raise SystemExit(f"unknown argument {a}")
    binary = next((e.split("=", 1)[1] for e in entries if e.startswith("=")), None)
    if not stamp or not binary:
        raise SystemExit("usage: -stamp FILE -entry =BINARY [-entry .debug=BINARY]")
    with open(stamp, "w") as f:
        f.write(elf_build_id(binary))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
