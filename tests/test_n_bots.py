"""Run the bot API checks with a real native plugin."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
binary = build / "tests" / "n-bots-fixture.dll"
for mode in ("normal", "missing-capability", "reuse", "reset"):
    capabilities = "read,props" if mode == "missing-capability" else "read,ai,props,write"
    with tempfile.TemporaryDirectory(prefix="n-bots-", dir=build / "tests") as folder:
        root = Path(folder)
        module = root / "plugins" / "test.nbots"
        module.mkdir(parents=True)
        shutil.copyfile(binary, module / "bots.dll")
        (module / "plugin.ini").write_text(
            "[plugin]\nid=test.nbots\nversion=0.1.0\nlibrary=bots.dll\napi=1\n"
            f"capabilities={capabilities}\nlive=1\n", encoding="ascii")
        (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.nbots=required\n", encoding="ascii")
        (root / "plugins" / "native-approvals.ini").write_text(
            "[native]\ntest.nbots=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii")
        arguments = [str(build / "tests" / "sv-n-bots-test.exe")]
        if mode != "normal":
            arguments.append("--" + mode)
        result = subprocess.run(arguments, cwd=root,
                                capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        marker = {"normal": "N_BOTS_PASS", "missing-capability": "N_BOTS_CAP_PASS",
                  "reuse": "N_BOTS_REUSE_PASS", "reset": "N_BOTS_RESET_PASS"}[mode]
        assert result.returncode == 0 and marker in result.stdout
