"""Check one-run approval scope, no persistent writes, locked content and minute warnings."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

BUILD = Path(sys.argv[1]).resolve()
SERVER = BUILD / 'subvulpes_server.exe'
WARNING = 'WARNING: development plugin approval is active for this run only.'
checks = 0

with tempfile.TemporaryDirectory(prefix='dev-plugins-', dir=BUILD / 'tests') as temporary:
    root = Path(temporary).resolve()
    plugin = root / 'plugins/test.trust'
    plugin.mkdir(parents=True)
    manifest = plugin / 'plugin.ini'
    manifest.write_bytes(b'[plugin]\nid=test.trust\nversion=0.1.0\nlibrary=trust-fixture.dll\napi=1\nlive=1\nruntime=native\ncapabilities=read\n')
    shutil.copyfile(BUILD / 'tests/trust-fixture.dll', plugin / 'trust-fixture.dll')
    profile = root / 'plugins/plugins.ini'
    profile.write_bytes(b'[plugins]\ntest.trust=enabled\n')
    private = root / 'user/plugins'
    private.mkdir(parents=True)
    (private / 'host.lock').touch()
    (private / 'test.trust').mkdir()
    (private / 'test.trust/storage.lock').touch()
    marker = root / 'loaded'
    environment = dict(os.environ, SV_PLUGIN_TRUST_MARKER=str(marker), SV_PLUGIN_TRUST_WAIT='0')

    def snapshot():
        return {str(p.relative_to(private)): p.read_bytes() for p in private.rglob('*') if p.is_file()}

    original = snapshot()

    def command(arguments, success, executes=False):
        global checks
        marker.unlink(missing_ok=True)
        result = subprocess.run([str(SERVER), *arguments], cwd=root, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=30)
        assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
        assert marker.exists() == executes, 'Native execution crossed the development approval boundary.'
        assert snapshot() == original, 'Development approval was saved.'
        checks += 1
        return result.stdout

    def development(folder, success=True):
        return command(['--dev-plugins', str(folder), '--plugins-check'], success, success)

    def outside():
        status = root / 'plugins/sv.status'
        shutil.copytree(BUILD / 'plugins/sv.status', status)
        profile.write_bytes(profile.read_bytes() + b'sv.status=enabled\n')
        (private / 'sv.status').mkdir()
        (private / 'sv.status/storage.lock').touch()
        original.update(snapshot())
        return status

    probe = sys.argv[2] if len(sys.argv) > 2 else None
    if probe == 'store':
        digest = hashlib.sha256((plugin / 'trust-fixture.dll').read_bytes()).hexdigest()
        command(['--dev-plugins', str(plugin), '--plugin-approve', 'test.trust', digest], False)
        print('PASS: development mode refuses persistent approval commands.')
        raise SystemExit(0)
    if probe == 'scope':
        outside()
        development(plugin, False)
        print('PASS: unapproved outsiders are rejected before DLL execution.')
        raise SystemExit(0)
    if probe == 'boundary':
        prefix = root / 'plugins/test'
        prefix.mkdir()
        development(prefix, False)
        print('PASS: a path prefix cannot expand the development folder.')
        raise SystemExit(0)

    command(['--plugins-check'], False)
    command(['--dev-plugins'], False)
    unmatched = root / 'unmatched'
    unmatched.mkdir()
    development(unmatched, False)
    output = development(root / 'plugins')
    assert WARNING in output and 'Approval is not saved.' in output
    command(['--plugins-check'], False)
    development(plugin)
    digest = hashlib.sha256((plugin / 'trust-fixture.dll').read_bytes()).hexdigest()
    command(['--dev-plugins', str(plugin), '--plugin-approve', 'test.trust', digest], False)
    command(['--dev-plugins', str(plugin), '--allow-unapproved-native', '--plugins-check'], False)
    command(['--dev-plugins', str(plugin), '--dev-plugins', str(plugin), '--plugins-check'], False)
    prefix = root / 'plugins/test'
    prefix.mkdir()
    development(prefix, False)
    status = outside()
    development(plugin, False)
    public_approvals = root / 'plugins/native-approvals.ini'
    status_hash = hashlib.sha256((status / 'status.dll').read_bytes()).hexdigest()
    public_approvals.write_bytes(('[native]\nsv.status=' + status_hash + '\n').encode())
    development(plugin)
    command(['--plugins-check'], False)

    # Keep enable active long enough to observe the minute warning independently of the game tick.
    marker.unlink(missing_ok=True)
    started = time.monotonic()
    process = subprocess.Popen([str(SERVER), '--dev-plugins', str(plugin), '--plugins-check'],
                               cwd=root, env=dict(environment, SV_PLUGIN_TRUST_WAIT='1'),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors='replace')
    try:
        deadline = started + 20
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
        assert marker.exists(), 'The development fixture did not start.'
        for path in (manifest, profile):
            try:
                with path.open('r+b'):
                    pass
            except PermissionError:
                continue
            raise AssertionError('Development content was not locked for the run.')
        output, error = process.communicate(timeout=85)
        assert process.returncode == 0, (output, error)
        assert time.monotonic() - started >= 60 and output.count(WARNING) >= 2, output
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    assert snapshot() == original, 'The minute warning saved development approval.'
    for path in (manifest, profile):
        with path.open('r+b'):
            pass
    assert public_approvals.read_bytes() == ('[native]\nsv.status=' + status_hash + '\n').encode()
    command(['--plugins-check'], False)

print('PASS: %d development checks plus locked content and the minute warning' % checks)
