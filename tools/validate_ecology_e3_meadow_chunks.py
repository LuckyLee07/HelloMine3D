#!/usr/bin/env python3
"""Audit fixed E3 meadow chunks exported by the production C++ generator."""

import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def read_csv(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frozen-sites', type=Path, required=True)
    parser.add_argument('--adjudication', type=Path, required=True)
    parser.add_argument('--original-debug', type=Path, required=True)
    parser.add_argument('--debug', type=Path, required=True)
    parser.add_argument('--release', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output; preserve failures')
    frozen = json.loads(args.frozen_sites.read_text())
    adjudication = json.loads(args.adjudication.read_text())
    checks = {}
    def check(name, condition, detail):
        checks[name] = {'status': 'PASS' if condition else 'FAIL',
                        'detail': detail}

    check('original-freeze-unchanged',
          digest(args.frozen_sites) ==
          adjudication['original_frozen_sites_sha256'],
          {'sha256': digest(args.frozen_sites)})
    original_sites = args.original_debug / 'sites.csv'
    original_columns = args.original_debug / 'columns.csv'
    initial_rows = read_csv(original_sites)
    initial_failures = sum(int(row['v9_converted_ground_non_grass'])
                           for row in initial_rows)
    check('original-fail-preserved',
          digest(original_sites) ==
          adjudication['original_debug_sites_sha256'] and
          digest(original_columns) ==
          adjudication['original_debug_columns_sha256'] and
          initial_failures == adjudication['original_non_grass_columns'] == 52,
          {'initial_non_grass': initial_failures,
           'original_predicate_status': 'FAIL'})
    file_hashes = {}
    for name in ('sites.csv', 'columns.csv', 'plans.csv'):
        debug_hash = digest(args.debug / name)
        release_hash = digest(args.release / name)
        file_hashes[name] = {'debug': debug_hash, 'release': release_hash}
        check(name + '-debug-release-identical',
              debug_hash == release_hash ==
              adjudication['production_debug_r2_sha256'][name.replace('.', '_')],
              file_hashes[name])

    sites = read_csv(args.debug / 'sites.csv')
    columns = read_csv(args.debug / 'columns.csv')
    plans = read_csv(args.debug / 'plans.csv')
    check('frozen-site-identity',
          len(sites) == len(frozen['sites']) == 16 and all(
              int(row['site']) == index and
              int(row['seed']) == frozen['sites'][index]['seed'] and
              row['sign'] == frozen['sites'][index]['sign'] and
              int(row['chunk_x']) == frozen['sites'][index]['chunk_x'] and
              int(row['chunk_z']) == frozen['sites'][index]['chunk_z'] and
              int(row['sample_x']) == frozen['sites'][index]['x'] and
              int(row['sample_z']) == frozen['sites'][index]['z']
              for index, row in enumerate(sites)),
          {'sites': len(sites)})
    changed = sum(row['v8_block_hash'] != row['v9_block_hash']
                  for row in sites)
    check('target-generated-chunks-changed', changed >= 12,
          {'changed': changed, 'frozen_minimum': 12})
    per_site = defaultdict(list)
    for row in columns:
        per_site[int(row['site'])].append(row)
    check('raw-column-coverage',
          len(columns) == 16 * 256 and
          all(len(per_site[index]) == 256 for index in range(16)),
          {'columns': len(columns),
           'per_site': {str(index): len(per_site[index])
                        for index in range(16)}})
    exact_coordinates = all(
        {(int(row['x']), int(row['z'])) for row in per_site[index]} ==
        {(site['x'] + dx, site['z'] + dz)
         for dx in range(16) for dz in range(16)} and
        all(int(row['seed']) == site['seed'] and
            row['sign'] == site['sign'] and
            int(row['chunk_x']) == site['chunk_x'] and
            int(row['chunk_z']) == site['chunk_z']
            for row in per_site[index])
        for index, site in enumerate(frozen['sites']))
    check('raw-column-site-and-world-XZ-identity', exact_coordinates,
          {'complete_chunks': 16 if exact_coordinates else 'FAIL'})
    height_changes = sum(row['v8_height'] != row['v9_height']
                         for row in columns)
    converted_labels = all(
        row['v8_biome'] in ('2', '3') and row['v9_biome'] == '1' and
        80 < int(row['v8_height']) < 135
        for row in columns if row['converted'] == '1')
    check('converted-columns-are-inland-forest-to-meadow',
          converted_labels, {'converted': sum(row['converted'] == '1'
                                            for row in columns)})
    planned_non_grass = sum(
        row['v9_surface'] != '1' for row in columns if row['converted'] == '1')
    final_non_grass = [row for row in columns if row['converted'] == '1' and
                       row['v9_top'] != '1']
    unexplained = [row for row in final_non_grass if
                   row['v9_raider_camp_cover'] != '1' or row['v9_top'] != '2']
    converted_roots = sum(row['v9_above'] == '4' for row in columns if
                          row['converted'] == '1')
    check('v8-v9-target-height-stable', height_changes == 0,
          {'height_changes': height_changes})
    check('planned-meadow-grass-and-no-trunk',
          planned_non_grass == 0 and converted_roots == 0,
          {'planned_non_grass': planned_non_grass,
           'converted_trunk_roots': converted_roots})
    check('final-ground-overrides-explained-by-camp',
          len(final_non_grass) == 52 and not unexplained,
          {'raw_non_grass': len(final_non_grass),
           'unexplained': len(unexplained),
           'by_site': dict(Counter(row['site'] for row in final_non_grass))})
    matching_summary = all(
        int(site['converted_columns']) == sum(row['converted'] == '1'
                                               for row in per_site[index]) and
        int(site['v9_converted_ground_non_grass']) == sum(
            row['converted'] == '1' and row['v9_top'] != '1'
            for row in per_site[index]) and
        int(site['v9_converted_non_grass_outside_camp']) == sum(
            row['converted'] == '1' and row['v9_top'] != '1' and
            row['v9_raider_camp_cover'] != '1'
            for row in per_site[index]) and
        int(site['v9_converted_camp_dirt']) == sum(
            row['converted'] == '1' and row['v9_top'] == '2' and
            row['v9_raider_camp_cover'] == '1'
            for row in per_site[index])
        for index, site in enumerate(sites))
    check('site-summary-matches-raw-columns', matching_summary,
          {'converted_columns': sum(int(row['converted_columns'])
                                    for row in sites),
           'camp_dirt': sum(int(row['v9_converted_camp_dirt'])
                            for row in sites)})
    camp_plans = [row for row in plans if row['version'] == '9' and
                  row['type'] == '2']
    expected_anchors = {(str(item['site']), str(item['seed']),
                         str(item['anchor_x']), str(item['anchor_y']),
                         str(item['anchor_z']))
                        for item in adjudication['v9_raider_camp_plans']}
    actual_anchors = {(row['site'], row['seed'], row['anchor_x'],
                       row['anchor_y'], row['anchor_z'])
                      for row in camp_plans}
    check('production-camp-plans-match-adjudication',
          actual_anchors == expected_anchors,
          {'anchors': sorted(actual_anchors)})
    result = {
        'schema': 1, 'source': 'PRODUCTION_CPP_GENERATED_CHUNKS',
        'classification': 'TARGETED_E3_AUTOMATIC_EVIDENCE_NOT_WINDOW_ACCEPTANCE',
        'original_frozen_predicate': 'FAIL_PRESERVED_AND_EXPLAINED',
        'file_hashes': file_hashes, 'checks': checks,
        'result': 'PASS' if all(item['status'] == 'PASS'
                                for item in checks.values()) else 'FAIL',
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(result['result'], 'changed_chunks', changed,
          'raw_camp_dirt', len(final_non_grass),
          'unexplained_non_grass', len(unexplained))
    return 0 if result['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
