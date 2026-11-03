"""Checks tools/release/version_name.ps1: titles, names and pictures of the release pages."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools' / 'release' / 'version_name.ps1'


def info(version):
    command = f"& '{SCRIPT}' -Version '{version}' | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', command],
                            capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        return None
    return json.loads(result.stdout)


def check(version, title, name, picture):
    got = info(version)
    assert got is not None, f'{version} was refused'
    assert got['Title'] == title, (version, got['Title'])
    assert got['Name'] == name, (version, got['Name'])
    assert bool(got['Picture']) == picture, (version, got['Picture'])
    if picture:
        assert got['Picture'] == f'https://raw.githubusercontent.com/TheGreatVulpzin/SubVulpes/{version}/docs/images/kit-bodega.jpg'
        assert got['Caption'] == 'Kit Bodega. Author unknown. Edit by Vulpzin.'


check('v0.2.0', 'SubVulpes 0.2 Kit', 'Kit', True)
check('v0.2.1', 'SubVulpes 0.2.1 Kit', 'Kit', False)
check('v0.3.0-rc.1', 'SubVulpes 0.3.0-rc.1 Kit', 'Kit', False)
check('v1.0.0', 'SubVulpes 1.0 Arctic', 'Arctic', False)
check('v1.0.3', 'SubVulpes 1.0.3 Arctic', 'Arctic', False)
check('v1.8.0', 'SubVulpes 1.8 Tibetan', 'Tibetan', False)
check('v2.0.0', 'SubVulpes 2.0 Red', 'Red', False)
check('v3.0.0', 'SubVulpes 3.0', '', False)
for bad in ('0.2.0', 'v0.2', 'v0.2.0-', 'latest', "v0.2.0'; calc"):
    assert info(bad) is None, f'{bad} was accepted'
print('VERSION_NAMES_PASS')
sys.exit(0)
