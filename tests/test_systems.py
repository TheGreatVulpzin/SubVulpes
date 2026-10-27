"""Run the test program of the system areas with its plugin as a required native module."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
areas = re.findall(r"X\((\w+)\)", (Path(__file__).resolve().parent / "systems" / "areas.h").read_text())
assert areas, "No area found in tests/systems/areas.h."
with tempfile.TemporaryDirectory(prefix="systems-api-", dir=build / "tests") as folder:
    root = Path(folder)
    module = root / "plugins" / "test.systems"
    module.mkdir(parents=True)
    binary = build / "tests" / "systems-fixture.dll"
    shutil.copyfile(binary, module / "systems.dll")
    (module / "plugin.ini").write_text(
        "[plugin]\nid=test.systems\nversion=0.1.0\nlibrary=systems.dll\napi=1\nlive=1\n", encoding="ascii"
    )
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.systems=required\n", encoding="ascii")
    (root / "plugins" / "native-approvals.ini").write_text(
        "[native]\ntest.systems=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii"
    )
    run = subprocess.run([str(build / "tests" / "sv-systems-test.exe")], cwd=root, env=os.environ.copy(),
                         capture_output=True, text=True, timeout=60)
    print(run.stdout, end="")
    print(run.stderr, end="", file=sys.stderr)
    required = ["SYSTEMS_PASS"] + [f"{a}_FIXTURE_PASS" for a in areas] + [f"{a.upper()}_PASS" for a in areas]
    missing = [text for text in required if text not in run.stdout]
    if run.returncode or missing:
        print("Missing markers:", missing, file=sys.stderr)
        raise SystemExit(run.returncode or 1)
