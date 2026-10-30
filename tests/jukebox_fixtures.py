"""MIDI test data exists only as bytes made by these helpers."""
import struct


def vlq(value):
    result = [value & 127]
    value >>= 7
    while value:
        result.insert(0, 128 | (value & 127))
        value >>= 7
    return bytes(result)


def event(delta, *data):
    return vlq(delta) + bytes(data)


END = event(0, 255, 47, 0)


def midi(*tracks, format=None, division=480):
    if format is None:
        format = int(len(tracks) != 1)
    return b'MThd' + struct.pack('>IHHH', 6, format, len(tracks), division) + b''.join(
        b'MTrk' + struct.pack('>I', len(track)) + track for track in tracks)


def padded(size):
    for payload in range(size - 40, size):
        result = midi(event(0, 255, 1) + vlq(payload) + bytes(payload) + END)
        if len(result) == size:
            return result
    raise AssertionError('Cannot make the requested file size.')
