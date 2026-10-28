"""The manifest rules seen from outside: the validate command, the validate script and the installer."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

BUILD = Path(sys.argv[1]).resolve()
ROOT = Path(__file__).resolve().parent.parent
SERVER = BUILD / 'subvulpes_server.exe'
DLL = BUILD / 'tests/fixture.dll'
CASES = BUILD / 'tests/cases'
CASES.mkdir(parents=True, exist_ok=True)
POWERSHELL = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File']
BASE = 'id=test.manifest\nversion=0.1.0\nlibrary=fixture.dll\napi=1\nlive=1\n'
checked = 0


def folder_with(extra, manifest=None):
    folder = Path(tempfile.mkdtemp(prefix='manifest-', dir=CASES)).resolve()
    (folder / 'plugin.ini').write_text('[plugin]\n' + (manifest if manifest is not None else BASE) + extra, encoding='ascii')
    shutil.copyfile(DLL, folder / 'fixture.dll')
    return folder


def validate(folder):
    result = subprocess.run([str(SERVER), '--plugin-validate-folder', str(folder)], capture_output=True, text=True, timeout=30)
    return result.returncode, result.stdout + result.stderr


def accept(extra, *needles):
    global checked
    code, output = validate(folder_with(extra))
    assert code == 0 and output.startswith('MANIFEST\t') and all(n in output for n in needles), (extra, code, output)
    checked += 1
    return output


def refuse(extra, key, manifest=None):
    global checked
    code, output = validate(folder_with(extra, manifest))
    assert code == 1 and f'INVALID\t{key}\t' in output, (extra, code, output)
    assert 'INVALID' in output.splitlines()[0]
    checked += 1
    return output


# A manifest with no new key is a native plugin with no capability.
accept('', '\tnative\t')
# Every new key, with the largest legal values.
accept('runtime=native\ncapabilities=read,write,props\nname=Health Commands\ndescription=%s\nauthor=%s\nhomepage=https://example.com/a\n' %
       ('d' * 160, 'a' * 48), '\tnative\t', 'read,write,props', 'Health Commands')
accept('capabilities=read,write,ai,economy,mission,settings,storage,engine,shop,props,config,plugins\n',
       'read,write,ai,economy,mission,settings,storage,engine,shop,props,config,plugins')
# Each wrong value gets its own line, and the report names the key and never the value.
refuse('runtime=java\n', 'runtime')
refuse('runtime=Native\n', 'runtime')
refuse('capabilities=read,fly\n', 'capabilities')
refuse('capabilities=read,read\n', 'capabilities')
refuse('capabilities=read,\n', 'capabilities')
refuse('name=%s\n' % ('x' * 49), 'name')
refuse('name=\n', 'name')
refuse('name=Tab\there\n', 'name')
refuse('description=%s\n' % ('d' * 161), 'description')
refuse('author=%s\n' % ('a' * 49), 'author')
refuse('homepage=ftp://example.com\n', 'homepage')
refuse('homepage=https://%s\n' % ('h' * 90), 'homepage')
refuse('name=A\nname=B\n', 'name')
output = refuse('runtime=java\ncapabilities=fly\nname=%s\n' % ('x' * 60), 'runtime')
assert 'INVALID\tcapabilities\t' in output and 'INVALID\tname\t' in output, 'The report must list every problem.'
assert 'java' not in output and 'fly' not in output and 'xxxx' not in output, 'The report repeated a value.'
# The library file must exist, and a script runtime has no adapter yet.
refuse('', 'library', manifest=BASE.replace('fixture.dll', 'absent.dll'))
# A script runtime needs a script file. With a DLL the extension does not match. With a script file the adapter is missing.
refuse('runtime=lua\n', 'library')
script_folder = folder_with('runtime=lua\n', manifest=BASE.replace('fixture.dll', 'main.lua'))
(script_folder / 'main.lua').write_text('-- empty\n', encoding='ascii')
code, output = validate(script_folder)
assert code == 1 and 'INVALID\truntime\t' in output, output
checked += 1

# The script checks a folder and a package with the same rules, and it refuses an unsafe entry name.
script = ROOT / 'tools/plugins/validate_plugin.ps1'


def run_script(path):
    result = subprocess.run([*POWERSHELL, str(script), '-Path', str(path), '-ServerExe', str(SERVER)],
                            capture_output=True, text=True, timeout=60)
    return result.returncode, result.stdout + result.stderr


def package(name, entries):
    path = CASES / name
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for entry, data in entries.items():
            archive.writestr(entry, data)
    return path


manifest_text = '[plugin]\n' + BASE + 'capabilities=read\nname=Packed\n'
good = package('good.svplugin', {'plugin.ini': manifest_text, 'fixture.dll': DLL.read_bytes()})
code, output = run_script(good)
assert code == 0 and 'MANIFEST\ttest.manifest\t0.1.0\tfixture.dll\tnative\tread\tPacked' in output, output
code, output = run_script(folder_with('capabilities=read\n'))
assert code == 0 and output.startswith('MANIFEST\t'), output
bad = package('bad.zip', {'plugin.ini': manifest_text.replace('capabilities=read', 'capabilities=fly'), 'fixture.dll': DLL.read_bytes()})
code, output = run_script(bad)
assert code != 0 and 'INVALID\tcapabilities' in output, output
unsafe = package('unsafe.zip', {'plugin.ini': manifest_text, 'fixture.dll': DLL.read_bytes(), '../evil.dll': b'MZ'})
code, output = run_script(unsafe)
assert code != 0 and 'unsafe name' in output, output
checked += 4

# The installer takes the new keys, and it asks the server about the values.
installer = ROOT / 'tools/plugins/install_plugin.ps1'


def install(package_path):
    server_dir = Path(tempfile.mkdtemp(prefix='install-', dir=CASES)).resolve()
    shutil.copyfile(SERVER, server_dir / 'subvulpes_server.exe')
    result = subprocess.run([*POWERSHELL, str(installer), '-ServerDir', str(server_dir), '-Package', str(package_path)],
                            capture_output=True, text=True, timeout=120)
    return result.returncode, result.stdout + result.stderr, server_dir


code, output, server_dir = install(good)
assert code == 0 and (server_dir / 'plugins/test.manifest/plugin.ini').exists(), output
assert 'capabilities=read' in (server_dir / 'plugins/test.manifest/plugin.ini').read_text(), 'The installer changed the manifest.'
code, output, server_dir = install(bad)
assert code != 0 and not (server_dir / 'plugins/test.manifest').exists(), output
long_name = package('long.zip', {'plugin.ini': manifest_text + 'description=%s\n' % ('d' * 161), 'fixture.dll': DLL.read_bytes()})
code, output, server_dir = install(long_name)
assert code != 0 and not (server_dir / 'plugins/test.manifest').exists(), output
script_runtime = package('lua.zip', {'plugin.ini': manifest_text + 'runtime=lua\n', 'fixture.dll': DLL.read_bytes()})
code, output, server_dir = install(script_runtime)
assert code != 0 and not (server_dir / 'plugins/test.manifest').exists(), output
checked += 4

print(f'MANIFEST_PASS {checked}')
