#!/usr/bin/env python3
"""Build Warm Wilderness atlas from frozen art sources (Python 3 + Pillow).

Only sampling, compositing and palette derivation happen here. PNG uses stored
DEFLATE blocks so the output bytes do not depend on the installed zlib version.
"""
import argparse
import binascii
from pathlib import Path
import re
import struct
import zlib
from PIL import Image
from adventure_texture_source import tiles as adventure_tiles

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/art-sources/hellomine3d-pre-warm-atlas.png'
SOURCE = ROOT / 'docs/art-sources/hellomine3d-warm-natural-source.png'
LAYOUT = ROOT / 'media/materials/Base.terrain-atlas'
OUTPUT = ROOT / 'media/textures/DefaultPack.png'


def layout(path):
    entries = {}
    coordinates = set()
    lines = path.read_text().splitlines()
    if lines[0] != '# HelloMine3D terrain atlas layout v1':
        raise ValueError('Invalid layout header')
    for line in lines[1:]:
        if not line.strip() or line.startswith('#'):
            continue
        name, x, y, alpha, fill, english, chinese = line.split('|')
        x, y = int(x), int(y)
        if (name in entries or (x, y) in coordinates or
                not 0 <= x < 16 or not 0 <= y < 16 or
                not re.fullmatch('[a-z][a-z0-9_]*', name) or
                alpha not in ('opaque', 'cutout', 'translucent', 'icon') or
                not re.fullmatch('[0-9A-F]{6}', fill) or not english or not chinese):
            raise ValueError(f'Invalid layout entry: {line}')
        entries[name] = (x * 16, y * 16, alpha)
        coordinates.add((x, y))
    if len(entries) != 132:
        raise ValueError('Expected 132 semantics')
    return entries


def png_bytes(image):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload +
                struct.pack('>I', binascii.crc32(kind + payload) & 0xffffffff))
    pixels = image.tobytes()
    raw = b''.join(b'\0' + pixels[y * 1024:(y + 1) * 1024] for y in range(256))
    stream = bytearray(b'\x78\x01')
    for offset in range(0, len(raw), 65535):
        block = raw[offset:offset + 65535]
        stream += bytes([int(offset + len(block) == len(raw))])
        stream += struct.pack('<HH', len(block), 65535 - len(block)) + block
    stream += struct.pack('>I', zlib.adler32(raw) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', 256, 256, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', bytes(stream)) + chunk(b'IEND', b''))


def build(source=SOURCE, base=BASE, layout_path=LAYOUT):
    entries = layout(layout_path)
    atlas = Image.open(base).convert('RGBA')
    if atlas.size != (256, 256):
        raise ValueError('Invalid frozen atlas dimensions')
    art = Image.open(source).convert('RGB')
    if art.size != (1774, 887):
        raise ValueError('Unexpected generated source dimensions')
    def tile(name):
        x, y, _ = entries[name]
        return atlas.crop((x, y, x + 16, y + 16))
    def put(name, image):
        atlas.paste(image, entries[name][:2])
    def sample(column, row):
        # Cell interiors exclude the generated edge seams, sampled at texel centres.
        box = (column * 443 + 12, row * 443 + 12,
               (column + 1) * 443 - 12, (row + 1) * 443 - 12)
        return art.crop(box).resize((16, 16), Image.Resampling.NEAREST).convert('RGBA')
    grasses = [sample(i, 0) for i in range(3)]
    leaf_alpha = tile('oak_leaves').getchannel('A')
    for name, col, row in [('dirt', 3, 0), ('stone', 0, 1),
                           ('oak_bark_side', 1, 1), ('oak_bark_top', 2, 1),
                           ('oak_leaves', 3, 1)]:
        image = sample(col, row)
        if name == 'oak_leaves':
            image.putalpha(leaf_alpha)
        put(name, image)
    put('grass_top', grasses[0])
    def grass_side(grass):
        side = tile('dirt')
        for x in range(16):
            for y in range(3 + (x * 7 % 3)):
                side.putpixel((x, y), grass.getpixel((x, y)))
        return side
    put('grass_side', grass_side(grasses[0]))
    plants = [(1.12, .92, .77), (1.02, 1.02, .95),
              (.96, 1.01, .96), (.91, .96, .94), (.92, .99, 1.04)]
    waters = [(.94, 1.02, 1.04), (.92, 1.02, 1.04),
              (.88, 1, 1.06), (.84, .96, 1.08), (.80, .98, 1.14)]
    for biome, plant, water in zip(('desert', 'grassland', 'light_forest',
                                   'temperate_forest', 'ocean'), plants, waters):
        for variant in range(3):
            for name in ('grass_top', 'grass_side', 'oak_leaves', 'water', 'tall_grass'):
                image = (grasses[variant].copy() if name == 'grass_top' else
                         grass_side(grasses[variant]) if name == 'grass_side' else tile(name))
                if name not in ('grass_top', 'grass_side') and variant:
                    image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT if name == 'tall_grass'
                                            else Image.Transpose.ROTATE_90 if variant == 1
                                            else Image.Transpose.ROTATE_270)
                tint = water if name == 'water' else plant
                for y in range(16):
                    for x in range(16):
                        rgba = image.getpixel((x, y))
                        # Preserve dirt colour below the grass fringe.
                        factor = (1, 1, 1) if name == 'grass_side' and y >= 3 + (x * 7 % 3) else tint
                        rgb = tuple(min(255, int(rgba[i] * factor[i] * (1.0, 1.02, .98)[variant] + .5)) for i in range(3))
                        image.putpixel((x, y), (*rgb, rgba[3]))
                put(f'{name}_{biome}_v{variant}', image)
    for name, image in adventure_tiles(16).items():
        put(name, image)
    return atlas


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUTPUT)
    parser.add_argument('--layout', type=Path, default=LAYOUT)
    args = parser.parse_args()
    args.output.write_bytes(png_bytes(build(layout_path=args.layout)))
    print(f'[WARM_ATLAS] PASS output={args.output}')
