"""Build and pack the plugin using only an extracted SDK, then use its runner."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]).resolve()
PS = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File']


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, timeout=180, **kwargs)
    assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)
    return result.stdout


with tempfile.TemporaryDirectory(prefix='jukebox-sdk-', dir=BUILD / 'tests') as root:
    work = Path(root).resolve()
    assert work.parent == (BUILD / 'tests').resolve()
    output = run([*PS, str(ROOT / 'tools/sdk/make_sdk.ps1'), '-Version', 'v0.0.0-jukebox-test', '-OutputDir', str(work),
                  '-Runner', str(BUILD / 'subvulpes_plugin_test.exe'), '-ProbeDir', str(BUILD / 'plugins/sv.probe')])
    archive = work / 'SubVulpes-sdk-v0.0.0-jukebox-test.zip'
    with zipfile.ZipFile(archive) as package:
        package.extractall(work)
    sdk = work / 'SubVulpes-sdk-v0.0.0-jukebox-test'
    source = work / 'jukebox-source'
    source.mkdir()
    for file in (ROOT / 'plugins/jukebox').iterdir():
        if file.is_file() and file.name != '.gitignore':
            shutil.copyfile(file, source / file.name)
    target = work / 'plugin-build'
    batch = work / 'build.bat'
    lines = ['@echo off', 'setlocal', 'set VSLANG=1033',
             f'call "{source / "build.bat"}" "{target}" "{sdk}" || exit /b 1',
             f'cmake -S "{source}" -B "{target}" -DSV_SERVER_EXE="{BUILD / "subvulpes_server.exe"}" || exit /b 1',
             f'cmake --build "{target}" --target sv-jukebox-package || exit /b 1']
    batch.write_text('\n'.join(lines) + '\n', encoding='ascii')
    run(['cmd', '/d', '/c', str(batch)], cwd=work)
    cache = (target / 'CMakeCache.txt').read_text()
    include_line = next(line for line in cache.splitlines() if line.startswith('SV_PLUGIN_INCLUDE_DIR:PATH='))
    assert Path(include_line.split('=', 1)[1]).resolve() == (sdk / 'include').resolve(), 'The build did not use SDK headers.'
    plugin = target / 'plugins/sv.jukebox'
    assert sorted(p.name for p in plugin.iterdir()) == ['jukebox.dll', 'plugin.ini']
    package = target / 'packages/sv.jukebox-0.1.0-native.svplugin'
    with zipfile.ZipFile(package) as zipped:
        assert sorted(zipped.namelist()) == ['jukebox.dll', 'plugin.ini']
    inspection = subprocess.run([str(BUILD / 'subvulpes_server.exe'), '--plugin-inspect', str(package)],
                                cwd=work, capture_output=True, text=True, timeout=120)
    assert inspection.returncode == 0 and '\tnative\tread,write,config\t' in inspection.stdout, inspection.stdout + inspection.stderr
    assert not (plugin / 'instruments.ini').exists()
    # This driver captures real public API calls from the DLL built with the extracted SDK.
    isolated = work / 'host'
    isolated.mkdir()
    run([str(BUILD / 'tests/jukebox-host-test.exe'), str(plugin / 'jukebox.dll'), 'demo'], cwd=isolated)
    run([sys.executable, '-I', str(ROOT / 'tests/jukebox_scenario_test.py'), str(BUILD), str(package), str(sdk / 'tools')])
print('JUKEBOX_SDK_PASS: isolated SDK build, two-file package, inspection, API calls and scenario')
