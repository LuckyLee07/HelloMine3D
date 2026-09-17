#!/usr/bin/env python3
"""Capture E2/E3/E4 ecology phases inside one reused macOS game process."""

import argparse
import csv
import hashlib
import json
import math
import platform
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

from capture_visual_macos import verified_package_entries


E2_ROOT = Path(__file__).resolve().parents[1] / 'build/ecology-e2-20260914'
V7_TEMPLATE = E2_ROOT / 'baseline/v7-world-template'
VISUALS = (
    ('dry_shore', 'shore', '0 82 512', '0 0 0'),
    ('shallow', 'shore', '0 78 480', '0 0 0'),
    ('grass_shore', 'shore', '128 84 672', '0 0 0'),
    ('forest_edge', 'forest', '1024 140 1024', '0 0 0'),
    ('grass_sand', 'shore', '112 86 544', '0 0 0'),
    ('grass_shore_inland', 'shore', '128 80 640', '0 180 0'),
)
GROUPS = (
    ('forest-steady', 'forest', '1024 140 1024', False),
    ('forest-streaming', 'forest', '1024 140 1024', True),
    ('shore-steady', 'shore', '0 82 512', False),
    ('shore-streaming', 'shore', '0 82 512', True),
)
E3_VISUALS = (
    ('meadow_positive', 'meadow', '1536 120 800', '0 0 0'),
    ('meadow_negative', 'meadow', '-896 124 -800', '0 0 0'),
)
E3_GROUPS = (
    ('meadow-steady', 'meadow', '1536 120 800', False),
    ('meadow-streaming', 'meadow', '1536 120 800', True),
)
E4_VISUALS = (
    ('relief_positive', 'relief', '1152 150 64', '0 0 0'),
    ('relief_negative', 'relief', '-960 180 -1088', '0 0 0'),
)
E4_GROUPS = (
    ('relief-steady', 'relief', '1152 150 64', False),
    ('relief-streaming', 'relief', '1152 150 64', True),
)
E6_VISUALS = (
    ('forest_positive', 'forest', '168 108 552', '0 0 0'),
    ('forest_edge_negative', 'forest', '-120 115 -440', '0 0 0'),
)
E6_GROUPS = (
    ('forest-positive-steady', 'forest', '168 108 552', False),
    ('forest-positive-streaming', 'forest', '168 108 552', True),
    ('forest-edge-negative-steady', 'forest', '-120 115 -440', False),
    ('forest-edge-negative-streaming', 'forest', '-120 115 -440', True),
)
SETTINGS = """settings_version 8
renderdistance 8
directionalshadowquality high
postprocessingquality on
fullscreen 0
windowsize 1280 720
fov 90
uiscale 1.0
locale zh-CN
audiocaptions 1
actionhints 1
sprintmode hold
sneakmode hold
feedbackintensity full
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
"""


@dataclass(frozen=True)
class Phase:
    name: str
    version: int
    scene: str
    position: str
    rotation: str
    streaming: bool
    kind: str
    warmup_ms: int
    duration_ms: int


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def keep_awake(command):
    return ['/usr/bin/caffeinate', '-dimsu', *command]


def prepare_world_save(template, destination, phase):
    shutil.copytree(template, destination)
    metadata = destination / 'world.meta'
    lines = metadata.read_text().splitlines()
    replacements = {
        'world_id': 'world-' + hashlib.sha256(
            phase.name.encode('utf-8')).hexdigest()[:32],
        'terrain_generation_version': str(phase.version),
        'difficulty_id': '1',
    }
    replaced = set()
    output = []
    for line in lines:
        key = line.split(' ', 1)[0]
        if key in replacements:
            if key in replaced:
                raise ValueError(f'E2 template repeats {key}')
            output.append(f'{key} {replacements[key]}')
            replaced.add(key)
        elif key == 'actor':
            raise ValueError('E2 template contains actor state')
        else:
            output.append(line)
    actor_counts = [line for line in output if line.startswith('actor_count ')]
    if (replaced != set(replacements) or
            actor_counts != ['actor_count 0']):
        raise ValueError('E2 template identity or actor baseline differs')
    metadata.write_text('\n'.join(output) + '\n')
    return {
        'template_sha256': digest(template / 'world.meta'),
        'world_meta_sha256': digest(metadata),
        'world_id': replacements['world_id'],
        'terrain_generation_version': phase.version,
        'difficulty_id': 1,
        'initial_actor_count': 0,
    }


