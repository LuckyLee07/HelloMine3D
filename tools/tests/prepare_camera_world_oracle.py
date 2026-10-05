#!/usr/bin/env python3
"""Compile the resident World/Ogre camera oracle from a macOS x64 client build.

By default, prepare link/run commands. --run also links and runs the CPU oracle.
Use a completed normal client build of the requested configuration.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, os, shlex, signal, struct, subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hashes(paths):
    result = {}
    for p in sorted(set(paths)):
        try: result[str(p)] = digest(p) if p.is_file() else None
        except OSError: result[str(p)] = None
    return result


def minimum_os(path):
    data = path.read_bytes()
    if len(data) < 32 or struct.unpack_from('<II', data) != (0xfeedfacf, 0x01000007):
        raise ValueError('Expected an x86_64 Mach-O client object')
    offset = 32
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError('Malformed Mach-O load command')
        if command == 0x32 and size >= 24:
            version = struct.unpack_from('<I', data, offset + 12)[0]
            return f'{version >> 16}.{(version >> 8) & 255}.{version & 255}'
        offset += size
    raise ValueError('Client object has no LC_BUILD_VERSION minimum OS')


def dependencies(path, root):
    names = shlex.split(path.read_text().replace('\\\n', ' ').split(':', 1)[1])
    paths = {(root / name).absolute() for name in names}
    if any(not p.is_file() for p in paths):
        raise ValueError('Compiler dependency file contains a missing input')
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Release')
    parser.add_argument('--output', required=True, type=Path, help='New output directory')
    parser.add_argument('--repository', type=Path, help='Repository root, otherwise discovered from this script')
    parser.add_argument('--source', type=Path, help='Oracle source, default tools/tests/near_wall_camera_world_test.cpp')
    parser.add_argument('--run', action='store_true', help='Compile, link and run; otherwise compile and prepare commands')
    parser.add_argument('--timeout', type=int, default=300, help='Seconds per stage (1..300), default 300')
    args = parser.parse_args()
    if not 1 <= args.timeout <= 300:
        parser.error('--timeout must be in 1..300')
    root = args.repository.resolve() if args.repository else next(
        (p for p in Path(__file__).resolve().parents if (p/'src/HelloMine3D').is_dir()), None)
    if root is None or not (root/'premake').is_dir():
        parser.error('Cannot locate repository; use --repository')
    requested_output = args.output.absolute()
    if requested_output.exists() or requested_output.is_symlink():
        parser.error('Output directory must be new')
    out = requested_output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = {'schema': 'hellomine3d-camera-world-oracle-v1', 'configuration': args.configuration,
               'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
               'cwd': str(root), 'output': str(out), 'steps': [], 'status': 'INCOMPLETE'}
    before, paths, inventory_before = {}, set(), None
    object_dir = root/f'build/HelloMine3D/obj/x64/{args.configuration}/HelloMine3D.build/Objects-normal/x86_64'
    exit_code = 1

    def inventory():
        game = sorted(p for p in (root/'src/HelloMine3D').rglob('*') if p.is_file() and p.suffix in {'.h', '.cpp', '.mm'})
        objects = sorted(p for p in object_dir.glob('*.o') if p.is_file() and p.name != 'OgreMain.o')
        archives = sorted(p for owner in ['Engine', 'External'] for p in (root/f'build/{owner}').rglob('*.a')
                          if f'/lib/x64/{args.configuration}/' in str(p))
        return game, objects, archives

    def step(name, argv):
        log = out/f'{name}.log'
        record = {'stage': name, 'argv': list(map(str, argv)), 'log': str(log), 'returncode': None}
        receipt['steps'].append(record)
        (out/f'{name}-command.txt').write_text(shlex.join(record['argv'])+'\n')
        with log.open('wb') as stream:
            child = subprocess.Popen(record['argv'], cwd=root, stdout=stream,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            try:
                child.wait(timeout=args.timeout)
            except (subprocess.TimeoutExpired, KeyboardInterrupt) as error:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
                record['timed_out'] = isinstance(error, subprocess.TimeoutExpired)
                record['interrupted'] = isinstance(error, KeyboardInterrupt)
        record.update(returncode=child.returncode, signal=-child.returncode if child.returncode < 0 else None,
                      log_sha256=digest(log))
        return 124 if record.get('timed_out') else 130 if record.get('interrupted') else (
            128-child.returncode if child.returncode < 0 else child.returncode)

    def guard():
        if hashes(paths) != before or inventory() != inventory_before:
            raise RuntimeError('Source, dependencies, objects or archives changed during verification')

    def verify():
        nonlocal paths, before, inventory_before, object_dir
        source = (args.source or root/'tools/tests/near_wall_camera_world_test.cpp').absolute()
        object_dir = root/f'build/HelloMine3D/obj/x64/{args.configuration}/HelloMine3D.build/Objects-normal/x86_64'
        inventory_before = inventory()
        game, objects, archives = inventory_before
        required = {'OgreThirdPersonCameraRig.o', 'OgrePlayerRenderer.o', 'World.o', 'Player.o'}
        if not source.is_file() or not required.issubset({p.name for p in objects}) or not archives:
            raise ValueError('Missing oracle or normal client objects/archives; build this configuration first')
        compiler = subprocess.check_output(['xcrun', '--find', 'clang++'], text=True).strip()
        sdk = subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
        minimum = minimum_os(object_dir/'OgrePlayerRenderer.o')
        flags = ['-std=c++17', '-arch', 'x86_64', f'-mmacosx-version-min={minimum}', '-isysroot', sdk,
                 '-Wall', '-Wextra', '-Werror', '-D_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR',
                 '-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION', '-DGLM_ENABLE_EXPERIMENTAL',
                 '-DOGRE_STATIC_LIB', '-DOGRE_BUILD_RENDERSYSTEM_GL3PLUS', '-DFREEIMAGE_LIB',
                 '-iquote', str(root/'src/HelloMine3D/Ogre'), '-I', str(root/'src/HelloMine3D')]
        for include in ['src/external/glm', 'src/Engine/ogre3d/include', 'src/Engine/ogre3d/include/OSX',
                        'src/Engine/ogre3d_gl3plus/include', 'src/Engine/ogre3d_gl3plus/include/GLSL',
                        'src/Engine/ogre3d_glsupport/include', 'src/Engine/ThirdParty/freeimage/include']:
            flags += ['-isystem', str(root/include)]
        flags += ['-O0', '-g'] if args.configuration == 'Debug' else ['-O3', '-DNDEBUG']
        paths = set([source, Path(__file__).absolute(), Path(compiler), *game, *objects, *archives])
        before = hashes(paths)
        if any(value is None for value in before.values()): raise ValueError('An input is missing or unreadable')
        receipt.update(compiler=compiler, sdk=sdk, client_minimum_os=minimum,
                       source=str(source), object_count=len(objects), archive_count=len(archives))
        exit_code = step('dependencies', [compiler, *flags, '-M', '-MF', out/'dependencies.d', '-MT', 'oracle.o', source])
        if exit_code: return exit_code
        for p, value in hashes(dependencies(out/'dependencies.d', root)).items():
            paths.add(Path(p)); before.setdefault(p, value)
        guard()
        exit_code = step('compile', [compiler, *flags, '-MD', '-MF', out/'oracle.d', '-serialize-diagnostics',
                                   out/'oracle.dia', '-c', source, '-o', out/'oracle.o'])
        if exit_code: return exit_code
        if not dependencies(out/'oracle.d', root).issubset(paths):
            raise RuntimeError('Compile used an input absent from the dependency guard')
        guard()
        link = [compiler, '-arch', 'x86_64', f'-mmacosx-version-min={minimum}', '-isysroot', sdk,
                out/'oracle.o', *objects, *archives, '-L/usr/local/lib', '-L/opt/homebrew/lib']
        for framework in ['AudioToolbox', 'Cocoa', 'Carbon', 'IOKit', 'Foundation', 'AppKit', 'CoreFoundation', 'OpenGL']:
            link += ['-framework', framework]
        link += ['-o', out/'camera-world-test']
        run = [out/'camera-world-test', root, out/'world-run']
        receipt['commands'] = {'link': list(map(str, link)), 'run': list(map(str, run))}
        if args.run:
            exit_code = step('link', link)
            if exit_code: return exit_code
            receipt['binary_sha256'] = digest(out/'camera-world-test'); guard()
            exit_code = step('run', run)
        receipt['status'] = ('PASS' if args.run else 'PREPARED_NOT_RUN') if exit_code == 0 else 'FAILED'
        return exit_code

    try:
        exit_code = verify()
    except Exception as error:
        receipt['exception'] = {'type': type(error).__name__, 'message': str(error)}
        exit_code = 1
    finally:
        receipt['inputs_before'], receipt['inputs_after'] = before, hashes(paths)
        receipt['inputs_unchanged'] = before == receipt['inputs_after'] if before else None
        receipt['inventory_unchanged'] = inventory() == inventory_before if inventory_before is not None else None
        if receipt['inputs_unchanged'] is False or receipt['inventory_unchanged'] is False:
            receipt['status'] = 'FAILED_INPUT_GUARD'; exit_code = exit_code or 1
        elif exit_code:
            receipt['status'] = 'FAILED'
        receipt['overall_exit'] = exit_code
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        print(json.dumps({'status': receipt['status'], 'exit': exit_code, 'receipt': str(out/'receipt.json')}))
    return exit_code


if __name__ == '__main__':
    raise SystemExit(main())
