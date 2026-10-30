"""Check scoped approval at the real loader, including the legacy CLI and bypass."""
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

BUILD = Path(sys.argv[1]).resolve()
SERVER = BUILD / 'subvulpes_server.exe'
ID = 'test.trust'
BASE = ('[plugin]\nid=test.trust\nversion=0.1.0\nlibrary=trust-fixture.dll\napi=1\nlive=1\n'
        'runtime=native\ncapabilities=read,write\nname=Trust Fixture\n')
checks = 0


def digest(data):
    return hashlib.sha256(data).hexdigest()


with tempfile.TemporaryDirectory(prefix='trust-', dir=BUILD / 'tests') as temporary:
    root = Path(temporary).resolve()
    plugin = root / 'plugins' / ID
    plugin.mkdir(parents=True)
    manifest = plugin / 'plugin.ini'
    manifest.write_bytes(BASE.encode())
    payload = plugin / 'trust-fixture.dll'
    shutil.copyfile(BUILD / 'tests/trust-fixture.dll', payload)
    (root / 'plugins/plugins.ini').write_bytes(b'[plugins]\ntest.trust=enabled\n')
    private = root / 'user/plugins'
    store = private / 'approvals.ini'
    marker = root / 'loaded'
    environment = dict(os.environ, SV_PLUGIN_TRUST_MARKER=str(marker))
    expected = [digest(payload.read_bytes()), digest(manifest.read_bytes()), 'native', 'read,write', '-']

    def command(*arguments, success=True):
        global checks
        result = subprocess.run([str(SERVER), *arguments], cwd=root, env=environment,
                                capture_output=True, text=True, errors='replace', timeout=25)
        assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
        checks += 1
        return result.stdout

    def load(success, bypass=False):
        marker.unlink(missing_ok=True)
        command(*(['--allow-unapproved-native'] if bypass else []), '--plugins-check', success=success)
        assert marker.exists() == success, 'Approval rejection occurred after native code executed.'

    def save(values=None, confirmed=True, success=True):
        return command('--plugin-trust', ID, *(expected if values is None else values),
                       *(['--native-confirmed'] if confirmed else []), success=success)

    def record(payload_hash=None, manifest_hash=None, runtime='native', caps='read,write'):
        return ['1', '0.1.0', runtime, caps, payload_hash or expected[0], manifest_hash or expected[1],
                '-', '2026-10-30T00:00:00Z']

    def write_record(fields):
        private.mkdir(parents=True, exist_ok=True)
        value = '|'.join(fields) if isinstance(fields, list) else fields
        store.write_bytes(('[approvals]\n' + ID + '=' + value + '\n').encode())

    # These probes isolate each comparison. The other record fields stay correct.
    probe = sys.argv[2] if len(sys.argv) > 2 else None
    if probe:
        fields = record()
        if probe == 'payload':
            fields[4] = '0' * 64
        elif probe == 'manifest':
            fields[5] = '0' * 64
        elif probe == 'runtime':
            fields[2] = 'lua'
        elif probe == 'caps':
            fields[3] = 'read'
        else:
            raise AssertionError('Unknown removal probe.')
        write_record(fields)
        (private / 'native-approvals.ini').write_bytes(('[native]\n' + ID + '=' + expected[0] + '\n').encode())
        load(False)
        print('PASS: %s approval mismatch rejected before DLL execution.' % probe)
        raise SystemExit(0)

    assert command('--plugin-trust-status', ID).strip() == 'TRUST_NONE\t' + ID
    assert not (root / 'user').exists()
    load(False)
    save(confirmed=False, success=False)
    for index, replacement in [(0, '0' * 64), (1, '0' * 64), (2, 'lua'), (3, 'read'),
                               (3, 'read,write,storage'), (4, 'bad'), (0, 'A' * 64)]:
        values = expected.copy()
        values[index] = replacement
        save(values, success=False)
    assert not store.exists(), 'A refused review created an approval.'
    save()
    fields = store.read_text().split(ID + '=', 1)[1].strip().split('|')
    assert fields[:7] == record()[:7], fields
    assert re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', fields[7]), fields
    saved_time = datetime.strptime(fields[7], '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc)
    assert abs((datetime.now(timezone.utc) - saved_time).total_seconds()) < 120
    line = command('--plugin-trust-status', ID).strip().split('\t')
    assert line[:9] == ['TRUST', '1', ID, '0.1.0', 'native', 'read,write', expected[0], expected[1], '']
    load(True)

    # A saved scoped record is authoritative, even when a legacy hash still matches.
    (private / 'native-approvals.ini').write_bytes(('[native]\n' + ID + '=' + expected[0] + '\n').encode())
    for index, replacement in [(4, '0' * 64), (5, '0' * 64), (2, 'lua'), (3, 'read')]:
        bad = record()
        bad[index] = replacement
        write_record(bad)
        load(False)
    write_record(record())
    changed = BASE.replace('read,write', 'read,write,storage')
    manifest.write_bytes(changed.encode())
    load(False)
    manifest.write_bytes(BASE.replace('Trust Fixture', 'Changed Name').encode())
    load(False)
    manifest.write_bytes(BASE.encode())
    payload.write_bytes(payload.read_bytes() + b'changed')
    load(False)
    payload.write_bytes(payload.read_bytes()[:-7])

    # No invalid record can fall back to a matching legacy hash.
    for bad in ['', '!', '?', 'not a record', 'x' * 700, '|'.join(record()) + '|extra',
                '|'.join(record()).replace('2026-10-30T00:00:00Z', 'invalid-time')]:
        write_record(bad)
        load(False)
    write_record(record())
    load(True)
    load(True, bypass=True)
    write_record(record(caps='read'))
    load(False)
    load(True, bypass=True)
    command('--plugin-approve', ID, '0' * 64, success=False)
    load(False)
    command('--plugin-approve', ID, expected[0])
    assert command('--plugin-trust-status', ID).strip() == 'TRUST_NONE\t' + ID
    load(True)

    # Both legacy files remain readable without a scoped record.
    (private / 'native-approvals.ini').unlink()
    (root / 'plugins/native-approvals.ini').write_bytes(('[native]\n' + ID + '=' + expected[0] + '\n').encode())
    load(True)
    save()
    write_record(record(caps='read'))
    load(False)
    store.unlink()
    store.mkdir()
    load(False)
    save(success=False)
    store.rmdir()
    save()
    snapshot = store.read_bytes()
    command('--plugin-trust-status', ID)
    assert store.read_bytes() == snapshot, 'Reading review status changed the trust store.'

print('PASS: %d scoped approval checks' % checks)
