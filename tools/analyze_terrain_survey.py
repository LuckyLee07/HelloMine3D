#!/usr/bin/env python3
"""Summarize production T0-SURVEY CSV. No terrain algorithm is reimplemented.

Usage: python3 tools/analyze_terrain_survey.py SURVEY_DIR [--plot]
The output is descriptive evidence; acceptance thresholds live in the contract.
"""

import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)] if ordered else None


def largest_component(points):
    remaining = set(points)
    largest = 0
    while remaining:
        stack = [remaining.pop()]
        size = 0
        while stack:
            x, z = stack.pop()
            size += 1
            for adjacent in ((x - 1, z), (x + 1, z), (x, z - 1), (x, z + 1)):
                if adjacent in remaining:
                    remaining.remove(adjacent)
                    stack.append(adjacent)
        largest = max(largest, size)
    return largest


def summarize(rows):
    heights = [r['height'] for r in rows]
    slopes = [max(abs(r['dx']), abs(r['dz'])) for r in rows]
    return {
        'samples': len(rows), 'height_min': min(heights), 'height_max': max(heights),
        'height_mean': statistics.mean(heights),
        'height_stddev': statistics.pstdev(heights),
        'height_unique': len(set(heights)),
        'height_63_fraction': heights.count(63) / len(rows),
        'cap_176_fraction': heights.count(176) / len(rows),
        'water_fraction': sum(h < 64 for h in heights) / len(rows),
        'slope_max': max(slopes), 'slope_p95': percentile(slopes, .95),
        'slope_p99': percentile(slopes, .99),
        'step_at_most_one_fraction': sum(s <= 1 for s in slopes) / len(rows),
        'biomes': dict(sorted(collections.Counter(r['biome'] for r in rows).items())),
    }


def analyze(directory, plot=False):
    groups = collections.defaultdict(list)
    fingerprints = collections.defaultdict(hashlib.sha256)
    with (directory / 'samples.csv').open(newline='') as stream:
        for row in csv.DictReader(stream):
            kind = row.pop('set')
            values = {key: int(value) for key, value in row.items()}
            groups[values['seed'], kind].append(values)
            canonical = ','.join([kind] + [str(values[k]) for k in
                ('seed', 'version', 'x', 'z', 'height', 'biome', 'dx', 'dz')])
            fingerprints[values['seed']].update((canonical + '\n').encode())
    seeds = sorted({seed for seed, _ in groups})
    if seeds != sorted([0, 1, 42, 424, 20260807, 20260809, 8675309, 325322]):
        raise ValueError('Survey seed protocol mismatch')
    expected = {'macro': 16641, 'local': 16641, 'axis_x': 12300, 'axis_z': 12300}
    if any(len(groups[seed, kind]) != count for seed in seeds
           for kind, count in expected.items()):
        raise ValueError('Survey sampling protocol incomplete')
    versions = {r['version'] for rows in groups.values() for r in rows}
    if len(versions) != 1:
        raise ValueError('Mixed terrain versions')
    results = {'schema': 1, 'version': versions.pop(), 'seeds': {},
               'protocol': {'macro_extent': 2048, 'macro_step': 32,
                            'local_extent': 64, 'boundary_extent': 512},
               'sample_csv_sha256': hashlib.sha256(
                   (directory / 'samples.csv').read_bytes()).hexdigest()}
    for seed in seeds:
        macro = groups[seed, 'macro']
        local = groups[seed, 'local']
        quadrants = {}
        for sx, sz, name in ((1, 1, '++'), (-1, 1, '-+'),
                             (1, -1, '+-'), (-1, -1, '--')):
            quadrants[name] = summarize([r for r in macro
                if r['x'] * sx > 0 and r['z'] * sz > 0])
        axes = groups[seed, 'axis_x'] + groups[seed, 'axis_z']
        results['seeds'][str(seed)] = {
            'surface_fingerprint': fingerprints[seed].hexdigest(),
            'macro': summarize(macro), 'quadrants': quadrants,
            'local': summarize(local), 'boundaries': summarize(axes),
            'macro_cap_component_samples': largest_component({
                (r['x'] // 32, r['z'] // 32) for r in macro if r['height'] == 176}),
            'local_cap_component_blocks': largest_component({
                (r['x'], r['z']) for r in local if r['height'] == 176}),
        }
    results['aggregate'] = summarize([r for seed in seeds for r in groups[seed, 'macro']])
    with (directory / 'chunks.csv').open(newline='') as stream:
        results['chunks'] = [{k: int(v) for k, v in row.items()}
                             for row in csv.DictReader(stream)]
    with (directory / 'timings.csv').open(newline='') as stream:
        results['timings'] = [{k: float(v) for k, v in row.items()}
                              for row in csv.DictReader(stream)]
    (directory / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    if plot:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, axs = plt.subplots(2, 4, figsize=(16, 8), constrained_layout=True)
        for ax, seed in zip(axs.flat, seeds):
            rows = groups[seed, 'macro']
            grid = [[0] * 129 for _ in range(129)]
            for row in rows:
                grid[(row['z'] + 2048) // 32][(row['x'] + 2048) // 32] = row['height']
            im = ax.imshow(grid, origin='lower', extent=(-2048, 2048, -2048, 2048),
                           vmin=20, vmax=176, cmap='terrain', interpolation='nearest')
            ax.axhline(0, color='black', linewidth=.4)
            ax.axvline(0, color='black', linewidth=.4)
            ax.set_title(f'Seed {seed}')
            ax.set_xlabel('World X'); ax.set_ylabel('World Z')
        fig.colorbar(im, ax=axs, label='Surface height (blocks)', shrink=.8)
        fig.suptitle(f"Terrain v{results['version']} — fixed production samples (step 32)")
        fig.savefig(directory / 'height-maps.png', dpi=150)
        plt.close(fig)
    return results


def self_test():
    assert largest_component({(0, 0), (1, 0), (4, 4)}) == 2
    assert largest_component(set()) == 0
    assert largest_component({(0, 0), (1, 1)}) == 1
    assert percentile([9, 1, 2, 3], .5) == 2
    example = summarize([{'height': 63, 'dx': -2, 'dz': 0, 'biome': 0},
                         {'height': 176, 'dx': 0, 'dz': 1, 'biome': 5}])
    assert example['water_fraction'] == .5 and example['slope_max'] == 2
    print('[TERRAIN_ANALYSIS] self-test=PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, nargs='?')
    parser.add_argument('--plot', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    if args.directory:
        summary = analyze(args.directory, args.plot)
        print(json.dumps({'version': summary['version'],
                          'aggregate': summary['aggregate']}, indent=2))
    elif not args.self_test:
        parser.error('Provide a survey directory or --self-test')
