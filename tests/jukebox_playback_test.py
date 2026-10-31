"""Check actual sound calls from the DLL and keep all fixture files temporary."""
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]).resolve()
F = SimpleNamespace(**runpy.run_path(str(Path(__file__).with_name('jukebox_fixtures.py'))))
DRIVER = BUILD / 'tests/jukebox-host-test.exe'
DLL = BUILD / 'plugins/sv.jukebox/jukebox.dll'
DEFAULTS = (ROOT / 'plugins/jukebox/instruments.ini').read_text(encoding='ascii').encode('ascii')


def fixtures(folder, many=False):
    tracks = []
    for channel in range(15):
        track = F.event(0, 192 + channel, channel)
        for step in range(3):
            track += F.event(0 if not step else 480, 144 + channel, 60 + channel + 2 * step, 50 + channel + step)
        tracks.append(track + F.END)
    (folder / 'fifteen.mid').write_bytes(F.midi(*tracks))
    (folder / 'other.mid').write_bytes(F.midi(F.event(0, 144, 60, 100) + F.END))
    (folder / 'burst.mid').write_bytes(F.midi(b''.join(F.event(0, 144, 60 + i, 10 + i) for i in range(12)) + F.END))
    (folder / 'keypad.mid').write_bytes(F.midi(b''.join(F.event(0 if not i else 240, 144, key, 100)
                                                              for i, key in enumerate([60, 64, 67, 72])) + F.END))
    (folder / 'extreme.mid').write_bytes(F.midi(F.event(0, 144, 0, 100) + F.event(0, 144, 127, 100) + F.END))

    (folder / 'broken.mid').write_bytes(b'bad')
    for name in ['zz-last', 'songalpha', 'timeline', 'mixsong', 'moonlight-sonata', 'cirno', 'cirno1', 'cirno2', 'alphabeta']:
        track = F.event(0, 144, 60, 127) + F.event(0, 153, 36, 127)
        track += F.event(480, 144, 60, 127) + F.event(0, 153, 36, 127)
        track += F.event(480, 144, 60, 127) + F.event(0, 153, 36, 127)
        (folder / (name + '.mid')).write_bytes(F.midi(track + F.END))
    (folder / 'mapped.mid').write_bytes(F.midi(F.event(0, 144, 60, 100) +
        F.event(480, 255, 81, 3, 15, 66, 64) + F.event(0, 144, 60, 100) +
        F.event(480, 144, 60, 100) + F.END))
    for name, keys, deltas in [('mono', [109, 109], [0, 160]),
                              ('low', [85, 85, 85, 85], [0, 224, 656, 16]),
                              ('high', [121, 121, 121], [0, 96, 16])]:
        (folder / (name + '.mid')).write_bytes(F.midi(b''.join(F.event(d, 144, k, 127)
            for k, d in zip(keys, deltas)) + F.END))
    tracks = [b''.join(F.event(0 if not i else 480, 144 + ch, 109, 100)
        for i in range(ch + 1)) + F.END for ch in range(6)]
    (folder / 'six.mid').write_bytes(F.midi(*tracks))
    for i in range(140 if many else 0):
        (folder / ('fill' + str(i) + '.mid')).write_bytes(F.midi(F.event(0, 144, 60, 100) + F.END))
    for i in range(12):
        (folder / ('many' + str(i) + '.mid')).write_bytes(F.midi(F.event(0, 144, 60, 100) + F.END))


