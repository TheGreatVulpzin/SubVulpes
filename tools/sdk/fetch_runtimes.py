"""Fetch fixed runtime archives into build/runtimes. Never vendor runtime sources."""
import argparse
import hashlib
from pathlib import Path
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2] / "build" / "runtimes"
PINS = (
    ("lua-5.4.9.tar.gz", "lua-5.4.9",
     "https://www.lua.org/ftp/lua-5.4.9.tar.gz",
     "2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6",
     "publisher: https://www.lua.org/ftp/"),
    ("wasm3-0.5.0.tar.gz", "wasm3-0.5.0",
     "https://codeload.github.com/wasm3/wasm3/tar.gz/refs/tags/v0.5.0",
     "b778dd72ee2251f4fe9e2666ee3fe1c26f06f517c3ffce572416db067546536c",
     "trust on first use: https://github.com/wasm3/wasm3/releases/tag/v0.5.0"),
    ("wamr-2.4.5.tar.gz", "wasm-micro-runtime-WAMR-2.4.5",
     "https://codeload.github.com/wasm-micro-runtime/wasm-micro-runtime/tar.gz/refs/tags/WAMR-2.4.5",
     "1ab09d51099f276ca4a1d6629f6b589aab2bd0caa01445e05031a4bed22c199b",
     "trust on first use: https://github.com/wasm-micro-runtime/wasm-micro-runtime/releases/tag/WAMR-2.4.5"),
)


def checked_archive(url, archive, expected):
    if not archive.exists():
        part = archive.with_suffix(archive.suffix + ".part")
        with urllib.request.urlopen(url, timeout=60) as source:
            part.write_bytes(source.read())
        actual = hashlib.sha256(part.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError("Runtime archive SHA-256 mismatch: " + archive.name)
        part.replace(archive)
    actual = hashlib.sha256(archive.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError("Runtime archive SHA-256 mismatch: " + archive.name)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, folder, url, digest, origin in PINS:
        archive = checked_archive(url, ROOT / name, digest)
        print(name + " SHA-256 " + digest + " (" + origin + ")")
        if args.check_only:
            continue
        stamp = ROOT / (folder + ".sha256")
        if (ROOT / folder).is_dir() and stamp.is_file() and stamp.read_text(encoding="ascii").strip() == digest:
            continue
        with tarfile.open(archive, "r:gz") as source:
            source.extractall(ROOT, filter="data")
        stamp.write_text(digest + "\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
