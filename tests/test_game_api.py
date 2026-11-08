"""Exercise the public read API through a required native test module."""
import base64
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def check_private_output(run, salt):
    output = run.stdout + run.stderr
    forms = (salt, salt.hex().encode("ascii"), salt.hex().upper().encode("ascii"), base64.b64encode(salt))
    assert all(value not in output for value in forms), "The network salt must stay out of process output."


build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="game-api-", dir=build / "tests") as folder:
    root = Path(folder)
    module = root / "plugins" / "test.read"
    module.mkdir(parents=True)
    binary = build / "tests" / "game-read-fixture.dll"
    shutil.copyfile(binary, module / "read.dll")
    (module / "plugin.ini").write_text(
        "[plugin]\nid=test.read\nversion=0.1.0\nlibrary=read.dll\napi=1\ncapabilities=read,write,props,plugins,server_control,console\nlive=1\nunsafe_events=1\n", encoding="ascii"
    )
    other = root / "plugins" / "test.runtime"
    other.mkdir()
    other_binary = build / "tests" / "runtime-fixture.dll"
    shutil.copyfile(other_binary, other / "runtime.dll")
    (other / "plugin.ini").write_text(
        "[plugin]\nid=test.runtime\nversion=0.1.0\nlibrary=runtime.dll\napi=1\ncapabilities=plugins\nlive=1\n", encoding="ascii")
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.read=required\ntest.runtime=enabled\n", encoding="ascii")
    (root / "plugins" / "native-approvals.ini").write_text(
        "[native]\ntest.read=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\ntest.runtime=" + hashlib.sha256(other_binary.read_bytes()).hexdigest() + "\n", encoding="ascii"
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
        environment["SV_TEST_RESTART"] = "0" if network else "1"
        run = subprocess.run([str(build / "tests" / "sv-game-test.exe")], cwd=root, env=environment,
                             capture_output=True, timeout=30)
        salt_path = root / "user" / "connection-salt.bin"
        salt_now = (salt_path.read_bytes(), salt_path.stat().st_mtime_ns)
        assert len(salt_now[0]) == 16, "The network salt must hold 16 bytes."
        check_private_output(run, salt_now[0])
        run.stdout = run.stdout.decode(errors="replace")
        run.stderr = run.stderr.decode(errors="replace")
        print(run.stdout, end="")
        print(run.stderr, end="", file=sys.stderr)
        required = ("PUBLIC_READ_API_PASS", "STOP_READ_DENIED", "PUBLIC_WRITE_API_PASS", "STOP_WRITE_DENIED",
                    "COMMAND echo hi there", "COMMAND secret two", "NPC_LIMIT_PASS", "LINE_MARK_PASS", "ACTION_GATE_PASS", "ACTION_SWEEP_PASS", "PLUGIN_RUNTIME_PASS", "CORE_COMMAND_LOGIN_KICKED_PASS", "PLUGIN_INFRA_PASS")
        # A player runs the PUBLIC command. The ADMIN command waits for the flag. The LOCAL command never runs.
        # A player with no reference must not become the local operator (actor zero).
        absent = ("COMMAND secret one", "COMMAND localonly", "COMMAND echo nope", "Plugin command result: echo",
                  "Plugin command result: corecheck", "Read API fixture failed")
        denied = run.stdout.count("Plugin command denied: secret") == 1 and \
            run.stdout.count("Plugin command denied: localonly") == 2
        if run.returncode or not denied or any(text not in run.stdout for text in required) or \
                any(text in run.stdout for text in absent):
            raise SystemExit(run.returncode or 1)
        ids_now = next(line.split("NETWORK_IDS ", 1)[1] for line in run.stdout.splitlines() if "NETWORK_IDS " in line)
        if salt_before is not None:
            assert salt_now == salt_before, "The profile must reuse the network salt without a rewrite."
            assert ids_now == ids_before, "Both permissions must return the same identifiers."
        salt_before, ids_before = salt_now, ids_now
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.read=enabled\ntest.runtime=enabled\n", encoding="ascii")
    run = subprocess.run([str(build / "tests" / "sv-game-test.exe"), "--infra-cleanup"], cwd=root,
                         env=environment, capture_output=True, timeout=30)
    check_private_output(run, salt_before[0])
    assert run.returncode == 0 and b"INFRA_CLEANUP_PASS" in run.stdout, "Stopping the request owner must cancel its pending save."
    salt_stat = salt_path.stat()
    try:
        for malformed in (salt_before[0][:-1], salt_before[0] + b"\x00"):
            salt_path.write_bytes(malformed)
            malformed_before = (salt_path.read_bytes(), salt_path.stat().st_mtime_ns)
            run = subprocess.run([str(build / "tests" / "sv-game-test.exe")], cwd=root, env=environment,
                                 capture_output=True, timeout=30)
            check_private_output(run, salt_before[0])
            check_private_output(run, malformed)
            assert run.returncode != 0 and b"sv_plugins_start()" in run.stderr, \
                "A malformed network salt must refuse host startup."
            assert b"PUBLIC_READ_API_PASS" not in run.stdout, "A malformed salt must stop plugin execution."
            assert (salt_path.read_bytes(), salt_path.stat().st_mtime_ns) == malformed_before, \
                "A malformed network salt must not be rewritten."
    finally:
        salt_path.write_bytes(salt_before[0])
        os.utime(salt_path, ns=(salt_stat.st_atime_ns, salt_stat.st_mtime_ns))
