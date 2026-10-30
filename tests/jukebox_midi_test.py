"""Check MIDI parsing, integer timing and every reader bound."""
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace

F = SimpleNamespace(**runpy.run_path(str(Path(__file__).with_name('jukebox_fixtures.py'))))
BINARY = Path(sys.argv[1]).resolve() if __name__ == '__main__' else None


def bad_cases():
    header = F.midi(F.END)[:14]
    return {
        'file-size': (F.padded(1048577), 'MIDI file exceeds 1 MB.'),
        'header-size': (header[:13], 'MIDI header is truncated.'),
        'header-tag': (b'FAIL' + F.midi(F.END)[4:], 'MIDI needs an MThd header.'),
        'header-length': (header[:4] + struct.pack('>I', 5) + header[8:] + F.midi(F.END)[14:], 'MIDI header length is invalid.'),
        'track-count': (F.midi(*([F.END] * 65)), 'MIDI needs 1 to 64 tracks.'),
        'track-zero': (header[:10] + bytes(2) + header[12:], 'MIDI needs 1 to 64 tracks.'),
        'format': (F.midi(F.END, format=2), 'MIDI supports format 0 with one track or format 1.'),
        'format-zero-tracks': (F.midi(F.END, F.END, format=0), 'MIDI supports format 0 with one track or format 1.'),
        'division': (F.midi(F.END, division=0), 'MIDI needs a positive PPQ division. SMPTE time is unsupported.'),
        'smpte': (F.midi(F.END, division=0xE728), 'MIDI needs a positive PPQ division. SMPTE time is unsupported.'),
        'track-header': (header + b'MTrk' + bytes(3), 'MIDI track header is truncated.'),
        'track-tag': (header + b'FAIL' + F.midi(F.END)[18:], 'MIDI needs an MTrk chunk for each track.'),
        'track-length': (header + b'MTrk' + struct.pack('>I', 99) + F.END, 'MIDI track length exceeds the file.'),
        'byte': (F.midi(F.event(0, 144, 60)), 'MIDI ends inside an event.'),
        'delta-length': (F.midi(bytes([129, 128, 128, 128, 0, 255, 47, 0])), 'MIDI delta or message length exceeds four bytes.'),
        'meta-vlq': (F.midi(F.event(0, 255, 1, 128, 128, 128, 128, 0) + F.END), 'MIDI delta or message length exceeds four bytes.'),
        'system-vlq': (F.midi(F.event(0, 240, 128, 128, 128, 128, 0) + F.END), 'MIDI delta or message length exceeds four bytes.'),
        'running-status': (F.midi(F.event(0, 60, 127) + F.END), 'MIDI running status has no previous channel status.'),
        'running-after-meta': (F.midi(F.event(0, 144, 60, 1) + F.event(0, 255, 1, 0) + F.event(0, 62, 1) + F.END), 'MIDI running status has no previous channel status.'),
        'running-after-system': (F.midi(F.event(0, 144, 60, 1) + F.event(0, 240, 0) + F.event(0, 62, 1) + F.END), 'MIDI running status has no previous channel status.'),
        'channel-data': (F.midi(F.event(0, 144, 128, 1) + F.END), 'MIDI channel data exceeds 127.'),
        'meta-length': (F.midi(F.event(0, 255, 1, 5, 1)), 'MIDI meta message exceeds its track.'),
        'system-length': (F.midi(F.event(0, 240, 5, 1)), 'MIDI system message exceeds its track.'),
        'meta-type': (F.midi(F.event(0, 255, 128, 0) + F.END), 'MIDI meta type exceeds 127.'),
        'tempo-length': (F.midi(F.event(0, 255, 81, 2, 1, 1) + F.END), 'MIDI tempo needs three bytes.'),
        'tempo-zero': (F.midi(F.event(0, 255, 81, 3, 0, 0, 0) + F.END), 'MIDI tempo must be positive.'),
        'end-track': (F.midi(F.event(0, 255, 47, 1, 0)), 'MIDI end-of-track must be empty and last.'),
        'missing-end': (F.midi(F.event(0, 144, 60, 1)), 'MIDI track has no end-of-track event.'),
        'system-status': (F.midi(F.event(0, 248) + F.END), 'MIDI contains an unsupported system status.'),
        'trailing-data': (F.midi(F.END) + b'x', 'MIDI has data after the declared tracks.'),
        'event-count': (F.midi(F.event(0, 128, 60, 0) * 200000 + F.END), 'MIDI has more than 200000 events.'),
    }


def main():
    selected = sys.argv[2] if len(sys.argv) > 2 else None
    with tempfile.TemporaryDirectory(prefix='jukebox-midi-', dir=BINARY.parent) as root:
        file = Path(root) / 'input.mid'
        def run(data):
            file.write_bytes(data)
            return subprocess.run([str(BINARY), str(file)], capture_output=True, text=True, timeout=15)
        checked = 0
        for name, (data, error) in bad_cases().items():
            if selected and name != selected:
                continue
            result = run(data)
            assert result.returncode == 2 and result.stdout.strip() == 'ERROR ' + error, (name, result.returncode, result.stdout, result.stderr)
            checked += 1
        if selected:
            assert checked == 1, selected
            return
        tempo = F.event(0, 255, 81, 3, 7, 161, 32) + F.event(480, 255, 81, 3, 3, 208, 144) + F.event(480, 255, 47, 0)
        notes = F.event(0, 192, 73) + F.event(0, 144, 60, 127) + F.event(480, 62, 64) + F.event(480, 64, 32) + F.END
        result = run(F.midi(tempo, notes))
        assert result.returncode == 0, result.stdout
        assert [line for line in result.stdout.splitlines() if line.startswith('NOTE')] == [
            'NOTE 0 0 60 127 73', 'NOTE 30 0 62 64 73', 'NOTE 45 0 64 32 73'], result.stdout
        result = run(F.midi(F.event(0, 153, 36, 100) + F.event(5, 36, 0) + F.event(0, 137, 36, 0) + F.END))
        assert result.returncode == 0 and 'NOTE 0 9 36 100 0' in result.stdout and 'SONG 1' in result.stdout, result.stdout
        # Fractional time carries across events. It never rounds each delta on its own.
        track = F.event(0, 255, 81, 3, 7, 161, 33) + F.event(0, 144, 60, 1)
        track += b''.join(F.event(1, 144, 60, 1) for _ in range(100)) + F.END
        result = run(F.midi(track, division=7))
        ticks = [int(line.split()[1]) for line in result.stdout.splitlines() if line.startswith('NOTE')]
        assert ticks == [i * 500001 * 60 // 7000000 for i in range(101)], result.stdout
        # The exact limits remain valid, with no notes held in memory for ignored events.
        for data in [F.padded(1048576), F.midi(*([F.END] * 64)), F.midi(F.event(0, 128, 60, 0) * 199999 + F.END)]:
            result = run(data)
            assert result.returncode == 0, result.stdout
        # System messages and unknown meta messages skip their bounded payloads.
        result = run(F.midi(F.event(0, 240, 2, 1, 247) + F.event(0, 247, 0) + F.event(0, 255, 1, 1, 65) + F.END))
        assert result.returncode == 0, result.stdout
        print(f'JUKEBOX_MIDI_PASS {checked} bad cases, timing, running status, drums and exact limits')


if __name__ == '__main__':
    main()
