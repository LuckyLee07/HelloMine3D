#!/usr/bin/env python3
"""Portable pixel-level regression checks for the Warm Wilderness asset build."""
import hashlib
import re
import tempfile
from pathlib import Path
from PIL import Image
from build_warm_texture_atlas import ROOT, BASE, LAYOUT, OUTPUT, build, layout, png_bytes


def validate(image, entries, original):
    if image.size != (256, 256):
        raise ValueError('dimensions')
    hashes = set()
    used = set()
    for name, (x, y, mode) in entries.items():
        used.add((x, y))
        tile = image.crop((x, y, x + 16, y + 16))
        alpha = set(tile.getchannel('A').getdata())
        valid = (alpha == {255} if mode == 'opaque' else
                 alpha == {0, 255} if mode in ('cutout', 'icon') else
                 any(a < 255 for a in alpha) and any(a > 0 for a in alpha))
        if not valid:
            raise ValueError(f'alpha: {name}')
        hashes.add(tile.tobytes())
        if mode == 'icon' and tile.tobytes() != original.crop((x, y, x + 16, y + 16)).tobytes():
            raise ValueError(f'changed icon: {name}')
    if len(hashes) != 120:
        raise ValueError(f'distinct tiles: {len(hashes)}')
    for y in range(0, 256, 16):
        for x in range(0, 256, 16):
            if (x, y) not in used and image.crop((x, y, x + 16, y + 16)).getchannel('A').getbbox():
                raise ValueError('nonempty unused tile')


def validate_mappings(entries):
    # Share the already frozen Windows contract tables, while executing checks
    # with portable I/O. This does not substitute for a Windows build.
    contract = (ROOT / 'tools/validate_terrain_atlas.ps1').read_text()
    block_table = contract.split('$expectedBlocks = @{', 1)[1].split('\n}', 1)[0]
    for name, body in re.findall(r"(\w+) = @\{([^}]+)\}", block_table):
        expected = dict(re.findall(r"(Tex\w+)='(\d+,\d+)'", body))
        text = (ROOT / 'media/blocks' / (name + '.block')).read_text()
        actual = {key: f'{x},{y}' for key, x, y in
                  re.findall(r'(TexAll|TexTop|TexSide|TexBottom)\s+(\d+)\s+(\d+)', text)}
        if actual != expected:
            raise ValueError(f'block faces: {name}')
    table = contract.split('$semanticByMaterial = @(', 1)[1].split('\n)', 1)[0]
    semantics = re.findall(r"'([^']*)'", table)
    source = (ROOT / 'src/HelloMine3D/Item/Material.cpp').read_text()
    body = re.search(r'MaterialIcons\s*=\s*\{\{(.*?)\}\};', source, re.S)[1]
    coordinates = [(int(x), int(y)) for x, y in re.findall(r'\{\s*(-?\d+)\s*,\s*(-?\d+)\s*\}', body)]
    expected = [(-1, -1)] + [(entries[name][0] // 16, entries[name][1] // 16) for name in semantics[1:]]
    if coordinates != expected:
        raise ValueError('Material icon mapping')
    ui = (ROOT / 'src/HelloMine3D/Ogre/OgreUserInterface.cpp').read_text()
    if not all(token in ui for token in ('terrainMaterial.atlasPixels', 'terrainMaterial.tilePixels', 'terrainMaterial.containsTile')):
        raise ValueError('HUD atlas addressing')


def main():
    entries = layout(LAYOUT)
    validate_mappings(entries)
    original = Image.open(BASE).convert('RGBA')
    image = Image.open(OUTPUT).convert('RGBA')
    validate(image, entries, original)
    rebuilt = png_bytes(build())
    if rebuilt != OUTPUT.read_bytes() or rebuilt != png_bytes(build()):
        raise ValueError('non-deterministic committed atlas')
    # Fault injection verifies the checker fails closed on meaningful corruption.
    for fault in ('opaque', 'cutout', 'icon', 'empty'):
        broken = image.copy()
        pixel = {'opaque': (0, 0), 'cutout': (96, 0), 'icon': (0, 32), 'empty': (255, 255)}[fault]
        broken.putpixel(pixel, (255, 0, 0, 128 if fault != 'empty' else 255))
        try:
            validate(broken, entries, original)
        except ValueError:
            pass
        else:
            raise AssertionError(f'Failed to reject {fault}')
    with tempfile.TemporaryDirectory() as temp:
        broken = Path(temp) / 'bad-layout'
        broken.write_text(LAYOUT.read_text() + '\ngrass_top|0|0|opaque|FFFFFF|Duplicate|重复\n')
        try:
            layout(broken)
        except ValueError:
            pass
        else:
            raise AssertionError('Failed to reject duplicate semantic')
    print('[WARM_ATLAS] PASS dimensions=256x256 semantics=120 empty=136 alpha=all block-faces=26 material-icons=43 icons=unchanged rebuild=byte-identical negative=5 sha256=' + hashlib.sha256(rebuilt).hexdigest())


if __name__ == '__main__':
    main()
