"""Exercise the real native loader with isolated, generated test profiles."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


BUILD = Path(sys.argv[1]).resolve()
CASES = BUILD / 'tests/cases'
CASES.mkdir(parents=True, exist_ok=True)
SERVER = BUILD / 'subvulpes_server.exe'
RUNNER = BUILD / 'tests/sv-plugin-test.exe'
SOURCE = {
    'sv.status': BUILD / 'plugins/sv.status/status.dll',
    'test.fixture': BUILD / 'tests/fixture.dll',
    'test.dependent': BUILD / 'tests/dependent.dll',
}
completed = 0


def profile(entries, *, dependencies=None, approval=True, abi='1', live='1'):
    folder = Path(tempfile.mkdtemp(prefix='native-', dir=CASES)).resolve()
    assert folder.is_relative_to(CASES.resolve())
    plugins = folder / 'plugins'
    plugins.mkdir()
    (plugins / 'plugins.ini').write_text('[plugins]\n' + ''.join(f'{key}={value}\n' for key, value in entries.items()))
    hashes = []
    for key in entries:
        if key not in SOURCE:
            continue
        target = plugins / key
        target.mkdir()
        binary = target / SOURCE[key].name
        shutil.copyfile(SOURCE[key], binary)
        if key == 'test.dependent':
            shutil.copyfile(BUILD / 'tests/sv_test_helper_0e9b6.dll', target / 'sv_test_helper_0e9b6.dll')
        deps = (dependencies or {}).get(key, '')
        (target / 'plugin.ini').write_text(
            f'[plugin]\nid={key}\nversion=0.1.0\nlibrary={binary.name}\napi={abi}\nlive={live}\ndependencies={deps}\n')
        hashes.append(f'{key}={hashlib.sha256(binary.read_bytes()).hexdigest()}\n')
    if approval:
        (plugins / 'native-approvals.ini').write_text('[native]\n' + ''.join(hashes))
    return folder


def run(executable, arguments, folder, *, code=0, mode='normal', contains=(), absent=(), input=None, marker=None):
    global completed
    environment = os.environ.copy()
    environment['SV_PLUGIN_TEST_MODE'] = mode
    environment.pop('SV_PLUGIN_TEST_MARKER', None)
    if marker:
        environment['SV_PLUGIN_TEST_MARKER'] = str(marker)
    result = subprocess.run([str(executable), *arguments], cwd=folder, env=environment,
                            input=input, text=True, capture_output=True, timeout=15)
    output = result.stdout + result.stderr
    if result.returncode != code or any(text not in output for text in contains) or \
            any(text in output for text in absent):
        raise AssertionError(f'{arguments}: exit {result.returncode}, expected {code}\n{output}')
    completed += 1
    return output


fixture = {'test.fixture': 'enabled'}
both = {'sv.status': 'enabled', 'test.fixture': 'enabled'}
run(RUNNER, ['lifecycle'], profile(fixture), contains=['fixture enabled', 'fixture disabled', 'PASS'])
run(RUNNER, ['required'], profile({'test.fixture': 'required'}), contains=['PASS'])
run(RUNNER, ['dependency'], profile(both, dependencies={'sv.status': 'test.fixture'}), contains=['PASS'])
run(RUNNER, ['required-fail'], profile({'sv.status': 'required'}, dependencies={'sv.status': 'missing'}), contains=['PASS'])
run(RUNNER, ['required-fail'], profile({'missing': 'required'}), contains=['PASS'])
run(RUNNER, ['required-fail'], profile({'sv.status': 'required', 'test.fixture': 'required'},
    dependencies={'sv.status': 'test.fixture', 'test.fixture': 'sv.status'}), contains=['PASS'])
run(RUNNER, ['optional-fail'], profile(both), mode='enable-fail', contains=['Registrations removed', 'PASS'])
run(RUNNER, ['optional-fail'], profile(both), mode='conflict', contains=['PASS'])
run(RUNNER, ['event-fail'], profile(both), mode='event-fail', contains=['PASS'])
run(RUNNER, ['required-event-fail'], profile({'test.fixture': 'required'}), mode='event-fail', contains=['PASS'])
# Actor 7 is an administrator and actor 8 is not. Actor 0 is the local operator.
run(RUNNER, ['permissions'], profile(fixture), mode='permissions',
    contains=['ran perm_public 0', 'ran perm_admin 0', 'ran perm_local 0', 'ran perm_public 8', 'ran perm_admin 7',
              'permission denied: perm_admin', 'permission denied: perm_local', 'PASS'],
    absent=['ran perm_admin 8', 'ran perm_local 7', 'ran perm_local 8'])
header = (Path(__file__).resolve().parents[1] / 'src/plugins/plugin_host.h').read_text()
command_max = int(re.search(r'SV_PLUGIN_COMMAND_MAX\s+(\d+)', header).group(1))
output = run(RUNNER, ['limit'], profile(fixture), mode='limit', contains=['PASS'])
assert output.count(f'commands registered {command_max}') == 2, 'The command table did not fill, then clear.'
run(RUNNER, ['metrics'], profile(fixture), mode='normal', contains=['Plugin metrics: test.fixture, 3 callbacks', 'fixture stop event'])
run(RUNNER, ['metrics'], profile(fixture), mode='resubscribe', contains=['PASS'], absent=['Plugin metrics'])
run(RUNNER, ['lifecycle'], profile(fixture), contains=['fixture stop event'])
run(RUNNER, ['fpu'], profile(fixture), mode='fpu', contains=['PASS'])
run(RUNNER, ['control'], profile(fixture), input='x' * 400 + '\nplugins disable test.fixture\n', contains=['PASS'])

folder = profile({'sv.status': 'required'}, approval=False)
run(SERVER, ['--plugins-check'], folder, code=2, contains=['approval required'])
run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, contains=['WARNING:', '\tactive\t'])
run(SERVER, ['--allow-unapproved-native', '--plugins-check'],
    profile({'sv.status': 'required'}, approval=False, abi='999'), code=2, contains=['failed'])
hash_value = hashlib.sha256(SOURCE['sv.status'].read_bytes()).hexdigest()
run(SERVER, ['--plugin-hash', 'sv.status'], folder, contains=[hash_value, 'crash'])
run(SERVER, ['--plugin-approve', 'sv.status', '0' * 64], folder, code=2, contains=['Approval failed'])
run(SERVER, ['--plugin-approve', 'sv.status', hash_value], folder, contains=['approved'])
assert hash_value in (folder / 'user/plugins/native-approvals.ini').read_text()
run(SERVER, ['--plugins-check'], folder, contains=['\tactive\t'])
binary = folder / 'plugins/sv.status/status.dll'
with binary.open('ab') as stream:
    stream.write(b'content identity changed')
run(SERVER, ['--plugins-check'], folder, code=2, contains=['approval required'])
run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, contains=['\tactive\t'])
run(SERVER, ['--plugins-check'], profile({'../escape': 'required'}), code=2, contains=['profile is invalid'])
for invalid_id in ('con', 'con.plugin', 'nul', 'com1', 'lpt9', 'trailing.'):
    run(SERVER, ['--plugins-check'], profile({invalid_id: 'required'}), code=2, contains=['profile is invalid'])
for invalid_library in ('con.dll', 'nul.dll', 'com1.dll', 'lpt9.dll'):
    folder = profile({'sv.status': 'required'})
    manifest = folder / 'plugins/sv.status/plugin.ini'
    manifest.write_text(manifest.read_text().replace('library=status.dll', 'library=' + invalid_library))
    run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, code=2, contains=['metadata'])

folder = profile({'sv.status': 'required'})
manifest = folder / 'plugins/sv.status/plugin.ini'
manifest.write_text(manifest.read_text().replace('version=0.1.0', 'version=0.1.1'))
run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, code=2, contains=['descriptor differs'])

folder = profile({'sv.status': 'enabled'})
run(SERVER, ['--plugin-disable', 'sv.status'], folder, contains=['state saved'])
assert 'sv.status=disabled' in (folder / 'user/plugins/state.ini').read_text()
output = run(SERVER, ['--plugins-check'], folder, contains=['\tdisabled\t'])
assert '\tactive\t' not in output
run(SERVER, ['--plugin-enable', 'sv.status'], folder, contains=['state saved'])
run(SERVER, ['--plugins-check'], folder, contains=['\tactive\t'])
run(SERVER, ['--plugin-disable', 'sv.status'], profile({'sv.status': 'required'}), code=2, contains=['cannot be disabled'])
folder = profile(fixture, abi='999')
run(RUNNER, ['invalid-metadata'], folder, contains=['PASS'])
run(SERVER, ['--plugin-enable', 'test.fixture'], folder, code=2, contains=['invalid plugin metadata'])
folder = profile({'test.fixture': 'disabled'}, live='0')
run(RUNNER, ['restart-enable'], folder, mode='restart', contains=['PASS'])
assert 'test.fixture=enabled' in (folder / 'user/plugins/state.ini').read_text()
run(RUNNER, ['restart'], folder, mode='restart', contains=['PASS'])
assert 'test.fixture=disabled' in (folder / 'user/plugins/state.ini').read_text()
run(SERVER, ['--plugins-check'], folder, mode='restart', contains=['\tdisabled\t'])
folder = profile({'test.fixture': 'required'}, approval=False)
marker = folder / 'executed.marker'
run(SERVER, ['--plugins-list'], folder, contains=['\tapproval\t'], marker=marker)
assert not marker.exists(), 'Metadata listing executed native initialization.'
run(SERVER, ['--plugins-check'], folder, code=2, contains=['approval required'], marker=marker)
assert not marker.exists(), 'An unapproved DLL executed native initialization.'
run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, code=2, marker=marker)
assert marker.exists(), 'The explicit bypass did not permit native loading.'
folder = profile({'test.dependent': 'required'})
marker = folder / 'helper-executed.marker'
run(SERVER, ['--plugins-check'], folder, code=2, contains=['DLL load failed'], marker=marker)
assert not marker.exists(), 'An adjacent helper DLL executed without approval.'
run(SERVER, ['--allow-unapproved-native', '--plugins-check'], folder, code=2,
    contains=['DLL load failed'], marker=marker)
assert not marker.exists(), 'The bypass weakened the system-library dependency restriction.'

folder = profile(fixture)
environment = os.environ.copy()
environment['SV_PLUGIN_TEST_MODE'] = 'normal'
process = subprocess.Popen([str(RUNNER), 'hold'], cwd=folder, env=environment,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    while True:
        line = process.stdout.readline()
        assert line, 'The host exited before obtaining its profile lock.'
        if line.strip() == 'READY':
            break
    run(SERVER, ['--plugins-check'], folder, code=2, contains=['Cannot lock plugin state'])
    run(SERVER, ['--plugin-disable', 'test.fixture'], folder, code=2, contains=['Stop the server'])
    output, error = process.communicate(timeout=5)
    assert process.returncode == 0 and 'PASS' in output, output + error
    completed += 1
finally:
    if process.poll() is None:
        process.kill()
        process.wait()

folder = profile(fixture)
environment = os.environ.copy()
environment['SV_PLUGIN_TEST_MODE'] = 'normal'
process = subprocess.Popen([str(RUNNER), 'control'], cwd=folder, env=environment,
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
process.stdin.write('plugins disable test.fixture\n')
process.stdin.flush()
try:
    exit_code = process.wait(timeout=5)
    output = process.stdout.read() + process.stderr.read()
    assert exit_code == 0 and 'PASS' in output, output
    completed += 1
finally:
    process.stdin.close()
    process.stdout.close()
    process.stderr.close()
    if process.poll() is None:
        process.kill()
        process.wait()
print(f'{completed} native plugin checks passed.')
