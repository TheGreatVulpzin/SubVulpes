"""Print hook costs with an idle plugin and a synthetic world."""
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="plugin-cost-", dir=build / "tests") as folder:
    root = Path(folder)
    module = root / "plugins" / "test.cost"
    module.mkdir(parents=True)
    binary = build / "tests" / "cost-fixture.dll"
    shutil.copyfile(binary, module / "cost.dll")
    (module / "plugin.ini").write_text(
        "[plugin]\nid=test.cost\nversion=0.1.0\nlibrary=cost.dll\napi=1\nlive=0\n", encoding="ascii")
    (root / "plugins" / "plugins.ini").write_text("[plugins]\ntest.cost=required\n", encoding="ascii")
    (root / "plugins" / "native-approvals.ini").write_text(
        "[native]\ntest.cost=" + hashlib.sha256(binary.read_bytes()).hexdigest() + "\n", encoding="ascii")
    run = subprocess.run([str(build / "tests" / "sv-game-test.exe"), "--cost"], cwd=root,
                         capture_output=True, text=True, timeout=60)
    print(run.stdout, end="")
    print(run.stderr, end="", file=sys.stderr)
    labels = ("shop_poll", "mission_before_tick", "game_event_poll", "actions_poll", "ai_256",
              "all_hooks_256", "idle_tick_single", "idle_tick_mean", "within_humans_dense", "within_items_dense", "within_humans_sparse", "within_items_sparse")
    for label in labels:
        if not re.search(rf"COST {label} [0-9]+\.[0-9]+ us", run.stdout):
            raise SystemExit(f"Missing timing output: {label}")
    raise SystemExit(run.returncode)
