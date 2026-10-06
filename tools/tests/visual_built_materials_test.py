#!/usr/bin/env python3
"""Independent CPU proof of V01h source identities and a two-layer asset update.

Reads frozen pre-update facts, original artwork, atlas bytes and all seven HMT
mips. Does not import a production builder, validator or game consumer. Source
appearance and ordinary gameplay acceptance require separate visual evidence.
"""

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'docs/art-sources/visual-polish-20260928'
SOURCES = {
    'oak_planks': ('oak-plank-planes-v1.png', 21,
                   'd29f7203e6c96c897900329456e3db21fe70f68f7fd06cf511f6bb143824a9ff'),
    'cobblestone': ('cobblestone-planes-v1.png', 23,
                    'f6c65a685e8742feb99cabf9c38f15d6d2b67d41baee47536304f97d9bf1e782'),
}
MAX_FILE = 32 * 1024 * 1024


class VerificationError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise VerificationError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class Inputs:
    def __init__(self):
        self.fingerprints = {}

    def read(self, path):
        path = Path(path).resolve(strict=True)
        size = path.stat().st_size
        require(0 < size <= MAX_FILE, f'Input byte bound differs: {path}')
        data = path.read_bytes()
        require(len(data) == size, f'Input changed while reading: {path}')
        fingerprint = {'bytes': size, 'sha256': sha256(data)}
        require(str(path) not in self.fingerprints or
                self.fingerprints[str(path)] == fingerprint, f'Repeated input changed: {path}')
        self.fingerprints[str(path)] = fingerprint
        return data

    def image(self, path):
        data = self.read(path)
        with Image.open(io.BytesIO(data)) as image:
            require(image.format == 'PNG', 'Source must be PNG')
            require(image.width * image.height * 4 <= MAX_FILE, 'PNG extent exceeds bound')
            return image.convert('RGBA')

    def unchanged(self):
        return all(Path(path).stat().st_size == item['bytes'] and
                   sha256(Path(path).read_bytes()) == item['sha256']
                   for path, item in self.fingerprints.items())


def fnv64(payload):
    result = 14695981039346656037
    for byte in payload:
        result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result


class Hmt:
    def __init__(self, data):
        require(len(data) >= 36, 'HMT header truncated')
        magic, version, edge, layers, mips, size, checksum = struct.unpack('<8sIIIIIQ', data[:36])
        require((magic, version, edge, layers, mips) == (b'HMTARRAY', 1, 64, 256, 7),
                'Default HMT header differs')
        require(size == len(data) - 36 == sum((64 >> m) ** 2 * 256 * 4 for m in range(7)),
                'HMT payload extent or trailing bytes differ')
        require(checksum == fnv64(data[36:]), 'Independent HMT FNV64 differs')
        self.data = data

    def layer(self, mip, layer):
        require(0 <= mip < 7 and 0 <= layer < 256, 'HMT layer/mip outside bounds')
        offset = 36 + sum((64 >> m) ** 2 * 256 * 4 for m in range(mip))
        length = (64 >> mip) ** 2 * 4
        return self.data[offset + layer * length:offset + (layer + 1) * length]


def atlas_cell(image, layer):
    x, y = layer % 16 * 16, layer // 16 * 16
    return image.crop((x, y, x + 16, y + 16)).tobytes()


def source_cell(image):
    # Direct sample centres in original full-image coordinates. No resizer.
    return [image.getpixel((int((x + .5) * image.width / 16),
                            int((y + .5) * image.height / 16)))
            for y in range(16) for x in range(16)]


def source_mip(tile, edge):
    # IEC sRGB, double precision equal-area integration of original 16px cells.
    require(len(tile) == 256 and all(p[3] == 255 for p in tile), 'Reference tile opacity differs')
    require(edge in (64, 32, 16, 8, 4, 2, 1), 'Default reference mip size differs')
    def linear(byte):
        value = byte / 255.0
        return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
    def encoded(value):
        value = 12.92 * value if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055
        return min(255, max(0, int(math.floor(value * 255 + .5))))
    result = bytearray()
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
            result.extend(pixel)
    return bytes(result)


