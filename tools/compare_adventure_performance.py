#!/usr/bin/env python3
"""Audit frozen adventure client captures, retaining every paired run."""
import argparse
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_run(directory, protocol, scene, version):
    record = json.loads((directory / 'capture.json').read_text())
    assert record['result'] == 'CAPTURED', directory
    assert not record['render_readback'] and not record['normal_input']
    assert record['window_mode'] == 'hidden'
    assert not record.get('inherited_diagnostic_environment'), directory
    settings = protocol['settings']
    assert record['framebuffer_size_pixels'] == [
        settings['width'] * settings['pixel_ratio'],
        settings['height'] * settings['pixel_ratio']]
    expected_settings = {
        'fov': settings['fov'], 'directionalshadowquality': settings['shadow'],
        'postprocessingquality': settings['post'], 'visualdetail': settings['visual_detail'],
        'windowsize': f"{settings['width']} {settings['height']}",
        'renderdistance': settings['render_distance']}
    actual_settings = dict(line.split(' ', 1) for line in record['settings'].splitlines())
    assert all(actual_settings[k] == str(v) for k, v in expected_settings.items())
    env = record['environment']
    assert env['HELLOMINE3D_PLAYER_POSITION'] == scene.get(
        'baseline_position' if version == 15 else 'candidate_position', scene['position'])
    assert env['HELLOMINE3D_PLAYER_ROTATION'] == scene['rotation']
    assert env['HELLOMINE3D_SEED'] == str(protocol['seed'])
    assert env['HELLOMINE3D_WORLD_TIME'] == str(settings['world_time'])
    assert bool(env.get('HELLOMINE3D_RC_PERF_PROFILE')) == scene['streaming']
    identity = record['package_identity']['executable_sha256']
    expected_hash = protocol['baseline_executable_sha256' if version == 15
                             else 'candidate_executable_sha256']
    assert identity == expected_hash
    resource_key = ('baseline_resource_manifest_sha256' if version == 15
                    else 'candidate_resource_manifest_sha256')
    if resource_key in protocol:
        assert record['package_identity']['resource_manifest_sha256'] == protocol[resource_key]
    summary_path = directory / 'performance/summary.txt'
    summary = dict(line.split('=', 1) for line in summary_path.read_text().splitlines()
                   if '=' in line)
    terrain_version = protocol.get('generation_versions', {}).get(str(version), version)
    expected = {'build_configuration': 'Release', 'terrain_generation_version': str(terrain_version),
                'terrain_seed': str(protocol['seed']), 'difficulty_id': '1',
                'entry_success': '1', 'startup_success': '1'}
    assert all(summary.get(k) == v for k, v in expected.items()), (directory, expected)
    for key in ('warmup_ms', 'duration_ms'):
        assert float(summary[key]) == protocol[key]
    frame_path = directory / 'performance/frames.csv'
    with frame_path.open() as stream:
        frames = list(csv.DictReader(stream))
    assert len(frames) == int(summary['frames']) and len(frames) >= 100
    times = sorted(float(row['frame_ms']) for row in frames)
    result = {}
    for p in (95, 99):
        key = f'frame_p{p}_ms'
        measured = times[math.ceil(p / 100 * (len(times) - 1))]
        assert abs(measured - float(summary[key])) <= .0011
        result[key] = float(summary[key])
    for key in ('update_p95_ms', 'render_p95_ms', 'last_existing_chunks',
                'last_loaded_chunks', 'last_solid_faces', 'last_water_faces',
                'last_flora_faces', 'last_resident_terrain_buffer_bytes',
                'last_mesh_build_total_ms', 'last_mesh_build_avg_ms',
                'last_mesh_rebuilds', 'frame_max_ms'):
        result[key] = float(summary[key])
    result.update(path=str(directory), started_unix=record['started_unix'],
                  frames=len(frames), executable_sha256=identity,
                  frames_sha256=digest(frame_path), summary_sha256=digest(summary_path),
                  peak_process_rss_bytes=record.get('peak_child_rss_bytes'),
                  resident_buffer_peak_bytes=max(int(f['resident_terrain_buffer_bytes']) for f in frames))
    return result


def compare(root):
    protocol = json.loads((root / 'protocol.json').read_text())
    groups = {}
    chronological = []
    for scene in protocol['scenes']:
        versions = {15: [], 19: []}
        for number, order in enumerate(protocol['round_order'], 1):
            for version in order:
                run = read_run(root / f"{scene['name']}-r{number}-v{version}",
                               protocol, scene, version)
                versions[version].append(run)
                chronological.append(run['started_unix'])
        metrics = {}
        for key in ('frame_p95_ms', 'frame_p99_ms'):
            old = statistics.median(r[key] for r in versions[15])
            new = statistics.median(r[key] for r in versions[19])
            metrics[key] = dict(baseline_median=old, candidate_median=new,
                                ratio=new / old,
                                status='PASS' if new <= old * protocol['guardrail'] else 'FAIL')
        groups[scene['name']] = dict(runs=versions, metrics=metrics,
            status='PASS' if all(m['status'] == 'PASS' for m in metrics.values()) else 'FAIL')
    assert chronological == sorted(chronological) and len(set(chronological)) == len(chronological)
    return dict(schema=1, protocol=protocol, identity_status='PASS', groups=groups,
                status='PASS' if all(g['status'] == 'PASS' for g in groups.values()) else 'FAIL')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Preserve previous reports; choose a new output')
    report = compare(args.root)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for name, group in report['groups'].items():
        print(name, group['status'], group['metrics'])
    raise SystemExit(report['status'] != 'PASS')
