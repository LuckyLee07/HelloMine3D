#!/usr/bin/env python3
"""Validate E5 production surveys without reimplementing terrain generation."""

import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def compare_surveys(baseline, candidate):
    checks = {}
    for version in range(1, 11):
        for filename in ('samples.csv', 'chunks.csv'):
            old_path = baseline / f'v{version}' / filename
            new_path = candidate / f'v{version}' / filename
            old_hash = digest(old_path)
            new_hash = digest(new_path)
            checks[f'v{version}-{filename}-unchanged'] = {
                'status': 'PASS' if old_hash == new_hash else 'FAIL',
                'baseline_sha256': old_hash,
                'candidate_sha256': new_hash,
            }

    old_path = baseline / 'v10' / 'samples.csv'
    new_path = candidate / 'v11' / 'samples.csv'
    rows = 0
    macro_total = 0
    macro_eligible = 0
    macro_changed = 0
    macro_added_water = 0
    identity_changes = 0
    biome_changes = 0
    frozen_height_changes = 0
    raised_columns = 0
    excessive_drop = 0
    maximum_slope = 0
    per_seed = defaultdict(lambda: {
        'macro_total': 0, 'eligible': 0, 'changed': 0,
        'added_water': 0, 'deep_water': 0,
    })
    with old_path.open(newline='') as old_stream, new_path.open(newline='') as new_stream:
        old_rows = csv.DictReader(old_stream)
        new_rows = csv.DictReader(new_stream)
        for old, new in zip(old_rows, new_rows):
            rows += 1
            if any(old[key] != new[key] for key in ('set', 'seed', 'x', 'z')):
                identity_changes += 1
            if old['biome'] != new['biome']:
                biome_changes += 1
            old_height = int(old['height'])
            new_height = int(new['height'])
            delta = new_height - old_height
            if (old_height < 64 or old_height > 95) and delta:
                frozen_height_changes += 1
            raised_columns += delta > 0
            excessive_drop += delta < -36
            maximum_slope = max(maximum_slope, abs(int(new['dx'])),
                                abs(int(new['dz'])))
            if old['set'] != 'macro':
                continue
            seed = int(old['seed'])
            macro_total += 1
            per_seed[seed]['macro_total'] += 1
            if 64 <= old_height <= 95:
                macro_eligible += 1
                per_seed[seed]['eligible'] += 1
                if delta:
                    macro_changed += 1
                    per_seed[seed]['changed'] += 1
            if old_height >= 64 and new_height < 64:
                macro_added_water += 1
                per_seed[seed]['added_water'] += 1
            if old_height >= 64 and 2 <= 64 - new_height <= 5:
                per_seed[seed]['deep_water'] += 1
        extra_old = next(old_rows, None)
        extra_new = next(new_rows, None)

    shape_ok = (rows == 463056 and extra_old is None and extra_new is None and
                identity_changes == 0 and biome_changes == 0 and
                frozen_height_changes == 0 and raised_columns == 0 and
                excessive_drop == 0 and maximum_slope <= 3)
    checks['v11-column-identity-biomes-bands-and-slope'] = {
        'status': 'PASS' if shape_ok else 'FAIL',
        'rows': rows,
        'identity_changes': identity_changes,
        'biome_changes': biome_changes,
        'frozen_height_changes': frozen_height_changes,
        'raised_columns': raised_columns,
        'excessive_drop': excessive_drop,
        'maximum_slope': maximum_slope,
        'raw_v11_sha256': digest(new_path),
    }
    changed_ratio = macro_changed / macro_eligible if macro_eligible else 0.0
    water_ratio = macro_added_water / macro_total if macro_total else 0.0
    seeds_with_deep_water = sum(
        values['deep_water'] > 0 for values in per_seed.values())
    metric_ok = (0.03 <= changed_ratio <= 0.22 and
                 0.004 <= water_ratio <= 0.05 and
                 seeds_with_deep_water >= 6)
    checks['v11-macro-valley-and-water-coverage'] = {
        'status': 'PASS' if metric_ok else 'FAIL',
        'macro_total': macro_total,
        'eligible': macro_eligible,
        'changed': macro_changed,
        'changed_ratio': changed_ratio,
        'added_water': macro_added_water,
        'added_water_ratio': water_ratio,
        'seeds_with_two_to_five_deep_water': seeds_with_deep_water,
        'per_seed': dict(sorted(per_seed.items())),
    }

    with ((baseline / 'v10' / 'chunks.csv').open(newline='') as old_stream,
          (candidate / 'v11' / 'chunks.csv').open(newline='') as new_stream):
        old_chunks = list(csv.DictReader(old_stream))
        new_chunks = list(csv.DictReader(new_stream))
    chunk_identity = all(
        all(old[key] == new[key] for key in
            ('seed', 'chunk_x', 'chunk_z'))
        for old, new in zip(old_chunks, new_chunks))
    changed_chunks = sum(
        old['block_hash'] != new['block_hash']
        for old, new in zip(old_chunks, new_chunks))
    # The generic T0 chunk fixture is intentionally small and may miss a
    # sparse waterway. Generated changed chunks are exercised by the focused
    # E5 production test at dynamically discovered two-to-five-deep sites.
    checks['v11-generic-generated-chunk-survey-integrity'] = {
        'status': 'PASS' if (len(old_chunks) == len(new_chunks) == 32 and
                             chunk_identity) else 'FAIL',
        'chunks': len(new_chunks),
        'changed': changed_chunks,
        'raw_v11_sha256': digest(candidate / 'v11' / 'chunks.csv'),
    }
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        assert hashlib.sha256(b'abc').hexdigest() == (
            'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
        print('[E5_VALIDATOR] self-test=PASS')
    if args.baseline or args.candidate or args.output:
        if not all((args.baseline, args.candidate, args.output)):
            parser.error('--baseline, --candidate and --output are required together')
        if args.output.exists():
            parser.error('Choose a new output; failed results are immutable')
        checks = compare_surveys(args.baseline, args.candidate)
        result = {
            'schema': 1,
            'source': 'PRODUCTION_CPP_TERRAIN_SURVEY',
            'checks': checks,
            'result': 'PASS' if all(item['status'] == 'PASS'
                                    for item in checks.values()) else 'FAIL',
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result['result'] == 'PASS' else 1
    if not args.self_test:
        parser.error('Provide --self-test or a validation triplet')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
