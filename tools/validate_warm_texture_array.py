#!/usr/bin/env python3
"""Validate terrain array identity, slot semantics, independent mips and alpha."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image
from build_warm_texture_array import ART, ROOT, fnv64
from build_warm_texture_atlas import layout
from adventure_texture_source import SOURCE as ADVENTURE_SOURCE, NAMES as ADVENTURE_NAMES, OVERRIDE_SOURCES
from visual_polish_texture_source import (SOURCES as POLISH_SOURCES,
    AUTHORED_EDGE as POLISH_EDGE, CUTOUT_KEY_MAX)


# Validator-owned identities: do not derive these from the packing tables.
ORE_SOURCES = {
    'coal_ore': ('coal-ore-v1.png', 13),
    'iron_ore': ('iron-ore-v1.png', 14),
}


# V01h official identities are frozen by reviewed source bytes, independently
# of the production source table and report generated from that table.
BUILT_MATERIAL_SOURCES = {
    'oak_planks': ('oak-plank-planes-v1.png', 21,
                   'd29f7203e6c96c897900329456e3db21fe70f68f7fd06cf511f6bb143824a9ff'),
    'cobblestone': ('cobblestone-planes-v1.png', 23,
                    'f6c65a685e8742feb99cabf9c38f15d6d2b67d41baee47536304f97d9bf1e782'),
}
ORIGINAL_POLISH_SOURCE_SHA256 = {
    "ground-sheet-v1.png": "4fdca793a0a753639231d7707d89b2b745b8efdfd28593190214da749c2b25f9",
    "earth-wood-sheet-v1.png": "c9d11749f61be9f889fc7b1ab8870004b8d40d2f63787b3155ab70d969bc3abe",
    "foliage-sand-sheet-v1.png": "9e617ffddb01c76582ce89305f14292db99b4d7e25738050290cd1f240f094ae",
    "birch-spruce-wood-v1.png": "d17e0d9438b59eae1e08e8b391e0f64af02d6a603325811719a66c032a5c2fff",
    "birch-spruce-stone-v1.png": "1e7203e7c8b06d0fdde778b7e05450d31b8c8d25f3bcb1aa5d0b172c8af29f57",
    "snow-sediment-v1.png": "0d4550dd42c49510dcca1bb881f543dc875bef99ba6f84203bcd6c99d41e2ac6",
    "stone-planes-v2.png": "fba5a126ef6a70168ce5c23cbcb81c6db6ea05ac7148f0e06a0926c7f04e8581",
    "coal-ore-v1.png": "59cfac7a05379144d919ed7563cfc9bfdb63bfa0f83d35b83dba57a7efc3cd8a",
    "iron-ore-v1.png": "6cc18b8780b51928a514bf85e5fe749aa774e69d78cf0891bbe0b40a979c2b00"
}


def legacy_shared_identities(polished_adventure):
    identities = {name: ('authored', ['visual-polish/' + name])
                  for name in ('dirt', 'stone', 'oak_bark_side', 'oak_bark_top',
                               'sand', 'tall_grass', 'coal_ore', 'iron_ore',
                               *sorted(polished_adventure))}
    identities['grass_top'] = ('authored', ['visual-polish/grass_top_a'])
    identities['grass_side'] = ('derived', ['visual-polish/grass_top_a', 'visual-polish/dirt'])
    identities['oak_leaves'] = ('authored', ['visual-polish/oak_leaves_a'])
    for biome in ('desert', 'grassland', 'light_forest', 'temperate_forest', 'ocean'):
        for variant in range(3):
            grass = 'visual-polish/grass_top_' + 'abc'[variant]
            leaf = 'visual-polish/oak_leaves_' + ('b' if variant == 1 else 'a')
            for base, sources in (
                    ('grass_top', [grass]), ('grass_side', [grass, 'visual-polish/dirt']),
                    ('oak_leaves', [leaf]), ('tall_grass', ['visual-polish/tall_grass'])):
                identities[f'{base}_{biome}_v{variant}'] = ('derived', sources)
    assert len(identities) == 83
    return identities


def authored_grid(source):
    # Direct centre sampling, independent of the builder's PIL resize chain.
    width, height = source.size
    result = Image.new('RGBA', (16, 16))
    result.putdata([source.getpixel((int((x + .5) * width / 16),
                                    int((y + .5) * height / 16)))
                    for y in range(16) for x in range(16)])
    return result


def opaque_source_mip(authored, edge):
    # IEC sRGB reference: equal-area means of the original 16px authored grid.
    # Use double precision and direct cell integration; no producer resizer,
    # intermediate image, tint function or mip builder is an oracle.
    tile = list(authored.getdata())
    assert len(tile) == 256 and all(p[3] == 255 for p in tile)
    assert edge >= 16 or 16 % edge == 0
    def linear(byte):
        value = byte / 255.0
        return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
    def encoded(value):
        value = value * 12.92 if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055
        return max(0, min(255, int(value * 255 + .5)))
    pixels = []
    for y in range(edge):
        for x in range(edge):
            if edge >= 16:
                pixel = tile[(y * 16 // edge) * 16 + x * 16 // edge]
            else:
                side = 16 // edge
                block = [tile[yy * 16 + xx]
                         for yy in range(y * side, (y + 1) * side)
                         for xx in range(x * side, (x + 1) * side)]
                pixel = tuple(encoded(sum(linear(p[c]) for p in block) / len(block))
                              for c in range(3)) + (255,)
            pixels.append(pixel)
    return np.asarray(pixels, dtype=np.int16).reshape(edge, edge, 4)


def validate(path, report_path):
    data = path.read_bytes()
    report = json.loads(report_path.read_text())
    magic, version, edge, layers, mips, length, checksum = struct.unpack('<8sIIIIIQ', data[:36])
    assert (magic, version, layers) == (b'HMTARRAY', 1, 256)
    assert edge in (64, 128) and mips == edge.bit_length()
    assert length == len(data) - 36 == sum((edge >> i)**2 * layers * 4 for i in range(mips))
    assert checksum == fnv64(data[36:])
    assert report['sha256'] == hashlib.sha256(data).hexdigest()
    entries = layout(ROOT / 'media/materials/Base.terrain-atlas')
    active = {y // 16 * 16 + x // 16 for x, y, _ in entries.values()}
    assert len(active) == 132 and len(set(range(256)) - active) == 124
    assert len(report['semantics']) == 132
    records = {record['semantic']: record for record in report['semantics']}
    assert len(records) == 132 and set(records) == set(entries), \
        'Array report must contain every semantic exactly once'
    assert report['adventure_source_sha256'] == hashlib.sha256(ADVENTURE_SOURCE.read_bytes()).hexdigest()
    assert report['adventure_override_sha256'] == {
        name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in OVERRIDE_SOURCES.items()}
    assert report['adventure_authored_edge'] == 32 and report['adventure_leaf_cutout_key_max'] == 12
    assert report['polish_source_sha256'] == {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in POLISH_SOURCES}
    assert report['polish_authored_edge'] == POLISH_EDGE
    assert report['polish_cutout_key_max'] == CUTOUT_KEY_MAX
    polished_adventure = {'forest_floor', 'spruce_bark_side', 'spruce_bark_top',
                         'spruce_leaves', 'birch_bark_side', 'birch_bark_top',
                         'birch_leaves', 'moss_stone', 'snow', 'gravel', 'clay', 'silt'}
    assert set(report['adventure_material_overrides']) == polished_adventure
    adventure_records = [r for r in report['semantics'] if r['semantic'] in ADVENTURE_NAMES]
    assert len(adventure_records) == 12
    assert all(r['provenance'] == 'authored' and r['sources'] == [
                   ('visual-polish/' if r['semantic'] in polished_adventure else 'adventure/') + r['semantic']]
               for r in adventure_records)
    leaf_records = [r for r in report['semantics'] if r['semantic'].startswith('oak_leaves')]
    assert len(leaf_records) == 16
    assert all(r['provenance'] in ('authored', 'derived') and
               all(source.startswith('visual-polish/oak_leaves_') for source in r['sources'])
               for r in leaf_records), 'Oak layers must use the authored voxel leaf cells'
    assert {source for r in leaf_records for source in r['sources']} == \
        {'visual-polish/oak_leaves_a', 'visual-polish/oak_leaves_b'}
    for record in report['semantics']:
        x, y, alpha = entries[record['semantic']]
        assert record['layer'] == y // 16 * 16 + x // 16 and record['alpha'] == alpha
        assert record['provenance'] in ('authored', 'derived', 'retained') and record['sources']
    authored_cutout = [r['layer'] for r in report['semantics'] if r['alpha'] == 'cutout' and r['provenance'] != 'retained']
    expected_shared = {
        'grass_top', 'grass_side', 'dirt', 'stone', 'oak_bark_side',
        'oak_bark_top', 'oak_leaves', 'sand', 'tall_grass',
        'coal_ore', 'iron_ore', *polished_adventure,
    }
    expected_shared.update(
        f'{base}_{biome}_v{variant}'
        for base in ('grass_top', 'grass_side', 'oak_leaves', 'tall_grass')
        for biome in ('desert', 'grassland', 'light_forest', 'temperate_forest', 'ocean')
        for variant in range(3))
    assert len(expected_shared) == 83
    legacy_identities = legacy_shared_identities(polished_adventure)
    assert set(legacy_identities) == expected_shared
    expected_shared.update(BUILT_MATERIAL_SOURCES)
    assert len(expected_shared) == 85
    declared_shared = {r['semantic'] for r in report['semantics']
                       if all(source.startswith('visual-polish/') for source in r['sources'])}
    assert declared_shared == expected_shared, \
        'Shared material semantics must match the independent original 83 plus two built-material layers'
    shared_records = [r for r in report['semantics'] if r['semantic'] in expected_shared]
    assert len(shared_records) == 85
    atlas = Image.open(ROOT / 'media/textures/DefaultPack.png').convert('RGBA')
    source_references = {}
    for semantic, (filename, layer) in ORE_SOURCES.items():
        record = records[semantic]
        assert entries[semantic] == (layer * 16, 0, 'opaque'), \
            'Ore semantic slot differs: ' + semantic
        assert (record['layer'], record['alpha'], record['provenance'], record['sources']) == \
            (layer, 'opaque', 'authored', ['visual-polish/' + semantic]), \
            'Ore authored provenance differs: ' + semantic
        source_path = ROOT / 'docs/art-sources/visual-polish-20260928' / filename
        assert filename in report['polish_source_sha256'] and \
            report['polish_source_sha256'][filename] == hashlib.sha256(source_path.read_bytes()).hexdigest(), \
            'Ore source hash differs: ' + semantic
        with Image.open(source_path) as source_image:
            assert source_image.size == (1254, 1254), 'Ore source dimensions differ: ' + semantic
            source = source_image.convert('RGBA')
        assert source.getchannel('A').getextrema() == (255, 255), \
            'Ore source must be opaque: ' + semantic
        authored = source.resize((16, 16), Image.Resampling.NEAREST)
        x, y, _ = entries[semantic]
        assert atlas.crop((x, y, x + 16, y + 16)).tobytes() == authored.tobytes(), \
            'Ore atlas differs from the independent authored source: ' + semantic
        source_references[layer] = (semantic, authored)
    for semantic, (filename, layer, frozen_sha) in BUILT_MATERIAL_SOURCES.items():
        record = records[semantic]
        assert entries[semantic] == (layer % 16 * 16, layer // 16 * 16, 'opaque'), \
            'Built material semantic slot differs: ' + semantic
        assert (record['layer'], record['alpha'], record['provenance'], record['sources']) == \
            (layer, 'opaque', 'authored', ['visual-polish/' + semantic]), \
            'Built material authored provenance differs: ' + semantic
        source_path = ROOT / 'docs/art-sources/visual-polish-20260928' / filename
        with Image.open(source_path) as image:
            assert image.size == (1254, 1254), 'Built material source dimensions differ: ' + semantic
            source = image.convert('RGBA')
        assert source.getchannel('A').getextrema() == (255, 255), \
            'Built material source must be opaque: ' + semantic
        assert report['polish_source_sha256'][filename] == frozen_sha == \
            hashlib.sha256(source_path.read_bytes()).hexdigest(), \
            'Built material frozen source SHA differs: ' + semantic
        authored = authored_grid(source)
        x, y, _ = entries[semantic]
        assert atlas.crop((x, y, x + 16, y + 16)).tobytes() == authored.tobytes(), \
            'Built material atlas differs from independent authored source: ' + semantic
        source_references[layer] = (semantic, authored)
    fixed_source_sha = dict(ORIGINAL_POLISH_SOURCE_SHA256)
    fixed_source_sha.update({filename: digest for filename, _, digest in BUILT_MATERIAL_SOURCES.values()})
    assert report['polish_source_sha256'] == fixed_source_sha, \
        'Original nine plus two official polish source identities differ'
    assert len({path.name for path in POLISH_SOURCES}) == len(POLISH_SOURCES) == 11
    for semantic, identity in legacy_identities.items():
        record = records[semantic]
        assert (record['provenance'], record['sources']) == identity, \
            'Original 83 authored provenance differs: ' + semantic
    offset, coverage = 36, {}
    for mip in range(mips):
        size = edge >> mip
        count = layers * size * size * 4
        pixels = np.frombuffer(data, dtype=np.uint8, count=count, offset=offset).reshape(layers, size, size, 4)
        if mip == 0:
            # World, held blocks and UI must retain the same authored material
            # cells; allow only one integer rounding step from offline tinting.
            for record in shared_records:
                x, y, _ = entries[record['semantic']]
                expected = np.asarray(atlas.crop((x, y, x + 16, y + 16)).resize(
                    (size, size), Image.Resampling.NEAREST), dtype=np.int16)
                actual = pixels[record['layer']].astype(np.int16)
                visible = expected[:, :, 3] >= 128
                assert np.array_equal(actual[:, :, 3] >= 128, visible), \
                    'World/UI cutout identity differs: ' + record['semantic']
                assert np.max(np.abs(actual[:, :, :3][visible] - expected[:, :, :3][visible])) <= 1, \
                    'World/UI material identity differs: ' + record['semantic']
                if record in leaf_records:
                    assert .70 <= float(np.mean(visible)) <= .93, \
                        'Oak canopy must keep both connected volume and authored holes'
                if record['semantic'] in ('birch_leaves', 'spruce_leaves'):
                    assert .75 <= float(np.mean(visible)) < 1, \
                        'Species canopy must keep both volume and authored holes'

        for layer, (semantic, authored) in source_references.items():
            reference = opaque_source_mip(authored, size)
            actual = pixels[layer].astype(np.int16)
            assert np.all(actual[:, :, 3] == 255) and \
                np.max(np.abs(actual[:, :, :3] - reference[:, :, :3])) <= 1, \
                f'Independent source mip differs: {semantic} mip {mip}'

        assert not pixels[list(set(range(256)) - active)].any(), f'Nonempty unused layer at mip {mip}'
        for record in report['semantics']:
            if record['alpha'] == 'opaque':
                assert np.all(pixels[record['layer'], :, :, 3] == 255), record['semantic']
        for layer in authored_cutout:
            value = float(np.mean(pixels[layer, :, :, 3] >= 128))
            if mip == 0:
                assert 0 < value < 1, f'Lost cutout holes in layer {layer}'
                coverage[layer] = value
            assert abs(value - coverage[layer]) <= max(.045, 1 / (size * size)), (layer, mip, value, coverage[layer])
        offset += count
    assert offset == len(data)
    if edge == 64:
        assert length + 256 * 256 * 4 <= 16 * 1024 * 1024
    return dict(status='PASS', edge=edge, mips=mips, active_slots=len(active),
                empty_slots=256-len(active), array_pixel_bytes=length,
                retained_legacy_atlas_bytes=262144, alpha_layers=len(authored_cutout),
                voxel_oak_leaf_layers=len(leaf_records),
                shared_material_layers=len(shared_records),
                preserved_original_shared_material_layers=len(legacy_identities),
                independent_opaque_source_mips=len(source_references) * mips,
                source_images=len(POLISH_SOURCES)+1+len(OVERRIDE_SOURCES),
                adventure_materials=len(adventure_records), sha256=report['sha256'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--array', type=Path, default=ROOT / 'media/textures/WarmWilderness64.hmt')
    parser.add_argument('--report', type=Path, default=ART / 'array-build.json')
    args = parser.parse_args()
    print(json.dumps(validate(args.array, args.report), indent=2))
