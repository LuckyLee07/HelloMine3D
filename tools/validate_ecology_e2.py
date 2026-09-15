#!/usr/bin/env python3
"""Aggregate E2 CSVs emitted by the production C++ generator; never generate terrain here."""

import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


def rows(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[int((len(ordered) - 1) * fraction)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--legacy', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output file; preserve earlier failures')

    checks = {}
    def check(name, passed, **details):
        checks[name] = {'status': 'PASS' if passed else 'FAIL', **details}

    legacy_result = json.loads((args.legacy / 'validation.json').read_text())
    check('v1-v5-frozen-production', legacy_result['result'] == 'PASS',
          source=str(args.legacy / 'validation.json'))
    v6_actual = args.legacy / 'v6/chunks.csv'
    v6_golden = Path(__file__).resolve().parent / 'fixtures/terrain/e1-v6-chunks.csv'
    check('v6-generated-chunks', v6_actual.read_bytes() == v6_golden.read_bytes(),
          actual_sha256=sha256(v6_actual), golden_sha256=sha256(v6_golden))
    for name in ('samples.csv', 'chunks.csv'):
        actual = args.legacy / 'v7' / name
        golden = args.baseline / 'v7-data' / name
        check('v7-frozen-' + name, actual.read_bytes() == golden.read_bytes(),
              actual_sha256=sha256(actual), golden_sha256=sha256(golden))
    v7_fixture = Path(__file__).resolve().parent / 'fixtures/terrain/e2-v7-chunks.csv'
    check('v7-committed-chunk-fixture',
          (args.legacy / 'v7/chunks.csv').read_bytes() == v7_fixture.read_bytes(),
          fixture_sha256=sha256(v7_fixture))

    survey_path = args.candidate / 'v8-survey/samples.csv'
    survey = rows(survey_path)
    heights = [int(row['height']) for row in survey]
    slopes = [max(abs(int(row['dx'])), abs(int(row['dz']))) for row in survey]
    check('v8-height-and-adjacent-slope',
          len(survey) == 463056 and all(row['version'] == '8' for row in survey) and
          min(heights) >= 1 and
          max(heights) <= 176 and max(slopes) <= 3,
          samples=len(survey), height_min=min(heights),
          height_max=max(heights), slope_max=max(slopes),
          over_three=sum(slope > 3 for slope in slopes),
          raw_sha256=sha256(survey_path))

    transect_path = args.candidate / 'v8-coasts/transects.csv'
    transects = defaultdict(dict)
    transect_rows = rows(transect_path)
    for row in transect_rows:
        transects[(row['seed'], row['transect'])][int(row['offset'])] = row
    dry_widths = []
    shallow_lengths = []
    transitions = []
    complete = True
    for samples in transects.values():
        complete = complete and set(samples) == set(range(-32, 33))
        if not complete:
            continue
        dry_widths.append(next((offset for offset in range(33)
            if not 64 <= int(samples[offset]['height']) <= 66), 33))
        shallow_lengths.append(next((distance - 1 for distance in range(1, 33)
            if not 60 <= int(samples[-distance]['height']) <= 63), 32))
        materials = [samples[offset]['planned_surface'] for offset in range(16)]
        transitions.append(sum(materials[index] != materials[index - 1]
            for index in range(1, 16)))
    valid_dry = [width for width in dry_widths if width >= 2]
    both = sum(dry >= 2 and shallow >= 2
               for dry, shallow in zip(dry_widths, shallow_lengths))
    bounded = sum(2 <= width <= 18 for width in valid_dry)
    p10 = percentile(valid_dry, .10) if valid_dry else None
    p90 = percentile(valid_dry, .90) if valid_dry else None
    check('v8-coast-shape', len(transects) == 96 and
          all(row['version'] == '8' for row in transect_rows) and complete and
          both / len(transects) >= .80 and bool(valid_dry) and
          bounded / len(valid_dry) >= .90 and p90 - p10 >= 4 and
          bool(transitions) and max(transitions) <= 3,
          transects=len(transects), complete=complete,
          dry_and_shallow_at_least_two=both,
          bounded_dry_widths=bounded, valid_dry_widths=len(valid_dry),
          dry_width_p10=p10, dry_width_p90=p90,
          max_surface_transitions_in_16=max(transitions) if transitions else None,
          raw_sha256=sha256(transect_path))

    blocks_path = args.candidate / 'v8-coasts/blocks.csv'
    blocks = rows(blocks_path)
    chunks = {(row['seed'], row['chunk_x'], row['chunk_z']) for row in blocks}
    hashes = defaultdict(set)
    for row in blocks:
        hashes[(row['seed'], row['chunk_x'], row['chunk_z'])].add(row['block_hash'])
    dry = [row for row in blocks if 64 <= int(row['height']) <= 68]
    inland = [row for row in blocks if 69 <= int(row['height']) <= 76]
    dry_sand = sum(row['top'] == '6' for row in dry)
    inland_soil = sum(row['top'] in ('1', '2') for row in inland)
    check('v8-generated-surface-materials',
          len(chunks) >= 32 and all(row['version'] == '8' for row in blocks) and
          all(len(value) == 1 for value in hashes.values()) and
          bool(dry) and bool(inland) and dry_sand / len(dry) >= .45 and
          inland_soil / len(inland) >= .55,
          generated_chunks=len(chunks), dry_columns=len(dry),
          dry_sand_fraction=dry_sand / len(dry) if dry else None,
          inland_columns=len(inland),
          inland_grass_or_dirt_fraction=inland_soil / len(inland) if inland else None,
          raw_sha256=sha256(blocks_path))

    v7_scenes_path = args.baseline / 'v7-scenes-final/chunks.csv'
    v8_scenes_path = args.candidate / 'v8-scenes/chunks.csv'
    v7_scenes = {(row['chunk_x'], row['chunk_z']): row
                 for row in rows(v7_scenes_path)}
    v8_scenes = rows(v8_scenes_path)
    preserved = 0
    changed = 0
    for row in v8_scenes:
        before = v7_scenes.get((row['chunk_x'], row['chunk_z']))
        if before is None:
            continue
        if row['scene'] == 'forest_edge':
            preserved += before['block_hash'] == row['block_hash']
        if row['scene'] == 'dry_shore':
            changed += before['block_hash'] != row['block_hash']
    check('v8-scene-scope', preserved == 9 and changed >= 1 and
          all(row['version'] == '8' for row in v8_scenes),
          preserved_forest_chunks=preserved, changed_dry_shore_chunks=changed,
          v7_raw_sha256=sha256(v7_scenes_path),
          v8_raw_sha256=sha256(v8_scenes_path))

    report = {
        'schema': 1, 'evidence_type': 'PRODUCTION_GENERATOR_AGGREGATION',
        'result': 'PASS' if all(value['status'] == 'PASS'
                                for value in checks.values()) else 'FAIL',
        'checks': checks,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return report['result'] != 'PASS'


if __name__ == '__main__':
    raise SystemExit(main())
