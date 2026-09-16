#!/usr/bin/env python3
"""Validate E4 production surveys without reimplementing terrain generation."""

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


def compare_surveys(baseline, candidate):
    checks = {}
    for version in range(1, 10):
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

    old_path = baseline / 'v9' / 'samples.csv'
    new_path = candidate / 'v10' / 'samples.csv'
    rows = 0
    macro_eligible = 0
    macro_changed = 0
    macro_lifted = 0
    macro_lowered = 0
    macro_absolute_delta = 0
    unexpected_identity = 0
    label_changes = 0
    frozen_height_changes = 0
    excessive_delta = 0
    maximum_slope = 0
    with old_path.open(newline='') as old_stream, new_path.open(newline='') as new_stream:
        old_rows = csv.DictReader(old_stream)
        new_rows = csv.DictReader(new_stream)
        for old, new in zip(old_rows, new_rows):
            rows += 1
            if any(old[key] != new[key] for key in ('set', 'seed', 'x', 'z')):
                unexpected_identity += 1
            if old['biome'] != new['biome']:
                label_changes += 1
            old_height = int(old['height'])
            new_height = int(new['height'])
            delta = new_height - old_height
            if (old_height <= 80 or old_height >= 135) and delta:
                frozen_height_changes += 1
            if abs(delta) > 8:
                excessive_delta += 1
            maximum_slope = max(maximum_slope, abs(int(new['dx'])),
                                abs(int(new['dz'])))
            if old['set'] == 'macro' and 80 < old_height < 135:
                macro_eligible += 1
                if delta:
                    macro_changed += 1
                    macro_absolute_delta += abs(delta)
                    macro_lifted += delta > 0
                    macro_lowered += delta < 0
        extra_old = next(old_rows, None)
        extra_new = next(new_rows, None)

    shape_ok = (rows == 463056 and extra_old is None and extra_new is None and
                unexpected_identity == 0 and label_changes == 0 and
                frozen_height_changes == 0 and excessive_delta == 0 and
                maximum_slope <= 3)
    checks['v10-column-identity-labels-bands-and-slope'] = {
        'status': 'PASS' if shape_ok else 'FAIL',
        'rows': rows,
        'unexpected_identity': unexpected_identity,
        'label_changes': label_changes,
        'frozen_height_changes': frozen_height_changes,
        'excessive_delta': excessive_delta,
        'maximum_slope': maximum_slope,
        'raw_v10_sha256': digest(new_path),
    }
    changed_ratio = macro_changed / macro_eligible if macro_eligible else 0.0
    mean_absolute_delta = (macro_absolute_delta / macro_eligible
                           if macro_eligible else 0.0)
    lifted_ratio = macro_lifted / macro_eligible if macro_eligible else 0.0
    lowered_ratio = macro_lowered / macro_eligible if macro_eligible else 0.0
    metric_ok = (0.45 <= changed_ratio <= 0.90 and
                 2.0 <= mean_absolute_delta <= 5.5 and
                 lifted_ratio >= 0.15 and lowered_ratio >= 0.15)
    checks['v10-macro-relief-coverage'] = {
        'status': 'PASS' if metric_ok else 'FAIL',
        'eligible': macro_eligible,
        'changed': macro_changed,
        'changed_ratio': changed_ratio,
        'mean_absolute_delta': mean_absolute_delta,
        'lifted_ratio': lifted_ratio,
        'lowered_ratio': lowered_ratio,
    }

    with ((baseline / 'v9' / 'chunks.csv').open(newline='') as old_stream,
          (candidate / 'v10' / 'chunks.csv').open(newline='') as new_stream):
        pairs = list(zip(csv.DictReader(old_stream), csv.DictReader(new_stream)))
    changed_chunks = sum(a['block_hash'] != b['block_hash'] for a, b in pairs)
    checks['v10-generated-chunks-change'] = {
        'status': 'PASS' if len(pairs) == 32 and changed_chunks > 0 else 'FAIL',
        'chunks': len(pairs),
        'changed': changed_chunks,
        'raw_v10_sha256': digest(candidate / 'v10' / 'chunks.csv'),
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
        print('[E4_VALIDATOR] self-test=PASS')
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
