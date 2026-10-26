#!/usr/bin/env python3
"""Audit nested corresponding-source tar archives, metadata and sparse payloads.

Configured private tokens are checked as UTF-8, UTF-16 and UTF-32. Invalid
regression-test tar fixtures are scanned as raw bytes and listed in the report.
Sparse holes contain zeroes; boundary padding preserves encoded-token checks.
No archive members are extracted into the filesystem.
"""
from pathlib import Path
import argparse
import getpass
import hashlib
import io
import json
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    project = Path(__file__).resolve().parents[1]
    parser.add_argument(
        "--archives", type=Path, default=project / "chromium/runtime_sources"
    )
    parser.add_argument("--deny-file", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    root = args.archives.resolve()
    archives = sorted(root.glob("*.tar.xz"))
    if not root.is_dir() or not archives:
        parser.error("no corresponding-source tar archives found")
    manifest = root / "manifest.json"
    if manifest.exists():
        expected = [
            root / row["file"]
            for row in json.loads(manifest.read_text())
            if row["file"].endswith(".tar.xz")
        ]
        if any(not path.is_file() for path in expected):
            parser.error("a source archive named by the manifest is missing")
    if args.deny_file.resolve().is_relative_to(project):
        parser.error("identity deny file must be outside the repository")
    private = args.deny_file.read_text().splitlines()
    private = [
        line.strip()
        for line in private
        if line.strip() and not line.lstrip().startswith("#")
    ]
    if not private:
        parser.error("identity deny file contains no tokens")
    tokens = [s.encode() for s in private if s] + [
        b"/usr/" + b"local/google/",
        b"/usr/" + b"loca/google/",
    ]
    needles = [
        (encoding, token.decode().encode(encoding).lower())
        for token in tokens
        for encoding in ("utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be")
    ]
    carry_size = max(len(needle) for _, needle in needles)
    findings = []
    opaque = []
    counts = {
        "files": 0,
        "metadata_entries": 0,
        "payload_bytes_read": 0,
        "sparse_zero_bytes_skipped": 0,
    }

    class Scan:
        def __init__(self, label):
            self.label = label
            self.carry = b""

        def feed(self, chunk):
            data = (self.carry + chunk).lower()
            for encoding, needle in needles:
                if needle in data:
                    item = {"file": self.label, "encoding": encoding}
                    if item not in findings:
                        findings.append(item)
                        print(json.dumps(item), flush=True)
            self.carry = data[-carry_size:]

    def check(stream, label, limit=None, scanner=None):
        scanner = scanner or Scan(label)
        while limit is None or limit > 0:
            chunk = stream.read(
                1024 * 1024 if limit is None else min(limit, 1024 * 1024)
            )
            if not chunk:
                break
            counts["payload_bytes_read"] += len(chunk)
            scanner.feed(chunk)
            if limit is not None:
                limit -= len(chunk)

    def walk(stream, label, depth=0):
        if depth > 8:
            raise ValueError("archive nesting too deep")
        with tarfile.open(fileobj=stream, mode="r:*") as archive:
            for entry in archive:
                name = label + "!" + entry.name
                counts["metadata_entries"] += 1
                for value in (
                    entry.name,
                    entry.linkname,
                    entry.uname,
                    entry.gname,
                    *entry.pax_headers.values(),
                ):
                    Scan(name + " [metadata]").feed(
                        str(value).encode(errors="surrogateescape")
                    )
                if (
                    getpass.getuser() not in ("root", "nobody", "builder", "build")
                    and entry.uname.lower() == getpass.getuser().lower()
                ):
                    findings.append(
                        {"file": name, "encoding": "current local archive owner"}
                    )
                if not entry.isfile():
                    continue
                counts["files"] += 1
                member = archive.extractfile(entry)
                if entry.name.endswith(
                    (".tar.xz", ".tar.gz", ".tgz", ".tar.bz2", ".tar")
                ):
                    data = member.read()
                    try:
                        walk(io.BytesIO(data), name, depth + 1)
                    except tarfile.ReadError:
                        opaque.append(name)
                        check(io.BytesIO(data), name)
                elif entry.sparse:
                    scan = Scan(name)
                    end = 0
                    for offset, length in entry.sparse:
                        if not length:
                            continue
                        gap = max(0, offset - end)
                        scan.feed(b"\0" * min(gap, carry_size))
                        counts["sparse_zero_bytes_skipped"] += gap
                        member.seek(offset)
                        check(member, name, length, scan)
                        end = offset + length
                    gap = max(0, entry.size - end)
                    scan.feed(b"\0" * min(gap, carry_size))
                    counts["sparse_zero_bytes_skipped"] += gap
                else:
                    check(member, name)

    for path in archives:
        with path.open("rb") as source:
            walk(source, path.name)
        print("Scanned", path.name, flush=True)
    report = {
        "counts": counts,
        "findings": findings,
        "opaque_invalid_tar_fixtures_scanned_as_raw_bytes": opaque,
        "archive_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.glob("*.tar.xz")
        },
    }
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "counts": counts,
                "findings": len(findings),
                "opaque_fixtures": len(opaque),
            }
        ),
        flush=True,
    )
    raise SystemExit(bool(findings))


if __name__ == "__main__":
    main()
