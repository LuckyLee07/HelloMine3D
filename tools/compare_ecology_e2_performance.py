#!/usr/bin/env python3
"""Compare E2's frozen four-scene, three-round Release capture protocol.

Only consumes the client's capture records, summaries and original frame CSVs.
"""

import argparse
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path


GROUPS = (('forest-steady', 'forest', False, '1024 140 1024'),
          ('forest-streaming', 'forest', True, '1024 140 1024'),
          ('shore-steady', 'shore', False, '0 82 512'),
          ('shore-streaming', 'shore', True, '0 82 512'))
ROUND_ORDER = ((7, 8), (8, 7), (7, 8))
RECHECK_ORDER = ((8, 7), (7, 8), (8, 7))
FRAME_LIMITS = (33, 50, 100)
SUMMARY_FIELDS = ('last_existing_chunks', 'last_loaded_chunks',
                  'last_solid_faces', 'last_resident_terrain_buffer_bytes',
                  'last_mesh_rebuilds', 'last_mesh_build_total_ms',
                  'last_mesh_build_avg_ms', 'last_queued_chunk_updates',
                  'chunk_visible_p95_ms', 'chunk_visible_p99_ms',
                  'stream_queue_peak', 'mesh_progress_completed',
                  'frame_max_ms')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_run(path, version, scene, streaming, position):
    record = json.loads((path / 'capture.json').read_text())
    if record['result'] != 'CAPTURED' or record['package_mode'] != 'REUSE_STABLE_APP':
        raise ValueError(f'{path}: capture did not complete in the reused client')
    if record['scene'] != scene or bool(record['environment'].get(
            'HELLOMINE3D_RC_PERF_PROFILE')) != streaming:
        raise ValueError(f'{path}: scene or streaming profile differs')
    environment = record['environment']
    for key, expected in {'HELLOMINE3D_SEED': '20260807',
                          'HELLOMINE3D_PLAYER_POSITION': position,
                          'HELLOMINE3D_PLAYER_ROTATION': '0 0 0',
                          'HELLOMINE3D_WORLD_TIME': '6000'}.items():
        if environment.get(key) != expected:
            raise ValueError(f'{path}: {key} differs')
    if record['settings'].find('directionalshadowquality high') < 0 or \
            record['settings'].find('postprocessingquality on') < 0 or \
            record['settings'].find('windowsize 1280 720') < 0:
        raise ValueError(f'{path}: visual quality differs')
    summary_path = path / 'performance/summary.txt'
    summary = dict(line.split('=', 1) for line in summary_path.read_text().splitlines()
                   if '=' in line)
    for key, expected in {'build_configuration': 'Release',
                          'terrain_generation_version': str(version),
                          'terrain_seed': '20260807',
                          'warmup_ms': '5000.000',
                          'duration_ms': '30000.000',
                          'startup_success': '1', 'entry_success': '1'}.items():
        if summary.get(key) != expected:
            raise ValueError(f'{path}: {key}={summary.get(key)} expected={expected}')
    frames_path = path / 'performance/frames.csv'
    with frames_path.open(newline='') as stream:
        frames = list(csv.DictReader(stream))
    if not frames or len(frames) != int(summary['frames']):
        raise ValueError(f'{path}: frame count differs from summary')
    values = [float(frame['frame_ms']) for frame in frames]
    ordered = sorted(values)
    for percentile in (95, 99):
        key = f'frame_p{percentile}_ms'
        from_frames = ordered[math.ceil((percentile / 100) * (len(ordered) - 1))]
        if abs(from_frames - float(summary[key])) > .001:
            raise ValueError(f'{path}: {key} differs from original frames')
    for threshold, key in ((33.333, 'frames_over_33ms'),
                           (50, 'frames_over_50ms')):
        if sum(value > threshold for value in values) != int(summary[key]):
            raise ValueError(f'{path}: {key} differs from original frames')
    return {'path': str(path), 'version': version,
            'started_unix': record['started_unix'],
            'executable_sha256': record['package_identity']['executable_sha256'],
            'runtime_app': record['runtime_app'], 'settings': record['settings'],
            'summary_sha256': digest(summary_path), 'frames_sha256': digest(frames_path),
            'frame_count': len(frames),
            'frame_p95_ms': float(summary['frame_p95_ms']),
            'frame_p99_ms': float(summary['frame_p99_ms']),
            'frames_over': {str(limit): sum(value > (33.333 if limit == 33 else limit)
                                                 for value in values)
                            for limit in FRAME_LIMITS},
            'summary': {key: float(summary[key]) for key in SUMMARY_FIELDS
                        if key in summary},
            'resident_buffer_max_bytes': max(int(frame['resident_terrain_buffer_bytes'])
                                             for frame in frames),
            'mesh_rebuilds_delta': int(frames[-1]['mesh_rebuilds']) -
                                   int(frames[0]['mesh_rebuilds']),
            'cpu_ready_max': max(int(frame['cpu_ready_sections']) for frame in frames)}


