"""Check text bounds, then remove each bound and require its rejection test to fail."""
from pathlib import Path
import os
import re
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]).resolve()
DRIVER = BUILD / 'tests/jukebox-files-test.exe'
SOURCES = [ROOT / 'plugins/jukebox' / name for name in ['files.c', 'instruments.c']]
SOUNDS = (ROOT / 'plugins/jukebox/sounds.ini').read_text(encoding='ascii')
MIX = 'jukebox-mix 2\n-3 39 0 100 0 100\n' + ''.join(
    f'{i} {2 if i == 10 else 1} {2 if i == 10 else 1} 0 -1 109 0 {0 if i == 10 else 1} 0 100\n'
    for i in range(1, 17))


def cases():
    result = {}
    def sound(name, old, new, error):
        result['sound-' + name] = ('sounds', SOUNDS.replace(old, new).encode(), error)
    result['sound-size'] = ('sounds', SOUNDS.encode() + b'#' + b'x\n' * 8200, '1 to 16384')
    result['sound-ascii'] = ('sounds', SOUNDS.encode() + b'#\xff\n', 'plain ASCII')
    result['sound-line'] = ('sounds', SOUNDS.encode() + b'#' * 192 + b'\n', 'line is too long')
    sound('id', '46=silent46', '47=silent46', 'ID needs 0 to 46')
    sound('name', 'silent46,silent', 'x' * 24 + ',silent', '1 to 23 characters')
    sound('tail', ',0,60,1.00', ',36001,60,1.00', 'tail needs 0 to 36000')
    sound('root', ',0,60,1.00', ',0,128,1.00', 'root needs 0 to 127')
    sound('gain', ',0,60,1.00', ',0,60,1.01', 'gain needs 0.05 to 1.0')
    result['mix-size'] = ('mix', MIX.encode() + b' ' * 4096, '1 to 4096')
    result['mix-ascii'] = ('mix', MIX.encode() + b'\xff', 'plain ASCII')
    result['mix-version'] = ('mix', MIX.replace('mix 2', 'mix 1').encode(), 'version must be 2')
    for name, index, value, error in [
        ('mode', 0, 11, 'mode is invalid'), ('fixed', 1, 47, 'fixed sound'),
        ('bpm', 2, 301, 'BPM needs'), ('speed', 3, 401, 'speed needs'),
        ('key', 4, -49, 'key needs'), ('master', 5, 201, 'master needs')]:
        lines = MIX.splitlines(); fields = lines[1].split(); fields[index] = str(value)
        lines[1] = ' '.join(fields)
        result['mix-' + name] = ('mix', ('\n'.join(lines) + '\n').encode(), error)
    for name, index, value, error in [
        ('channel', 0, 17, 'channels 1 to 16'), ('output', 1, 3, 'output needs'),
        ('resume', 2, 0, 'resume output'), ('muted', 3, 2, 'mute needs'),
        ('voice', 4, 47, 'voice needs'), ('root', 5, 128, 'root needs'),
        ('transpose', 6, 37, 'transpose needs'), ('mono', 7, 2, 'mono needs'),
        ('automatic', 8, 2, 'automatic mute'), ('volume', 9, 201, 'volume needs')]:
        lines = MIX.splitlines(); fields = lines[-1].split(); fields[index] = str(value)
        lines[-1] = ' '.join(fields)
        result['mix-' + name] = ('mix', ('\n'.join(lines) + '\n').encode(), error)
    result['mix-trailing'] = ('mix', MIX.encode() + b'0\n', 'extra data')
    return result


