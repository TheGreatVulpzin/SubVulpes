"""Run the street map, trigger and aim snap checks with a real native plugin."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
binary = build / "tests" / "n-roads-fixture.dll"
for mode in ("normal", "missing-capability", "reuse", "reset"):
    capabilities = "read,props" if mode == "missing-capability" else "read,ai,props"
    with tempfile.TemporaryDirectory(prefix="n-roads-", dir=build / "tests") as folder:
        root = Path(folder)
        module = root / "plugins" / "test.nroads"
        module.mkdir(parents=True)
        shutil.copyfile(binary, module / "roads.dll")
        (module / "plugin.ini").write_text(
            "[plugin]\nid=test.nroads\nversion=0.1.0\nlibrary=roads.dll\napi=1\n"
            f"capabilities={capabilities}\nlive=1\n", encoding="ascii")
        (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.nroads=required\n", encoding="ascii")
        (root / "plugins" / "native-approvals.ini").write_text(
            "[native]\ntest.nroads=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii")
        arguments = [str(build / "tests" / "sv-n-roads-test.exe")]
        if mode != "normal":
            arguments.append("--" + mode)
        result = subprocess.run(arguments, cwd=root,
                                capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        marker = {"normal": "N_ROADS_PASS", "missing-capability": "N_ROADS_CAP_PASS",
                  "reuse": "N_ROADS_REUSE_PASS", "reset": "N_ROADS_RESET_PASS"}[mode]
        assert result.returncode == 0 and marker in result.stdout
