"""Matches the sounds that the test room sent to the sounds that the client played.

Inputs, from one session on one computer:
  server.log        the log of the server. The sound tests write TOURSOUND lines: the local time, the sound index and the pitch.
  openal_probe.log  the log of the pass-through OpenAL32.dll in the client folder. BUF lines list the buffers, PLAY lines the sounds.
  CLIENT_SOUND      the sound folder of the client, to name a buffer from its WAV file.
The tool prints one line for each sound index: the buffer order, the file, and the pitch that the client set.
It also compares the pitch the plugin sent with the pitch the client set, which shows a limit in the client.
The audio belongs to Cryptic Sea. The tool reads it and writes numbers only.

Usage: python tools/audio/match_probe.py SERVER_LOG PROBE_LOG CLIENT_SOUND [OUTPUT.csv]
"""
import csv
import re
import sys
import wave
import zlib
from datetime import timedelta
from pathlib import Path

WINDOW = timedelta(milliseconds=700)

TOUR = re.compile(r'TOURSOUND (\d\d):(\d\d):(\d\d)\.(\d\d\d) index=(\d+) pitch=([0-9.]+)')
BUF = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d\d\d) BUF order=(\d+) id=(\d+) format=0x([0-9a-f]+) bytes=(\d+) rate=(\d+) crc=([0-9a-f]{8})')
PLAY = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d\d\d) PLAY source=(\d+) buffer=(\d+) order=(-?\d+) pitch=([0-9.]+) gain=([0-9.]+)')
PITCH = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d\d\d) PITCH source=(\d+) value=([0-9.]+) error=0x([0-9a-f]+)')


def when(match):
    hour, minute, second, milli = (int(match.group(i)) for i in range(1, 5))
    return timedelta(hours=hour, minutes=minute, seconds=second, milliseconds=milli)


def wav_crcs(folder):
    """The CRC-32 of the sample bytes of each WAV file, as the client gives them to OpenAL."""
    names = {}
    for path in sorted(Path(folder).glob('*.wav')):
        try:
            with wave.open(str(path), 'rb') as handle:
                data = handle.readframes(handle.getnframes())
        except (wave.Error, EOFError):
            continue
        names[zlib.crc32(data) & 0xffffffff] = path.name
    return names


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    server_log, probe_log, sound_folder = (Path(a) for a in sys.argv[1:4])
    out = Path(sys.argv[4]) if len(sys.argv) > 4 else Path('build/audio/match.csv')
    names = wav_crcs(sound_folder)
    buffers, plays, pitch_errors, tour = {}, [], [], []
    for line in probe_log.read_text(encoding='ascii', errors='replace').splitlines():
        match = BUF.match(line)
        if match:
            order = int(match.group(5))
            crc = int(match.group(10), 16)
            buffers[order] = {'bytes': int(match.group(8)), 'rate': int(match.group(9)), 'file': names.get(crc, '')}
        match = PLAY.match(line)
        if match:
            plays.append((when(match), int(match.group(7)), float(match.group(8)), float(match.group(9))))
        match = PITCH.match(line)
        if match:
            pitch_errors.append((when(match), float(match.group(6)), match.group(7)))
    for line in server_log.read_text(encoding='ascii', errors='replace').splitlines():
        match = TOUR.search(line)
        if match:
            tour.append((when(match), int(match.group(5)), float(match.group(6))))
    if not tour:
        print('The server log has no TOURSOUND line. Run the test sound_tour or pitch_octaves first.')
        return 1
    rows = []
    used = set()
    for sent_at, index, pitch in tour:
        found = None
        for number, (played_at, order, seen_pitch, gain) in enumerate(plays):
            if number not in used and sent_at - timedelta(milliseconds=50) <= played_at <= sent_at + WINDOW and order >= 0:
                found = (number, played_at, order, seen_pitch, gain)
                break
        if not found:
            rows.append({'index': index, 'sent_pitch': pitch, 'order': '', 'file': '', 'client_pitch': '', 'delay_ms': '', 'bytes': '', 'rate': ''})
            continue
        used.add(found[0])
        info = buffers.get(found[2], {})
        rows.append({'index': index, 'sent_pitch': pitch, 'order': found[2], 'file': info.get('file', '') or 'buffer_%03d.wav (dump)' % found[2],
                     'client_pitch': found[3], 'delay_ms': int((found[1] - sent_at).total_seconds() * 1000),
                     'bytes': info.get('bytes', ''), 'rate': info.get('rate', '')})
    out.parent.mkdir(parents=True, exist_ok=True)
    columns = ['index', 'order', 'file', 'sent_pitch', 'client_pitch', 'delay_ms', 'bytes', 'rate']
    with open(out, 'w', newline='', encoding='ascii') as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)
    print(f'{"index":>5} {"order":>5}  {"file":28} {"sent":>8} {"client":>8} {"ms":>5} {"bytes":>8} {"rate":>6}')
    for row in rows:
        print(f'{row["index"]:5} {str(row["order"]):>5}  {row["file"]:28} {row["sent_pitch"]:8.3f} {str(row["client_pitch"]):>8} '
              f'{str(row["delay_ms"]):>5} {str(row["bytes"]):>8} {str(row["rate"]):>6}')
    missed = [r['index'] for r in rows if r['order'] == '']
    if missed:
        print('No client sound found for the indexes:', ', '.join(str(i) for i in missed))
    changed = [r for r in rows if r['client_pitch'] != '' and abs(float(r['client_pitch']) - r['sent_pitch']) > 0.001]
    if changed:
        print('The client set another pitch than the plugin sent for', len(changed), 'sounds. Look at the columns sent and client.')
    for played_at, value, error in pitch_errors[:20]:
        print(f'PITCH outside 0.5 to 2.0 at {played_at}: value {value}, library error 0x{error}')
    print(f'Wrote {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
