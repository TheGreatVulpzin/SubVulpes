"""Check that a plugin API change only adds to the ABI snapshot.

Usage: python -I tools/ci/check_abi_additive.py BASE_SNAPSHOT HEAD_SNAPSHOT

The snapshot is tests/abi_snapshot.txt. A change may add records, tables and methods.
It may not remove or change what exists. See CONTRIBUTING.md.
"""
import re
import sys


def parse(text):
    """Map the key of each snapshot line to its fields."""
    lines = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        name, _, rest = line.partition(" ")
        key = name + " " + re.split(r"[= ]", rest, maxsplit=1)[0]
        fields = dict(re.findall(r"(\w+)=(\S+)", rest))
        lines[key] = (line, name, fields)
    return lines


def offset(field):
    """The byte offset in a field such as 'last=possess:60'."""
    return int(field.rsplit(":", 1)[1])


def check(base_text, head_text):
    """Return the list of problems. An empty list means the change only adds."""
    base, head = parse(base_text), parse(head_text)
    tables = {name for _, name, fields in base.values() if "functions" in fields}
    problems = []
    for key, (line, name, fields) in base.items():
        if key not in head:
            problems.append("Removed: " + line)
            continue
        new_line, _, new_fields = head[key]
        if new_line == line:
            continue
        if "functions" in fields:
            if int(new_fields["functions"]) < int(fields["functions"]):
                problems.append("Fewer methods: " + line + " -> " + new_line)
            elif new_fields["prefix"] != fields["prefix"] or new_fields["first"] != fields["first"]:
                problems.append("Moved the first method: " + line + " -> " + new_line)
            elif offset(new_fields["last"]) < offset(fields["last"]):
                problems.append("Moved the last method back: " + line + " -> " + new_line)
        elif "size" in fields and name in tables:
            if int(new_fields["size"]) < int(fields["size"]):
                problems.append("Smaller table: " + line + " -> " + new_line)
        else:
            problems.append("Changed: " + line + " -> " + new_line)
    return problems


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8") as base, open(argv[2], encoding="utf-8") as head:
        problems = check(base.read(), head.read())
    for problem in problems:
        print("ABI: " + problem)
    if problems:
        print("The plugin API can only grow. Add to the end of a table or add a new record.")
        return 1
    print("ABI snapshot only adds.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
