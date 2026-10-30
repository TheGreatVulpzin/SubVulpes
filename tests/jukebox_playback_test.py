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


def fixtures(folder):
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


def run(mode, instruments=None, bad_midi=None):
    with tempfile.TemporaryDirectory(prefix='jukebox-host-', dir=BUILD / 'tests') as root:
        assert Path(root).resolve().parent == (BUILD / 'tests').resolve()
        folder = Path(root) / 'user/jukebox'
        folder.mkdir(parents=True)
        fixtures(folder)
        file = folder / 'instruments.ini'
        if instruments is not None:
            file.write_bytes(instruments)
        if bad_midi is not None:
            (folder / 'bad.mid').write_bytes(bad_midi)
        result = subprocess.run([str(DRIVER), str(DLL), mode], cwd=root, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0 and 'JUKEBOX_HOST_PASS' in result.stdout, (mode, result.returncode, result.stdout, result.stderr)
        assert file.read_bytes() == (DEFAULTS if instruments is None else instruments), 'Owner instrument file changed.'
        return result.stdout


for mode in ['demo', 'channels', 'budget', 'keypad', 'scale', 'stop']:
    run(mode)
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
print('JUKEBOX_PLAYBACK_PASS: demo, 15 channels, priority, fan-out, keypad, scale, administrator checks, files and bad MIDI')
