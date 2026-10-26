"""Check the built server's sources, include paths and linked symbols."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    build = ROOT / 'build/server'
    reply = build / '.cmake/api/v1/reply'
    indexes = list(reply.glob('index-*.json'))
    if not indexes:
        raise SystemExit('Run tools/build_hook.bat before the native dependency check.')
    index = json.loads(max(indexes, key=lambda path: path.stat().st_mtime).read_text())
    model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
    ref = next(target for target in model['configurations'][0]['targets'] if target['name'] == 'subvulpes-server')
    target = json.loads((reply / ref['jsonFile']).read_text())
    sources = [source['path'] for source in target['sources']]
    if 'src/server/storage.c' not in sources:
        raise SystemExit('The server must own its storage.')
    forbidden = re.compile(r'instrumentation|lifted|image_024d|fn_table')
    for path in sources:
        if forbidden.search(path):
            raise SystemExit(f'Transition source in server target: {path}')
    for group in target['compileGroups']:
        for command in group.get('compileCommandFragments', []):
            if '/Gh' in command['fragment']:
                raise SystemExit('Entry recording is not allowed in the normal server.')
        if any(entry['define'].startswith('SR_TEST_MEMORY') for entry in group.get('defines', [])):
            raise SystemExit('The server cannot select the verification memory adapter.')
        for entry in group.get('includes', []):
            if forbidden.search(entry['path']):
                raise SystemExit('Transition include path in server target.')
    files = [ROOT / path for path in sources if path.endswith('.c')]
    for folder in ('common', 'game', 'net', 'server'):
        files.extend((ROOT / 'src' / folder).glob('*.h'))
    files.extend(ROOT / 'src/include' / name for name in
                 ('sr_runtime.h', 'sr_storage.h', 'sr_storage_types.h', 'sr_limits.h', 'sr_types.h', 'sr_math.h'))
    access = re.compile(r'\b(?:CODE|sr_fn)\s*\(|\b(?:sr_base|SR_AT|SR_VA|SR_RVA_\w*)\b')
    for path in files:
        code = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
        if access.search(code):
            raise SystemExit(f'Address-based game access: {path.relative_to(ROOT)}')
    symbols = (build / 'subvulpes_server.map').read_text(encoding='utf-8', errors='replace')
    if re.search(r'\b_(?:sr_base|sr_fn(?:_init)?|sr_image_\w+|lf_table(?:_count)?|_penter)\b', symbols):
        raise SystemExit('Transition symbol in the linked server.')
    print(f'Native server dependency check passed: {len(sources)} sources, no image or address dispatch.')


if __name__ == '__main__':
    main()
