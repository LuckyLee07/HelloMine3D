#!/usr/bin/env python3
"""Compare E4's frozen v9/v10 inland-relief Release protocol."""

import argparse
import json
import statistics
from pathlib import Path

from compare_ecology_e2_performance import read_run


GROUPS = (
    ('relief-steady', 'relief', False, '1152 150 64'),
    ('relief-streaming', 'relief', True, '1152 150 64'),
)
ROUND_ORDER = ((9, 10), (10, 9), (9, 10))


def compare(root):
    groups = {}
    all_runs = []
    for name, scene, streaming, position in GROUPS:
        versions = {9: [], 10: []}
        chronological = []
        for round_number, order in enumerate(ROUND_ORDER, 1):
            for version in order:
                run = read_run(root / f'{name}-r{round_number}-v{version}',
                               version, scene, streaming, position)
                versions[version].append(run)
                chronological.append(run)
                all_runs.append(run)
        if any(right['started_unix'] <= left['started_unix']
               for left, right in zip(chronological, chronological[1:])):
            raise ValueError(f'{name}: capture order differs from E4 protocol')
        metrics = {}
        for key in ('frame_p95_ms', 'frame_p99_ms'):
            baseline = [run[key] for run in versions[9]]
            candidate = [run[key] for run in versions[10]]
            old = statistics.median(baseline)
            new = statistics.median(candidate)
            metrics[key] = {
                'v9_runs': baseline,
                'v10_runs': candidate,
                'v9_median': old,
                'v10_median': new,
                'ratio': new / old if old else None,
                'status': ('PASS' if old > 0 and new <= old * 1.10
                           else 'FAIL'),
            }
        groups[name] = {
            'status': ('PASS' if all(metric['status'] == 'PASS'
                                     for metric in metrics.values())
                       else 'FAIL'),
            'frame_metrics': metrics,
            'runs': versions,
        }

    identities = {run['executable_sha256'] for run in all_runs}
    packages = {run['runtime_app'] for run in all_runs}
    settings = {run['settings'] for run in all_runs}
    methods = {run['launch_method'] for run in all_runs}
    fixtures = [run['world_fixture'] for run in all_runs]
    batch_ok = (methods == {'ONE_PROCESS_BATCH'} and
                len({run['batch_pid'] for run in all_runs}) == 1 and
                len({run['batch_manifest'] for run in all_runs}) == 1 and
                [run['batch_phase'] for run in all_runs] == list(range(12)))
    fixture_ok = all(
        fixture and fixture.get('difficulty_id') == 1 and
        fixture.get('initial_actor_count') == 0 and
        fixture.get('terrain_generation_version') == run['version']
        for fixture, run in zip(fixtures, all_runs)) and len({
            fixture['template_sha256'] for fixture in fixtures}) == 1
    order_ok = all(right['started_unix'] > left['started_unix']
                   for left, right in zip(all_runs, all_runs[1:]))
    identity_ok = (len(identities) == len(packages) == len(settings) == 1 and
                   batch_ok and fixture_ok)
    all_groups = all(result['status'] == 'PASS'
                     for result in groups.values())
    return {
        'schema': 1,
        'source': 'E4_RELEASE_CLIENT_CAPTURE',
        'guardrail': 'three-run median v10/v9 P95 and P99 <= 1.10 per group',
        'round_order': ROUND_ORDER,
        'identity_status': 'PASS' if identity_ok else 'FAIL',
        'batch_status': 'PASS' if batch_ok else 'FAIL',
        'world_fixture_status': 'PASS' if fixture_ok else 'FAIL',
        'order_status': 'PASS' if order_ok else 'FAIL',
        'executable_sha256': sorted(identities),
        'runtime_app': sorted(packages),
        'status': ('PASS' if identity_ok and order_ok and all_groups
                   else 'FAIL'),
        'groups': groups,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output file; preserve prior results')
    report = compare(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    for name, result in report['groups'].items():
        print(name, result['status'], ' '.join(
            f'{key}={value["ratio"]:.3f}x'
            for key, value in result['frame_metrics'].items()))
    print('[E4_PERF]', report['status'])
    return report['status'] != 'PASS'


if __name__ == '__main__':
    raise SystemExit(main())
