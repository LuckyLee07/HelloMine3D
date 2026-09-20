#!/usr/bin/env python3
"""Bake frozen art into linear-filtered, independently mipmapped terrain layers.

HMTARRAY v1: 36-byte little-endian header (8-byte magic, version, edge,
layers, mip count, payload bytes, FNV-1a-64), then mip-major RGBA8 layers.
Stored RGB is sRGB; filtering is premultiplied linear light. Runtime deliberately
uses the existing gamma-space lighting pipeline, so hardware sRGB decode is off.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image
from build_warm_texture_atlas import layout
from adventure_texture_source import SOURCE as ADVENTURE_SOURCE, tiles as adventure_tiles

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'docs/art-sources/warm-wilderness-v2/pixel-revision'
LEAF_ART = ROOT / 'docs/art-sources/warm-wilderness-v2/canopy-voxel-oak-20260913'
NAMES = ('grass-top-a grass-top-b grass-top-c grass-side-a grass-side-b '
         'dirt-a dirt-b stone-a stone-b bark-a bark-b bark-top '
         'sand-a sand-b tallgrass-a tallgrass-b voxel-oak-a-rgb voxel-oak-b-rgb').split()


def source_path(name):
    return (LEAF_ART if name.startswith('voxel-oak-') else ART) / (name + '.png')


def srgb_to_linear(rgb):
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)


def linear_to_srgb(rgb):
    return np.where(rgb <= .0031308, rgb * 12.92,
                    1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - .055)


def extend_rgb(rgba):
    """Extend visible colour into transparency; never filter black/green matte."""
    known = rgba[:, :, 3] >= .5
    if not known.any():
        return rgba
    rgb = rgba[:, :, :3].copy()
    while not known.all():
        count = np.zeros(known.shape, dtype=np.float32)
        total = np.zeros(rgb.shape, dtype=np.float32)
        for axis, step in ((0, -1), (0, 1), (1, -1), (1, 1)):
            valid = np.roll(known, step, axis)
            total += np.roll(rgb, step, axis) * valid[:, :, None]
            count += valid
        new = ~known & (count > 0)
        rgb[new] = total[new] / count[new, None]
        known |= new
    rgba[:, :, :3] = rgb
    return rgba


def resize(rgba, edge, coverage=None):
    linear = srgb_to_linear(rgba[:, :, :3])
    alpha = rgba[:, :, 3:4]
    data = np.concatenate((linear * alpha, alpha), axis=2)
    filtered = np.stack([np.asarray(Image.fromarray(data[:, :, i], 'F').resize(
        (edge, edge), Image.Resampling.BOX), dtype=np.float32) for i in range(4)], axis=2)
    a = filtered[:, :, 3:4]
    rgb = linear_to_srgb(filtered[:, :, :3] / np.maximum(a, 1e-8))
    result = np.concatenate((np.clip(rgb, 0, 1), a), axis=2)
    if coverage is not None and 0 < coverage < 1:
        # Choose the closest representable coverage, keeping holes down to the
        # last meaningful mip. At 1x1 the target can only quantize to 0 or 1.
        count = int(coverage * edge * edge + .5)
        if count:
            threshold = np.sort(a.ravel())[-count]
            if threshold > 0:
                result[:, :, 3] = np.clip(a[:, :, 0] * (.501 / threshold), 0, 1)
        else:
            result[:, :, 3] = np.minimum(a[:, :, 0], .49)
    return extend_rgb(result)


def bytes_rgba(rgba):
    return np.floor(np.clip(rgba, 0, 1) * 255 + .5).astype(np.uint8).tobytes()


def fnv64(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def build(edge=64):
    entries = layout(ROOT / 'media/materials/Base.terrain-atlas')
    masters = {}
    source_hashes = {}
    leaf_cutout_thresholds = {'voxel-oak-a-rgb': 16, 'voxel-oak-b-rgb': 12}
    leaf_visible_rgb_floor = [26, 47, 21]
    leaf_colour_gain = [1.14, 1.18, 1.10]
    master_hashes = {}
    master_dir = ART / 'masters128'
    master_dir.mkdir(exist_ok=True)
    for name in NAMES:
        path = source_path(name)
        source_hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        src = np.asarray(Image.open(path).convert('RGBA'), dtype=np.float32) / 255
        if name in leaf_cutout_thresholds:
            # The original bitmap is RGB. Pure near-black cells are authored
            # gaps; derive a hard pixel cutout before linear-light filtering.
            src[:, :, 3] = (np.max(src[:, :, :3], axis=2) >=
                            leaf_cutout_thresholds[name] / 255).astype(np.float32)
            # Near-black cells just above the cutout threshold otherwise bake
            # as opaque black specks and make the whole crown look dirty.
            src[:, :, :3] = np.maximum(
                src[:, :, :3], np.array(leaf_visible_rgb_floor) / 255)
        if name.startswith(('voxel-oak-', 'tallgrass')):
            src[:, :, 3] = np.where(src[:, :, 3] < .05, 0, src[:, :, 3])
        coverage = float(np.mean(src[:, :, 3] >= .5)) if name.startswith(('voxel-oak-', 'tallgrass')) else None
        master = resize(src, 128, coverage)
        master_path = master_dir / (name + '.png')
        # Preserve an existing PNG's bytes when its pixels match the freshly
        # calculated master; Pillow metadata may differ across releases.
        master_bytes = bytes_rgba(master)
        if not master_path.exists() or Image.open(master_path).convert('RGBA').tobytes() != master_bytes:
            Image.frombytes('RGBA', (128, 128), master_bytes).save(master_path)
        master_hashes[name] = hashlib.sha256(master_path.read_bytes()).hexdigest()
        masters[name] = master
    old = Image.open(ROOT / 'media/textures/DefaultPack.png').convert('RGBA')
    adventure = adventure_tiles(32)
    # The compatibility atlas remains classic; only standard array leaf layers
    # use this voxel-oak material candidate.
    direct = {'grass_top': ['grass-top-a', 'grass-top-b', 'grass-top-c'],
              'grass_side': ['grass-side-a', 'grass-side-b', 'grass-side-a'],
              'dirt': ['dirt-a', 'dirt-b'], 'stone': ['stone-a', 'stone-b'],
              'oak_bark_side': ['bark-a', 'bark-b'], 'oak_bark_top': ['bark-top'],
              'sand': ['sand-a', 'sand-b'], 'tall_grass': ['tallgrass-a', 'tallgrass-b', 'tallgrass-a'],
              'oak_leaves': ['voxel-oak-a-rgb', 'voxel-oak-b-rgb', 'voxel-oak-a-rgb']}
    tints = dict(zip(('desert', 'grassland', 'light_forest', 'temperate_forest', 'ocean'),
                    ((1.12, .92, .77), (1.02, 1.02, .95), (.96, 1.01, .96),
                     (.91, .96, .94), (.92, .99, 1.04))))
    layers = [np.zeros((edge, edge, 4), dtype=np.float32) for _ in range(256)]
    records = []
    cutouts = set()
    for semantic, (x, y, alpha) in entries.items():
        base, variant, biome = semantic, 0, None
        for candidate in tints:
            marker = '_' + candidate + '_v'
            if marker in semantic:
                base, index = semantic.split(marker)
                variant, biome = int(index), candidate
                break
        if base in adventure:
            authored = adventure[base].resize((128, 128), Image.Resampling.NEAREST)
            rgba = np.asarray(authored, dtype=np.float32) / 255
            used, provenance = ['adventure/' + base], 'authored'
        elif base in direct:
            names = direct[base]
            # Single-address earth/rock/bark/sand use both authored sources;
            # ecology variants retain independent grass/leaf silhouettes.
            if base in ('dirt', 'stone', 'oak_bark_side', 'sand'):
                rgba = masters[names[0]].copy()
                rgba[:, :, :3] = linear_to_srgb(sum(srgb_to_linear(masters[n][:, :, :3]) for n in names) / len(names))
                used = names
            else:
                used = [names[variant % len(names)]]
                rgba = masters[used[0]].copy()
                if variant == 2 and base in ('grass_side', 'tall_grass', 'oak_leaves'):
                    rgba = rgba[:, ::-1].copy()
            if biome:
                tint = np.array(tints[biome], dtype=np.float32)
                if base == 'grass_side':
                    rgba[:26, :, :3] *= tint
                else:
                    rgba[:, :, :3] *= tint
            if base == 'oak_leaves':
                rgba[:, :, :3] *= np.array(leaf_colour_gain,
                                           dtype=np.float32)
            provenance = 'authored' if len(used) == 1 and not biome else 'derived'
        else:
            rgba = np.asarray(old.crop((x, y, x + 16, y + 16)).resize((128, 128), Image.Resampling.NEAREST), dtype=np.float32) / 255
            used, provenance = ['Warm Wilderness v1 / ' + semantic], 'retained'
        layer = y // 16 * 16 + x // 16
        if alpha == 'cutout':
            cutouts.add(layer)
        coverage = float(np.mean(rgba[:, :, 3] >= .5)) if layer in cutouts else None
        layers[layer] = resize(rgba, edge, coverage)
        records.append(dict(semantic=semantic, layer=layer, alpha=alpha, provenance=provenance, sources=used))
    mips, mip_data, coverage_records = edge.bit_length(), [], []
    target = {i: float(np.mean(layers[i][:, :, 3] >= .5)) for i in cutouts}
    for mip in range(mips):
        size = edge >> mip
        images = [resize(layer, size, target.get(i)) if mip else layer for i, layer in enumerate(layers)]
        mip_data.extend(bytes_rgba(rgba) for rgba in images)
        coverage_records.append({str(i): float(np.mean(images[i][:, :, 3] >= .5)) for i in sorted(cutouts)})
    payload = b''.join(mip_data)
    header = struct.pack('<8sIIIIIQ', b'HMTARRAY', 1, edge, 256, mips, len(payload), fnv64(payload))
    report = dict(format_version=1, edge=edge, layers=256, mips=mips, payload_bytes=len(payload),
                  sha256=hashlib.sha256(header + payload).hexdigest(), sources=source_hashes,
                  adventure_source_sha256=hashlib.sha256(ADVENTURE_SOURCE.read_bytes()).hexdigest(),
                  adventure_authored_edge=32, adventure_leaf_cutout_key_max=12,
                  leaf_cutout_thresholds=leaf_cutout_thresholds,
                  leaf_visible_rgb_floor=leaf_visible_rgb_floor,
                  leaf_colour_gain=leaf_colour_gain,
                  masters128=master_hashes,
                  active_slots=len(records), empty_slots=256-len(records), semantics=records,
                  alpha_coverage=coverage_records, colour_space='sRGB RGBA8, premultiplied linear-light offline filtering')
    return header + payload, report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--edge', type=int, choices=(64, 128), default=64)
    parser.add_argument('--output', type=Path, default=ROOT / 'media/textures/WarmWilderness64.hmt')
    parser.add_argument('--report', type=Path, default=ART / 'array-build.json')
    args = parser.parse_args()
    data, report = build(args.edge)
    args.output.write_bytes(data)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f'[TERRAIN_ARRAY] PASS {args.output} bytes={len(data)} sha256={report["sha256"]}')
