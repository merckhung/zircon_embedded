#!/usr/bin/env python3
"""Prepare the GCC corresponding-source archive for publication.

Neutralize workstation source prefixes in four upstream Go trace fixtures,
retain their encoded lengths, preserve copyright/license contents, and update
an unsigned source descriptor. The original signed descriptor is retained.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import io
import json
import re
import shutil
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream-archive", type=Path, required=True)
    parser.add_argument("--staging-dir", type=Path, required=True)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    root = project / "chromium/runtime_sources"
    stage = args.staging_dir.resolve()
    if stage.is_relative_to(project):
        parser.error("staging directory must be outside the publication tree")
    stage.mkdir(parents=True, exist_ok=True)
    archive = root / "gcc-10_10.2.1.orig.tar.xz"
    records = json.loads((root / "manifest.json").read_text())
    record = next(row for row in records if row["file"] == archive.name)
    expected = record.get("upstream_sha256", record["sha256"])
    upstream = args.upstream_archive.resolve()
    if hashlib.sha256(upstream.read_bytes()).hexdigest() != expected:
        parser.error("input is not the pinned upstream archive")
    pristine = stage / "gcc-10_10.2.1.orig.upstream.tar.xz"
    if upstream != pristine:
        shutil.copyfile(upstream, pristine)
    report = []
    names = {
        "gcc-10.2.0/libgo/go/internal/trace/testdata/user_task_span_1_11_good",
        "gcc-10.2.0/libgo/go/internal/trace/testdata/stress_start_stop_1_11_good",
        "gcc-10.2.0/libgo/go/internal/trace/testdata/http_1_11_good",
        "gcc-10.2.0/libgo/go/internal/trace/testdata/stress_1_11_good",
    }
    pattern = re.compile(rb"/usr/" + rb"local/google/home/[A-Za-z0-9_.-]+/")
    notice = b"""Publication source sanitization, 2026-10-25

    Local workstation home prefixes in four upstream Go trace fixtures were
    replaced with equal-length neutral source prefixes. No compiler or runtime
    source code was changed. Copyright and license notices are retained.
    See the source distribution's publication-redactions.json for changed files.
    """

    def neutral(match):
        old = match.group()
        prefix = b"/opt/source/"
        return prefix + b"_" * (len(old) - len(prefix) - 1) + b"/"

    def clean_info(member):
        member = copy.copy(member)
        member.uid = 0
        member.gid = 0
        member.uname = ""
        member.gname = ""
        member.pax_headers = {}
        return member

    inner_name = "gcc-10-10.2.0/gcc-10.2.0-dfsg.tar.xz"
    with tarfile.open(pristine, "r:xz") as outer:
        inner_data = outer.extractfile(inner_name).read()
    inner_new = stage / "gcc-10.2.0-dfsg.public.tar.xz"
    with tarfile.open(
        fileobj=io.BytesIO(inner_data), mode="r:xz"
    ) as source, tarfile.open(inner_new, "w:xz", preset=6) as output:
        for member in source:
            info = clean_info(member)
            if member.name in names:
                original = source.extractfile(member).read()
                data, count = pattern.subn(neutral, original)
                assert count and len(data) == len(original), member.name
                assert b"/usr/" + b"local/google/" not in data, member.name
                report.append(
                    {
                        "file": member.name,
                        "replacements": count,
                        "bytes": len(data),
                        "upstream_sha256": hashlib.sha256(original).hexdigest(),
                        "published_sha256": hashlib.sha256(data).hexdigest(),
                    }
                )
                output.addfile(info, io.BytesIO(data))
                print("Sanitized", member.name, count, flush=True)
            else:
                output.addfile(
                    info, source.extractfile(member) if member.isfile() else None
                )
        info = tarfile.TarInfo("gcc-10.2.0/PUBLICATION-SANITIZATION.txt")
        info.size = len(notice)
        info.mode = 0o644
        output.addfile(info, io.BytesIO(notice))
    assert {r["file"] for r in report} == names
    published = stage / archive.name
    with tarfile.open(pristine, "r:xz") as source, tarfile.open(
        published, "w:xz", preset=6
    ) as output:
        for member in source:
            info = clean_info(member)
            if member.name == inner_name:
                info.size = inner_new.stat().st_size
                with inner_new.open("rb") as contents:
                    output.addfile(info, contents)
            else:
                output.addfile(
                    info, source.extractfile(member) if member.isfile() else None
                )
    print("Published archive staged", published.stat().st_size, flush=True)
    old_descriptor = root / "gcc-10_10.2.1-6.dsc"
    original_dir = root / "upstream-descriptors"
    original_dir.mkdir(exist_ok=True)
    original_descriptor = original_dir / old_descriptor.name
    if not original_descriptor.exists():
        shutil.copyfile(old_descriptor, original_descriptor)
    text = (
        original_descriptor.read_text()
        .split("\n\n", 1)[1]
        .split("-----BEGIN PGP SIGNATURE-----", 1)[0]
        .rstrip()
        + "\n"
    )
    text = (
        "\n".join(
            line[2:] if line.startswith("- ") else line for line in text.splitlines()
        )
        + "\n"
    )
    data = published.read_bytes()
    hashes = {
        name: hashlib.new(name, data).hexdigest() for name in ("md5", "sha1", "sha256")
    }
    section = None
    lines = []
    for line in text.splitlines():
        if line and not line.startswith(" "):
            section = line.split(":", 1)[0]
        if section in (
            "Files",
            "Checksums-Sha1",
            "Checksums-Sha256",
        ) and line.strip().endswith(" " + archive.name):
            algo = {
                "Files": "md5",
                "Checksums-Sha1": "sha1",
                "Checksums-Sha256": "sha256",
            }[section]
            line = f" {hashes[algo]} {len(data)} {archive.name}"
        lines.append(line)
    lines += [
        "X-Publication-Changes: Neutralized workstation paths in four Go trace fixtures; no compiler or runtime source changes."
    ]
    new_descriptor = ("\n".join(lines) + "\n").encode()
    redactions = {
        "upstream_archive_sha256": hashlib.sha256(pristine.read_bytes()).hexdigest(),
        "published_archive_sha256": hashes["sha256"],
        "modified_files": report,
        "notice": "gcc-10.2.0/PUBLICATION-SANITIZATION.txt",
        "scope": "Trace-fixture source prefixes only; compiler and runtime sources unchanged.",
    }
    (root / "publication-redactions.json").write_text(
        json.dumps(redactions, indent=2) + "\n"
    )
    # Replace only after the modified archive and descriptor are fully prepared.
    temporary = archive.with_name(archive.name + ".publication-tmp")
    shutil.copyfile(published, temporary)
    temporary.replace(archive)
    old_descriptor.write_bytes(new_descriptor)
    manifest_path = root / "manifest.json"
    manifest = [
        row
        for row in json.loads(manifest_path.read_text())
        if not (row["file"] == old_descriptor.name and "derived_from" in row)
    ]
    for row in manifest:
        if row["file"] == archive.name:
            row["upstream_url"] = row.get("upstream_url", row.get("url"))
            row.pop("url", None)
            row["upstream_sha256"] = hashlib.sha256(pristine.read_bytes()).hexdigest()
            row["sha256"] = hashes["sha256"]
            row["size"] = len(data)
            row["publication_changes"] = "publication-redactions.json"
        if row["file"] == old_descriptor.name:
            row["file"] = "upstream-descriptors/" + old_descriptor.name
    manifest.append(
        {
            "file": old_descriptor.name,
            "sha256": hashlib.sha256(new_descriptor).hexdigest(),
            "derived_from": "upstream-descriptors/" + old_descriptor.name,
            "publication_changes": "publication-redactions.json",
        }
    )
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print("Sanitized source archive and checksum descriptor installed", flush=True)


if __name__ == "__main__":
    main()
