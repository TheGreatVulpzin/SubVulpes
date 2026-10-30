"""Use the SDK runner and check the demo trace order twice."""
from pathlib import Path
import math
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]).resolve()
plugin = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else BUILD / 'plugins/sv.jukebox'
tools = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else ROOT / 'tools/sdk'
runner = tools / 'subvulpes_plugin_test.exe' if (tools / 'subvulpes_plugin_test.exe').exists() else BUILD / 'subvulpes_plugin_test.exe'
probe = tools / 'probe' if (tools / 'probe/probe.dll').exists() else BUILD / 'plugins/sv.probe'
command = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(tools / 'run_plugin_test.ps1'),
           '-Plugin', str(plugin), '-Scenario', str(ROOT / 'tests/scenarios/jukebox-demo.svtest'),
           '-ServerExe', str(BUILD / 'subvulpes_server.exe'), '-Runner', str(runner), '-Probe', str(probe)]
expected = [(0, 0, 1, 60, 29, 0.375359491), (0, 0, 10, 36, 21, 0.25),
            (0, 15, 1, 64, 29, 0.472923323), (0, 30, 1, 67, 29, 0.562403782),
            (0, 30, 10, 42, 21, 2.0), (0, 45, 1, 72, 29, 0.750718983)]
pattern = re.compile(r'^\[sv\.jukebox\] JUKEBOX_SOUND tick=(\d+) at=(\d+) ch=(\d+) note=(\d+) sound=(\d+) pitch=([0-9.]+)', re.M)
previous = None
for _ in range(2):
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    assert result.returncode == 0 and 'PASS jukebox-demo.svtest' in result.stdout, result.stdout + result.stderr
    actual = [tuple(map(int, row[:5])) + (float(row[5]),) for row in pattern.findall(result.stdout)]
    assert len(actual) == len(expected), (actual, result.stdout)
    for got, want in zip(actual, expected):
        assert got[:5] == want[:5] and math.isclose(got[5], want[5], rel_tol=0, abs_tol=0.000002), (got, want, result.stdout)
    if previous is not None:
        assert actual == previous, 'The demo gave different sound calls.'
    previous = actual
print('JUKEBOX_SCENARIO_PASS: SDK runner, exact sound order, tick, pitch and repeated trace')
