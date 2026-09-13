#!/usr/bin/env python3
"""Validate terrain array identity, slot semantics, independent mips and alpha."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from build_warm_texture_array import ART, ROOT, NAMES, fnv64, source_path
from build_warm_texture_atlas import layout


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
    assert len(active) == 120 and len(set(range(256)) - active) == 136
    assert len(report['semantics']) == 120
    leaf_records = [r for r in report['semantics'] if r['semantic'].startswith('oak_leaves')]
    assert len(leaf_records) == 16
    assert all(r['provenance'] in ('authored', 'derived') and
               all(source.startswith('voxel-oak-') for source in r['sources'])
               for r in leaf_records), 'Standard leaf layers must use voxel-oak sources'
    assert {source for r in leaf_records for source in r['sources']} == \
        {'voxel-oak-a-rgb', 'voxel-oak-b-rgb'}
    assert report['leaf_cutout_thresholds'] == {
        'voxel-oak-a-rgb': 16, 'voxel-oak-b-rgb': 12}
    assert report['leaf_visible_rgb_floor'] == [26, 47, 21]
    assert report['leaf_colour_gain'] == [1.14, 1.18, 1.10]
    for record in report['semantics']:
        x, y, alpha = entries[record['semantic']]
        assert record['layer'] == y // 16 * 16 + x // 16 and record['alpha'] == alpha
        assert record['provenance'] in ('authored', 'derived', 'retained') and record['sources']
    for name in NAMES:
        assert hashlib.sha256(source_path(name).read_bytes()).hexdigest() == report['sources'][name]
        assert hashlib.sha256((ART / 'masters128' / (name + '.png')).read_bytes()).hexdigest() == report['masters128'][name]
    authored_cutout = [r['layer'] for r in report['semantics'] if r['alpha'] == 'cutout' and r['provenance'] != 'retained']
    offset, coverage = 36, {}
    for mip in range(mips):
        size = edge >> mip
        count = layers * size * size * 4
        pixels = np.frombuffer(data, dtype=np.uint8, count=count, offset=offset).reshape(layers, size, size, 4)
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
                source_images=len(NAMES), sha256=report['sha256'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--array', type=Path, default=ROOT / 'media/textures/WarmWilderness64.hmt')
    parser.add_argument('--report', type=Path, default=ART / 'array-build.json')
    args = parser.parse_args()
    print(json.dumps(validate(args.array, args.report), indent=2))