def validate(args):
    inputs = Inputs()
    facts = json.loads(inputs.read(args.baseline_facts))['baseline']
    old_report = facts['array_report']
    old_array_path = args.before_array or Path(facts['protected_array'])
    old_atlas_path = args.before_atlas or Path(facts['protected_atlas'])
    old_array_data = inputs.read(old_array_path)
    require(sha256(old_array_data) == facts['protected_array_sha256'] == old_report['sha256'],
            'Frozen before array identity differs')
    before_array = Hmt(old_array_data)
    old_atlas_data = inputs.read(old_atlas_path)
    require(sha256(old_atlas_data) == facts['protected_atlas_sha256'], 'Frozen before atlas identity differs')
    before_atlas = inputs.image(old_atlas_path)
    after_atlas = inputs.image(args.atlas)
    require(before_atlas.size == after_atlas.size == (256, 256), 'Default atlas dimensions differ')
    for relative, digest in facts['fixed_files'].items():
        require(sha256(inputs.read(ROOT / relative)) == digest, f'Fixed runtime/resource identity differs: {relative}')
    data = inputs.read(args.array)
    after_array = Hmt(data)
    report = json.loads(inputs.read(args.report))
    require(report['sha256'] == sha256(data), 'Report array SHA differs')
    require(set(report) == set(old_report), 'Report field set differs')
    for key in set(old_report) - {'sha256', 'polish_source_sha256', 'semantics'}:
        require(report[key] == old_report[key], f'Unaffected report field differs: {key}')
    before_records = {r['semantic']: r for r in old_report['semantics']}
    records = {r['semantic']: r for r in report['semantics']}
    require(len(report['semantics']) == len(records) == len(before_records) == 132 and
            set(records) == set(before_records), 'Fixed 132 semantic identities differ')
    selected = {item[1] for item in SOURCES.values()}
    require(selected == {21, 23}, 'Independent selected-layer contract differs')
    for semantic, old in before_records.items():
        if semantic not in SOURCES:
            require(records[semantic] == old, 'Unaffected semantic record differs: ' + semantic)
        else:
            expected = dict(old, provenance='authored', sources=['visual-polish/' + semantic])
            require(records[semantic] == expected, 'Built material semantic provenance differs: ' + semantic)
            require((expected['layer'], expected['alpha']) == (SOURCES[semantic][1], 'opaque'),
                    'Built material fixed slot differs: ' + semantic)
    legacy = set(facts['original_shared_semantics'])
    require(len(legacy) == 83 and not legacy.intersection(SOURCES), 'Original 83 baseline differs')
    declared_shared = {r['semantic'] for r in report['semantics']
                       if all(source.startswith('visual-polish/') for source in r['sources'])}
    require(declared_shared == legacy | set(SOURCES) and len(declared_shared) == 85,
            'Shared identities must preserve original 83 plus exactly two built materials')
    expected_sha = dict(facts['original_source_sha256'])
    require(len(expected_sha) == 9, 'Original nine source baseline differs')
    expected_sha.update({name: digest for name, _, digest in SOURCES.values()})
    require(report['polish_source_sha256'] == expected_sha, 'Fixed original nine plus two source report SHA differ')
    for filename, digest in expected_sha.items():
        require(sha256(inputs.read(args.sources / filename)) == digest, 'Official source bytes differ: ' + filename)
    source_checks = []
    cells = {}
    for semantic, (filename, layer, _) in SOURCES.items():
        image = inputs.image(args.sources / filename)
        require(image.size == (1254, 1254), 'Official source dimensions differ: ' + semantic)
        require(image.getchannel('A').getextrema() == (255, 255), 'Official source opacity differs: ' + semantic)
        tile = source_cell(image)
        cells[semantic] = b''.join(bytes(p) for p in tile)
        require(atlas_cell(after_atlas, layer) == cells[semantic], 'Atlas/original-source identity differs: ' + semantic)
        for mip in range(7):
            reference = source_mip(tile, 64 >> mip)
            actual = after_array.layer(mip, layer)
            maximum = max(abs(a - b) for i, (a, b) in enumerate(zip(actual, reference)) if i % 4 != 3)
            opaque = all(actual[i] == 255 for i in range(3, len(actual), 4))
            require(maximum <= 1 and opaque, f'Independent original-source mip differs: {semantic} mip {mip}')
            source_checks.append({'semantic': semantic, 'layer': layer, 'mip': mip,
                                  'maximum_rgb_delta': maximum, 'alpha255': opaque})
    require(len(set(cells.values())) == 2, 'Two built-material authored tiles must be distinct')
    atlas_changed = [layer for layer in range(256)
                     if atlas_cell(before_atlas, layer) != atlas_cell(after_atlas, layer)]
    require(set(atlas_changed) == selected, 'Atlas must change exactly layers 21 and 23')
    mip_diffs = []
    for mip in range(7):
        changed = [layer for layer in range(256)
                   if before_array.layer(mip, layer) != after_array.layer(mip, layer)]
        require(set(changed) == selected, f'Array mip {mip} must change exactly layers 21 and 23')
        mip_diffs.append({'mip': mip, 'edge': 64 >> mip, 'changed_layers': changed, 'unchanged_layers': 254})
    require(inputs.unchanged(), 'Inputs changed during independent verification')
    return {'status': 'PASS', 'shared_material_layers': 85, 'original_shared_layers_preserved': 83,
            'original_source_images_preserved': 9, 'new_source_images': 2,
            'unaltered_semantic_records': 130, 'atlas_changed_layers': atlas_changed,
            'array_mips': mip_diffs, 'source_reference_checks': source_checks,
            'input_fingerprints': inputs.fingerprints, 'all_inputs_held': True,
            'ordinary_visual_acceptance': 'separate; not asserted by this CPU source/byte oracle'}


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-facts', type=Path, required=True)
    parser.add_argument('--before-array', type=Path)
    parser.add_argument('--before-atlas', type=Path)
    parser.add_argument('--atlas', type=Path, default=ROOT / 'media/textures/DefaultPack.png')
    parser.add_argument('--array', type=Path, default=ROOT / 'media/textures/WarmWilderness64.hmt')
    parser.add_argument('--report', type=Path, default=ROOT / 'docs/art-sources/warm-wilderness-v2/pixel-revision/array-build.json')
    parser.add_argument('--sources', type=Path, default=ART)
    parser.add_argument('--output', type=Path)
    return parser.parse_args()


if __name__ == '__main__':
    args = arguments()
    try:
        receipt = validate(args)
    except (VerificationError, OSError, ValueError, KeyError, struct.error) as error:
        receipt = {'status': 'FAIL', 'reason': str(error)}
    text = json.dumps(receipt, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text, end='')
    sys.exit(0 if receipt['status'] == 'PASS' else 1)