def compile_driver(work):
    result = subprocess.run(['cl', '/nologo', '/W4', '/WX', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
        '/I' + str(ROOT / 'plugins/jukebox'), '/I' + str(BUILD / 'plugins/jukebox'),
        str(ROOT / 'tests/jukebox_files_test.c'), *map(str, SOURCES),
        str(ROOT / 'plugins/jukebox/midi.c'), '/Fe:' + str(DRIVER)],
        cwd=work, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr


def run_case(work, data):
    kind, content, error = data
    file = work / 'input.txt'; file.write_bytes(content)
    result = subprocess.run([str(DRIVER), kind, str(file)], capture_output=True, text=True, timeout=10)
    return result.returncode == 2 and error in result.stdout


def control_removals():
    source = ROOT / 'plugins/jukebox/jukebox.c'
    original = source.read_bytes()
    text = original.decode()
    playback = runpy.run_path(str(ROOT / 'tests/jukebox_playback_test.py'))
    cases = {'bpm': ('bpm 301', 'BPM must be'), 'speed': ('speed 401', 'Speed must be'),
             'key': ('key -49', 'Key must be'), 'volume': ('vol 1 201', 'Volume must be'),
             'master': ('master 201', 'Master must be')}
    def build():
        result = subprocess.run(['cmake', '--build', str(BUILD), '--target', 'jukebox-host-test'],
            cwd=ROOT, capture_output=True, timeout=120)
        assert result.returncode == 0, result.stdout.decode(errors='replace')[-3000:]
    try:
        for match in re.finditer(r'!integer\((words\[\d\]), [^,]+, [^,]+, &value\)\) /\* CONTROL_BOUND: ([a-z]+) \*/', text):
            command, error = cases[match[2]]
            mutant = text[:match.start()] + '!integer(' + match[1] + ', LONG_MIN, LONG_MAX, &value))' + text[match.end():]
            source.write_text('#include <limits.h>\n' + mutant)
            build()
            try:
                playback['run']('v2-control-bad', control=command + '\n' + error + '\n')
            except AssertionError:
                print('PASS removal control-' + match[2], flush=True)
            else:
                raise AssertionError('Control removal survived: ' + match[2])
            source.write_bytes(original)
    finally:
        source.write_bytes(original)
        build()
    for command, error in cases.values():
        playback['run']('v2-control-bad', control=command + '\n' + error + '\n')
    print('JUKEBOX_CONTROL_REMOVAL_PASS: 5 bounds, source restored', flush=True)


def main():
    baseline = cases()
    saved = {source: source.read_bytes() for source in SOURCES}
    with tempfile.TemporaryDirectory(prefix='jukebox-bounds-', dir=BUILD / 'tests') as root:
        work = Path(root)
        compile_driver(work)
        for kind, data in [('mix', MIX), ('sounds', SOUNDS)]:
            file = work / 'valid.txt'; file.write_text(data, encoding='ascii')
            result = subprocess.run([str(DRIVER), kind, str(file)], capture_output=True)
            assert result.returncode == 0, (kind, result.stdout)
        for name, data in baseline.items():
            assert run_case(work, data), 'Baseline: ' + name
        if '--check' in sys.argv:
            print('JUKEBOX_TEXT_PASS:', len(baseline), 'bound rejections')
            return
        count = 0
        try:
            for source in SOURCES:
                text = saved[source].decode()
                matches = list(re.finditer(r'^(\s*)(?:else )?if \((.+)\)( \{)? /\* (SOUND|MIX)_BOUND: ([a-z]+) \*/', text, re.M))
                for match in matches:
                    group = 'sound' if match[4] == 'SOUND' else 'mix'
                    name = group + '-' + match[5]
                    condition = match[2]
                    if group == 'mix' and match[5] not in ('size', 'ascii', 'trailing'):
                        condition = re.sub(r'mix_number\(&at, [^,]+, [^,]+,',
                            'mix_number(&at, LONG_MIN, LONG_MAX,', condition)
                        condition = condition.replace(' || (next.bpm && next.bpm < 20)', '')
                        condition = condition.replace(' || channel != (int)i + 1', '')
                        replacement = match[1] + 'if (' + condition + ')' + (match[3] or '')
                    else:
                        replacement = match[1] + '(void)(' + condition + ');\n' + match[1] + 'if (0)' + (match[3] or '')
                    mutant = text[:match.start()] + replacement + text[match.end():]
                    source.write_text('#include <limits.h>\n' + mutant)
                    compile_driver(work)
                    assert not run_case(work, baseline[name]), 'Removal survived: ' + name
                    print('PASS removal', name, flush=True)
                    count += 1
                    source.write_bytes(saved[source])
        finally:
            for source, data in saved.items(): source.write_bytes(data)
            compile_driver(work)
            for name, data in baseline.items(): assert run_case(work, data), 'Restored: ' + name
        print('JUKEBOX_V2_REMOVAL_PASS:', count, 'bounds, source restored', flush=True)
    control_removals()


if __name__ == '__main__':
    if '--controls' in sys.argv: control_removals()
    else: main()
