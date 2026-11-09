"""Check the ABI growth rule with small snapshots."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "ci"))
import check_abi_additive as abi  # noqa: E402

BASE = """SvRecord size=20
SvTable size=16
SvTable functions=2 prefix=0 first=one:8 last=two:12
SvEvent kind=5 bit=7
"""


def problems(head):
    return abi.check(BASE, head)


def expect(head, text):
    found = problems(head)
    assert any(text in problem for problem in found), (text, found)


assert problems(BASE) == []
# New records, new lines and a new method at the end are fine.
assert problems(BASE + "SvNew size=8\n") == []
assert problems(BASE.replace("SvTable size=16", "SvTable size=20")
                .replace("functions=2 prefix=0 first=one:8 last=two:12",
                         "functions=3 prefix=0 first=one:8 last=three:16")) == []
# A removed line, a changed record, a lost method or a moved first method is not.
expect(BASE.replace("SvRecord size=20\n", ""), "Removed: SvRecord size=20")
expect(BASE.replace("SvRecord size=20", "SvRecord size=24"), "Changed: SvRecord size=20")
expect(BASE.replace("SvEvent kind=5 bit=7", "SvEvent kind=6 bit=7"), "Changed: SvEvent")
expect(BASE.replace("functions=2 prefix=0 first=one:8 last=two:12", "functions=1 prefix=0 first=one:8 last=one:8"),
       "Fewer methods")
expect(BASE.replace("functions=2 prefix=0 first=one:8 last=two:12", "functions=3 prefix=0 first=zero:8 last=three:16"),
       "Moved the first method")
expect(BASE.replace("SvTable size=16", "SvTable size=12"), "Smaller table")
print("ABI growth rule passed.")
