#!/usr/bin/env python3
"""Compare E3 production-generator CSV with frozen E2 outputs; no terrain simulation."""

import argparse
import csv
import hashlib
import json
from pathlib import Path


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output; do not replace a failed result')

    checks = {}
    for version in range(1, 9):
        old = (args.baseline / 'legacy' / f'v{version}' if version <= 7
               else args.baseline / 'v8-survey')
        new = args.candidate / f'v{version}'
        for filename in ('samples.csv', 'chunks.csv'):
            old_path = old / filename
            new_path = new / filename
            old_hash = digest(old_path)
            new_hash = digest(new_path)
            checks[f'v{version}-{filename}-unchanged'] = {
                'status': 'PASS' if old_hash == new_hash else 'FAIL',
                'baseline_sha256': old_hash,
                'candidate_sha256': new_hash,
                'baseline': str(old_path),
                'candidate': str(new_path),
            }

    old_path = args.baseline / 'v8-survey' / 'samples.csv'
    new_path = args.candidate / 'v9' / 'samples.csv'
    forest = 0
    meadow = 0
    unexpected = 0
    height_changes = 0
    rows = 0
    slope_max = 0
    with old_path.open(newline='') as old_stream, new_path.open(newline='') as new_stream:
        old_samples = csv.DictReader(old_stream)
        new_samples = csv.DictReader(new_stream)
        for old, new in zip(old_samples, new_samples):
            rows += 1
            same_position = all(old[key] == new[key]
                                for key in ('set', 'seed', 'x', 'z'))
            if not same_position:
                unexpected += 1
            if old['height'] != new['height']:
                height_changes += 1
            if old['biome'] in ('2', '3') and 80 < int(old['height']) < 135:
                forest += 1
                if new['biome'] == '1':
                    meadow += 1
                elif new['biome'] != old['biome']:
                    unexpected += 1
            elif new['biome'] != old['biome']:
                unexpected += 1
            slope_max = max(slope_max, abs(int(new['dx'])), abs(int(new['dz'])))
        extra_old = next(old_samples, None)
        extra_new = next(new_samples, None)
    checks['v9-height-and-unaffected-biomes'] = {
        'status': 'PASS' if rows == 463056 and extra_old is None and
                  extra_new is None and height_changes == 0 and
                  unexpected == 0 and slope_max <= 3 else 'FAIL',
        'rows': rows, 'height_changes': height_changes,
        'unexpected_changes': unexpected, 'slope_max': slope_max,
        'raw_v9_sha256': digest(new_path),
    }
    ratio = meadow / forest if forest else 0
    checks['v9-inland-meadow-coverage'] = {
        'status': 'PASS' if forest and .05 <= ratio <= .30 else 'FAIL',
        'forest_samples': forest, 'meadow_samples': meadow, 'ratio': ratio,
    }

    old_chunks = args.baseline / 'v8-survey' / 'chunks.csv'
    new_chunks = args.candidate / 'v9' / 'chunks.csv'
    with old_chunks.open(newline='') as old_stream, new_chunks.open(newline='') as new_stream:
        pairs = list(zip(csv.DictReader(old_stream), csv.DictReader(new_stream)))
    changed = sum(old['block_hash'] != new['block_hash'] for old, new in pairs)
    checks['v9-generated-chunks-change'] = {
        'status': 'PASS' if len(pairs) == 32 and changed > 0 else 'FAIL',
        'chunks': len(pairs), 'changed': changed,
        'raw_v9_sha256': digest(new_chunks),
    }

    result = {'schema': 1, 'source': 'PRODUCTION_CPP_TERRAIN_SURVEY',
              'checks': checks,
              'result': 'PASS' if all(item['status'] == 'PASS'
                                    for item in checks.values()) else 'FAIL'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
