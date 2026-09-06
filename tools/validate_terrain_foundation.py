#!/usr/bin/env python3
"""Validate frozen T0/T1 statistical gates and old-version survey fingerprints."""
import argparse
import copy
import json
from pathlib import Path

from analyze_terrain_survey import analyze

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / 'tools/fixtures/terrain/t0-v1-v4.json'
FOUNDATION_GOLDEN = ROOT / 'tools/fixtures/terrain/t1-v5.json'


def identity_errors(summaries, golden, versions):
    errors = []
    for version in versions:
        actual = summaries[str(version)]
        expected = golden['versions'][str(version)]
        hashes = {k: s['surface_fingerprint'] for k, s in actual['seeds'].items()}
        if actual['version'] != version or hashes != expected['surface_fingerprints']:
            errors.append(f'v{version}: surface output drift')
        if actual['chunks'] != expected['chunks']:
            errors.append(f'v{version}: generated block/metadata/entity drift')
    return errors


def legacy_errors(summaries, golden):
    return identity_errors(summaries, golden, range(1, 5))


def foundation_errors(data, baseline):
    errors = []
    def require(condition, message):
        if not condition:
            errors.append(message)
    require(data['version'] == 5, 'expected terrain v5')
    aggregate = data['aggregate']
    require(aggregate['height_min'] <= 56 and aggregate['height_max'] >= 140,
            'aggregate landform height range')
    require(aggregate['slope_p99'] <= 2 and
            aggregate['step_at_most_one_fraction'] >= .95, 'aggregate slope distribution')
    for biome in range(6):
        require(aggregate['biomes'].get(str(biome), aggregate['biomes'].get(biome, 0)) /
                aggregate['samples'] >= .01, f'biome {biome}: aggregate share below 1%')
    require(aggregate['cap_176_fraction'] <= .001, 'aggregate cap frequency')
    maximum_cap = max(s['macro_cap_component_samples'] for s in baseline['seeds'].values())
    for seed, result in data['seeds'].items():
        require(result['macro_cap_component_samples'] <= maximum_cap, f'{seed}: cap component')
        for group in ('macro', 'local', 'boundaries'):
            stats = result[group]
            require(1 <= stats['height_min'] <= stats['height_max'] <= 176,
                    f'{seed}/{group}: height safety range')
        for group in ('local', 'boundaries'):
            require(result[group]['slope_max'] <= 3, f'{seed}/{group}: adjacent height step')
        for quadrant, stats in result['quadrants'].items():
            label = f'{seed}/{quadrant}'
            require(stats['height_stddev'] >= 8 and stats['height_unique'] >= 32,
                    label + ': height diversity')
            require(stats['height_63_fraction'] <= .10, label + ': constant sea shelf')
            require(stats['height_min'] < 64 and stats['height_max'] >= 100,
                    label + ': lowland/highland coverage')
            require(len(stats['biomes']) >= 4 and max(stats['biomes'].values()) /
                    stats['samples'] < .90, label + ': biome diversity')
    return errors


def self_test():
    # Corrupt a real baseline-shaped record: validator must detect a plateau,
    # a cliff and a removed biome, independent of the candidate implementation.
    fixture = json.loads(GOLDEN.read_text())
    summaries = {v: {'version': int(v), 'seeds': {s: {'surface_fingerprint': h}
                     for s, h in d['surface_fingerprints'].items()}, 'chunks': d['chunks']}
                 for v, d in fixture['versions'].items()}
    assert not legacy_errors(summaries, fixture)
    broken = copy.deepcopy(summaries)
    broken['4']['chunks'][0]['block_hash'] ^= 1
    assert legacy_errors(broken, fixture) == ['v4: generated block/metadata/entity drift']
    broken = copy.deepcopy(summaries)
    broken['1']['seeds']['0']['surface_fingerprint'] = '0' * 64
    assert legacy_errors(broken, fixture) == ['v1: surface output drift']
    foundation = json.loads(FOUNDATION_GOLDEN.read_text())
    frozen = foundation['versions']['5']
    current = {'5': {'version': 5, 'seeds': {
        seed: {'surface_fingerprint': digest}
        for seed, digest in frozen['surface_fingerprints'].items()},
        'chunks': copy.deepcopy(frozen['chunks'])}}
    assert not identity_errors(current, foundation, (5,))
    current['5']['chunks'][0]['block_hash'] ^= 1
    assert identity_errors(current, foundation, (5,)) == [
        'v5: generated block/metadata/entity drift']
    baseline = json.loads((ROOT / 'docs/reports/terrain-t0-t1-evidence/baseline-v4-summary.json').read_text())
    errors = foundation_errors(baseline, baseline)
    assert 'expected terrain v5' in errors
    assert any('constant sea shelf' in e for e in errors)
    assert any('adjacent height step' in e for e in errors)
    broken = copy.deepcopy(baseline)
    broken['aggregate']['biomes'].pop('3')
    assert 'biome 3: aggregate share below 1%' in foundation_errors(broken, baseline)
    print('[TERRAIN_FOUNDATION] self-test=PASS')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--survey-root', type=Path,
                        help='Contains v1/ .. v5/ production survey directories')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    if args.survey_root:
        summaries = {str(v): analyze(args.survey_root / f'v{v}') for v in range(1, 6)}
        baseline = json.loads((ROOT / 'docs/reports/terrain-t0-t1-evidence/baseline-v4-summary.json').read_text())
        errors = legacy_errors(summaries, json.loads(GOLDEN.read_text()))
        errors += identity_errors(summaries,
                                  json.loads(FOUNDATION_GOLDEN.read_text()), (5,))
        errors += foundation_errors(summaries['5'], baseline)
        result = {'schema': 1, 'result': 'FAIL' if errors else 'PASS', 'errors': errors,
                  'scope': 'statistical gates and v1-v5 generated snapshots; not gameplay'}
        (args.survey_root / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return bool(errors)
    if not args.self_test:
        parser.error('Provide --survey-root or --self-test')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
