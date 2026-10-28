"""Check archive identity for cached and newly fetched runtime bytes."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import sys
import unittest

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("fetch_runtimes", ROOT / "tools/sdk/fetch_runtimes.py")
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class ArchiveIdentity(unittest.TestCase):
    def test_cached_bytes(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as folder:
            path = Path(folder) / "runtime.tar.gz"
            path.write_bytes(b"fixed runtime archive")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(fetch.checked_archive("unused", path, digest), path)

    def test_changed_cached_bytes(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as folder:
            path = Path(folder) / "runtime.tar.gz"
            path.write_bytes(b"changed runtime archive")
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                fetch.checked_archive("unused", path, hashlib.sha256(b"fixed runtime archive").hexdigest())

    def test_changed_download(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as folder:
            source = Path(folder) / "source"
            source.write_bytes(b"changed runtime archive")
            target = Path(folder) / "runtime.tar.gz"
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                fetch.checked_archive(source.as_uri(), target, hashlib.sha256(b"fixed runtime archive").hexdigest())
            self.assertFalse(target.exists())

    def test_fixed_download(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as folder:
            source = Path(folder) / "source"
            source.write_bytes(b"fixed runtime archive")
            target = Path(folder) / "runtime.tar.gz"
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            self.assertEqual(fetch.checked_archive(source.as_uri(), target, digest).read_bytes(), source.read_bytes())


if __name__ == "__main__":
    unittest.main()
