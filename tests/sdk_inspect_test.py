"""Exercise the read-only plugin inspection contract with real packages and installed plugins."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

BUILD = Path(sys.argv[1]).resolve()
SERVER = BUILD / 'subvulpes_server.exe'
DLL = (BUILD / 'tests/fixture.dll').read_bytes()
BASE = ('[plugin]\nid=test.inspect\nversion=0.1.0\nlibrary=fixture.dll\napi=1\nlive=1\n'
        'runtime=native\ncapabilities=read,write,storage\nname=Review Example\n'
        'author=An Author\ndescription=Tests package review.\nhomepage=https://example.test/review\n')
checks = 0


def digest(data):
    return hashlib.sha256(data).hexdigest()


def oversized_file(path, size):
    with path.open('wb') as stream:
        if os.name == 'nt':
            import ctypes
            from ctypes import wintypes
            import msvcrt
            count = wintypes.DWORD()
            # NTFS keeps the logical size without allocating zero-filled test data.
            ctypes.windll.kernel32.DeviceIoControl(
                wintypes.HANDLE(msvcrt.get_osfhandle(stream.fileno())), 0x900C4,
                None, 0, None, 0, ctypes.byref(count), None)
        stream.truncate(size)


with tempfile.TemporaryDirectory(prefix='inspect-', dir=BUILD / 'tests') as temporary:
    root = Path(temporary).resolve()
    temp = root / 'tmp'
    temp.mkdir()
    environment = dict(os.environ, TEMP=str(temp), TMP=str(temp))
    installed = root / 'plugins/test.inspect'
    installed.mkdir(parents=True)
    (installed / 'fixture.dll').write_bytes(DLL)

    def run(value, command='--plugin-inspect', success=True, server=SERVER):
        global checks
        result = subprocess.run([str(server), command, str(value)], cwd=root, env=environment,
                                capture_output=True, text=True, errors='replace', timeout=45)
        assert (result.returncode == 0) == success, (value, result.returncode, result.stdout, result.stderr)
        assert not (root / 'user').exists(), 'Inspection saved private state.'
        assert not list(temp.iterdir()), 'Inspection left a staging folder.'
        if not success:
            assert not any(line.startswith('INSPECT\t') for line in result.stdout.splitlines()), result.stdout
        checks += 1
        return [line.split('\t') for line in result.stdout.splitlines()]

    def package(name, text=BASE, entries=None, compression=zipfile.ZIP_DEFLATED):
        path = root / name
        with zipfile.ZipFile(path, 'w', compression=compression) as archive:
            for key, data in (entries if entries is not None else [('plugin.ini', text.encode()), ('fixture.dll', DLL)]):
                archive.writestr(key, data)
        return path

    def inspect_ok(value, text, bundle=''):
        rows = run(value)
        assert rows[0] == ['MANIFEST', 'test.inspect', '0.1.0', 'fixture.dll', 'native',
                           'read,write,storage', 'Review Example', 'Tests package review.',
                           'An Author', 'https://example.test/review'], rows
        assert rows[1] == ['INSPECT', '1', 'test.inspect', digest(DLL), digest(text.encode())], rows
        assert rows[-1] == ['PACKAGE', bundle], rows
        danger = {row[1]: row[2:] for row in rows[2:-1]}
        assert set(danger) == {'read', 'write', 'storage', 'native'}, danger
        assert danger['read'][0] == danger['storage'][0] == 'low', danger
        assert danger['write'][0] == 'high' and 'kick' in danger['write'][1], danger
        assert danger['native'][0] == 'highest' and 'full access' in danger['native'][1], danger
        assert all(len(row) == 4 for row in rows[2:-1]), rows

    manifest = installed / 'plugin.ini'
    manifest.write_bytes(BASE.encode('ascii'))
    inspect_ok('test.inspect', BASE)
    original = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
    inspect_ok('test.inspect', BASE)
    assert original == {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
    for compression, extension in [(zipfile.ZIP_STORED, '.zip'), (zipfile.ZIP_DEFLATED, '.svplugin')]:
        path = package("review ' $ [name]" + extension, compression=compression)
        inspect_ok(path, BASE, digest(path.read_bytes()))

    # A review hashes the manifest as well as the payload. Metadata-only changes remain visible.
    changed = BASE.replace('Review Example', 'Changed Example')
    manifest.write_bytes(changed.encode('ascii'))
    rows = run('test.inspect')
    assert rows[1][3] == digest(DLL) and rows[1][4] == digest(changed.encode()), rows
    unsafe = BASE + 'unsafe_events=1\n'
    manifest.write_bytes(unsafe.encode('ascii'))
    rows = run('test.inspect')
    assert any(row[:3] == ['DANGER', 'unsafe_events', 'high'] for row in rows), rows
    manifest.write_bytes(BASE.encode('ascii'))
    payload = installed / 'fixture.dll'
    payload.write_bytes(DLL + b'changed')
    rows = run('test.inspect')
    assert rows[1][3] == digest(DLL + b'changed') and rows[1][4] == digest(BASE.encode()), rows
    payload.write_bytes(DLL)

    for text in [BASE.replace('id=test.inspect', 'id=other.id'),
                 BASE + 'capabilities=read\n',
                 BASE.replace('read,write,storage', 'read,unknown'),
                 BASE.replace('runtime=native', 'runtime=lua'),
                 BASE.replace('name=Review Example', 'name=Bad\tName'),
                 BASE + ';' * 65536]:
        manifest.write_bytes(text.encode('ascii'))
        run('test.inspect', success=False)
    manifest.write_bytes(BASE.encode('ascii'))
    payload.write_bytes(b'MZ' + b'x' * 100)
    run('test.inspect', success=False)
    oversized_file(payload, 67108865)
    run('test.inspect', success=False)
    payload.write_bytes(DLL)
    run('missing.plugin', success=False)
    run('../outside', success=False)

    cases = [
        [('plugin.ini', BASE.encode())],
        [('plugin.ini', BASE.encode()), ('fixture.dll', DLL), ('extra.txt', b'no')],
        [('plugin.ini', BASE.encode()), ('../escape.dll', DLL)],
        [('plugin.ini', BASE.encode()), ('folder/fixture.dll', DLL)],
        [('plugin.ini', BASE.encode()), ('Fixture.dll', DLL)],
        [('plugin.ini', BASE.encode()), ('fixture.dll', b'bad DLL')],
        [('plugin.ini', (BASE + 'capabilities=read\n').encode()), ('fixture.dll', DLL)],
        [('plugin.ini', BASE.encode()), ('fixture.dll', DLL), ('FIXTURE.DLL', DLL)],
        [('plugin.ini', BASE.encode()), ('fixture.dll', b'x' * 67108865)],
        [('plugin.ini', b';' * 65537), ('fixture.dll', DLL)],
    ]
    symlink = zipfile.ZipInfo('fixture.dll')
    symlink.create_system = 3
    symlink.external_attr = 0o120777 << 16
    cases.append([('plugin.ini', BASE.encode()), (symlink, b'outside')])
    for index, entries in enumerate(cases):
        run(package('invalid-%d.zip' % index, entries=entries), success=False)
    assert not (root / 'escape.dll').exists()
    invalid = root / 'invalid.zip'
    invalid.write_bytes(b'PK invalid ZIP')
    run(invalid, success=False)
    oversized_file(invalid, 73400321)
    run(invalid, success=False)

    # Directory junctions must not bypass the plain-path check.
    junction = root / 'linked'
    made = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(junction), str(installed.parent)],
                          capture_output=True, text=True)
    assert made.returncode == 0, made.stderr
    try:
        run(junction / installed.name, command='--plugin-inspect-folder', success=False)
    finally:
        junction.rmdir()

    # Package review needs the shipped helper. Installed review needs only the server.
    standalone = root / 'standalone'
    standalone.mkdir()
    copied_server = standalone / 'subvulpes_server.exe'
    shutil.copyfile(SERVER, copied_server)
    run(package('no-helper.zip'), success=False, server=copied_server)
    run('test.inspect', server=copied_server)

print('PASS: %d plugin inspection checks' % checks)
