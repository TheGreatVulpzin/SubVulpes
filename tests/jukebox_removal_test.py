"""Run reader guard removals one at a time. Use an x86 MSVC prompt, with no other build running."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]).resolve()
SOURCE = ROOT / 'plugins/jukebox/midi.c'
DRIVER = BUILD / 'tests/jukebox-midi-test.exe'
TEST = ROOT / 'tests/jukebox_midi_test.py'
original = SOURCE.read_bytes()
text = original.decode()


def build():
    result = subprocess.run(['cmake', '--build', str(BUILD), '--target', 'jukebox-midi-test'], cwd=ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert result.returncode == 0, result.stdout.decode(errors='replace')[-4000:]


def test(name=None):
    return subprocess.run([sys.executable, '-I', str(TEST), str(DRIVER)] + ([name] if name else []),
                          cwd=ROOT, capture_output=True, text=True, timeout=90)


checks = list(re.finditer(r'^([ \t]*)if \((.*)\)( \{)? /\* BOUND: ([a-z-]+) \*/', text, re.M))
assert len(checks) >= 20
try:
    for check in checks:
        name = check[4]
        replacement = check[1] + '(void)(' + check[2] + ');\n' + check[1] + 'if (0)' + (check[3] or '') + ' /* BOUND: ' + name + ' */'
        SOURCE.write_bytes((text[:check.start()] + replacement + text[check.end():]).encode())
        build()
        result = test(name)
        assert result.returncode != 0, 'Removal did not fail: ' + name
        print('PASS removal ' + name + ': rejection test fails.', flush=True)
        SOURCE.write_bytes(original)
    # This status rejection is an else branch rather than a numeric bound.
    guard = 'return fail(r, "MIDI contains an unsupported system status.");'
    assert text.count(guard) == 1
    SOURCE.write_bytes(text.replace(guard, 'event.kind = IGNORE;').encode())
    build()
    assert test('system-status').returncode != 0
    print('PASS removal system-status: rejection test fails.', flush=True)
finally:
    SOURCE.write_bytes(original)
    build()
    result = test()
    print(result.stdout, end='', flush=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert SOURCE.read_bytes() == original
print(f'JUKEBOX_REMOVAL_PASS {len(checks) + 1} checks, source restored.', flush=True)
