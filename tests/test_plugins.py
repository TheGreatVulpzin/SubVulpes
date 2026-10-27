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


def profile(entries, *, dependencies=None, approval=True, abi='1', live='1', unsafe=None, settings=''):
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
            f'[plugin]\nid={key}\nversion=0.1.0\nlibrary={binary.name}\napi={abi}\nlive={live}\ndependencies={deps}\n' +
            (f'unsafe_events={unsafe}\n' if unsafe is not None else '') + settings)
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
# The status answer goes to the server log, so a player must not run it.
output = run(RUNNER, ['status-local'], profile({'sv.status': 'enabled'}),
             contains=['Status: tick 42', 'Plugin command permission denied: status', 'PASS'])
assert output.count('Status: tick') == 1, 'A player ran the status command.'
run(RUNNER, ['metrics'], profile(fixture), mode='normal', contains=['Plugin metrics: test.fixture, 3 callbacks', 'fixture stop event'])
run(RUNNER, ['metrics'], profile(fixture), mode='resubscribe', contains=['PASS'], absent=['Plugin metrics'])
run(RUNNER, ['lifecycle'], profile(fixture), contains=['fixture stop event'])
run(RUNNER, ['fpu'], profile(fixture), mode='fpu', contains=['PASS'])
# A decision inside the simulation needs unsafe_events=1. Without it the subscribe call is refused (every other run above
# checks that in the fixture). A slow plugin and a failing plugin are disabled at the next tick.
optional = {'test.fixture': 'enabled'}
run(RUNNER, ['unsafe-budget'], profile(optional, unsafe='1'), mode='unsafe-budget',
    contains=['Plugin failed: test.fixture: Too many slow damage decisions.', 'PASS'])
run(RUNNER, ['unsafe-fail'], profile(optional, unsafe='1'), mode='unsafe-fail',
    contains=['Plugin failed: test.fixture: Event callback reported failure.', 'PASS'])
# The plugin list tells the administrator that this plugin takes damage decisions inside the simulation.
run(RUNNER, ['metrics'], profile(fixture, unsafe='1'), mode='unsafe-budget',
    contains=['takes damage decisions inside the simulation (unsafe_events)', 'PASS'])
# The settings of a plugin (N23): every type, every error, a restart, an update of the manifest, the file command, and the
# declarations that the host refuses.
SETTINGS = """
[setting.price]
type=int
default=125
min=0
max=1000000
label=Price
help=Price of one item.
live=1
admin=1
[setting.rate]
type=float
default=1.5
min=0.1
max=3
label=Rate
help=A factor.
[setting.enabled]
type=flag
default=1
label=Enabled
live=1
[setting.name]
type=text
default=Name
maxlen=20
label=Name
live=1
[setting.mode]
type=choice
choices=easy,normal,hard
default=normal
label=Mode
[setting.token]
type=secret
maxlen=30
label=Token
live=1
"""
folder = profile(fixture, settings=SETTINGS)
output = run(RUNNER, ['config'], folder, mode='config', contains=[
    'cfg price=125 rate=1.50 enabled=1 name=Name mode=1 token_len=0',
    'Setting saved: test.fixture.price (applies now)', 'CONFIG price [125] [300]',
    'cfg price=300 rate=1.50 enabled=1 name=Name mode=1 token_len=0',
    'Setting saved: test.fixture.rate (applies at the next server start)',
    'Setting refused: test.fixture.nosuch: unknown key',
    'Setting saved: test.fixture.mode (applies at the next server start)',
    'Setting saved: test.fixture.token (applies now)', 'CONFIG token [] []',
    'cfg price=300 rate=1.50 enabled=1 name=Name mode=1 token_len=6',
    'CONFIG price [300] [125]', 'cfg price=125 rate=1.50 enabled=1 name=Name mode=1 token_len=6',
    'test.fixture: 6 settings', 'Plugin not found: missing',
    'SETTING\ttest.fixture\tprice\tint\t300\t125\t0\t1000000\t1\t1\t0\tPrice\tPrice of one item.',
    'SETTING\ttest.fixture\trate\tfloat\t2\t1.5\t0.100000001\t3\t0\t0\t1\tRate\tA factor.',
    'SETTING\ttest.fixture\tmode\tchoice\thard\tnormal\t\teasy,normal,hard\t0\t0\t1\tMode\t',
    'SETTING\ttest.fixture\ttoken\tsecret\t*\t\t0\t30\t1\t0\t0\tToken\t',
    'CONFIG price [300] [77]', 'TELL 7 Saved test.fixture.price (now)',
    'TELL 7 Refused test.fixture.rate: denied by the plugin', 'TELL 7 price = 77',
    'TELL 8 Plugin command permission denied: plugins', 'PASS'],
    absent=['s3cret', 'TELL 7 rate =', 'TELL 7 token', 'TELL 8 price'])
