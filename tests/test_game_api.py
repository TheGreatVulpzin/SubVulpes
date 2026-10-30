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
        "[plugin]\nid=test.read\nversion=0.1.0\nlibrary=read.dll\napi=1\ncapabilities=read,write,props,plugins\nlive=1\nunsafe_events=1\n", encoding="ascii"
    )
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.read=required\n", encoding="ascii")
    (root / "plugins" / "native-approvals.ini").write_text(
        "[native]\ntest.read=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii"
    )
    salt_before = None
    ids_before = None
    for network in (True, False):
        manifest = (module / "plugin.ini").read_text(encoding="ascii")
        manifest = manifest.replace("props,plugins,network", "props,plugins")
        if network:
            manifest = manifest.replace("props,plugins", "props,plugins,network")
        (module / "plugin.ini").write_text(manifest, encoding="ascii")
        environment = os.environ.copy()
        environment.pop("SV_GAME_READ_RUNTIME", None)
        environment["SV_TEST_NETWORK"] = "1" if network else "0"
        run = subprocess.run([str(build / "tests" / "sv-game-test.exe")], cwd=root, env=environment,
                             capture_output=True, text=True, timeout=30)
        print(run.stdout, end="")
        print(run.stderr, end="", file=sys.stderr)
        required = ("PUBLIC_READ_API_PASS", "STOP_READ_DENIED", "PUBLIC_WRITE_API_PASS", "STOP_WRITE_DENIED",
                    "COMMAND echo hi there", "COMMAND secret two", "NPC_LIMIT_PASS", "LINE_MARK_PASS", "ACTION_GATE_PASS", "ACTION_SWEEP_PASS")
        # A player runs the PUBLIC command. The ADMIN command waits for the flag. The LOCAL command never runs.
        # A player with no reference must not become the local operator (actor zero).
        absent = ("COMMAND secret one", "COMMAND localonly", "COMMAND echo nope", "Plugin command result: echo")
        denied = run.stdout.count("Plugin command permission denied: secret") == 1 and \
            run.stdout.count("Plugin command permission denied: localonly") == 2
        if run.returncode or not denied or any(text not in run.stdout for text in required) or \
                any(text in run.stdout for text in absent):
            raise SystemExit(run.returncode or 1)
        salt_path = root / "user" / "connection-salt.bin"
        salt_now = (salt_path.read_bytes(), salt_path.stat().st_mtime_ns)
        assert len(salt_now[0]) == 16, "The network salt must hold 16 bytes."
        ids_now = next(line.split("NETWORK_IDS ", 1)[1] for line in run.stdout.splitlines() if "NETWORK_IDS " in line)
        if salt_before is not None:
            assert salt_now == salt_before, "The profile must reuse the network salt without a rewrite."
            assert ids_now == ids_before, "Both permissions must return the same identifiers."
        salt_before, ids_before = salt_now, ids_now
