#!/usr/bin/env python3
"""Deterministically export single-cell v2 kit assets from editable eighth-grid JSON."""
import argparse
import json
from pathlib import Path
import sys


def export_text(part):
    shape = ['Version', '2', '']
    for box in part['boxes']:
        values = [format(value / 8, '.3f').rstrip('0').rstrip('.') for value in box['bounds']]
        values += [str(int(box['collidable'])), str(int(box['selectable']))]
        values += [str(value) for value in box['face_roles']]
        shape += ['Box', ' '.join(values), '']
    fields = [('Name', part['name']), ('Id', part['block_id'])]
    fields += [('Tex' + role.title(), ' '.join(map(str, part['textures'][role]))) for role in ['top', 'side', 'bottom']]
    fields += [('Opaque', int(part['opaque'])), ('Collidable', int(part['collidable'])),
               ('OccludesFaces', int(part['occludes_faces'])), ('AoOccluder', int(part['ao_occluder'])),
               ('BlocksLight', int(part['blocks_light'])), ('MeshType', 1), ('Shape', part['name']),
               ('ShaderType', 0), ('Light', part['light']), ('Hardness', part['hardness']),
               ('MiningClass', part['mining_class']), ('RequiredToolTier', 0), ('WrongToolDrops', 1)]
    block = '\n\n'.join(str(key) + '\n' + str(value) for key, value in fields) + '\n'
    return '\n'.join(shape).rstrip() + '\n', block


def validate(source):
    if (source['version'], source['grid_per_metre'], source['shape_version'], source['max_boxes']) != (1, 8, 2, 8):
        raise ValueError('Unsupported authoring format')
    parts = source['parts']
    if len(parts) != 12 or [p['block_id'] for p in parts] != list(range(33, 45)):
        raise ValueError('The twelve stable architectural IDs must stay contiguous 33..44')
    if [p['material_id'] for p in parts] != list(range(49, 61)):
        raise ValueError('The twelve material IDs must stay contiguous 49..60')
    for part in parts:
        if not 1 <= len(part['boxes']) <= 8:
            raise ValueError('Box budget exceeded: ' + part['name'])
        for box in part['boxes']:
            bounds = box['bounds']
            if len(bounds) != 6 or any(type(v) is not int for v in bounds):
                raise ValueError('Bounds must use integer eighths')
            if any(not 0 <= bounds[a] < bounds[a + 3] <= 8 for a in range(3)):
                raise ValueError('Bounds escape a cell or are empty')
            if len(box['face_roles']) != 6 or any(role not in (0, 1, 2) for role in box['face_roles']):
                raise ValueError('Exactly six existing v2 material roles are required')


def preview(source, destination):
    # An authoring preview, not gameplay or rendering evidence. Every box is
    # projected at the same metre scale and painted by its side material role.
    palette = {144: '#d2c8ad', 146: '#ac5b3e', 147: '#795234', 148: '#52585c', 149: '#e9b657', 137: '#6c5337'}
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="780" viewBox="0 0 1000 780">',
           '<rect width="1000" height="780" fill="#f3f0e8"/>',
           '<text x="24" y="28" font-family="sans-serif" font-size="16">Architectural kit v1 · one metre cells · authoring preview</text>']
    for index, part in enumerate(source['parts']):
        col, row = index % 4, index // 4
        ox, oy = 120 + col * 245, 145 + row * 245
        def point(x, y, z):
            return f'{ox + (x - z) * 6},{oy + (x + z) * 3 - y * 8}'
        for box in sorted(part['boxes'], key=lambda box: box['bounds'][0] + box['bounds'][2]):
            x, y, z, xx, yy, zz = box['bounds']
            faces = [(4, [(x, yy, z), (xx, yy, z), (xx, yy, zz), (x, yy, zz)]),
                     (0, [(x, y, zz), (xx, y, zz), (xx, yy, zz), (x, yy, zz)]),
                     (3, [(xx, y, z), (xx, y, zz), (xx, yy, zz), (xx, yy, z)])]
            for face, corners in faces:
                role = ['top', 'side', 'bottom'][box['face_roles'][face]]
                tx, ty = part['textures'][role]
                colour = palette.get(tx + 16 * ty, '#888477')
                svg.append(f'<polygon points="{" ".join(point(*corner) for corner in corners)}" fill="{colour}" stroke="#39352d" stroke-width="0.8"/>')
        svg += [f'<text x="{col * 245 + 28}" y="{oy + 73}" font-family="sans-serif" font-size="15">{part["en_US"]}</text>',
                f'<text x="{col * 245 + 28}" y="{oy + 96}" font-family="sans-serif" font-size="12">Block {part["block_id"]} · {len(part["boxes"])} boxes · 4 yaw variants</text>']
    svg.append('</svg>')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text('\n'.join(svg) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--check', action='store_true', help='Reject any generated asset drift without writing')
    parser.add_argument('--preview', type=Path, help='Write an optional SVG authoring preview')
    args = parser.parse_args()
    source = json.loads((args.root / 'media/sources/architectural-kit-v1.json').read_text())
    validate(source)
    changed = []
    for part in source['parts']:
        shape, block = export_text(part)
        for kind, extension, text in [('shapes', 'shape', shape), ('blocks', 'block', block)]:
            path = args.root / 'media' / kind / (part['name'] + '.' + extension)
            if not path.exists() or path.read_text() != text:
                changed.append(str(path.relative_to(args.root)))
                if not args.check:
                    path.write_text(text)
    if args.check and changed:
        print('[ARCHITECTURAL-EXPORT] drift=' + ', '.join(changed), file=sys.stderr)
        return 1
    if args.preview:
        preview(source, args.preview)
    print(f'[ARCHITECTURAL-EXPORT] status=PASS parts=12 assets=24 mode={"check" if args.check else "export"} changed={len(changed)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
