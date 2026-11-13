"""Run the arm pose checks with a real native plugin."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
binary = build / "tests" / "n-arms-fixture.dll"
for mode in ("normal", "reuse", "reset"):
    capabilities = "read,props"
    with tempfile.TemporaryDirectory(prefix="n-arms-", dir=build / "tests") as folder:
        root = Path(folder)
        module = root / "plugins" / "test.narms"
        module.mkdir(parents=True)
        shutil.copyfile(binary, module / "arms.dll")
        (module / "plugin.ini").write_text(
            "[plugin]\nid=test.narms\nversion=0.1.0\nlibrary=arms.dll\napi=1\n"
            f"capabilities={capabilities}\nlive=1\n", encoding="ascii")
        (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.narms=enabled\n", encoding="ascii")
        (root / "plugins" / "native-approvals.ini").write_text(
            "[native]\ntest.narms=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii")
        arguments = [str(build / "tests" / "sv-n-arms-test.exe")]
        if mode != "normal":
            arguments.append("--" + mode)
        result = subprocess.run(arguments, cwd=root,
                                capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        marker = {"normal": "N_ARMS_PASS",
                  "reuse": "N_ARMS_REUSE_PASS", "reset": "N_ARMS_RESET_PASS"}[mode]
        assert result.returncode == 0 and marker in result.stdout