def phases_for(mode, reverse, profile='e2'):
    if profile == 'e6':
        groups, visuals = E6_GROUPS, E6_VISUALS
        baseline, candidate = 11, 12
    elif profile == 'e4':
        groups, visuals = E4_GROUPS, E4_VISUALS
        baseline, candidate = 9, 10
    elif profile == 'e3':
        groups, visuals = E3_GROUPS, E3_VISUALS
        baseline, candidate = 8, 9
    else:
        groups, visuals = GROUPS, VISUALS
        baseline, candidate = 7, 8
    phases = []
    if mode in ('pilot', 'performance', 'all'):
        for group, scene, position, streaming in groups:
            for round_number in range(1, 4):
                order = ((baseline, candidate) if round_number != 2
                         else (candidate, baseline))
                if reverse:
                    order = tuple(reversed(order))
                for version in order:
                    phases.append(Phase(
                        f'{group}-r{round_number}-v{version}', version,
                        scene, position, '0 0 0', streaming,
                        'performance', 5000, 30000))
        if mode == 'pilot':
            phases = [phase for phase in phases
                      if phase.name.startswith(groups[0][0] + '-r1-')]
    if mode in ('visual', 'all'):
        for name, scene, position, rotation in visuals:
            for version in (baseline, candidate):
                phases.append(Phase(f'{name}-v{version}', version,
                                    scene, position, rotation, False,
                                    'visual', 1, 10500))
    return phases


def parse_events(path):
    if not path.is_file():
        return []
    lines = path.read_text().splitlines()
    if not lines or lines[0] != 'run\tindex\tevent\tunix_ms\tpid\tversion\tscene':
        raise ValueError('E2 batch event header differs')
    events = []
    for line in lines[1:]:
        fields = line.split('\t')
        if len(fields) != 7:
            raise ValueError(f'Malformed E2 batch event: {line}')
        events.append(dict(zip(('run', 'index', 'event', 'unix_ms',
                                'pid', 'version', 'scene'), fields)))
    return events


