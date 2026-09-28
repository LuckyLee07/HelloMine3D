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
                         'birch_leaves', 'moss_stone'}
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
    shared_records = [r for r in report['semantics']
                      if all(source.startswith('visual-polish/') for source in r['sources'])]
    assert len(shared_records) == 77
    atlas = Image.open(ROOT / 'media/textures/DefaultPack.png').convert('RGBA')
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
                source_images=len(POLISH_SOURCES)+1+len(OVERRIDE_SOURCES),
                adventure_materials=len(adventure_records), sha256=report['sha256'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--array', type=Path, default=ROOT / 'media/textures/WarmWilderness64.hmt')
    parser.add_argument('--report', type=Path, default=ART / 'array-build.json')
    args = parser.parse_args()
    print(json.dumps(validate(args.array, args.report), indent=2))
