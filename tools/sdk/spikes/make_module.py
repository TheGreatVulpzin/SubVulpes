"""Build the small, dependency-free WASM module for runtime spikes."""
from pathlib import Path
import struct


def integer(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def section(kind, data):
    return bytes([kind]) + integer(len(data)) + data


def name(text):
    data = text.encode("ascii")
    return integer(len(data)) + data


types = b"\x04\x60\x00\x01\x7f\x60\x01\x7f\x01\x7f\x60\x00\x00\x60\x01\x7c\x01\x7c"
functions = b"\x04\x00\x01\x02\x03"
memory = b"\x01\x01\x01\x20"
exports = (b"\x05" + name("call") + b"\x00\x00" + name("grow") + b"\x00\x01" +
           name("spin") + b"\x00\x02" + name("fp") + b"\x00\x03" + name("memory") + b"\x02\x00")
bodies = (b"\x00\x41\x2a\x0b", b"\x00\x20\x00\x40\x00\x0b", b"\x00\x03\x40\x0c\x00\x0b\x0b",
          b"\x00\x20\x00\x44" + struct.pack("<d", 0.5) + b"\xa2\x44" + struct.pack("<d", 1.0) + b"\xa0\x0b")
code = integer(len(bodies)) + b"".join(integer(len(body)) + body for body in bodies)
module = b"\x00asm\x01\x00\x00\x00" + b"".join(
    section(kind, data) for kind, data in ((1, types), (3, functions), (5, memory), (7, exports), (10, code)))
output = Path(__file__).resolve().parents[3] / "build/runtimes/spike.wasm"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(module)
print(output.name, len(module), "bytes")