def run(mode, instruments=None, bad_midi=None, mix=None, sounds=None, control=None):
    with tempfile.TemporaryDirectory(prefix='jukebox-host-', dir=BUILD / 'tests') as root:
        assert Path(root).resolve().parent == (BUILD / 'tests').resolve()
        folder = Path(root) / 'user/jukebox'
        folder.mkdir(parents=True)
        fixtures(folder, mode == 'v2-names')
        file = folder / 'instruments.ini'
        if instruments is not None:
            file.write_bytes(instruments)
        if mix is not None:
            (folder / 'timeline.mix').write_bytes(mix)
        if sounds is not None:
            (folder / 'sounds.ini').write_bytes(sounds)
        if control is not None:
            (Path(root) / 'control.txt').write_bytes(control.encode('ascii'))
        if bad_midi is not None:
            (folder / 'bad.mid').write_bytes(bad_midi)
        result = subprocess.run([str(DRIVER), str(DLL), mode], cwd=root, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0 and 'JUKEBOX_HOST_PASS' in result.stdout, (mode, result.returncode, result.stdout, result.stderr)
        assert file.read_bytes() == (DEFAULTS if instruments is None else instruments), 'Owner instrument file changed.'
        if mix is not None: assert (folder / 'timeline.mix').read_bytes() == mix
        if sounds is not None: assert (folder / 'sounds.ini').read_bytes() == sounds
        return result.stdout


if __name__ == '__main__':
    for mode in ['demo', 'channels', 'budget', 'keypad', 'scale', 'stop', 'v2-voices', 'v2-mono', 'v2-channels', 'v2-mix', 'v2-tempo', 'v2-key', 'v2-names', 'v2-volume', 'v2-restart', 'v2-transaction']:
        output = run(mode)
        if mode == 'v2-names':
            import re
            assert len(re.findall(r'^LOG many[0-9]+$', output, re.M)) == 8
    # Exact bytes, comments, line endings and owner frequency edits survive reload and re-enable.
    run('preserve', b'# owner edit\r\n' + DEFAULTS.replace(b'base_hz=1997', b'base_hz=1900').replace(b'base_hz=697', b'base_hz=700').replace(b'\n', b'\r\n'))
    for data in [DEFAULTS.replace(b'version=1', b'version=2'), DEFAULTS.replace(b'version=1', b''),
                 DEFAULTS.replace(b'sound=27', b'sound=47'), DEFAULTS.replace(b'base_hz=1997', b'base_hz=nan'),
                 DEFAULTS.replace(b'base_hz=1997', b'base_hz=inf'), DEFAULTS.replace(b'base_hz=1997', b'base_hz=0'),
                 DEFAULTS.replace(b'42=2', b'42=2.1'), DEFAULTS.replace(b'42=2\n', b''),
                 DEFAULTS + b'42=1\n', DEFAULTS.replace(b'[ring]', b'[unknown]'), bytes(16385)]:
        run('instruments-bad', data)
    # Every MIDI rejection also returns a clear message through the plugin, without a sound call.
    parser = SimpleNamespace(**runpy.run_path(str(Path(__file__).with_name('jukebox_midi_test.py'))))
    for name, (data, error) in parser.bad_cases().items():
        output = run('bad-midi', bad_midi=data)
        assert error in output, (name, output)

    # Keep parser diagnostics and failed preset loads visible through the real DLL.
    bounds = SimpleNamespace(**runpy.run_path(str(Path(__file__).with_name('jukebox_v2_removal_test.py'))))
    for name, (kind, data, error) in bounds.cases().items():
        if kind == 'mix':
            output = run('v2-mix-bad', mix=data)
            # Oversize files fail before the text parser.
            assert ('Preset exceeds 4096 bytes.' if name == 'mix-size' else error) in output, (name, output)
        else:
            run('instruments-bad', sounds=data)
    for command, error in [('bpm 0', 'BPM must be'), ('bpm 301', 'BPM must be'), ('bpm text', 'BPM must be'),
                           ('speed 0', 'Speed must be'), ('speed 401', 'Speed must be'), ('speed text', 'Speed must be'),
                           ('key -49', 'Key must be'), ('key 49', 'Key must be'), ('key text', 'Key must be'),
                           ('vol 1 -1', 'Volume must be'), ('vol 1 201', 'Volume must be'), ('vol 1 text', 'Volume must be'),
                           ('master -1', 'Master must be'), ('master 201', 'Master must be'), ('master text', 'Master must be')]:
        run('v2-control-bad', control=command + '\n' + error + '\n')
    subprocess.run([sys.executable, '-I', str(ROOT / 'tests/jukebox_v2_removal_test.py'), str(BUILD), '--check'], check=True)
    print('JUKEBOX_PLAYBACK_PASS: legacy and v2 controls, 47 sounds, mono, timing, presets, names and volume')
