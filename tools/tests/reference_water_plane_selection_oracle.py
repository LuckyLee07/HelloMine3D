#!/usr/bin/env python3
"""Strict independent audit of actual owned multi-level Water selection.

Never launches a client or constructs a passing native fixture. Optional fault
calibration mutates copies of one actual complete positive. Original transition
oracle/schema and its pressure failures remain untouched.
"""
import argparse
from array import array
import copy
import hashlib
from fractions import Fraction
import itertools
import math
from pathlib import Path
import re
import struct
import sys
import zlib
import reference_world_edit_oracle as shared
import reference_water_transition_oracle as transition

require, integer, numeric, vector = shared.require, shared.integer, shared.numeric, shared.vector
PHASES = ('high-a', 'low-b', 'high-return', 'low-return')
SCENARIO = 'plane-switch-v1'
DOMAIN = 'hellomine3d-reference-water-plane-selection-independent-oracle-v1'
SUMMARY = 'hellomine3d-reference-water-plane-selection-summary-v1'
JOURNAL = 'hellomine3d-reference-water-plane-selection-journal-v1'
MAX_BYTES = 256 * 1024**2
PLANE = (transition.f32(66.9), transition.f32(64.9))
POSE_XZ = ((194.5, -182.5), (176.5, -175.5))
COLUMNS = {(194, y, -183): (7, 0) for y in (64, 65, 66)}
COLUMNS.update({(194, y, -183): (0, 0) for y in (67, 68)})
COLUMNS[(194, 63, -183)] = (32, 0)
COLUMNS.update({(176, y, -176): (7, 0) for y in (63, 64)})
COLUMNS.update({(176, y, -176): (0, 0) for y in (65, 66)})
COLUMNS[(176, 34, -176)] = (3, 0)
COLUMN_DOMAIN = 'blocking-World.getBlock-find-only-column-water-bed-witness'
OPEN = ['ordinary_input_water_route', 'continuous_transition_visual_quality',
        'native_pixel_fallback_of_other_water_layers', 'full_goal_acceptance']


def transpose(values):
    return [values[c*4+r] for r in range(4) for c in range(4)]


def product(a,b):
    return [sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]


def matrix_near(a,b):
    return all(abs(x-y) <= 5e-5 for x,y in zip(vector(a,16),vector(b,16)))


def fma32(a, b, c):
    """Single binary32 rounding of an exact product-plus-sum (ties to even)."""
    value = Fraction(a) * Fraction(b) + Fraction(c)
    if not value:
        return 0.0
    sign = -1 if value < 0 else 1
    value = abs(value)
    exponent = value.numerator.bit_length() - value.denominator.bit_length()
    if value < Fraction(2) ** exponent:
        exponent -= 1
    exponent = max(exponent, -126)
    rounded = sign * math.ldexp(round(value * Fraction(2) ** (23-exponent)), exponent-23)
    require(math.isfinite(rounded) and abs(rounded) <= 3.4028234663852886e38,
            'binary32 reference overflow')
    return rounded


def copied_mirror_camera(camera):
    # PlanarWaterReflection copies the actual main quaternion through
    # Ogre::Camera::setOrientation, which calls Quaternion::normalise again.
    # These actual macOS Debug/Release builds contract Norm to x*x (rounded),
    # then fused w*w+x*x, y*y+norm, z*z+norm; see the archived otool certificate.
    # Reusing the main cached view omits that normalization and is incorrect.
    w, x, y, z = vector(camera['quaternion_wxyz'], 4)
    require(all(transition.f32(v) == v for v in (w,x,y,z)), 'actual binary32 quaternion required')
    norm = fma32(w,w,transition.f32(x*x))
    norm = fma32(y,y,norm)
    norm = fma32(z,z,norm)
    require(norm > 0, 'zero actual camera quaternion')
    factor = transition.f32(1 / transition.f32(math.sqrt(norm)))
    return dict(camera, quaternion_wxyz=[transition.f32(v*factor) for v in (w,x,y,z)])


