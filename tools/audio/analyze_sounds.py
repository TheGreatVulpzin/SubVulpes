"""Finds the pitch of each WAV sound of the game client.

A plugin plays a sound with a pitch multiplier: the sound plays at (base frequency x multiplier). To play a note, the
plugin needs the base frequency of the sound. This tool measures it. It reads the WAV files, so it needs the client sound
folder. The audio belongs to Cryptic Sea and stays outside this repository. The tool writes numbers only.

Usage: python tools/audio/analyze_sounds.py CLIENT_SOUND_FOLDER [OUTPUT.csv]
The default output is build/audio/sounds.csv (the build folder is ignored by Git).
"""
import csv
import math
import sys
import wave
from pathlib import Path

import numpy as np

FRAME = 4096
FREQ_MIN, FREQ_MAX = 50.0, 2000.0
ENERGY_SHARE = 0.25


def read_wav(path):
    with wave.open(str(path), 'rb') as handle:
        rate, width, channels = handle.getframerate(), handle.getsampwidth(), handle.getnchannels()
        raw = handle.readframes(handle.getnframes())
    if width != 2:
        raise ValueError(f'{path.name}: only 16-bit PCM is supported')
    data = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    return rate, data


def frame_pitch(frame, rate):
    """The pitch of one frame: (frequency, confidence, flatness). Confidence is the strength of the best periodic peak."""
    window = np.hanning(len(frame))
    spectrum = np.fft.rfft(frame * window)
    power = np.abs(spectrum) ** 2
    bins = np.fft.rfftfreq(len(frame), 1.0 / rate)
    band = (bins >= 20.0) & (bins <= min(5000.0, rate / 2.0))
    band_power = power[band] + 1e-18
    flatness = float(np.exp(np.mean(np.log(band_power))) / np.mean(band_power))
    # Autocorrelation from the power spectrum. The first strong peak after the first dip is the period.
    autocorr = np.fft.irfft(power)[: len(frame) // 2]
    if autocorr[0] <= 0:
        return 0.0, 0.0, flatness
    autocorr = autocorr / autocorr[0]
    low, high = int(rate / FREQ_MAX), min(int(rate / FREQ_MIN), len(autocorr) - 1)
    if high <= low + 2:
        return 0.0, 0.0, flatness
    segment = autocorr[low:high]
    peak = int(np.argmax(segment)) + low
    confidence = float(autocorr[peak])
    # Parabolic interpolation around the peak, for a period between two samples.
    if 0 < peak < len(autocorr) - 1:
        a, b, c = autocorr[peak - 1], autocorr[peak], autocorr[peak + 1]
        denominator = a - 2 * b + c
        offset = 0.5 * (a - c) / denominator if denominator != 0 else 0.0
    else:
        offset = 0.0
    return rate / (peak + offset), confidence, flatness


def analyze(path):
    rate, data = read_wav(path)
    duration = len(data) / rate
    size = min(FRAME, len(data))
    if size < 256:
        return None
    hop = max(size // 2, 1)
    frames = [data[i:i + size] for i in range(0, len(data) - size + 1, hop)] or [data[:size]]
    energy = np.array([float(np.sum(f * f)) for f in frames])
    loud = [f for f, e in zip(frames, energy) if e >= ENERGY_SHARE * energy.max() and e > 0]
    if not loud:
        return None
    pitches = [frame_pitch(f, rate) for f in loud]
    good = [p for p in pitches if p[0] > 0]
    if not good:
        return {'rate': rate, 'seconds': round(duration, 2), 'hz': 0.0, 'confidence': 0.0, 'cents_spread': 0.0,
                'flatness': round(float(np.mean([p[2] for p in pitches])), 3), 'frames': len(loud)}
    hz = float(np.median([p[0] for p in good]))
    cents = [1200.0 * math.log2(p[0] / hz) for p in good]
    return {'rate': rate, 'seconds': round(duration, 2), 'hz': round(hz, 2),
            'confidence': round(float(np.mean([p[1] for p in good])), 3),
            'cents_spread': round(float(np.std(cents)), 1),
            'flatness': round(float(np.mean([p[2] for p in pitches])), 3), 'frames': len(loud)}


def note_name(hz):
    if hz <= 0:
        return ''
    number = 69 + 12 * math.log2(hz / 440.0)
    nearest = round(number)
    names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    return f'{names[nearest % 12]}{nearest // 12 - 1} {100 * (number - nearest):+.0f}c'


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    folder = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('build/audio/sounds.csv')
    out.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for path in sorted(folder.glob('*.wav')):
        try:
            result = analyze(path)
        except (ValueError, wave.Error) as error:
            print(f'skip {path.name}: {error}')
            continue
        if result is None:
            print(f'skip {path.name}: too short or silent')
            continue
        result['file'] = path.name
        result['note'] = note_name(result['hz'])
        rows.append(result)
    columns = ['file', 'hz', 'note', 'confidence', 'cents_spread', 'flatness', 'seconds', 'rate', 'frames']
    with open(out, 'w', newline='', encoding='ascii') as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row[key] for key in columns})
    # The best instruments first: periodic (high confidence), steady (low spread) and tonal (low flatness).
    rows.sort(key=lambda r: (-r['confidence'], r['cents_spread']))
    print(f'{"file":22} {"hz":>8} {"note":>10} {"conf":>6} {"spread":>7} {"flat":>6} {"sec":>6}')
    for row in rows:
        print(f'{row["file"]:22} {row["hz"]:8.1f} {row["note"]:>10} {row["confidence"]:6.2f} {row["cents_spread"]:7.1f} '
              f'{row["flatness"]:6.3f} {row["seconds"]:6.2f}')
    print(f'Wrote {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
