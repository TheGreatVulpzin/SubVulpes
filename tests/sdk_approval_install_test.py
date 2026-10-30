"""Install through the shared reviewed package path and check its stored approval."""
import hashlib
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
SCRIPT = ROOT / 'tools/plugins/install_plugin.ps1'
MANIFEST = ('[plugin]\nid=test.trust\nversion=0.1.0\nlibrary=trust-fixture.dll\napi=1\nlive=1\n'
            'runtime=native\ncapabilities=read,write\nname=Approval Fixture\nauthor=An Author\ndescription=Tests combined installation.\n')
checks = 0


def digest(data):
    return hashlib.sha256(data).hexdigest()


with tempfile.TemporaryDirectory(prefix='approve-install-', dir=BUILD / 'tests') as temporary:
    work = Path(temporary).resolve()
    server_dir = work / 'server'
    server_dir.mkdir()
    shutil.copyfile(SERVER, server_dir / SERVER.name)
    shutil.copyfile(SCRIPT, server_dir / SCRIPT.name)
    marker = work / 'loaded'
    environment = dict(os.environ, SV_PLUGIN_TRUST_MARKER=str(marker), TEMP=str(work), TMP=str(work))
    payload = (BUILD / 'tests/trust-fixture.dll').read_bytes()
    package = work / 'test.trust.svplugin'
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('plugin.ini', MANIFEST.encode())
        archive.writestr('trust-fixture.dll', payload)

    def command(args, success=True):
        global checks
        result = subprocess.run(args, cwd=server_dir, env=environment, capture_output=True,
                                text=True, errors='replace', timeout=60)
        assert (result.returncode == 0) == success, (result.returncode, result.stdout, result.stderr)
        checks += 1
        return result.stdout

    report = command([str(server_dir / SERVER.name), '--plugin-inspect', str(package)])
    rows = [line.split('\t') for line in report.splitlines()]
    identity = next(row for row in rows if row[0] == 'INSPECT')
    declaration = next(row for row in rows if row[0] == 'MANIFEST')
    bundle = next(row[1] for row in rows if row[0] == 'PACKAGE')
    assert identity[3:] == [digest(payload), digest(MANIFEST.encode())]
    assert declaration[4:6] == ['native', 'read,write'] and bundle == digest(package.read_bytes())

    def install(confirmed=True, success=True, changes=None):
        fields = {'ExpectedPackageHash': bundle, 'ExpectedPayloadHash': identity[3],
                  'ExpectedManifestHash': identity[4], 'ExpectedRuntime': 'native',
                  'ExpectedCapabilities': 'read,write'}
        if changes:
            fields.update(changes)
        args = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(SCRIPT),
                '-ServerDir', str(server_dir), '-Package', str(package), '-Approve']
        for key, value in fields.items():
            args += ['-' + key, value]
        if confirmed:
            args += ['-NativeConfirmed']
        return command(args, success)

    install(confirmed=False, success=False)
    assert not (server_dir / 'plugins/test.trust').exists()
    install()
    store = server_dir / 'user/plugins/approvals.ini'
    fields = store.read_text().split('test.trust=', 1)[1].strip().split('|')
    assert fields[:7] == ['1', '0.1.0', 'native', 'read,write', identity[3], identity[4], bundle]
    assert not (server_dir / 'user/plugins/native-approvals.ini').exists()
    command([str(server_dir / SERVER.name), '--plugin-enable', 'test.trust'])
    command([str(server_dir / SERVER.name), '--plugins-check'])
    assert marker.exists(), 'The approved installed DLL did not execute.'

    def snapshot():
        return {str(p.relative_to(server_dir)): p.read_bytes() for p in server_dir.rglob('*')
                if p.is_file() and not p.relative_to(server_dir).as_posix().startswith('backup/')}

    original = snapshot()
    for changes in [{'ExpectedPayloadHash': '0' * 64}, {'ExpectedManifestHash': '0' * 64},
                    {'ExpectedCapabilities': 'read'}, {'ExpectedRuntime': 'lua'},
                    {'ExpectedPackageHash': '0' * 64}]:
        install(success=False, changes=changes)
        assert snapshot() == original, 'A refused reviewed install changed installed or private data.'
    # A reviewed update keeps private data and replaces the scoped content hashes.
    private_data = server_dir / 'user/plugins/test.trust.data'
    private_data.write_bytes(b'preserved plugin data')
    updated_manifest = MANIFEST.replace('Tests combined installation.', 'Tests the next reviewed build.')
    updated_payload = payload + b'next build'
    with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('plugin.ini', updated_manifest.encode())
        archive.writestr('trust-fixture.dll', updated_payload)
    report = command([str(server_dir / SERVER.name), '--plugin-inspect', str(package)])
    rows = [line.split('\t') for line in report.splitlines()]
    identity = next(row for row in rows if row[0] == 'INSPECT')
    bundle = next(row[1] for row in rows if row[0] == 'PACKAGE')
    install()
    saved = store.read_text().split('test.trust=', 1)[1].strip().split('|')
    assert saved[3:7] == ['read,write', digest(updated_payload), digest(updated_manifest.encode()), bundle]
    assert private_data.read_bytes() == b'preserved plugin data'
    command([str(server_dir / SERVER.name), '--plugins-check'])
    original = snapshot()
    package.write_bytes(package.read_bytes() + b'changed after review')
    install(success=False)
    assert snapshot() == original, 'Package replacement changed files after refusal.'

print('PASS: %d combined install and approval checks' % checks)