def png(data):
    # Preserve the complete CRC/pixel checks while bounding inflation before it
    # happens. Only the two actual production capture dimensions are admitted.
    require(data[:8] == b'\x89PNG\r\n\x1a\n', 'actual PNG signature missing')
    offset = 8; header = None; compressed = bytearray(); finished = False
    while offset + 12 <= len(data):
        size = struct.unpack_from('>I', data, offset)[0]; tag = data[offset+4:offset+8]
        require(size <= 32*1024**2 and offset+12+size <= len(data), 'PNG chunk bound')
        body = data[offset+8:offset+8+size]; crc = struct.unpack_from('>I', data, offset+8+size)[0]
        require(zlib.crc32(tag+body) & 0xffffffff == crc, 'PNG CRC mismatch')
        if tag == b'IHDR':
            require(header is None and size == 13, 'PNG duplicate/header')
            header = struct.unpack('>IIBBBBB', body)
        if tag == b'IDAT': compressed.extend(body)
        offset += size + 12
        if tag == b'IEND':
            require(size == 0 and offset == len(data), 'PNG EOF'); finished = True; break
    require(finished and header is not None and compressed, 'PNG incomplete')
    w,h,depth,colour,compression,filtering,interlace = header
    require((w,h) in ((1280,720),(2560,1440)), 'PNG actual bounded dimensions')
    require(depth == 8 and colour in (2,6) and compression == filtering == interlace == 0,
            'PNG actual 8bit RGB(A) required')
    expected = h * (1 + w * (3 if colour == 2 else 4))
    decoder = zlib.decompressobj(); raw = decoder.decompress(compressed, expected+1)
    require(len(raw) == expected and decoder.eof and not decoder.unused_data
            and not decoder.unconsumed_tail, 'PNG complete pixel bytes differ')
    return [w,h]