def compare(root, round_order=ROUND_ORDER):
    groups = {}
    all_runs = []
    for name, scene, streaming, position in GROUPS:
        versions = {7: [], 8: []}
        chronological = []
        for round_number, order in enumerate(round_order, 1):
            for version in order:
                run = read_run(root / f'{name}-r{round_number}-v{version}',
                               version, scene, streaming, position)
                versions[version].append(run)
                chronological.append(run)
                all_runs.append(run)
        if any(right['started_unix'] <= left['started_unix']
               for left, right in zip(chronological, chronological[1:])):
            raise ValueError(f'{name}: capture order differs from frozen protocol')
        metrics = {}
        for key in ('frame_p95_ms', 'frame_p99_ms'):
            baseline = [run[key] for run in versions[7]]
            candidate = [run[key] for run in versions[8]]
            old = statistics.median(baseline)
            new = statistics.median(candidate)
            metrics[key] = {'v7_runs': baseline, 'v8_runs': candidate,
                            'v7_median': old, 'v8_median': new,
                            'ratio': new / old if old else None,
                            'status': 'PASS' if old > 0 and new <= old * 1.10
                            else 'FAIL'}
        groups[name] = {'status': 'PASS' if all(value['status'] == 'PASS'
                                              for value in metrics.values())
                        else 'FAIL', 'frame_metrics': metrics,
                        'runs': versions}
    identities = {run['executable_sha256'] for run in all_runs}
    packages = {run['runtime_app'] for run in all_runs}
    settings = {run['settings'] for run in all_runs}
    all_groups = [groups[name]['status'] == 'PASS' for name, *_ in GROUPS]
    order_ok = all(right['started_unix'] > left['started_unix']
                   for left, right in zip(all_runs, all_runs[1:]))
    identity_ok = len(identities) == len(packages) == len(settings) == 1
    return {'schema': 1, 'source': 'E2_RELEASE_CLIENT_CAPTURE',
            'guardrail': 'three-run median v8/v7 P95 and P99 <= 1.10 per group',
            'round_order': round_order,
            'identity_status': 'PASS' if identity_ok else 'FAIL',
            'order_status': 'PASS' if order_ok else 'FAIL',
            'executable_sha256': sorted(identities),
            'runtime_app': sorted(packages),
            'status': 'PASS' if identity_ok and order_ok and all(all_groups) else 'FAIL',
            'groups': groups}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reverse-order', action='store_true',
                        help='Validate the predeclared second-pass order')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Choose a new output file; preserve prior results')
    report = compare(args.root, RECHECK_ORDER if args.reverse_order else ROUND_ORDER)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    for name, result in report['groups'].items():
        print(name, result['status'], ' '.join(
            f'{key}={value["ratio"]:.3f}x'
            for key, value in result['frame_metrics'].items()))
    print('[E2_PERF]', report['status'])
    return report['status'] != 'PASS'


if __name__ == '__main__':
    raise SystemExit(main())