def verify_render_phase_frames(path):
    with path.open(newline='') as stream:
        frame_rows = csv.DictReader(stream)
        required = {'render_ms', 'render_draw_ms',
                    'render_post_draw_ms', 'render_ended_ms',
                    'render_phase_valid'}
        if not required.issubset(set(frame_rows.fieldnames or [])):
            raise ValueError('Render phase CSV columns are missing')
        phase_frames = 0
        for frame in frame_rows:
            phase_frames += 1
            parts = (float(frame['render_draw_ms']),
                     float(frame['render_post_draw_ms']),
                     float(frame['render_ended_ms']))
            total = float(frame['render_ms'])
            if (frame['render_phase_valid'] != '1' or
                not all(math.isfinite(value) and value >= 0
                        for value in (total, *parts)) or
                abs(total - sum(parts)) > 0.02):
                raise ValueError('Render phase timing differs')
        if phase_frames < 100:
            raise ValueError('Render phase frames are incomplete')
    return phase_frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('pilot', 'performance',
                                            'visual', 'all'), required=True)
    parser.add_argument('--profile', choices=('e2', 'e3', 'e4', 'e6'), default='e2',
                        help='Frozen ecology phase set; defaults to E2')
    parser.add_argument('--reverse-order', action='store_true')
    parser.add_argument('--render-phase-diagnostics', action='store_true',
                        help='Capture Ogre draw and post-draw frame spans')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('macOS is required')
    if args.reverse_order and args.mode not in ('performance', 'pilot'):
        parser.error('Reverse order applies to the performance phases only')

    app = args.app.resolve(strict=True)
    output = args.output.absolute()
    if output.exists():
        parser.error('Output must be new; preserve failed or interrupted batches')
    verified_package_entries(app)
    resources = app / 'Contents/Resources'
    identity = json.loads((resources / 'build-identity.json').read_text())
    if digest(resources / 'bin/HelloMine3D') != identity['executable_sha256']:
        raise ValueError('Reused client binary identity differs')
    phases = phases_for(args.mode, args.reverse_order, args.profile)
    output.mkdir(parents=True)
    (resources / 'bin/config.txt').write_text(SETTINGS)
    paths = {}
    world_fixtures = {}
    manifest_lines = ['E2_BATCH_V1']
    for phase in phases:
        directory = output / phase.kind / phase.name
        directory.mkdir(parents=True)
        save = directory / 'save'
        world_fixtures[phase.name] = prepare_world_save(
            V7_TEMPLATE, save, phase)
        paths[phase.name] = directory
        manifest_lines.append('\t'.join((
            phase.name, str(phase.version), phase.scene, phase.position,
            phase.rotation, '1' if phase.streaming else '0',
            str(save), str(directory), str(phase.warmup_ms),
            str(phase.duration_ms))))
    manifest = output / 'batch-manifest.tsv'
    manifest.write_text('\n'.join(manifest_lines) + '\n')
    first = phases[0]
    first_dir = paths[first.name]
    environment = {
        'HELLOMINE3D_ROOT': str(resources),
        'HELLOMINE3D_CATALOGUE_DIR': str(output / 'catalogue'),
        'HELLOMINE3D_SAVE_DIR': str(first_dir / 'save'),
        'HELLOMINE3D_SEED': '20260807',
        'HELLOMINE3D_PLAYER_POSITION': first.position,
        'HELLOMINE3D_PLAYER_ROTATION': first.rotation,
        'HELLOMINE3D_WORLD_TIME': '6000',
        'HELLOMINE3D_RC_PERF_PROFILE':
            'fast-streaming' if first.streaming else '',
        'HELLO_RENDER_CAPTURE': '1',
        'HELLO_RENDER_CAPTURE_DIR': str(first_dir / 'frames'),
        'HELLO_RENDER_CAPTURE_MS': '5000,10000',
        'HELLO_RENDER_CAPTURE_MAX_DELTA_MS': '5000',
        'HELLO_RENDER_CAPTURE_EXIT': '0',
        'HELLO_PERF_CAPTURE': '1',
        'HELLO_PERF_CAPTURE_DIR': str(first_dir / 'performance'),
        'HELLO_PERF_CAPTURE_WARMUP_MS': str(first.warmup_ms),
        'HELLO_PERF_CAPTURE_DURATION_MS': str(first.duration_ms),
        'HELLO_PERF_CAPTURE_EXIT': '0',
        'HELLOMINE3D_E2_BATCH_MANIFEST': str(manifest),
        'HELLOMINE3D_E2_BATCH_EVENTS': str(output / 'batch-events.tsv'),
    }
    if args.render_phase_diagnostics:
        environment['HELLOMINE3D_E2_RENDER_PHASES'] = '1'
    launch_command = ['/usr/bin/open', '-n', '-W', '--stdout',
                      str(output / 'client.log'), '--stderr',
                      str(output / 'client-stderr.log')]
    for key, value in environment.items():
        launch_command.extend(('--env', f'{key}={value}'))
    launch_command.append(str(app))
    # Keep the unlocked graphical session awake for the lifetime of the one
    # process batch.  This cannot unlock an already locked login session.
    command = keep_awake(launch_command)
    status = {'schema': 1,
              'evidence_type': args.profile.upper() + '_ONE_PROCESS_BATCH',
              'profile': args.profile,
              'mode': args.mode, 'reverse_order': args.reverse_order,
              'source_app': str(app), 'package_identity': identity,
              'manifest_sha256': digest(manifest),
              'render_phase_diagnostics': args.render_phase_diagnostics,
              'world_fixture': {
                  'template': str(V7_TEMPLATE),
                  'template_sha256': digest(V7_TEMPLATE / 'world.meta'),
                  'difficulty_id': 1,
                  'initial_actor_count': 0,
                  'identity_rule': 'SAME_TEMPLATE_TERRAIN_VERSION_AND_WORLD_ID_ONLY',
              },
              'phase_count': len(phases), 'command': command,
              'application_command': launch_command,
              'idle_prevention': 'CAFFEINATE_BATCH_LIFETIME',
              'started_unix': time.time(), 'result': 'RUNNING'}
    status_path = output / 'batch-status.json'
    status_path.write_text(json.dumps(status, indent=2) + '\n')
    timeout = 120 + sum((p.warmup_ms + p.duration_ms) / 1000 + 60
                        for p in phases)
    error = None
    try:
        subprocess.run(command, check=True, timeout=timeout)
    except Exception as exception:
        error = str(exception)
    finally:
        status['finished_unix'] = time.time()

    failures = []
    events = parse_events(output / 'batch-events.tsv')
    status['batch_pid'] = sorted({event['pid'] for event in events})
    status['event_count'] = len(events)
    status['events_sha256'] = (digest(output / 'batch-events.tsv')
                               if (output / 'batch-events.tsv').is_file()
                               else None)
    expected_events = [(phase.name, event)
                       for phase in phases
                       for event in ('started', 'completed')]
    if [(event['run'], event['event']) for event in events] != expected_events or \
            any(int(right['unix_ms']) <= int(left['unix_ms'])
                for left, right in zip(events, events[1:])):
        failures.append({'batch_order': 'Phase events differ from manifest'})
    for index, phase in enumerate(phases):
        directory = paths[phase.name]
        run_events = [event for event in events if event['run'] == phase.name]
        started = next((e for e in run_events if e['event'] == 'started'), None)
        completed = next((e for e in run_events if e['event'] == 'completed'), None)
        record_env = dict(environment)
        record_env.update({
            'HELLOMINE3D_SAVE_DIR': str(directory / 'save'),
            'HELLOMINE3D_PLAYER_POSITION': phase.position,
            'HELLOMINE3D_PLAYER_ROTATION': phase.rotation,
            'HELLOMINE3D_RC_PERF_PROFILE':
                'fast-streaming' if phase.streaming else '',
            'HELLO_RENDER_CAPTURE_DIR': str(directory / 'frames'),
            'HELLO_PERF_CAPTURE_DIR': str(directory / 'performance'),
            'HELLO_PERF_CAPTURE_WARMUP_MS': str(phase.warmup_ms),
            'HELLO_PERF_CAPTURE_DURATION_MS': str(phase.duration_ms),
        })
        record = {
            'schema': 1, 'evidence_type': 'DEVELOPER_DIAGNOSTIC',
            'normal_input': False, 'source_app': str(app),
            'runtime_app': str(app), 'package_mode': 'REUSE_STABLE_APP',
            'launch_method': 'ONE_PROCESS_BATCH', 'batch_phase': index,
            'batch_pid': started['pid'] if started else None,
            'batch_manifest': str(manifest), 'package_identity': identity,
            'scene': phase.scene, 'settings': SETTINGS,
            'world_fixture': world_fixtures[phase.name],
            'environment': record_env, 'command': command,
            'application_command': launch_command,
            'idle_prevention': 'CAFFEINATE_BATCH_LIFETIME',
            'started_unix': (int(started['unix_ms']) / 1000
                             if started else None),
            'finished_unix': (int(completed['unix_ms']) / 1000
                              if completed else None),
            'result': 'RUNNING',
        }
        artifacts = [directory / 'frames/capture_05000ms.png',
                     directory / 'frames/capture_10000ms.png',
                     directory / 'performance/summary.txt',
                     directory / 'performance/frames.csv',
                     directory / 'save/world.meta']
        try:
            if (started is None or completed is None or
                int(started['index']) != index or
                int(completed['index']) != index or
                started['pid'] != completed['pid'] or
                int(completed['unix_ms']) <= int(started['unix_ms'])):
                raise ValueError('Phase event pair is missing or differs')
            for artifact in artifacts:
                if not artifact.is_file() or not artifact.stat().st_size:
                    raise ValueError(f'Missing {artifact.relative_to(output)}')
            summary = dict(line.split('=', 1) for line in
                           artifacts[2].read_text().splitlines() if '=' in line)
            if (summary.get('build_configuration') != 'Release' or
                summary.get('terrain_generation_version') !=
                    str(phase.version) or
                summary.get('terrain_seed') != '20260807' or
                summary.get('difficulty_id') != '1' or
                summary.get('warmup_ms') != f'{phase.warmup_ms:.3f}' or
                summary.get('duration_ms') != f'{phase.duration_ms:.3f}'):
                raise ValueError('Phase summary identity or duration differs')
            if args.render_phase_diagnostics:
                record['render_phase_frames'] = verify_render_phase_frames(
                    artifacts[3])
            record['artifacts'] = {
                str(path.relative_to(directory)): digest(path)
                for path in artifacts}
            record['world_metadata'] = artifacts[4].read_text()
            record['result'] = 'CAPTURED'
        except Exception as exception:
            record['result'] = 'FAIL'
            record['error'] = str(exception)
            failures.append({'phase': phase.name, 'error': str(exception)})
        (directory / 'capture.json').write_text(
            json.dumps(record, indent=2) + '\n')
    if error:
        failures.append({'batch': error})
    if len(status['batch_pid']) != 1:
        failures.append({'batch_pid': status['batch_pid']})
    status['failures'] = failures
    status['result'] = 'FAIL' if failures else 'CAPTURED'
    status_path.write_text(json.dumps(status, indent=2) + '\n')
    print(f'[{args.profile.upper()}_BATCH] phases={len(phases)} pid={status["batch_pid"]} '
          f'result={status["result"]} output={output}')
    if failures:
        for failure in failures:
            print(f'[{args.profile.upper()}_BATCH] failure={failure}')
        raise SystemExit(1)


if __name__ == '__main__':
    main()
