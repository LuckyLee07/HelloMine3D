#!/usr/bin/env python3
"""Fixed-condition developer terrain capture from an isolated macOS package.

Uses existing render/performance diagnostics. Forced positions and streaming
teleports mean this is NOT normal-input or AI_INTERACTIVE acceptance.
"""
import argparse
import hashlib
import json
import platform
import subprocess
import time
from pathlib import Path

SCENES = {
    'forest': ('256 90 256', '0 0 0'),
    'negative': ('-256 70 -256', '0 0 0'),
    'mountain': ('1024 140 1024', '0 0 0'),
    'steady': ('256 90 256', '0 0 0'),
    'streaming': ('256 90 256', '0 0 0'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scene', choices=SCENES, required=True)
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('macOS required')
    app = args.app.resolve(strict=True)
    output = args.output.absolute()
    if output.exists():
        parser.error('Output must be new; failures are retained')
    root = app / 'Contents/Resources'
    binary = root / 'bin/HelloMine3D'
    identity = json.loads((root / 'build-identity.json').read_text())
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if digest != identity['executable_sha256']:
        parser.error('Package executable identity mismatch')
    output.mkdir(parents=True)
    position, rotation = SCENES[args.scene]
    environment = {
        'HELLOMINE3D_ROOT': str(root),
        'HELLOMINE3D_SAVE_DIR': str(output / 'save'),
        'HELLOMINE3D_CATALOGUE_DIR': str(output / 'catalogue'),
        'HELLOMINE3D_SEED': '20260807',
        'HELLOMINE3D_PLAYER_POSITION': position,
        'HELLOMINE3D_PLAYER_ROTATION': rotation,
        'HELLOMINE3D_WORLD_TIME': '6000',
    }
    performance = args.scene in ('steady', 'streaming')
    if performance:
        environment.update({
            'HELLO_PERF_CAPTURE': '1', 'HELLO_PERF_CAPTURE_DIR': str(output / 'performance'),
            'HELLO_PERF_CAPTURE_WARMUP_MS': '5000',
            'HELLO_PERF_CAPTURE_DURATION_MS': '30000', 'HELLO_PERF_CAPTURE_EXIT': '1',
        })
        if args.scene == 'streaming':
            environment['HELLOMINE3D_RC_PERF_PROFILE'] = 'fast-streaming'
    else:
        environment.update({
            'HELLO_RENDER_CAPTURE': '1', 'HELLO_RENDER_CAPTURE_DIR': str(output / 'frames'),
            'HELLO_RENDER_CAPTURE_MS': '5000,10000',
            'HELLO_RENDER_CAPTURE_MAX_DELTA_MS': '5000',
            'HELLO_RENDER_CAPTURE_EXIT': '1',
        })
    command = ['/usr/bin/open', '-n', '-W', '--stdout', str(output / 'client.log'),
               '--stderr', str(output / 'client-stderr.log')]
    for key, value in environment.items():
        command.extend(['--env', f'{key}={value}'])
    command.append(str(app))
    record = {'schema': 1, 'scene': args.scene, 'app': str(app),
              'evidence_type': 'DEVELOPER_DIAGNOSTIC', 'normal_input': False,
              'package_identity': identity, 'environment': environment,
              'platform': platform.platform(), 'host_architecture': platform.machine(),
              'command': command, 'started_unix': time.time(), 'result': 'RUNNING'}
    record_path = output / 'capture.json'
    record_path.write_text(json.dumps(record, indent=2) + '\n')
    try:
        subprocess.run(command, check=True, timeout=90)
        if performance:
            artifacts = [output / 'performance/summary.txt', output / 'performance/frames.csv']
        else:
            artifacts = sorted((output / 'frames').glob('*.png'))
            if len(artifacts) != 2:
                raise RuntimeError(f'Expected two readback frames, got {len(artifacts)}')
        for artifact in artifacts:
            if not artifact.is_file() or not artifact.stat().st_size:
                raise RuntimeError(f'Missing artifact {artifact}')
        record['artifacts'] = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in artifacts}
        metadata = output / 'save/world.meta'
        if not metadata.is_file():
            raise RuntimeError('No saved world identity')
        record['world_metadata'] = metadata.read_text()
        record['result'] = 'CAPTURED'
    except Exception as error:
        record['result'] = 'FAIL'
        record['error'] = str(error)
        raise
    finally:
        record['finished_unix'] = time.time()
        record_path.write_text(json.dumps(record, indent=2) + '\n')
    print(record_path)


if __name__ == '__main__':
    main()
