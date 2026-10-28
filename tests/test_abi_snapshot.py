"""Compare the public x86 ABI with the reviewed snapshot."""
import difflib
from pathlib import Path
import subprocess
import sys

run = subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True)
expected = Path(sys.argv[2]).read_text(encoding="ascii")
if run.stdout != expected:
    sys.stderr.writelines(difflib.unified_diff(
        expected.splitlines(keepends=True), run.stdout.splitlines(keepends=True),
        fromfile="expected ABI", tofile="current ABI"))
    raise SystemExit(1)
print("ABI snapshot matches.")
