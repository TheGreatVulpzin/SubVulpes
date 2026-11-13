"""Run the shared native acquisition checks with a real native plugin."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
binary = build / "tests" / "n-mission-fixture.dll"
for mode in ("normal", "missing-capability", "reset", "completed"):
    capabilities = "read,props" if mode == "missing-capability" else "read,mission,props"
    with tempfile.TemporaryDirectory(prefix="n-mission-", dir=build / "tests") as folder:
        root = Path(folder)
        module = root / "plugins" / "test.nmission"
        module.mkdir(parents=True)
        shutil.copyfile(binary, module / "mission.dll")
        (module / "plugin.ini").write_text(
            "[plugin]\nid=test.nmission\nversion=0.1.0\nlibrary=mission.dll\napi=1\n"
            f"capabilities={capabilities}\nlive=1\n", encoding="ascii")
        (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.nmission=required\n", encoding="ascii")
        (root / "plugins" / "native-approvals.ini").write_text(
            "[native]\ntest.nmission=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii")
        arguments = [str(build / "tests" / "sv-n-mission-test.exe")]
        if mode != "normal":
            arguments.append("--" + mode)
        result = subprocess.run(arguments, cwd=root,
                                capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        marker = {"normal": "N_MISSION_PASS", "missing-capability": "N_MISSION_CAP_PASS",
                  "reset": "N_MISSION_RESET_PASS", "completed": "N_MISSION_COMPLETED_PASS"}[mode]
        assert result.returncode == 0 and marker in result.stdout