class Audit(transition.Audit):
    """Reuse only bounded files/JSON and actual attachment checks, not old phases."""
    def __init__(self, output, exit_code, mutation=None, cache=None):
        original = Path(output).absolute()
        require(not any(p.is_symlink() for p in (original,*original.parents)), 'native evidence symlink prohibited')
        super().__init__(output,exit_code,mutation,cache)

    def pixels(self, name):
        data = self.file(name); key = 'decoded-PNG:' + hashlib.sha256(data).hexdigest()
        if key not in self.cache: self.cache[key] = png(data)
        return self.cache[key]

    def phase(self, label, f, index, previous):
        frame = integer(f['frame'], 0, 2047); plane = PLANE[index % 2]
        self.check(label+'/frame-monotone', previous is None or frame > integer(previous['frame']))
        identity = f['identity']
        self.check(label+'/same-actual-identity', all(type(identity[k]) is str and identity[k] not in ('','0','0x0')
            for k in ('root_instance','scene_instance','window_instance','world_instance','world_id','save_directory'))
            and (previous is None or identity == previous['identity']))
        integer(identity['seed']); integer(identity['terrain_generation_version'])
        self.check(label+'/actual-owned-save-directory', identity['save_directory'] == str(self.output.parent/'save'))
        self.check(label+'/physical-window', [integer(v) for v in vector(f['physical_window'],2)] == [2560,1440])
        self.check(label+'/normal-update', numeric(f['simulation_delta']) > 0
            and integer(f['warm_frames'],12,511) >= 12 and integer(f['production_teleports'],12,2048) >= 12
            and (previous is None or numeric(f['world_time']) >= numeric(previous['world_time'])))
        player = vector(f['player']['position'],3); eye = vector(f['main_camera']['eye'],3)
        logic = vector(f['logic_camera']['position'],3); requested = vector(f['requested_player'],3)
        x,z = POSE_XZ[index % 2]
        self.check(label+'/fixed-requested-pose', requested == [x,(67.5,65.5)[index % 2],z])
        stationary = player[1] == requested[1]
        self.check(label+'/actual-production-pose', player[0] == requested[0] == x and player[2] == requested[2] == z
            and max(abs(a-b) for a,b in zip(eye,logic)) < 1e-5
            and ((index % 2 == 1 and not stationary) or abs(eye[1]-player[1]-.6) < 1e-5))
        # Every frame teleports through WorldManager, clearing velocity and
        # resetting previous=requested. The normal 20Hz tick then applies
        # gravity -40*.05; the actual falling low-phase sample therefore has
        # exactly one tick, v=(0,-2,0), and independently derived post-tick Y.
        # SandboxRuntime renders previous/current interpolation, not current Y.
        velocity = vector(f['player']['velocity'],3)
        post_tick_y = transition.f32(requested[1] + velocity[1] * transition.f32(.05))
        self.check(label+'/actual-low-gravity-single-tick', stationary or
            (index % 2 == 1 and velocity == [0,-2,0] and player[1] == post_tick_y))
        lower = transition.f32(min(player[1],requested[1]) + transition.f32(.6))
        upper = transition.f32(max(player[1],requested[1]) + transition.f32(.6))
        self.check(label+'/actual-camera-interpolation-segment', stationary or
            (lower-1e-5 <= eye[1] <= upper+1e-5))
        self.check(label+'/actual-camera-rotation', vector(f['player']['rotation'],3) == [5,45,0]
            and vector(f['logic_camera']['rotation'],3) == [5,45,0])
        self.check(label+'/actual-World-plane-domain', f['world_plane_observation'] == 'actual-World.observeWaterSurfacePlane-main-eye')
        self.check(label+'/actual-World-selected-plane', numeric(f['selected_plane_y']) == plane
            and numeric(f['eye_plane_delta']) == transition.f32(eye[1]-plane))
        medium = f['medium']; cell = [integer(v,-2**30,2**30) for v in vector(medium['cell'],3)]
        self.check(label+'/World-medium-domain', medium['observation_domain'] == 'actual-World.getBlock-main-eye'
            and cell == [math.floor(v) for v in eye] and medium['camera_underwater'] is False
            and integer(medium['id'],0,255) != 7)
        integer(medium['metadata'],0,255); integer(medium['above_id'],0,255)
        self.check(label+'/derived-active-phase', eye[1]-plane > .15
            and numeric(medium['surface_depth']) == 0 and numeric(medium['immersion']) == 0)
        blocks = f['water_column']; require(type(blocks) is list, 'actual both-column array')
        observed = {}
        for b in blocks:
            pos = tuple(integer(v,-2**30,2**30) for v in vector(b['position'],3))
            require(pos not in observed, 'duplicate column coordinate')
            observed[pos] = (integer(b['id'],0,255), integer(b['metadata'],0,255))
            self.check(label+'/column-domain-'+','.join(map(str,pos)), b['known'] is True and b['observation'] == COLUMN_DOMAIN
                and integer(b['observed_frame'],0,2047) == frame)
        self.check(label+'/actual-both-Water-Air-bed-columns', observed == COLUMNS)
        revision = integer(f['frame_input_scene_revision'])
        self.check(label+'/input-clock-separation', integer(f['later_current_world_visual_revision']) >= revision)
        self.storage(f['hdr'],2560,1440,4); self.check(label+'/HDR-active', f['hdr']['active'] is True)
        p = f['planar']; target = p['target']; pas = p['pass']; draw = f['draw']; uniforms = draw['uniforms']; sampler = draw['sampler']
        self.check(label+'/actual-component-plane', numeric(p['selected_plane_y']) == plane)
        self.check(label+'/actual-input-clock', integer(p['frame']) == frame and integer(p['scene_revision']) == revision
            and p['input_camera_underwater'] is False and p['input_linear_hdr'] is True
            and p['input_enabled'] is True and p['selected'] is True)
        self.check(label+'/bounded-component', integer(target['camera_count']) == 1
            and integer(target['target_count']) <= 1 and integer(target['depth_count']) <= 1
            and integer(target['private_materials']) <= 96 and integer(target['private_passes']) <= 384
            and target['listeners_active'] is False and integer(target['observer_failures']) == 0)
        self.check(label+'/actual-pass-domain', pas['observation_domain'] == 'actual-Ogre-Water-pass-parameter-and-TUS'
            and pas['tus_name'] == 'planarReflection')
        self.check(label+'/actual-driver-domain', draw['observation_domain'] == 'actual-driver-after-native-Water-draw'
            and integer(draw['frame']) == frame and integer(draw['program']) > 0 and draw['program_linked'] is True
            and draw['attached_production_stages'] is True and integer(draw['primitive_count']) > 0
            and draw['vertex_program'] == 'HelloMine3D/WaterVertex' and draw['fragment_program'] == 'HelloMine3D/WaterFragment'
            and draw['state_restored'] is True and integer(draw['gl_error']) == 0)
        blend = draw['blend']
        self.check(label+'/actual-transparent-state', blend['enabled'] is True and blend['depth_write'] is False
            and integer(blend['src_rgb']) == 770 and integer(blend['dst_rgb']) == 771)
        self.check(label+'/actual-driver-plane', numeric(uniforms['planarReflectionPlaneY']) == plane)
        self.check(label+'/actual-linear-uniform', numeric(uniforms['linearHdrMode']) == 1
            and numeric(uniforms['waterDetailStrength']) > 0 and numeric(uniforms['globalTime']) >= 0
            and numeric(uniforms['fogDensity']) > 0)
        self.check(label+'/actual-pass-plane', numeric(pas['plane_y']) == plane)
        count = integer(p['update_count']); floor = integer(f['update_floor'])
        self.check(label+'/target-update-consistency', integer(target['update_count']) == count
            and p['active'] is True and target['active'] is True and target['selected'] is True
            and target['binder_bound'] is True and target['lod_camera_bound'] is True
            and (previous is None or floor == integer(previous['planar']['update_count'])))
        self.check(label+'/new-same-frame-RTT', p['reason'] == 'rendered'
            and integer(p['last_rendered_frame']) == frame and count > floor
            and integer(p['colour_batches']) > 0 and integer(p['shadow_updates'],0,1) <= 1)
        self.storage(target,1280,720,0)
        self.check(label+'/native-sampler-actual-attachment', numeric(uniforms['planarReflectionEnabled']) == 1
            and numeric(pas['enabled']) == 1 and pas['tus_present'] is True and pas['tus_matches_target'] is True
            and integer(pas['tus_index']) >= 0 and pas['tus_texture_name'] == target['texture_name']
            and target['water_sampler_bound'] is True and sampler['sampling_enabled'] is True
            and integer(sampler['unit'],0,31) >= 0 and integer(sampler['location']) >= 0
            and [integer(sampler[k]) for k in ('texture','width','height','format')]
                == [target['native']['resolved']['object'],1280,720,34842])
        facts = self.file(label+'.facts.txt').decode(); values = dict(re.findall(r'([a-z_]+)=([^\s]+)',facts))
        raw = self.file(label+'.native-linear.rgba32f')
        self.check(label+'/actual-raw-budget', len(raw) == 1280*720*16)
        stats = 'float-stats:'+hashlib.sha256(raw).hexdigest()
        if stats not in self.cache:
            floats = array('f'); floats.frombytes(raw)
            self.cache[stats] = (len(floats), all(math.isfinite(v) for v in floats), min(floats), max(floats))
        size,finite,minimum,maximum = self.cache[stats]
        self.check(label+'/actual-raw-finite', size == 1280*720*4 and finite)
        self.check(label+'/actual-raw-signal', maximum > minimum)
        self.check(label+'/readback-facts', values['frame'] == str(frame) and values['scene_revision'] == str(revision)
            and values['width'] == '1280' and values['height'] == '720' and values['raw_bytes'] == str(len(raw))
            and values['gl_error_before'] == values['gl_error_after'] == values['nonfinite_components'] == '0'
            and values['main_water_texture_bound'] == '1'
            and values['colour_object'] == str(target['native']['resolved']['object'])
            and transition.f32(float(values['plane_y'])) == plane)
        flip = integer(int(values['requires_texture_flipping']),0,1) == 1
        main_view = vector(f['main_camera']['view_row_major'],16)
        independent_view = list(itertools.chain.from_iterable(shared.view_from_pose(f['main_camera'])))
        self.check(label+'/actual-main-view-from-camera-pose', all(abs(a-b) <= 5e-5 for a,b in zip(main_view,independent_view)))
        view = f['planar_view']
        self.check(label+'/component-view-current-frame', integer(view['frame']) == frame
            and integer(view['scene_revision']) == revision and integer(view['update_count']) == count
            and numeric(view['plane_y']) == plane and numeric(view['clip_y']) == transition.f32(plane+transition.f32(.03))
            and view['texture_flip'] is flip and view['camera_name'] == target['camera_name']
            and integer(view['target']['generation']) == integer(target['generation'])
            and integer(view['target']['native']['resolved']['object']) == integer(target['native']['resolved']['object']))
        reflect = [1,0,0,0, 0,-1,0,2*plane, 0,0,1,0, 0,0,0,1]
        reflected_view = vector(view['view_row_major'],16)
        projection = vector(view['projection_row_major'],16)
        component = vector(view['view_projection_row_major'],16)
        mirror_camera = copied_mirror_camera(f['main_camera'])
        mirror_view = list(itertools.chain.from_iterable(shared.view_from_pose(mirror_camera)))
        self.check(label+'/actual-reflected-view-from-main-World-plane', matrix_near(reflected_view,product(mirror_view,reflect)))
        self.check(label+'/component-projection-times-view', matrix_near(component,product(projection,reflected_view)))
        self.check(label+'/actual-matrix-domains', pas['matrix_observation_domain'] == 'actual-Ogre-Water-fragment-pass-raw-constants'
            and draw['matrix_observation_domain'] == 'actual-driver-linked-Water-uniform-column-major'
            and type(pas['params_transpose_matrices']) is bool and type(pas['vertex_column_major_matrices']) is bool
            and type(pas['fragment_column_major_matrices']) is bool)
        raw_pass = vector(pas['matrix_raw16'],16)
        actual_pass = transpose(raw_pass) if pas['params_transpose_matrices'] else raw_pass
        self.check(label+'/actual-Ogre-raw-to-component-matrix', pas['matrix_matches_component'] is True and matrix_near(actual_pass,component))
        # Local OgreGLSLMonolithicProgram.cpp uses the VERTEX stage flag for
        # a GPT_FRAGMENT_PROGRAM upload. GL readback itself is column-major.
        uploaded = raw_pass if pas['vertex_column_major_matrices'] else transpose(raw_pass)
        driver = transpose(vector(draw['planar_matrix_column_major16'],16))
        self.check(label+'/actual-GL-matrix-layout-and-upload', matrix_near(driver,uploaded))
        readback = vector([float(v) for v in values['view_projection_row_major'].split(',')],16)
        self.check(label+'/actual-component-driver-readback-matrix', matrix_near(driver,component) and matrix_near(driver,readback))
        rows = shared.reflected_xyw(mirror_camera,plane,flip)
        self.check(label+'/independent-mirror-projection-xyw', all(abs(rows[r][c]-driver[row*4+c]) <= 5e-5
            for r,row in enumerate((0,1,3)) for c in range(4)))
        reflected = vector([float(v) for v in values['reflected_eye'].split(',')],3)
        self.check(label+'/actual-reflected-eye', all(abs(a-b) <= 5e-5 for a,b in zip(reflected,[eye[0],2*plane-eye[1],eye[2]])))
        self.check(label+'/preview-full-pixels', self.pixels(label+'.camera-preview.png') == [1280,720])
        self.check(label+'/main-full-pixels', self.pixels(label+'.main.png') == [2560,1440])
        return f

    def run(self):
        s = self.summary
        self.check('actual-process-exit', self.exit == 0)
        self.check('actual-summary-complete', s['schema'] == SUMMARY and s['scenario'] == SCENARIO and s['status'] == 'COMPLETE'
            and integer(s['completed_phases']) == 4 and integer(s['records']) == 16
            and s['normal_save'] is True and s['native_fault'] == '' and s['normal_simulation'] is True
            and s['normal_input'] is False and integer(s['input_event_count']) == 0)
        self.check('summary/hard-bound', integer(s['frame'],0,2047) >= 0 and 0 <= numeric(s['elapsed_ms']) < 30000)
        restoration = s['restoration']
        self.check('actual-origin-restored', restoration['production_teleport'] is True
            and vector(restoration['actual_player'],3) == vector(restoration['original_player'],3)
            and vector(restoration['actual_rotation'],3) == vector(restoration['original_rotation'],3) == [5,45,0])
        expected = [(phase,event) for phase in PHASES for event in ('begin','observation','checkpoint','end')]
        self.check('journal/exact-protocol', len(self.events) == 16 and [(e['phase'],e['event']) for e in self.events] == expected)
        previous = None
        for i,phase in enumerate(PHASES):
            records = self.events[4*i:4*i+4]; begin,observation,checkpoint,end = records
            for seq,e in enumerate(records,4*i+1):
                self.check('record/'+str(seq), e['schema'] == JOURNAL and e['scenario'] == SCENARIO and integer(e['sequence']) == seq
                    and e['normal_input'] is False and integer(e['input_event_count']) == 0
                    and integer(e['frame'],0,2047) >= 0 and 0 <= numeric(e['elapsed_ms']) < 30000)
            self.check(phase+'/phase-bounds', 11 <= checkpoint['frame']-begin['frame'] < 512
                and 0 <= checkpoint['elapsed_ms']-begin['elapsed_ms'] < 8000
                and checkpoint['frame'] == observation['frame'] == end['frame']
                and checkpoint['facts'] == observation['facts'] and checkpoint['facts']['frame'] == checkpoint['frame'])
            self.check(phase+'/actual-teleport-begin', begin['facts']['production_teleport'] is True
                and vector(begin['facts']['requested_player'],3) == checkpoint['facts']['requested_player'])
            previous = self.phase(phase,checkpoint['facts'],i,previous)
        self.check('final-frame-summary', s['frame'] == previous['frame'])
        self.check('actual-world-clock-progressed', previous['world_time'] > self.events[2]['facts']['world_time'])
        files = list(self.output.iterdir())
        self.check('whole-output-bound', len(files) <= 32 and all(p.is_file() and not p.is_symlink() for p in files)
            and sum(p.stat().st_size for p in files) <= MAX_BYTES)
        exact = {'journal.jsonl','summary.json',*(phase+suffix for phase in PHASES
            for suffix in ('.main.png','.native-linear.rgba32f','.camera-preview.png','.facts.txt'))}
        self.check('only-declared-captures', {p.name for p in files} == exact)
        meta = self.output.parent/'save/world.meta'
        self.check('owned-saved-metadata-kind', meta.is_file() and not any(p.is_symlink() for p in (meta,*meta.parents))
            and meta.stat().st_size < 65536)
        data = meta.read_bytes(); self.inputs[str(meta)] = {'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
        lines = data.decode().splitlines()
        ids = [line[9:] for line in lines if line.startswith('world_id ')]
        seeds = [line[5:] for line in lines if line.startswith('seed ')]
        generations = [line[27:] for line in lines if line.startswith('terrain_generation_version ')]
        self.check('actual-World-id-matches-normal-save', ids == [previous['identity']['world_id']])
        self.check('actual-World-seed-generation-match-normal-save', seeds == [str(previous['identity']['seed'])]
            and generations == [str(previous['identity']['terrain_generation_version'])])
        return {'schema':DOMAIN,'status':'PASS','checks':len(self.checks),'failures':0,'checks_detail':self.checks,
            'inputs':self.inputs,'scope':'same Root actual World plane -> current component/pass/linked-driver matrix/RTT, input0 normal simulation',
            'ordinary_acceptance':'NOT_RUN','open':OPEN}


def evaluate(output, exit_code, mutation=None, cache=None):
    a = None
    try:
        a = Audit(output,exit_code,mutation,cache); return a.run(),a
    except (shared.Rejected,KeyError,ValueError,TypeError,OSError,zlib.error,struct.error) as e:
        return {'schema':DOMAIN,'status':'FAIL','checks':len(a.checks) if a else 0,'failures':1,'reason':str(e),
                'checks_detail':a.checks if a else [],'inputs':a.inputs if a else {},'ordinary_acceptance':'NOT_RUN','open':OPEN},a


def native_fault_facts(a):
    require(a is not None and a.exit == 1 and a.summary['schema'] == SUMMARY
        and a.summary['status'] == 'FAILED' and a.summary['scenario'] == SCENARIO
        and a.summary['native_fault'] == 'retain-selected-plane' and a.summary['normal_input'] is False
        and integer(a.summary['input_event_count']) == 0 and integer(a.summary['completed_phases']) == 1,
        'actual deliberate failed plane-selection process required')
    before = [e for e in a.events if e['event'] == 'checkpoint']
    wrong = [e for e in a.events if e['event'] == 'observation' and e['phase'] == 'low-b']
    require(len(before) == len(wrong) == 1 and before[0]['phase'] == 'high-a', 'actual high-A then low-B observation required')
    baseline = before[0]['facts']; f = wrong[0]['facts']; p = f['planar']; d = f['draw']
    frame = integer(f['frame'],0,2047)
    require(shared.blocks_map(f['water_column']) == COLUMNS and all(b['known'] is True
        and b['observation'] == COLUMN_DOMAIN and integer(b['observed_frame']) == frame for b in f['water_column']),
        'actual fault both resident water/Air/bed witnesses required')
    require(f['requested_player'] == [176.5,65.5,-175.5] and integer(f['warm_frames'],12,511) >= 12
        and numeric(f['simulation_delta']) > 0, 'actual normal-warm low pose required')
    require(f['identity'] == baseline['identity'] and f['medium']['camera_underwater'] is False
        and f['world_plane_observation'] == 'actual-World.observeWaterSurfacePlane-main-eye'
        and numeric(f['selected_plane_y']) == PLANE[1] and numeric(f['eye_plane_delta']) > .15,
        'same-Root actual dry low-World selection required')
    require(numeric(p['selected_plane_y']) == PLANE[0] and numeric(p['pass']['plane_y']) == PLANE[0]
        and numeric(d['uniforms']['planarReflectionPlaneY']) == PLANE[0]
        and d['program_linked'] is True and integer(d['frame']) == integer(f['frame']) and integer(d['gl_error']) == 0,
        'actual component/driver stale high plane required')
    require(not any((a.output/('low-b'+suffix)).exists() for suffix in
        ('.native-linear.rgba32f','.camera-preview.png','.facts.txt')), 'old RTT must not be labelled current low readback')
    return {'status':'EXPECTED_NATIVE_FAULT_REJECTED','actual_exit_code':1,'same_identity':True,'frame':f['frame'],
        'actual_World_plane':f['selected_plane_y'],'actual_component_plane':p['selected_plane_y'],
        'actual_driver_plane':d['uniforms']['planarReflectionPlaneY'],'observation_preserved_before_rejection':True,
        'positive_status':'FAIL','old_RTT_not_substituted_for_low_current_readback':True}


def phase_fault(fn, phase='low-b'):
    def mutate(events):
        for e in events:
            if e['phase'] == phase and e['event'] in ('observation','checkpoint'): fn(e['facts'])
        return events
    return mutate


def calibrate(output, cache):
    # Actual original files stay immutable. Both identical fact copies mutate
    # together, so the semantic fault cannot be caught only by equality tokens.
    def summary(k,v):
        def mutate(s): s[k] = v; return s
        return mutate
    def set_uniform(k,v): return phase_fault(lambda f: f['draw']['uniforms'].__setitem__(k,v))
    def wrong_height(events):
        for event in events:
            if event['phase'] == 'low-b' and event['event'] in ('begin','observation','checkpoint'):
                event['facts']['requested_player'][1] = 67.5
        return events
    def stale_matrix(events):
        old = copy.deepcopy(events[2]['facts'])
        for e in events:
            if e['phase'] == 'low-b' and e['event'] in ('observation','checkpoint'):
                e['facts']['draw']['planar_matrix_column_major16'] = old['draw']['planar_matrix_column_major16']
                e['facts']['planar']['pass']['matrix_raw16'] = old['planar']['pass']['matrix_raw16']
        return events
    def outside_interpolation(f):
        eye_y = transition.f32(f['requested_player'][1] + transition.f32(.6) + .2)
        f['main_camera']['eye'][1] = eye_y
        f['logic_camera']['position'][1] = eye_y
    faults = {
        'wrong-low-physics-Y':({'events':phase_fault(lambda f: f['player']['position'].__setitem__(1,transition.f32(f['requested_player'][1]-.2)))}, 'actual-low-gravity-single-tick'),
        'wrong-low-gravity-velocity':({'events':phase_fault(lambda f: f['player']['velocity'].__setitem__(1,-4))}, 'actual-low-gravity-single-tick'),
        'eye-outside-interpolation':({'events':phase_fault(outside_interpolation)}, 'actual-camera-interpolation-segment'),
        'wrong-scenario':({'summary':summary('scenario','single-plane')}, 'actual-summary-complete'),
        'wrong-requested-height':({'events':wrong_height}, 'fixed-requested-pose'),
        'false-column-known':({'events':phase_fault(lambda f: f['water_column'][0].__setitem__('known',False))}, 'column-domain-'),
        'forged-matrix-layout':({'events':phase_fault(lambda f: f['planar']['pass'].__setitem__('params_transpose_matrices',not f['planar']['pass']['params_transpose_matrices']))}, 'actual-Ogre-raw-to-component-matrix'),
        'bad-driver-column-layout':({'events':phase_fault(lambda f: f['draw'].__setitem__('planar_matrix_column_major16',transpose(f['draw']['planar_matrix_column_major16'])))}, 'actual-GL-matrix-layout-and-upload'),
        'component-projection-change':({'events':phase_fault(lambda f: f['planar_view']['projection_row_major'].__setitem__(0,0))}, 'component-projection-times-view'),
        'false-complete':({'summary':summary('completed_phases',3)}, 'actual-summary-complete'),
        'wrong-phase':({'events':lambda e: [dict(v,phase='high-a') if v['phase']=='low-b' else v for v in e]}, 'journal/exact-protocol'),
        'old-journal-schema':({'events':lambda e: [dict(v,schema='hellomine3d-reference-water-transition-journal-v1') if v['phase']=='low-b' else v for v in e]}, 'record/5'),
        'bad-origin-restore':({'summary':lambda s: dict(s,restoration=dict(s['restoration'],actual_player=[0,0,0]))}, 'actual-origin-restored'),
        'old-summary-schema':({'summary':summary('schema','hellomine3d-reference-water-transition-summary-v1')}, 'actual-summary-complete'),
        'fake-ordinary':({'summary':summary('normal_input',True)}, 'actual-summary-complete'),
        'no-normal-save':({'summary':summary('normal_save',False)}, 'actual-summary-complete'),
        'bool-frame':({'summary':summary('frame',True)}, 'invalid/bounded integer'),
        'over-frame-budget':({'summary':summary('frame',2048)}, 'invalid/bounded integer'),
        'over-time-budget':({'summary':summary('elapsed_ms',30000)}, 'summary/hard-bound'),
        'cold-Root':({'events':phase_fault(lambda f: f['identity'].__setitem__('root_instance','0x1234'))}, 'same-actual-identity'),
        'requested-not-actual':({'events':phase_fault(lambda f: f['player']['position'].__setitem__(0,194.5))}, 'actual-production-pose'),
        'wrong-World-medium':({'events':phase_fault(lambda f: f['medium'].__setitem__('camera_underwater',True))}, 'World-medium-domain'),
        'wrong-water-meta':({'events':phase_fault(lambda f: f['water_column'][0].__setitem__('metadata',3))}, 'actual-both-Water-Air-bed-columns'),
        'wrong-Air':({'events':phase_fault(lambda f: next(v for v in f['water_column'] if v['position']==[176,65,-176]).__setitem__('id',7))}, 'actual-both-Water-Air-bed-columns'),
        'wrong-bed':({'events':phase_fault(lambda f: next(v for v in f['water_column'] if v['position']==[176,34,-176]).__setitem__('id',32))}, 'actual-both-Water-Air-bed-columns'),
        'fake-column-domain':({'events':phase_fault(lambda f: f['water_column'][0].__setitem__('observation','saved-lookup'))}, 'column-domain-'),
        'bool-observed-frame':({'events':phase_fault(lambda f: f['water_column'][0].__setitem__('observed_frame',True))}, 'invalid/bounded integer'),
        'fake-World-plane-domain':({'events':phase_fault(lambda f: f.__setitem__('world_plane_observation','requested-plane'))}, 'actual-World-plane-domain'),
        'old-World-plane':({'events':phase_fault(lambda f: f.__setitem__('selected_plane_y',PLANE[0]))}, 'actual-World-selected-plane'),
        'old-component-plane':({'events':phase_fault(lambda f: f['planar'].__setitem__('selected_plane_y',PLANE[0]))}, 'actual-component-plane'),
        'old-driver-plane':({'events':set_uniform('planarReflectionPlaneY',PLANE[0])}, 'actual-driver-plane'),
        'old-pass-plane':({'events':phase_fault(lambda f: f['planar']['pass'].__setitem__('plane_y',PLANE[0]))}, 'actual-pass-plane'),
        'old-sampling-matrix':({'events':stale_matrix}, 'actual-Ogre-raw-to-component-matrix'),
        'stale-rendered-frame':({'events':phase_fault(lambda f: f['planar'].__setitem__('last_rendered_frame',f['frame']-1))}, 'new-same-frame-RTT'),
        'noncurrent-input':({'events':phase_fault(lambda f: f['planar'].__setitem__('scene_revision',f['frame_input_scene_revision']-1))}, 'actual-input-clock'),
        'missing-TUS':({'events':phase_fault(lambda f: f['planar']['pass'].__setitem__('tus_present',False))}, 'native-sampler-actual-attachment'),
        'sampler-alias':({'events':phase_fault(lambda f: f['draw']['sampler'].__setitem__('texture',0))}, 'native-sampler-actual-attachment'),
        'disabled-driver':({'events':set_uniform('planarReflectionEnabled',0)}, 'native-sampler-actual-attachment'),
        'unlinked-program':({'events':phase_fault(lambda f: f['draw'].__setitem__('program_linked',False))}, 'actual-driver-domain'),
        'GL-error':({'events':phase_fault(lambda f: f['draw'].__setitem__('gl_error',1280))}, 'actual-driver-domain'),
        'bad-HDR-samples':({'events':phase_fault(lambda f: f['hdr']['native']['colour'].__setitem__('samples',0))}, 'native/colour-and-depth'),
        'bad-RTT-format':({'events':phase_fault(lambda f: f['planar']['target']['native']['resolved'].__setitem__('format',32856))}, 'native/resolved-half-float'),
        'normal-delta-zero':({'events':phase_fault(lambda f: f.__setitem__('simulation_delta',0))}, 'normal-update'),
        'raw-nonfinite':({'bytes':lambda n,b: struct.pack('<f',float('nan'))+b[4:] if n=='low-return.native-linear.rgba32f' else b}, 'actual-raw-finite'),
        'PNG-corrupt':({'bytes':lambda n,b: b[:-1]+bytes([b[-1]^1]) if n=='high-return.main.png' else b}, 'PNG CRC mismatch'),
    }
    baseline,_ = evaluate(output,0,cache=cache)
    require(baseline['status'] == 'PASS', 'calibration requires one actual complete positive')
    results = []
    for name,(mutation,gate) in faults.items():
        result,_ = evaluate(output,0,mutation,cache)
        results.append({'case':name,'expected':'FAIL','actual':result['status'],'expected_gate':gate,
            'expected_gate_reached':gate in result.get('reason',''),'reason':result.get('reason')})
    result,_ = evaluate(output,1,cache=cache)
    results.append({'case':'nonzero-process-exit','expected':'FAIL','actual':result['status'],
        'expected_gate':'actual-process-exit','expected_gate_reached':result.get('reason')=='actual-process-exit',
        'reason':result.get('reason')})
    return {'schema':DOMAIN+'-calibration','source':'one actual COMPLETE positive and strictly mutated copies; no constructed positive',
        'status':'PASS' if all(r['actual']=='FAIL' and r['expected_gate_reached'] for r in results) else 'FAIL',
        'positive':1,'negative_cases':len(results),'results':results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--process-exit-code',required=True,type=int)
    parser.add_argument('--result',required=True,type=Path)
    parser.add_argument('--calibration',type=Path)
    args = parser.parse_args(); output = args.output.resolve(strict=True)
    require(not any(p.is_symlink() for p in (args.output.absolute(),*args.output.absolute().parents)), 'native evidence symlink prohibited')
    for path in (args.result,args.calibration):
        if path is not None:
            require(not any(p.is_symlink() for p in (path.absolute(),*path.absolute().parents)), 'oracle result symlink prohibited')
            resolved = path.resolve()
            require(not path.exists() and not (resolved.is_relative_to(output) or output.is_relative_to(resolved)),
                'oracle outputs must be new and outside immutable native evidence')
    require(args.calibration is None or args.result.resolve() != args.calibration.resolve(), 'distinct result/calibration paths required')
    result,a = evaluate(output,args.process_exit_code)
    if result['status']=='FAIL' and a is not None and a.summary.get('native_fault')=='retain-selected-plane':
        try: result['native_negative_control'] = native_fault_facts(a)
        except (shared.Rejected,KeyError,TypeError,ValueError) as error:
            result['native_negative_control'] = {'status':'NOT_ESTABLISHED','reason':str(error)}
    args.result.parent.mkdir(parents=True,exist_ok=True)
    with args.result.open('x') as stream: stream.write(__import__('json').dumps(result,indent=2)+'\n')
    if args.calibration:
        require(result['status']=='PASS', 'calibration requires actual native positive')
        calibrated = calibrate(output,a.cache); args.calibration.parent.mkdir(parents=True,exist_ok=True)
        with args.calibration.open('x') as stream: stream.write(__import__('json').dumps(calibrated,indent=2)+'\n')
        require(calibrated['status']=='PASS','fault copy did not reach its specified semantic gate')
    print(__import__('json').dumps({'status':result['status'],'checks':result['checks'],'failures':result['failures'],
        'reason':result.get('reason'),'result':str(args.result)}))
    return 0 if result['status']=='PASS' else 1

if __name__=='__main__': sys.exit(main())
