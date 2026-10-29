"""The plugin test runner: a good scenario passes, and each kind of wrong scenario or wrong plugin fails with exit 1."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

BUILD = Path(sys.argv[1]).resolve()
ROOT = Path(__file__).resolve().parent.parent
SERVER = BUILD / 'subvulpes_server.exe'
RUNNER = BUILD / 'subvulpes_plugin_test.exe'
PROBE = BUILD / 'plugins/sv.probe'
STATUS = BUILD / 'plugins/sv.status'
SCRIPT = ROOT / 'tools/sdk/run_plugin_test.ps1'
CASES = BUILD / 'tests/scenario-cases'
CASES.mkdir(parents=True, exist_ok=True)
POWERSHELL = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File']
checked = 0


def run(plugin, scenario_text):
    scenario = Path(tempfile.mkdtemp(prefix='case-', dir=CASES)) / 'case.svtest'
    scenario.write_text(scenario_text, encoding='ascii')
    result = subprocess.run([*POWERSHELL, str(SCRIPT), '-Plugin', str(plugin), '-Scenario', str(scenario), '-Runner', str(RUNNER),
                             '-Probe', str(PROBE), '-ServerExe', str(SERVER)], capture_output=True, text=True, timeout=120)
    return result.returncode, result.stdout + result.stderr


def passes(scenario_text, *needles):
    global checked
    code, output = run(STATUS, scenario_text)
    assert code == 0 and 'PASS case.svtest' in output and all(n in output for n in needles), (scenario_text, code, output)
    checked += 1
    return output


def fails(scenario_text, message, plugin=STATUS):
    global checked
    code, output = run(plugin, scenario_text)
    assert code == 1 and 'FAIL case.svtest step' in output and message in output, (scenario_text, code, output)
    checked += 1


# The real scenario of the status plugin.
code, first = subprocess.run([*POWERSHELL, str(SCRIPT), '-Plugin', str(STATUS), '-Scenario', str(ROOT / 'tests/scenarios/status.svtest'),
                              '-Runner', str(RUNNER), '-Probe', str(PROBE), '-ServerExe', str(SERVER)],
                             capture_output=True, text=True, timeout=120), None
assert code.returncode == 0 and 'PASS status.svtest' in code.stdout, code.stdout + code.stderr
second = subprocess.run([*POWERSHELL, str(SCRIPT), '-Plugin', str(STATUS), '-Scenario', str(ROOT / 'tests/scenarios/status.svtest'),
                         '-Runner', str(RUNNER), '-Probe', str(PROBE), '-ServerExe', str(SERVER)], capture_output=True, text=True, timeout=120)
assert code.stdout == second.stdout, 'The same scenario gave a different output.'
checked += 2

# Small scenarios, one rule each.
passes('# only a comment\n\n')
passes('spawn player Ana team=2 admin=1\nexpect entities players 1\n')
passes('spawn player Ana\ntick 2\nsay Ana /status\nexpect no-failure\n')
passes('expect no-log "this text is never logged"\n')
passes('console status\nconsole status\nexpect count 2 "Status:"\n')
# A wrong expectation, a wrong step or a wrong name fails the run at that step, and the message names the cause.
fails('expect log "this text is never logged"\n', 'does not hold')
fails('console status\nexpect count 2 "Status:"\n', 'the scenario wants 2')
fails('expect failed\n', 'no plugin failed')
fails('expect no-log "Plugin active"\n', 'does not hold')
fails('dance\n', 'unknown step dance')
fails('say Nobody hello\n', 'no player named Nobody')
fails('spawn player Ana\nspawn player Ana\n', 'the player Ana exists')
fails('spawn player Ana\nexpect prop human.health Ana 99\n', 'wants 99')
fails('spawn player Ana\nexpect prop no.such.prop Ana 1\n', 'could not read')
fails('console nosuchcommand\n', 'no plugin handled')
fails('spawn player Ana\nset human.health Ana 5000\n', 'could not be set')
fails('expect entities players 3\n', 'scenario wants players 3')
fails('tick 0\n', 'tick takes 1 to 100000')
fails('spawn player ' + 'x' * 40 + '\n', 'a player name has')
fails('spawn player Ana\nsay Ana ' + 'x' * 100 + '\n', 'at most 63 characters')
fails('expect log needs quotes\n', 'double quotes')
# A plugin that is not valid never reaches the runner.
bad = Path(tempfile.mkdtemp(prefix='bad-', dir=CASES)).resolve()
shutil.copyfile(STATUS / 'status.dll', bad / 'status.dll')
(bad / 'plugin.ini').write_text((STATUS / 'plugin.ini').read_text(encoding='ascii') + 'capabilities=fly\n', encoding='ascii')
code, output = run(bad, 'expect no-failure\n')
assert code != 0 and 'INVALID' in output and 'STEP' not in output, output
checked += 1

print(f'SCENARIO_PASS {checked}')