# Ten values fail the check: four for the price, and the flag, the choice, two floats and two texts.
assert output.count('invalid value') == 10, 'A wrong value was not refused ten times.'
assert output.count('Setting refused: test.fixture.price: invalid value') == 4
# The values stay after a restart. The secret stays too, and its value is in no output.
output = run(RUNNER, ['config-read'], folder, mode='config-read', contains=[
    'cfg price=77 rate=2.00 enabled=1 name=Name mode=2 token_len=6', 'PASS'], absent=['s3cret'])
# The manifest changes: the key price stays, rate changes its type, extra is new, and four keys go away.
(folder / 'plugins/test.fixture/plugin.ini').write_text(
    '[plugin]\nid=test.fixture\nversion=0.1.0\nlibrary=fixture.dll\napi=1\nlive=1\ndependencies=\n'
    '[setting.price]\ntype=int\ndefault=125\nmin=0\nmax=1000000\nlabel=Price\nlive=1\nadmin=1\n'
    '[setting.rate]\ntype=int\ndefault=3\nmin=0\nmax=5\nlabel=Rate\n'
    '[setting.extra]\ntype=int\ndefault=9\nmin=0\nmax=99\nlabel=Extra\n')
run(RUNNER, ['config-read'], folder, mode='config-update', contains=['cfg price=77 rate=3 extra=9', 'PASS'])
# The file command of the launcher works with the server stopped.
run(SERVER, ['--plugin-config', 'test.fixture'], folder, contains=[
    'SubVulpes-plugin-config-v1', 'SETTING\ttest.fixture\tprice\tint\t77\t125', 'SETTING\ttest.fixture\textra\tint\t9\t9'])
run(SERVER, ['--plugin-config', 'test.fixture', 'extra', '11'], folder, contains=['Setting saved'])
run(SERVER, ['--plugin-config', 'test.fixture'], folder, contains=['SETTING\ttest.fixture\textra\tint\t11\t9'])
run(SERVER, ['--plugin-config', 'test.fixture', 'extra'], folder, contains=['Setting saved'])
run(SERVER, ['--plugin-config', 'test.fixture'], folder, contains=['SETTING\ttest.fixture\textra\tint\t9\t9'])
run(SERVER, ['--plugin-config', 'test.fixture', 'extra', '100'], folder, code=2, contains=['Setting refused'])
run(SERVER, ['--plugin-config', 'test.fixture', 'nosuch', '1'], folder, code=2, contains=['Setting refused'])
run(SERVER, ['--plugin-config', 'missing'], folder, code=2, contains=['not installed'])
BAD = {
    'unknown type': '[setting.a]\ntype=nosuch\ndefault=1\nlabel=A\n',
    'int range': '[setting.a]\ntype=int\ndefault=1\nmin=5\nmax=2\nlabel=A\n',
    'int default': '[setting.a]\ntype=int\ndefault=9\nmin=0\nmax=5\nlabel=A\n',
    'float default': '[setting.a]\ntype=float\ndefault=9\nmin=0\nmax=5\nlabel=A\n',
    'flag default': '[setting.a]\ntype=flag\ndefault=2\nlabel=A\n',
    'choice default': '[setting.a]\ntype=choice\nchoices=x,y\ndefault=z\nlabel=A\n',
    'one choice': '[setting.a]\ntype=choice\nchoices=x\ndefault=x\nlabel=A\n',
    'secret default': '[setting.a]\ntype=secret\ndefault=abc\nlabel=A\n',
    'text default length': '[setting.a]\ntype=text\ndefault=abcdef\nmaxlen=3\nlabel=A\n',
    'maxlen': '[setting.a]\ntype=text\ndefault=a\nmaxlen=64\nlabel=A\n',
    'uppercase key': '[setting.Price]\ntype=flag\ndefault=1\nlabel=A\n',
    'no label': '[setting.a]\ntype=flag\ndefault=1\n',
    'live value': '[setting.a]\ntype=flag\ndefault=1\nlabel=A\nlive=2\n',
    'admin value': '[setting.a]\ntype=flag\ndefault=1\nlabel=A\nadmin=yes\n',
    'semicolon in label': '[setting.a]\ntype=flag\ndefault=1\nlabel=A;B\n',
    'nine choices': '[setting.a]\ntype=choice\nchoices=a,b,c,d,e,f,g,h,i\ndefault=a\nlabel=A\n',
    'long choice name': '[setting.a]\ntype=choice\nchoices=x,abcdefghijklmnopqrstuvwx\ndefault=x\nlabel=A\n',
    'long key': '[setting.abcdefghijklmnopqrstuvwxyzabcdefg]\ntype=flag\ndefault=1\nlabel=A\n',
    'too many': ''.join(f'[setting.k{i}]\ntype=flag\ndefault=1\nlabel=K\n' for i in range(33)),
}
for name, bad in BAD.items():
    print('declaration:', name)
    run(RUNNER, ['invalid-metadata'], profile(fixture, settings=bad), contains=['PASS'])
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
    # The listing of the settings only reads files, so it works beside a running server. A change needs the stopped server.
    run(SERVER, ['--plugin-config', 'test.fixture'], folder, contains=['SubVulpes-plugin-config-v1'])
    run(SERVER, ['--plugin-config', 'test.fixture', 'anykey', '1'], folder, code=2, contains=['Stop the server'])
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
