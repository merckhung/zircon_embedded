#!/usr/bin/env python3
"""Exercise privacy checks without embedding real identities in the project."""
from pathlib import Path
import gzip
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("audit_publication.py")


class PublicationAuditTest(unittest.TestCase):
    def audit(self, data, name="payload"):
        with tempfile.TemporaryDirectory() as private:
            base = Path(private)
            root = base / "publish"
            root.mkdir()
            deny = base / "deny.txt"
            deny.write_text("private-token-42\n")
            (root / name).write_bytes(data)
            return subprocess.run(
                ["python3", str(SCRIPT), "--root", str(root), "--deny-file", str(deny)],
                capture_output=True,
                text=True,
            )

    def test_private_token_in_text_and_gzip(self):
        for data, name in (
            (b"private-token-42", "text"),
            (gzip.compress(b"private-token-42"), "graph.gz"),
        ):
            with self.subTest(name=name):
                self.assertEqual(1, self.audit(data, name).returncode)

    def test_wide_string_identities(self):
        for encoding in ("utf-16le", "utf-16be", "utf-32le", "utf-32be"):
            with self.subTest(encoding=encoding):
                result = self.audit("private-token-42".encode(encoding))
                self.assertEqual(1, result.returncode, result.stdout)
                self.assertIn(encoding, result.stdout)
                self.assertNotIn("private-token-42", result.stdout)

    def test_local_home_path(self):
        self.assertEqual(1, self.audit(b"/home/" + b"example/build/").returncode)

    def test_public_url_is_not_a_local_home(self):
        result = self.audit(b"https://www.chromium.org/home/chromium-security/")
        self.assertEqual(0, result.returncode, result.stdout)

    def test_private_filename_is_rejected_without_echoing_identity(self):
        result = self.audit(b"ordinary payload", "private-token-42.txt")
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertNotIn("private-token-42", result.stdout)

    def test_missing_scope_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            deny = root / "deny.txt"
            deny.write_text("private-token-42\n")
            result = subprocess.run(
                [
                    "python3",
                    str(SCRIPT),
                    "--root",
                    str(root / "absent"),
                    "--deny-file",
                    str(deny),
                ],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(0, result.returncode)


if __name__ == "__main__":
    unittest.main()
