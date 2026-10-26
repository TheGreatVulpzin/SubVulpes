"""Generate typed C storage from retained metadata and checked public defaults.

Run with python -I. No game binary or verification adapter is needed.
"""
import argparse
import csv
import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TYPES = runpy.run_path(str(ROOT / 'tools/re/gen_types.py'))


def schema():
    addresses = {}
    for name in ('globals', 'manual_globals'):
        for r in csv.DictReader((ROOT / f'analysis/structs/{name}.csv').open(encoding='utf-8')):
            addresses[r['name']] = int(r['rva'], 16)
    rows = []
    overrides = {
        'area_sector': ('SrSectorStore', 1, 0x40 + 8192 * 0x6068),
        'track': ('TrackPiece', 4096, 0x578),
        'cityobject': ('CityObjectRec', 1024, 0x54),
        'cityobjecttype': ('CityObjectTypeRec', 256, 0x2418),
    }
    for typ, (name, size) in TYPES['STRUCTS'].items():
        if name in ('objectpacket', 'itemtype'):
            continue
        typ, count, size = overrides.get(name, (typ, TYPES['COUNTS'].get(name, 1), size))
        rows.append((addresses[name], name, typ, count, size))
    extra = [
        (0xf437b60, 'accounts', 'SrAccountStore', 1, 8 + 65536 * 0x4c),
        (0x9c1d8c0, 'landscape', 'SrLandscape', 1, 12 + 2049 * 2049 * 4),
        (0xb7e90a0, 'surface', 'SrSurface', 1, 0x140),
        (0x1d65a40, 'surface2', 'SrSurface', 1, 0x140),
        (0xb7e91e0, 'interior', 'SrInteriorStore', 1, 4 + 4096 * 0x2a30),
        (0x2043a0, 'floorplan', 'FloorplanCell', 262144, 0x48),
        (0x9bdd8a0, 'newspaper', 'Newspaper', 1, 0x40008),
        (0x16d50020, 'materials', 'SrMaterialStore', 1, 4 + 1024 * 0x58),
        (0x1b497a0, 'clientaccounts', 'SrClientAccountStore', 1, 4 + 65536 * 16),
        (0xf437320, 'bullettypes', 'BulletType', 4, 0x18),
        (0xeca4bc0, 'voice', 'VoiceChannel', 8, 0x29228),
        (0xf437940, 'camera', 'float', 136, 4),
        (0x1c497c0, 'pointtemp', 'vec3', 512, 12),
        (0x17529aa0, 'modellist', 'int', 0x7a4 // 4, 4),
        (0x13e22e80, 'texturelist', 'int', 0x1144 // 4, 4),
        (0xb0661c0, 'bezier_curve_dv', 'float', 12, 4),
        (0x761ea20, 'bezier_curve', 'float', 12, 4),
        (0x13c22d40, 'bezier_curve_uv', 'float', 8, 4),
        (0x12a80c44, 'level_marks', 'uint8_t', 4096, 1),
        (0x61780, 'manager_msg', 'char', 64, 1),
        (0x617c0, 'world_msg', 'char', 128, 1),
        (0x203ee0, 'world_digits', 'char', 64, 1),
        (0x203f20, 'newspaper_money', 'char', 128, 1),
        (0x203fa0, 'farbs_line', 'char', 128, 1),
        (0x61d48, 'farbs_digits', 'char', 128, 1),
        (0x60d40, 'newspaper_line', 'char', 128, 1),
        (0x61848, 'message', 'char', 64, 1),
        (0x5efec, 'filtered_word', 'char', 7, 1),
        (0x203de0, 'guard_used', 'int', 64, 4),
        (0x103485a0, 'packet_workspace', 'SrPacketWorkspace', 1, 0x28664),
        (0x91022a4, 'file_read', 'SrFileTransfer', 1, 4),
        (0x13bf2d00, 'file_write', 'SrFileTransfer', 1, 4),
        (0xac218e0, 'wsadata', 'uint8_t', 0x190, 1),
    ]
    scalars = {
        0x1ca5a30: 'numofdynamicbonds', 0x13c22d24: 'numofstaticbonds',
        0x90dc520: 'numoftracks', 0xf437318: 'numofevents',
        0x17179a28: 'numofbonds', 0x17529a40: 'numofbullets',
        0x1c4afc0: 'numofconnections',
        0xff47be0: 'view',
        0x916d2c0: 'random_counter', 0x204380: 'dedicated',
        0x10348524: 'slogfp', 0x61840: 'tzset_done',
        0x761e600: 'numofsprites', 0x12a81c44: 'numofsprite_models',
        0xb7e8ff4: 'tiremarkstart', 0xb7a8c80: 'tiremarkend',
        0xb7a8c84: 'startup_counter',
        0x761eb00: 'sprite_points', 0x766eb04: 'sprite_lines',
        0x76ceb08: 'sprite_quads', 0x7eceb0c: 'sprite_triangles',
    }
    for addr, name in scalars.items():
        typ = 'void *' if name == 'slogfp' else 'uint32_t' if name in ('numofevents', 'random_counter') else 'int'
        extra.append((addr, name, typ, 1, 4))
    existing = {r[1] for r in extra}
    for r in csv.DictReader((ROOT / 'analysis/structs/data_globals.csv').open(encoding='utf-8')):
        if r['name'] in existing:
            continue
        typ, count = r['type'], int(r['count'] or 1)
        if typ == 'FloorplanCell' or typ == 'Newspaper':
            continue
        size = {'char': 1, 'uint8_t': 1, 'float': 4, 'int': 4, 'uint32_t': 4, 'char*': 4, 'void*': 4}[typ]
        typ = {'char*': 'const char *', 'void*': 'void *'}.get(typ, typ)
        if r['name'] in ('xaxis', 'yaxis', 'zaxis'):
            typ, count, size = 'vec3', 1, 12
        if r['name'] in ('cubevertex', 'cubenormal', 'cubetangent', 'cubebinormal'):
            typ, count, size = 'vec3', count // 3, 12
        extra.append((int(r['rva'], 16), r['name'], typ, count, size))
    rows.extend(extra)
    rows.sort()
    for a, b in zip(rows, rows[1:]):
        if a[0] + a[3] * a[4] > b[0]:
            raise ValueError(f'Overlapping storage: {a} / {b}')
    return rows


def declaration(row):
    _, name, typ, count, _ = row
    return f'{typ} sr_data_{name}' + (f'[{count}]' if count > 1 else '')


def generate(check=False):
    rows = schema()
    defaults = json.loads((ROOT / 'analysis/structs/storage_defaults.json').read_text(encoding='utf-8'))['initializers']
    if set(defaults) != {row[1] for row in rows}:
        raise SystemExit('Storage schema and checked defaults differ.')
    header = ['/* Generated by tools/re/gen_storage.py. Do not edit. */',
              '#ifndef SR_STORAGE_H', '#define SR_STORAGE_H', '#include "sr_storage_types.h"', '']
    source = ['/* Generated by tools/re/gen_storage.py. Do not edit. */', '#include "sr_storage.h"', '']
    for row in rows:
        addr, name, typ, count, size = row
        header.append('extern ' + declaration(row) + ';')
        source.append(declaration(row) + defaults[name] + ';')
        header.append(f'SR_STATIC_ASSERT(sizeof(sr_data_{name}) == {count * size}, "storage {name}");')
    aliases = [f'#define sr_data_{name} sr_data_packet_workspace.{name}'
               for name in ('kickedip', 'objectpacket', 'itemtype', 'numofkicked')]
    header.extend(['', *aliases, '', '#endif'])
    outputs = {
        'src/include/sr_storage.h': header,
        'src/server/storage.c': source,
    }
    for path, lines in outputs.items():
        p = ROOT / path
        content = '\n'.join(lines) + '\n'
        if check:
            if not p.exists() or p.read_text(encoding='utf-8') != content:
                raise SystemExit(f'Stale generated storage: {path}')
        elif not p.exists() or p.read_text(encoding='utf-8') != content:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content, encoding='utf-8', newline='\n')
    print(f'{"Checked" if check else "Generated"} {len(rows)} independent storage objects, '
          f'{sum(r[3] * r[4] for r in rows)} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true', help='Fail if a generated file is stale')
    generate(parser.parse_args().check)
