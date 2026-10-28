"""Map the samples of the server profiler to function names.

Capture: set SV_SAMPLE=<file> before you start subvulpes_server.exe. A thread reads the
instruction address of the main thread every millisecond. SV_TIMERS=1 also prints the game timers
once a minute. Stop the server, then run:

    python -I tools/profile/map_samples.py samples.txt build/server/subvulpes_server.map [top]

An address outside the program (a wait, a system call) counts as "outside". The linker map must
come from the same build as the server that ran.
"""
import bisect
import collections
import re
import sys


def read_map(path):
    """Returns the preferred load address and the sorted list of (address, name) for the public symbols."""
    preferred = 0x400000
    symbols = []
    in_publics = False
    with open(path, encoding="latin-1") as handle:
        for line in handle:
            match = re.search(r"Preferred load address is ([0-9a-fA-F]+)", line)
            if match:
                preferred = int(match.group(1), 16)
            if "Publics by Value" in line:
                in_publics = True
                continue
            if not in_publics:
                continue
            match = re.match(r"\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})\s", line)
            if match and match.group(1) != "0000":
                symbols.append((int(match.group(4), 16), match.group(3)))
    symbols.sort()
    return preferred, symbols


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    top = int(argv[3]) if len(argv) > 3 else 25
    preferred, symbols = read_map(argv[2])
    addresses = [address for address, _ in symbols]
    counts = collections.Counter()
    base = None
    total = 0
    with open(argv[1], encoding="ascii") as handle:
        for line in handle:
            parts = line.split()
            if parts and parts[0] == "base":
                base = int(parts[1], 16)
                continue
            if base is None or not parts:
                continue
            total += 1
            at = int(parts[0], 16) - base + preferred
            if at < preferred or at > preferred + 0x4000000:
                counts["(outside the program)"] += 1
                continue
            index = bisect.bisect_right(addresses, at) - 1
            counts[symbols[index][1] if index >= 0 else "(before the first symbol)"] += 1
    print(f"{total} samples")
    for name, count in counts.most_common(top):
        print(f"{100.0 * count / total:6.2f}%  {count:7d}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
