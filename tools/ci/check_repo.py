"""Reject private files, game assets, binaries, and verification tooling.

Run with python -I tools/ci/check_repo.py from the repository root.
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FORBIDDEN = ('original/', 'build/', 'out/', 'data/', 'user/', 'logs/', 'backup/',
             'third_party/', 'instrumentation/', 'hook/', 'ghidra-project/',
             'tools/lift/', 'tools/ghidra/', 'tools/recording/')
BINARY = re.compile(r'\.(exe|dll|so|dylib|bin|dmp|zip|7z|rar|srk|sbl|sbb|sbc|csm|pak|ogg|wav|gbf|gpr|wasm)$', re.I)
PRIVATE_NAME = re.compile(r'^(config\.txt|launcher\.ini|\.env(?:\..*)?|id_rsa|id_ed25519|credentials|settings\.json)$', re.I)
SECRET = re.compile(r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{40,}')
PASSWORD = re.compile(r'(?im)^[ \t]*(?:serverpassword|password|passwd|token|secret|api_key)[ \t]*[=:][ \t]*(?!["\']?\s*$)\S+')


def main():
    result = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'],
                            cwd=ROOT, capture_output=True, check=True)
    files = sorted(set(result.stdout.decode('utf-8').strip('\0').split('\0')) - {''})
    bad = []
    for name in files:
        lower = name.lower()
        reason = None
        if lower.startswith(FORBIDDEN) or '.rep/' in lower:
            reason = 'private, local, or excluded directory'
        elif lower.startswith('analysis/') and not lower.startswith('analysis/structs/'):
            reason = 'only type metadata belongs in analysis/structs/'
        elif PRIVATE_NAME.match(name.rsplit('/', 1)[-1]):
            reason = 'private settings or credentials'
        elif BINARY.search(name):
            reason = 'binary or game data'
        elif (ROOT / name).is_symlink():
            reason = 'symbolic link'
        elif (ROOT / name).is_file() and name != 'tools/ci/check_repo.py':
            data = (ROOT / name).read_bytes()
            if data.startswith((b'MZ', b'\x7fELF', b'PK\x03\x04')) or b'\0' in data:
                reason = 'binary content'
            else:
                text = data.decode('utf-8-sig', errors='replace')
                if SECRET.search(text) or PASSWORD.search(text):
                    reason = 'possible private credential (value omitted)'
        if reason:
            bad.append((name, reason))
    for name, reason in bad:
        print(f'{name}: {reason}')
    print(f'{len(files)} files checked, {len(bad)} problem(s)')
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
