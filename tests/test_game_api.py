"""Exercise the public read API through a required native test module."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="game-api-", dir=build / "tests") as folder:
    root = Path(folder)
    module = root / "plugins" / "test.read"
    module.mkdir(parents=True)
    binary = build / "tests" / "game-read-fixture.dll"
    shutil.copyfile(binary, module / "read.dll")
    (module / "plugin.ini").write_text(
        "[plugin]\nid=test.read\nversion=0.1.0\nlibrary=read.dll\napi=1\nlive=1\n", encoding="ascii"
    )
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.read=required\n", encoding="ascii")
    (root / "plugins" / "native-approvals.ini").write_text(
        "[native]\ntest.read=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii"
    )
    environment = os.environ.copy()
    environment.pop("SV_GAME_READ_RUNTIME", None)
    run = subprocess.run([str(build / "tests" / "sv-game-test.exe")], cwd=root, env=environment,
                         capture_output=True, text=True, timeout=30)
    print(run.stdout, end="")
    print(run.stderr, end="", file=sys.stderr)
    required = ("PUBLIC_READ_API_PASS", "STOP_READ_DENIED", "PUBLIC_WRITE_API_PASS", "STOP_WRITE_DENIED")
    if run.returncode or any(text not in run.stdout for text in required):
        raise SystemExit(run.returncode or 1)
