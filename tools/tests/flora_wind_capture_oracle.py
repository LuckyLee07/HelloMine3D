#!/usr/bin/env python3
"""Audit original Fern storage and native client draw observations, using CPU only.

Two independent client sessions are required (standard and compatibility), each
containing Off0, Off1, High0 and High1. The World reference is a separately run
natural locator; its mesh ranges and revision are deliberately not reused.
This does not establish visible pixels, inner draw VAO fetching, or atomic ABA.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

from fern_native_geometry import audit_source, column_to_row, multiply, read_original

PHASES = ('Off0', 'Off1', 'High0', 'High1')
VERTEX = ('HelloMine3D/FloraVertex', 'HelloMine3D/FloraShadowVertex')
FRAGMENT = {
    'standard': ('HelloMine3D/TerrainArrayFragment', 'HelloMine3D/TerrainShadowArrayFragment'),
    'compatibility': ('HelloMine3D/TerrainFragment', 'HelloMine3D/TerrainShadowFragment'),
}
SESSION_BYTES = 256 * 1024 * 1024
OP_BYTES = 16 * 1024 * 1024


class OpenProof(Exception):
    pass


def require(ok, message):
    if not ok:
        raise ValueError(message)


def integer(value, minimum=0):
    require(type(value) is int and value >= minimum, 'Expected bounded integer')
    return value


def matrix(value):
    require(isinstance(value, list) and len(value) == 16 and all(
        type(x) in (int, float) and math.isfinite(x) for x in value), 'Expected finite16-value matrix')
    return value


def equal_matrix(actual, expected, tolerance=1e-5, relative=0):
    matrix(actual)
    matrix(expected)
    require(all(abs(a-b) <= tolerance + relative*max(abs(a), abs(b))
                for a, b in zip(actual, expected)), 'Actual matrix differs from independent source')


def translation(origin):
    require(len(origin) == 3 and all(type(x) is int for x in origin), 'Invalid actual batch origin')
    return [1, 0, 0, origin[0]*16, 0, 1, 0, origin[1]*16, 0, 0, 1, origin[2]*16, 0, 0, 0, 1]


def png_dimensions(data):
    require(data[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid PNG signature')
    cursor, dimensions, ended = 8, None, False
    while cursor < len(data):
        require(cursor+12 <= len(data), 'Truncated PNG chunk')
        count = struct.unpack_from('>I', data, cursor)[0]
        end = cursor+12+count
        require(end <= len(data), 'PNG chunk exceeds file')
        kind = data[cursor+4:cursor+8]
        payload = data[cursor+8:cursor+8+count]
        require(zlib.crc32(kind+payload) & 0xffffffff == struct.unpack_from(
            '>I', data, cursor+8+count)[0], 'PNG CRC mismatch')
        if kind == b'IHDR':
            require(dimensions is None and count == 13, 'Invalid PNG IHDR')
            dimensions = struct.unpack_from('>II', payload)
        if kind == b'IEND':
            require(count == 0 and end == len(data), 'Invalid PNG ending')
            ended = True
        cursor = end
    require(ended and dimensions is not None, 'Incomplete PNG')
    return dimensions


class Audit:
    def __init__(self):
        self.inputs = {}
        self.results = []
        self.sources = []
        self.frames = []
        self.required_open = []
        self.reference = {}

    def read(self, path, limit=SESSION_BYTES):
        path = Path(path).resolve()
        if not path.is_file():
            raise OpenProof('Required input is missing: '+str(path))
        require(path.stat().st_size <= limit, 'Input exceeds file bound: '+str(path))
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if str(path) in self.inputs:
            require(self.inputs[str(path)]['sha256'] == digest, 'Input changed while auditing')
        self.inputs[str(path)] = {'bytes': len(data), 'sha256': digest}
        return data

    def json(self, path):
        def fields(pairs):
            result = {}
            for key, value in pairs:
                require(key not in result, 'Duplicate JSON field cannot overwrite earlier evidence: '+key)
                result[key] = value
            return result
        return json.loads(self.read(path, 1024*1024), object_pairs_hook=fields)

    def local(self, directory, name):
        require(isinstance(name, str) and name, 'Packet path must be nonempty')
        path = (directory/name).resolve()
        require(path.is_relative_to(directory.resolve()), 'Packet sidecar escapes session directory')
        return path

    def check(self, label, callback):
        try:
            result = callback()
            self.results.append({'check': label, 'status': 'PASS'})
            return result
        except (OpenProof, KeyError, FileNotFoundError) as error:
            self.results.append({'check': label, 'status': 'OPEN', 'detail': str(error)})
            self.required_open.append(label)
        except (ValueError, TypeError, IndexError, struct.error) as error:
            self.results.append({'check': label, 'status': 'FAIL', 'detail': str(error)})

    def load_reference(self, path):
        data = self.json(path)
        require(data['schema'] == 'hellomine3d-fern-natural-locator-v1', 'Wrong World reference schema')
        require(data['known_window'] is True and data['chunks_unchanged'] is True,
                'Independent World locator did not retain known resident source')
        require(0 < integer(data['queries']) <= 32*32*192, 'Natural locator query bound')
        require(len(data['sources']) == 2, 'Exactly two independent natural Fern sources required')
        for source in data['sources']:
            xyz = tuple(source['block_'+axis] for axis in 'xyz')
            require(len(xyz) == 3 and all(type(x) is int for x in xyz), 'Noninteger World source')
            require(source['block_id'] == 10 and source['metadata'] == 2 and source['height_scale'] == 1,
                    'Reference is not existing natural scale1 Fern')
            require(source['seed'] == data['seed'] and source['terrain_generation_version'] == data[
                'terrain_generation_version'], 'Reference World generation identity differs')
            require(xyz not in self.reference, 'Duplicate independent World source')
            self.reference[xyz] = source
        return data

    def source(self, record, parts=None):
        xyz = tuple(record['query_xyz'])
        require(len(xyz) == 3 and all(type(x) is int for x in xyz), 'Invalid World query coordinates')
        require(record['known'] is True and record['returned_id'] == 10 and record['returned_metadata'] == 2,
                'Actual query is unknown or not existing Fern')
        section = [math.floor(x/16) for x in xyz]
        require(record['section'] == section, 'Actual query section does not contain World coordinate')
        revision = integer(record['actual_block_revision'])
        require(all(record[key] == revision for key in ('version_before', 'version_after',
            'live_revision', 'upload_revision')) and record['gpu_resident'] is True,
            'Actual locked live/upload/GpuResident endpoint chain differs')
        require(record['renderer_incarnation'] is None and record['renderer_incarnation_known'] is False,
                'Public locked World snapshot cannot establish renderer incarnation')
        incarnation = None
        require(record['atomic_copy'] is False and record['world_incarnation'] ==
                'OPEN_NO_INCARNATION_IN_PUBLIC_LIVE_VERSION_SNAPSHOT', 'Unsubstantiated atomic World identity claim')
        require(type(record['seed']) is int and type(record['terrain_generation_version']) is int,
                'World generation identity must be integral')
        if parts is not None:
            matches = [p for p in parts if p['section'] == section]
            require(len(matches) == 1, 'Source does not have exactly one actual retained batch part')
            part = matches[0]
            require(part['incarnation'] is None and part['incarnation_known'] is False and
                    part['incarnation_scope'] == 'OPEN_PUBLIC_LOCKED_SNAPSHOT_HAS_NO_INCARNATION' and
                    part['live_revision'] == revision and
                    part['upload_revision'] == revision and part['gpu_resident'] is True,
                    'Source query differs from actual retained renderer part')
        if not self.reference:
            raise OpenProof('Independent natural World locator reference is required')
        require(xyz in self.reference, 'Captured source is absent from independent natural World reference')
        expected = self.reference[xyz]
        for key, refkey in [('seed', 'seed'), ('terrain_generation_version', 'terrain_generation_version'),
                           ('biome', 'biome'), ('sunlight', 'sunlight'), ('blocklight', 'block_light')]:
            require(record[key] == expected[refkey], 'Actual World '+key+' differs from independent locator')
        return xyz, revision, incarnation

    def operation(self, directory, op, frame):
        require(op['material_name'] == 'HelloMine3D/Flora' and integer(op['object_id'], 1), 'Not actual Flora instance')
        require(op['operation_type'] == 4 and op['index_type'] == 'u32' and op['instances'] == 1 and
                op['vertex_start'] == 0 and op['index_start'] == 0, 'Unexpected production operation')
        require(op['state_restored'] is True, 'Native observer did not restore GL state')
        buffers = op['vertex_buffers']
        require(len(buffers) == 1, 'Expected original single source0 VBO')
        vb, ib = buffers[0], op['index_buffer']
        require(vb['source'] == 0 and vb['stride'] == 44 and integer(vb['gl_id'], 1) and
                integer(ib['gl_id'], 1), 'Not actual44B/u32 native storage')
        require(integer(vb['bytes'])+integer(ib['bytes']) <= OP_BYTES, 'Combined original storage exceeds16MiB')
        raw_v = self.read(self.local(directory, vb['file']), OP_BYTES)
        raw_i = self.read(self.local(directory, ib['file']), OP_BYTES)
        cpu_v = self.read(self.local(directory, vb['cpu_file']), OP_BYTES)
        cpu_i = self.read(self.local(directory, ib['cpu_file']), OP_BYTES)
        require(raw_v == cpu_v and raw_i == cpu_i and op['native_cpu_bytes_equal'] is True,
                'Original native buffers differ from actual retained production CPU storage')
        require(len(raw_v) == vb['bytes'] == vb['vertices']*44 == op['vertex_count']*44 and
                len(raw_i) == ib['bytes'] == op['index_count']*4, 'Original whole operation storage size differs')
        vertices, indices = read_original(raw_v, raw_i, 0, op['vertex_count'], 0, op['index_count'], op['elements'])
        node = matrix(op['node_world_row_major'])
        equal_matrix(node, translation(op['origin']))
        equal_matrix(matrix(op['prebind_world_row_major']), node)
        require(1 <= len(op['parts']) <= 4 and 1 <= len(op['source_queries']) <= 2, 'Retained part/source bound')
        sections = []
        for part in op['parts']:
            require(part['section'] not in sections and part['incarnation'] is None and
                    part['incarnation_known'] is False and
                    part['incarnation_scope'] == 'OPEN_PUBLIC_LOCKED_SNAPSHOT_HAS_NO_INCARNATION' and
                    part['live_revision'] == part['upload_revision'] and part['gpu_resident'] is True,
                    'Duplicate, stale or nonresident renderer part')
            sections.append(part['section'])
        sources = []
        for query in op['source_queries']:
            xyz, revision, incarnation = self.source(query, op['parts'])
            geometry = audit_source(vertices, indices, 0, op['vertex_count'], 0, op['index_count'], node,
                list(xyz), query['biome'], query['sunlight'], query['blocklight'],
                [query[key] for key in ('vertex_begin', 'vertex_end', 'index_begin', 'index_end')])
            self.sources.append({'mode': frame['mode'], 'phase': frame['phase'], 'object_id': op['object_id'],
                                 'world_xyz': list(xyz), 'revision': revision, 'renderer_incarnation': incarnation,
                                 'geometry': geometry})
            sources.append(xyz)
        query = op['primitive_query']
        require(query['target'] == 0x8c87 and integer(query['id'], 1) and query['conflicting_query'] == 0,
                'Primitive result is not an owned GL_PRIMITIVES_GENERATED query')
        if query['began'] is not True or query['ended'] is not True or query['result_read'] is not True:
            raise OpenProof('Actual native primitive query result has not been read')
        require(type(query['available_before_blocking_read']) is bool, 'Invalid query availability observation')
        # GL_QUERY_RESULT is blocking; availability before that read may be false.
        require(integer(query['result'], 1) == op['index_count']//3 == query['expected_whole_operation'],
                'Actual native generated primitive count is not the whole original operation')
        require(integer(op['actual_program'], 1) and integer(op['selected_pass_id'], 1) and
                op['linked'] is True, 'Missing actually active linked program/pass')
        high = frame['phase'].startswith('High')
        require(op['selected_vertex_program'] == VERTEX[high] and
                op['selected_fragment_program'] == FRAGMENT[frame['mode']][high], 'Wrong actual production shader route')
        attached = op['attached_shaders']
        require(len(attached) == 2 and {s['type'] for s in attached} == {0x8b31, 0x8b30} and
                len({s['id'] for s in attached}) == 2, 'Native program does not attach one vertex and one fragment shader')
        wanted = {0x8b31: op['selected_vertex_shader_id'], 0x8b30: op['selected_fragment_shader_id']}
        for shader in attached:
            require(integer(shader['id'], 1) == wanted[shader['type']] and shader['compiled'] is True,
                    'Actual attached shader handle differs from selected loaded production pass')
            code = self.read(self.local(directory, shader['source_file']), 256*1024)
            require(code and b'void main' in code, 'Missing original native compiled shader source')
            if 'loaded_source_file' in shader:
                require(code == self.read(self.local(directory, shader['loaded_source_file']), 256*1024),
                        'Native shader source differs from selected loaded/preprocessed source')
        require(op['attached_matches_selected_pass'] is True, 'Captured attached/pass identity contradiction')
        uniforms = op['native_uniforms']
        time = uniforms['globalTime']
        require(integer(time['location']) >= 0 and time['type'] == 0x1406 and type(time['value']) in
                (int, float) and math.isfinite(time['value']) and time['value'] >= 0, 'Invalid actual native globalTime')
        native_matrices = {}
        for name in ('world', 'worldViewProj'):
            uniform = uniforms[name]
            require(integer(uniform['location']) >= 0 and uniform['type'] == 0x8b5c and
                    uniform['layout'] == 'column_major', 'Invalid actual native mat4 uniform')
            native_matrices[name] = column_to_row(matrix(uniform['values']))
        equal_matrix(native_matrices['world'], node)
        equal_matrix(native_matrices['worldViewProj'], matrix(op['prebind_wvp_row_major']))
        # The GL converter is identity for both Ogre projection getters. Float
        # production matrix products have rounding, hence scaled tolerance here.
        computed = multiply(matrix(frame['camera_projection_rs_row_major']),
                            multiply(matrix(frame['camera_view_row_major']), node))
        equal_matrix(native_matrices['worldViewProj'], computed, 1e-4, 5e-7)
        texture = op['native_texture_unit0']
        require(integer(texture['sampler']) >= 0 and integer(texture[
            'texture_array' if frame['mode'] == 'standard' else 'texture_2d'], 1), 'Missing actual route texture binding')
        require(op['inner_draw_vertex_fetch'] == 'OPEN_POST_RENDER_VAO_MAY_BE_UNBOUND_ATTRIBUTES_NOT_OBSERVED',
                'Unobserved inner VAO fetch claimed as proven')
        return {'object_id': op['object_id'], 'time': time['value'], 'sources': sources,
                'primitive_count': query['result'], 'native_vbo_sha256': hashlib.sha256(raw_v).hexdigest(),
                'native_ibo_sha256': hashlib.sha256(raw_i).hexdigest()}

    def frame(self, directory, data):
        require(data['schema'] == 'hellomine3d-flora-wind-native-frame-v1', 'Wrong frame schema')
        require(data['mode'] in FRAGMENT and data['phase'] in PHASES, 'Unknown mode/phase')
        require(integer(data['frame_id']) >= 0 and data['endpoint_identity_stable'] is True and
                data['atomic_snapshot'] is False and data['state_restored'] is True, 'Actual endpoint/state evidence differs')
        require(data['gl_errors'] == [], 'Actual diagnostic frame contains GL errors')
        require(data['ordinary_input'] == 'NOT_RUN', 'Diagnostic must not self-claim ordinary input')
        before = [self.source(s) for s in data['source_queries_before']]
        after = [self.source(s) for s in data['source_queries_after']]
        require(len(before) == 2 and sorted(before) == sorted(after) and
                len({s[0] for s in before}) == 2, 'Actual before/after World endpoints changed or duplicate')
        require(1 <= len(data['operations']) <= 4, 'No bounded original operations captured')
        operations = [self.operation(directory, op, data) for op in data['operations']]
        ids = [op['object_id'] for op in operations]
        require(len(set(ids)) == len(ids), 'Duplicate native operation masks selected object')
        require(sorted(x for op in operations for x in op['sources']) == sorted(self.reference),
                'Actual operations do not cover each independently known Fern exactly once')
        require(len({op['time'] for op in operations}) == 1, 'One frame native times disagree between selected operations')
        fb = data['framebuffer']
        width, height = integer(fb['width'], 1), integer(fb['height'], 1)
        require(width <= 8192 and height <= 8192 and fb['origin'] == 'bottom_left' and fb['format'] == 'RGBA8',
                'Invalid actual framebuffer metadata')
        rgba = self.read(self.local(directory, fb['file']))
        png = self.read(self.local(directory, fb['png']))
        require(len(rgba) == width*height*4 and png_dimensions(png) == (width, height),
                'Actual RGBA/PNG dimensions or byte length differ')
        # Packet reasons are retained rather than silently overridden by another
        # correct operation. Concrete contradictions above fail independently.
        if data['open_reasons']:
            raise OpenProof('Actual observer left proof open: '+repr(data['open_reasons']))
        result = {'mode': data['mode'], 'phase': data['phase'], 'frame_id': data['frame_id'],
                  'time': operations[0]['time'], 'operations': operations,
                  'rgba_sha256': hashlib.sha256(rgba).hexdigest(), 'png_sha256': hashlib.sha256(png).hexdigest()}
        self.frames.append(result)
        return result

    def session(self, path):
        path = Path(path).resolve()
        directory = path.parent
        data = self.json(path)
        require(data['schema'] == 'hellomine3d-flora-wind-native-session-v1', 'Wrong native session schema')
        if data.get('synthetic'):
            raise OpenProof('Synthetic protocol packet cannot establish actual native client evidence')
        require(data['frame_count'] == 4 and data['complete'] is True and len(data['frames']) == 4 and
                len(set(data['frames'])) == 4, 'Actual session does not contain four distinct frames')
        require(data['max_frames'] == 12 and data['max_ops_per_frame'] == 4 and
                data['max_raw_bytes_per_op'] == OP_BYTES and data['max_session_bytes'] == SESSION_BYTES,
                'Native capture budget contract differs')
        profile = data['profile']
        require(profile['format_version'] == 2 and profile['tiles_per_row'] == 16 and
                profile['atlas_pixels'] == 256 and profile['tile_pixels'] == 16 and
                type(profile['uses_array']) is bool, 'Actual default frozen profile differs')
        resources = data['resources']
        require(resources and len({r['logical_path'] for r in resources}) == len(resources),
                'Missing or duplicate actual resolved resource source')
        for resource in resources:
            self.read(resource['resolved_file'])
        packets = [self.json(self.local(directory, name)) for name in data['frames']]
        require(len({frame['framebuffer']['file'] for frame in packets}) == 4 and
                len({frame['framebuffer']['png'] for frame in packets}) == 4,
                'Four observed frames must retain distinct actual RGBA/PNG sidecars')
        expected_mode = 'standard' if profile['uses_array'] else 'compatibility'
        require([frame['phase'] for frame in packets] == list(PHASES) and
                all(frame['mode'] == expected_mode for frame in packets), 'Actual four-phase mode route differs')
        results = [self.check(expected_mode+'/'+frame['phase'], lambda frame=frame: self.frame(directory, frame))
                   for frame in packets]
        if all(result is not None for result in results):
            require(all(b['frame_id'] > a['frame_id'] and b['time'] > a['time']
                        for a, b in zip(results, results[1:])), 'Actual sampled native clock reset or failed to advance')
            # Each Off/High pair has two actual observations; sampled monotonicity
            # is not continuous-time flicker or uniform-rate proof.
            identities = [[(s['world_xyz'], s['revision'], s['renderer_incarnation']) for s in self.sources
                           if s['mode'] == expected_mode and s['phase'] == phase] for phase in PHASES]
            require(all(sorted(values) == sorted(identities[0]) for values in identities),
                    'Actual World/renderer source identity changed during four observations')
        # Count each actual packet file only once; resource inputs are separate.
        payload = sum(f.stat().st_size for f in directory.iterdir() if f.is_file())
        require(payload <= SESSION_BYTES+1024*1024, 'Actual immutable packet payload/index exceeds session bounds')
        return expected_mode

    def report(self):
        unchanged = all(Path(path).is_file() and hashlib.sha256(Path(path).read_bytes()).hexdigest() ==
                        info['sha256'] for path, info in self.inputs.items())
        self.results.append({'check': 'all-read-inputs-unchanged', 'status': 'PASS' if unchanged else 'FAIL'})
        counts = {status: sum(r['status'] == status for r in self.results) for status in ('PASS', 'FAIL', 'OPEN')}
        status = 'FAIL' if counts['FAIL'] else 'OPEN' if counts['OPEN'] else 'PASS'
        return {'schema': 'hellomine3d-flora-wind-native-cpu-oracle-v1', 'status': status, 'counts': counts,
                'checks': self.results, 'frames': self.frames, 'native_source_geometry': self.sources,
                'inputs': self.inputs, 'inputs_unchanged': unchanged,
                'scope_open': ['inner draw VAO/attribute fetch (postRender VAO may be unbound; attributes unobserved)',
                    'World/renderer incarnation and atomic ABA identity between separately locked reads',
                    'visible source pixels and native texture byte identity',
                    'continuous ordinary-input wind/flicker and shader output root motion',
                    'transparent Alpha cutout contour; sampled Grass Top replay was all opaque',
                    'growth scaling (existing registered Fern has scale1)',
                    'raw source file equality with engine-preprocessed native GLSL unless loaded_source_file is exported'],
                'clock_scope': 'four actual sampled finite native times, ordered and advancing within each mode',
                'rgba_scope': 'actual whole-frame length/dimensions/CRC/SHA; no pixel-object visibility attribution',
                'execution': 'CPU only; no renderer, compile, link, GPU or UI calls'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='append', required=True, type=Path, help='Actual client session index.json (twice)')
    parser.add_argument('--world-reference', required=True, type=Path, help='Independent natural-locate.json')
    parser.add_argument('--output', required=True, type=Path, help='New report JSON path')
    args = parser.parse_args()
    require(not args.output.exists() and len(args.capture) == 2, 'Two sessions and a new output are required')
    audit = Audit()
    audit.read(Path(__file__).resolve(), 1024*1024)
    audit.read(Path(__file__).with_name('fern_native_geometry.py'), 1024*1024)
    audit.check('independent-natural-World-reference', lambda: audit.load_reference(args.world_reference))
    modes = [audit.check('session/'+str(path), lambda path=path: audit.session(path)) for path in args.capture]
    audit.check('paired-real-modes', lambda: require(sorted(x for x in modes if x) ==
        ['compatibility', 'standard'], 'One distinct actual session per mode required'))
    report = audit.report()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'status': report['status'], 'counts': report['counts'], 'report': str(args.output.resolve())}))
    return 1 if report['status'] == 'FAIL' else 2 if report['status'] == 'OPEN' else 0


if __name__ == '__main__':
    raise SystemExit(main())
