#!/usr/bin/env python3
"""Independent semantic audit of bounded, default-off native World edits.

Consumes real observations; never launches a client or creates a passing input.
Ordinary input/menu acceptance and the original lifecycle oracle stay separate.
"""
import argparse
from array import array
import copy
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import struct
import sys
import time

PHASES = ('baseline-a', 'edited-b', 'restored-a')
LABELS = ('matched-a0', *PHASES)
SCHEMA = 'hellomine3d-reference-world-edit-independent-oracle-v1'
MAX_JSON, MAX_EVENTS = 8 * 1024**2, 64
MAX_FILE, MAX_ALL_BYTES = 1920 * 1080 * 16, 512 * 1024**2
TARGETS = {'lamp': {(201, 69, -186): (44, 0)},
           'wall': {(x, y, -196): (35, 0) for x in (199, 200) for y in (68, 69, 70)},
           'shore': {(194, 66, -183): (7, 3)}}
OPEN = ['ordinary_input', 'ordinary_editing_and_save_reopen',
        'ordinary_settings_lifecycle', 'full_goal_exit_conditions']

class Rejected(ValueError):
    pass

def require(condition, message):
    if not condition:
        raise Rejected(message)

def integer(value, low=0, high=2**64 - 1):
    require(type(value) is int and low <= value <= high, 'invalid/bounded integer')
    return value

def numeric(value):
    require(type(value) in (int, float) and math.isfinite(value), 'nonfinite numeric fact')
    return value

def vector(value, count):
    require(type(value) is list and len(value) == count, 'vector dimension differs')
    return [numeric(v) for v in value]

def strict_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result

def decode(data):
    return json.loads(data, object_pairs_hook=strict_object,
                      parse_constant=lambda v: (_ for _ in ()).throw(Rejected('nonfinite JSON: ' + v)))

def sha(data):
    return hashlib.sha256(data).hexdigest()

def finite_tree(value):
    if type(value) in (float, int):
        numeric(value)
    elif type(value) is dict:
        for v in value.values():
            finite_tree(v)
    elif type(value) is list:
        for v in value:
            finite_tree(v)

def quat_rotation(q):
    w, x, y, z = vector(q, 4)
    require(abs(sum(v*v for v in q) - 1) <= 2e-6, 'actual camera quaternion is not unit')
    return ((1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)),
            (2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)),
            (2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)))

def view_from_pose(camera):
    eye = vector(camera['eye'], 3)
    rotation = quat_rotation(camera['quaternion_wxyz'])
    rows = [[rotation[c][r] for c in range(3)] for r in range(3)]
    return [row + [-sum(row[c]*eye[c] for c in range(3))] for row in rows] + [[0, 0, 0, 1]]

def reflected_xyw(camera, plane_y, flipped):
    """Independent pinhole x/y/w rows. Oblique clipping changes only z row."""
    view = view_from_pose(camera)
    fovy, aspect = numeric(camera['fov_y_radians']), numeric(camera['aspect'])
    require(0 < fovy < math.pi and aspect > 0 and camera['frustum_offset'] == [0, 0],
            'pinhole lens or nonzero frustum offset outside this fixed fixture')
    cot = 1 / math.tan(fovy / 2)
    rows = [[v * cot / aspect for v in view[0]],
            [v * cot * (-1 if flipped else 1) for v in view[1]],
            [-v for v in view[2]]]
    # Reflect input world point across y=planeY before the original camera view.
    for row in rows:
        old_y = row[1]
        row[1] = -old_y
        row[3] += 2 * plane_y * old_y
    return rows

def project(rows, point, width, height):
    p = vector(list(point), 3) + [1]
    x, y, w = [sum(a*b for a, b in zip(row, p)) for row in rows]
    require(w > 0.01, 'edit/source surface projection behind camera')
    return ((x/w + 1) * width/2, (y/w + 1) * height/2)

def rectangle(points, rows, width, height, padding=4):
    xy = [project(rows, p, width, height) for p in points]
    bounds = (max(0, math.floor(min(p[0] for p in xy))-padding),
              max(0, math.floor(min(p[1] for p in xy))-padding),
              min(width, math.ceil(max(p[0] for p in xy))+padding),
              min(height, math.ceil(max(p[1] for p in xy))+padding))
    require(bounds[0] < bounds[2] and bounds[1] < bounds[3], 'target ROI does not intersect actual RTT')
    return bounds

def overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]

def changed_pixels(a, b, roi, width):
    require(len(a) == len(b), 'raw image byte count differs')
    result = 0
    for y in range(roi[1], roi[3]):
        start, end = (y*width+roi[0])*16, (y*width+roi[2])*16
        ar, br = a[start:end], b[start:end]
        if ar == br:
            continue
        result += sum(ar[x:x+16] != br[x:x+16] for x in range(0, len(ar), 16))
    return result

def blocks_map(blocks):
    require(type(blocks) is list and 1 <= len(blocks) <= 64, 'bounded actual block observations required')
    result = {}
    for b in blocks:
        p = tuple(integer(v, -2**30, 2**30) for v in vector(b['position'], 3))
        require(p not in result, 'duplicate actual block coordinate')
        result[p] = (integer(b['id'], 0, 255), integer(b['metadata'], 0, 255))
    return result

class Audit:
    def __init__(self, session, file_cache=None, mutations=None):
        self.session = Path(session).resolve(strict=True)
        self.edit = self.session / 'edit'
        self.checks, self.inputs, self.derived = [], {}, {}
        self.started = time.monotonic()
        self.file_cache = file_cache if file_cache is not None else {}
        self.mutations = mutations or {}
        self.run = self.json_file(self.session/'run.json')
        self.summary = self.json_file(self.edit/'summary.json')
        lines = self.file(self.edit/'journal.jsonl', MAX_JSON).decode('utf8').splitlines()
        require(1 <= len(lines) <= MAX_EVENTS and all(line for line in lines), 'journal count/blank outside bounds')
        self.events = [decode(line) for line in lines]
        if 'run' in self.mutations: self.run = self.mutations['run'](copy.deepcopy(self.run))
        if 'summary' in self.mutations: self.summary = self.mutations['summary'](copy.deepcopy(self.summary))
        if 'events' in self.mutations: self.events = self.mutations['events'](copy.deepcopy(self.events))
        self.mode = self.run.get('mode')
        require(self.mode in TARGETS, 'unknown edit mode')
        finite_tree(self.run); finite_tree(self.summary); finite_tree(self.events)
        self.partial = [{'seq':e.get('seq'), 'phase':e.get('phase'), 'facts':e.get('facts')}
                        for e in self.events if e.get('event') in ('failure', 'checkpoint')]

    def file(self, path, maximum=MAX_FILE):
        path = Path(path)
        require(path.is_absolute() and '..' not in path.parts, 'absolute safe evidence path required')
        for p in (path, *path.parents):
            require(not p.is_symlink(), 'evidence symlink forbidden')
        resolved = path.resolve(strict=True)
        require(resolved.is_file() and 0 < resolved.stat().st_size <= maximum, 'evidence size outside bound: ' + str(path))
        if str(resolved) not in self.file_cache:
            self.file_cache[str(resolved)] = resolved.read_bytes()
        data = self.file_cache[str(resolved)]
        if 'bytes' in self.mutations:
            data = self.mutations['bytes'](resolved, data, self.file_cache)
        self.inputs[str(resolved)] = {'bytes':len(data), 'sha256':sha(data)}
        require(sum(v['bytes'] for v in self.inputs.values()) <= MAX_ALL_BYTES, 'total evidence exceeds512MiB')
        return data

    def json_file(self, path):
        value = decode(self.file(path, MAX_JSON))
        require(type(value) is dict, 'JSON object required')
        return value

    def evidence(self, relative):
        require(type(relative) is str and relative and len(relative) <= 4096, 'bounded evidence filename required')
        path = Path(relative)
        require(not path.is_absolute() and '..' not in path.parts and len(path.parts) == 1,
                'evidence must be an immediate edit child')
        return self.edit/path

    def check(self, name, condition, detail=None):
        require(time.monotonic()-self.started <= 60, 'oracle exceeds60-second bound')
        self.checks.append({'name': name, 'status':'PASS' if condition else 'FAIL',
                            **({'detail':detail} if detail is not None else {})})
        require(condition, name)

    def protocol(self):
        r, s = self.run, self.summary
        self.check('run/actual-natural-success', r['schema']=='hellomine3d-reference-world-edit-run-v1' and
                   r['launch_count']==1 and type(r['child_returncode']) is int and r['child_returncode']==0 and
                   r.get('child_signal') is None and not r.get('external_deadline_exceeded',False) and
                   not r.get('owned_child_cleanup_required',False) and r['result']=='NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE')
        self.check('scope/hidden-no-input-no-perf', r['evidence_type']=='DEVELOPER_DIAGNOSTIC' and
                   r['normal_input'] is False and r['input_actions']==0 and r['performance_isolation_claimed'] is False and
                   r['native_fault_requested'] is None and s['normal_input'] is False and s['input_event_count']==0)
        self.check('summary/three-phases-four-captures-no-fault', s['schema']=='reference-world-edit-v1' and
                   s['status']=='COMPLETE' and s['mode']==self.mode and s['completed_phases']==3 and
                   s['matched_control_captured'] is True and s['readbacks']==4 and s['fault']=='' and
                   s['reason']=='' and s['time_parameters_restored'] is True and s['normal_world_save_returned'] is True)
        self.check('summary/fixed-bounds', integer(s['frames'],0,2047)<2048 and 0 <= numeric(s['elapsed_ms']) < 30000 and
                   s['simulation_delta']==0 and s['animation_time']==4)
        expected = [('header', 'baseline-a'), ('begin','baseline-a'), ('matched-control','baseline-a'),
                    ('checkpoint','baseline-a'), ('end','baseline-a'), ('begin','edited-b'),
                    ('mutation','edited-b'), ('checkpoint','edited-b'), ('end','edited-b'), ('begin','restored-a'),
                    ('mutation','restored-a'), ('checkpoint','restored-a'), ('end','restored-a'), ('save','complete')]
        self.check('journal/exact-stage-and-control-sequence', [(e['event'],e['phase']) for e in self.events] == expected)
        self.check('journal/starts-one-contiguous', [e['seq'] for e in self.events] == list(range(1,len(self.events)+1)))
        self.check('journal/fixed-mode-frames-time', all(e['mode']==self.mode and 0 <= integer(e['frame'])<2048 and
                   0 <= numeric(e['elapsed_ms'])<30000 for e in self.events) and
                   all(a['frame']<=b['frame'] and a['elapsed_ms']<=b['elapsed_ms'] for a,b in zip(self.events,self.events[1:])))
        self.header = self.events[0]['facts']
        self.check('header/strict-engineering-limits', self.header['normal_input'] is False and
                   self.header['input_event_count']==0 and self.header['simulation_delta']==0 and
                   self.header['performance'] is False and self.header['window_mode']=='hidden' and
                   self.header['maximum_frames']==2048 and self.header['maximum_seconds']==30 and
                   self.header['phase_maximum_seconds']==8 and self.header['maximum_journal_records']==64 and
                   self.header['readback_budget']==4 and self.header['fault']=='')
        self.begins = {e['phase']:e for e in self.events if e['event']=='begin'}
        self.checkpoints = {e['phase']:e for e in self.events if e['event']=='checkpoint'}
        self.control = next(e for e in self.events if e['event']=='matched-control')
        for phase in PHASES:
            begin, checkpoint = self.begins[phase], self.checkpoints[phase]
            self.check(phase+'/waits-real-frames-within-bound', checkpoint['frame']-begin['frame']>=12 and
                       checkpoint['elapsed_ms']-begin['elapsed_ms']<8000)
        self.check('control/prior-real-frame', self.control['frame'] < self.checkpoints['baseline-a']['frame'])

    def identity(self):
        r = self.run
        identity, cert, loaded = r['package_identity'], r['current_source_certificate'], r['loaded_executable']
        self.check('identity/protected-source-and-template', r['source_app_all_files_unchanged'] is True and
                   r['save_template_all_files_unchanged'] is True and
                   r['save_clone_initial_hashes']==r['template_file_hashes_before'])
        self.check('identity/current-full-source-certificate', cert['all_entries_current_exact'] is True and
                   integer(cert['matching_entries'],1,10000)>0 and cert['receipt_sha256']==identity['source_manifest_sha256'])
        runtime = Path(r['runtime_app'])
        self.check('identity/owned-runtime-layout', Path(r['session']).resolve()==self.session and runtime==self.session/'Runtime.app' and
                   r['command']==[str(runtime/'Contents/Resources/bin/HelloMine3D')])
        executable = self.file(runtime/'Contents/Resources/bin/HelloMine3D',128*1024**2)
        self.check('identity/actual-native-executable', loaded['verified'] is True and
                   loaded['executable_sha256']==identity['executable_sha256']==sha(executable))
        receipt = self.file(Path(loaded['receipt']),MAX_JSON)
        native_receipt=decode(receipt)
        self.check('identity/lsof-original-receipt', sha(receipt)==loaded['receipt_sha256'] and
                   native_receipt['pid']==integer(r['game_pid'],1,2**31) and
                   native_receipt['expected']==str(runtime/'Contents/Resources/bin/HelloMine3D') and
                   native_receipt['executable_sha256']==sha(executable) and
                   any(a.get('returncode')==0 and
                       re.search(r'^n'+re.escape(str(runtime/'Contents/Resources/bin/HelloMine3D'))+r'$',a.get('stdout',''),re.M) is not None and
                       re.search(r'^p'+str(r['game_pid'])+r'$',a.get('stdout',''),re.M) is not None
                       for a in native_receipt['attempts']))
        src = self.file(runtime/'Contents/Resources/source-tree-sha256.txt',MAX_JSON)
        resources = self.file(runtime/'Contents/Resources/media/resource-manifest.txt',MAX_JSON)
        self.check('identity/actual-source-and-resource-receipts', sha(src)==identity['source_manifest_sha256'] and
                   sha(resources)==identity['resource_manifest_sha256'])
        self.check('identity/owned-marker', self.file(self.session/'.hellomine3d-reference-world-edit-owned',256)==
                   b'HelloMine3D owned reference world edit session v1\n')
        self.check('identity/fixed-effective-environment', all(r['environment'].get(k)==v for k,v in {
            'HELLOMINE3D_WINDOW_HIDDEN':'1','HELLOMINE3D_REFERENCE_EDIT_PROBE':self.mode,
            'HELLOMINE3D_REFERENCE_EDIT_DIR':str(self.edit),'HELLOMINE3D_SAVE_DIR':str(self.session/'save'),
            'HELLOMINE3D_CATALOGUE_DIR':str(self.session/'catalogue'),'HELLO_RENDER_CAPTURE':'1',
            'HELLO_RENDER_CAPTURE_MS':'60000','HELLO_RENDER_CAPTURE_EXIT':'0','HELLO_PERF_CAPTURE':'0',
            'HELLOMINE3D_PLAYER_POSITION':'195.5 68.02 -176.2','HELLOMINE3D_PLAYER_ROTATION':'5 45 0',
            'HELLOMINE3D_WORLD_TIME':'6000','HELLOMINE3D_SEED':'42','HELLOMINE3D_MSAA4':'1'}.items()))

    def report(self, error=None):
        return {'schema':SCHEMA,'oracle_source_sha256':sha(Path(__file__).read_bytes()),
                'status':'FAIL' if error else 'PASS_SCOPED_WORLD_EDIT_RENDER',
                'mode':self.mode, 'normal_input':False, 'ordinary_acceptance_closed':False,
                'scope':'Actual owned World edit, current original upload/draw, linked local light uniforms and native RTT causality only.',
                'check_count':len(self.checks),'pass_count':sum(c['status']=='PASS' for c in self.checks),
                'fail_count':max(int(bool(error)),sum(c['status']=='FAIL' for c in self.checks)), 'checks':self.checks,
                'inputs':self.inputs,'derived':self.derived,'open':OPEN,
                **({'error':str(error),'partial_native_observations':self.partial} if error else {})}

    def target_storage(self, facts, dimensions, samples, label):
        w, h = dimensions
        self.check(label+'/bounded-native-target', facts['active'] is True and facts['width']==w and facts['height']==h and
                   w*h <= (3840*2160 if samples else 1920*1080) and facts['target_count']==1 and
                   facts['depth_count']==1 and facts['owned_depth_attached'] is True and facts['depth_pool']==0 and
                   facts['observer_failures']==0)
        n = facts['native']
        self.check(label+'/complete-real-FBO-no-GL-errors', n['complete'] is True and integer(n['resolve_fbo'],1)>0 and
                   integer(n['draw_fbo'],1)>0 and n['gl_error_before']==n['gl_error_after']==0)
        for kind, wanted_samples in (('colour', samples), ('resolved', 0), ('depth', samples)):
            a = n[kind]
            expected_kind = 5890 if kind=='resolved' or (kind=='colour' and samples==0) else 36161
            self.check(label+'/'+kind+'-real-storage', a['kind']==expected_kind and integer(a['object'],1)>0 and
                       a['width']==w and a['height']==h and a['samples']==wanted_samples and
                       (a['format']==34842 if kind!='depth' else a['format'] in (33189,33190,33191,35056,36012,36013)))
        stencil = n['stencil']
        self.check(label+'/stencil-storage-or-absence', stencil['kind']==0 and stencil['object']==0 or
                   stencil['kind']==36161 and stencil['object']==n['depth']['object'] and
                   stencil['width']==w and stencil['height']==h and stencil['samples']==samples)
        self.check(label+'/same-draw-resolve-FBO-for-no-MSAA', samples>0 or n['resolve_fbo']==n['draw_fbo'])

    def section_readiness(self, facts, label, expected_ready):
        """Derive selected readiness; offered CPU items are a budgeted subset."""
        summary, sections = facts['section_readiness'], facts['sections']
        total = integer(summary['global_cpu_ready_total'],0,2**32-1)
        deferred = integer(summary['global_cpu_ready_deferred'],0,total)
        offered = integer(summary['offered_cpu_ready_total'],0,total)
        self.check(label+'/same-snapshot-offered-budget-facts', summary['sections']==sections and
                   summary['selected_ready'] is facts['sections_ready'] and
                   offered==total-deferred)
        locations=set()
        derived_ready=bool(sections)
        selected_offered=0
        for section in sections:
            location=tuple(integer(v,-2**30,2**30) for v in vector(section['location'],3))
            require(location not in locations,'duplicate selected readiness section')
            locations.add(location)
            live=integer(section['live_revision'],1,2**32-1)
            uploaded=section['uploaded_revision']
            known=uploaded is not None
            if known: integer(uploaded,1,2**32-1)
            current=known and uploaded==live
            self.check(label+'/section-'+str(location)+'/factual-offered-and-revision-flags',
                       type(section['gpu_resident']) is bool and type(section['offered_cpu_ready']) is bool and
                       section['cpu_ready'] is section['offered_cpu_ready'] and
                       section['upload_known'] is known and section['revision_current'] is current)
            selected_offered+=int(section['offered_cpu_ready'])
            derived_ready=derived_ready and current and section['gpu_resident'] and not section['offered_cpu_ready']
        targets={(p[0]//16,p[1]//16,p[2]//16) for p in TARGETS[self.mode]}
        self.check(label+'/independently-derived-selected-readiness', locations==targets and
                   type(facts['sections_ready']) is bool and facts['sections_ready'] is derived_ready and
                   facts['sections_ready'] is expected_ready and selected_offered<=offered)
        self.derived.setdefault('section_readiness_observations',[]).append({
            'label':label,'selected_ready':derived_ready,'global_cpu_ready_total':total,
            'global_cpu_ready_deferred':deferred,'offered_cpu_ready_total':offered,
            'scope':'CPU-ready fields describe budgeted offers, not the complete mesh-state set; current selected live/upload revisions establish readiness.'})

    def world(self):
        target = TARGETS[self.mode]
        baseline = blocks_map(self.checkpoints['baseline-a']['facts']['blocks'])
        self.check('world/fixed-real-baseline-cells', set(baseline)==set(target) and
                   all(baseline[p][0]==ids[0] for p, ids in target.items()))
        previous_revision, previous_sections = None, None
        for phase in PHASES:
            e = self.checkpoints[phase]; f = e['facts']
            actual = blocks_map(f['blocks'])
            expected = {p:(ids[1],0) for p,ids in target.items()} if phase=='edited-b' else baseline
            self.check(phase+'/actual-world-ID-and-metadata', actual==expected)
            rev = integer(f['frame_input_scene_revision'],1)
            self.check(phase+'/new-authoritative-revision', previous_revision is None or rev>previous_revision)
            previous_revision = rev
            self.check(phase+'/ready-current-sections', f['sections_ready'] is True and type(f['sections']) is list and
                       1 <= len(f['sections']) <= 8)
            current = {}
            for s in f['sections']:
                loc = tuple(integer(x,-2**30,2**30) for x in vector(s['location'],3))
                require(loc not in current, 'duplicate real section')
                current[loc]=integer(s['live_revision'],1,2**32-1)
                self.check(phase+'/section-'+str(loc)+'/current-upload-and-residency',
                           s['uploaded_revision']==s['live_revision'] and s['gpu_resident'] is True and s['cpu_ready'] is False)
            expected_sections = {(p[0]//16,p[1]//16,p[2]//16) for p in target}
            self.check(phase+'/actual-target-section-coverage', set(current)==expected_sections)
            self.check(phase+'/section-revision-increases', previous_sections is None or
                       all(current[p]>previous_sections[p] for p in current))
            previous_sections=current
        for phase in PHASES:
            self.section_readiness(self.checkpoints[phase]['facts'],phase,True)
        self.section_readiness(self.control['facts'],'matched-a0',True)
        self.original_blocks = baseline
        for phase in ('edited-b','restored-a'):
            mutation=next(e for e in self.events if e['event']=='mutation' and e['phase']==phase)
            m=mutation['facts']; end=self.checkpoints[phase]['facts']
            self.section_readiness(m,phase+'/mutation',False)
            previous=self.checkpoints['baseline-a' if phase=='edited-b' else 'edited-b']['facts']
            self.check(phase+'/real-post-edit-before-upload-observation', mutation['frame']==self.begins[phase]['frame'] and
                       blocks_map(m['blocks'])==blocks_map(end['blocks']) and
                       m['world_visual_revision']<=end['frame_input_scene_revision'] and m['sections_ready'] is False)
            old={tuple(s['location']):s for s in previous['sections']}
            current={tuple(s['location']):s for s in end['sections']}
            self.check(phase+'/actual-live-revision-leads-old-upload',
                       set(tuple(s['location']) for s in m['sections'])==set(current) and
                       all(s['uploaded_revision']==old[tuple(s['location'])]['uploaded_revision'] and
                           s['live_revision']<=current[tuple(s['location'])]['live_revision'] and
                           s['uploaded_revision']<s['live_revision'] for s in m['sections']))
        saved=self.events[-1]['facts']
        self.check('save/actual-production-save-return',saved['normal_world_save_returned'] is True and
                   blocks_map(saved['blocks'])==baseline)
        self.check('control/actual-same-world-before-edit', blocks_map(self.control['facts']['blocks'])==baseline and
                   self.control['facts']['sections']==self.checkpoints['baseline-a']['facts']['sections'])

    def draw(self, draw, facts, phase):
        name = phase+'/draw-'+str(len(self.inputs))
        self.check(name+'/actual-production-original-draw', draw['view'] in ('main','reflection') and
                   draw['frame']==facts['frame'] and draw['program_linked'] is True and
                   draw['production_attached_shaders'] is True and draw['gl_is_buffer'] is True and
                   all(integer(draw[key],1)>0 for key in ('program','vao','vbo','ibo')) and
                   draw['gl_error']==0 and draw['state_restored'] is True)
        attributes=draw['attributes']
        expected={'vertex':(3,0),'uv0':(2,12),'uv1':(2,20),'uv2':(3,28),'uv3':(1,40)}
        self.check(name+'/actual-VAO-fetch-from-read-VBO',type(attributes) is list and
                   {a['name'] for a in attributes}>={'vertex','uv2'} and
                   len(attributes)==len(set(a['name'] for a in attributes)) and
                   all(a['name'] in expected and a['enabled'] is True and a['buffer']==draw['vbo'] and
                       a['type']==5126 and (a['size'],a['pointer_offset'])==expected[a['name']] and
                       a['stride']==44 and a['normalized'] is False and a['integer'] is False and
                       a['divisor']==0 and type(a['location']) is int and a['location']>=0 for a in attributes))
        loc = tuple(vector(draw['section'],3))
        sections = {tuple(s['location']):s for s in facts['sections']}
        self.check(name+'/draw-current-section-revision', loc in sections and
                   draw['uploaded_revision']==sections[loc]['live_revision'] and integer(draw['upload_serial'],1)>0)
        vbo = self.file(self.evidence(draw['vbo_file']),16*1024**2)
        ibo = self.file(self.evidence(draw['ibo_file']),16*1024**2)
        cvbo = self.file(self.evidence(draw['cpu_vbo_file']),16*1024**2)
        cibo = self.file(self.evidence(draw['cpu_ibo_file']),16*1024**2)
        self.check(name+'/independent-CPU-and-real-GL-bytes', vbo==cvbo and ibo==cibo and
                   len(vbo)==draw['vertex_bytes'] and len(ibo)==draw['index_bytes'] and
                   len(vbo)%44==0 and len(ibo)%12==0 and draw['cpu_gpu_bytes_equal'] is True)
        vertices = list(struct.iter_unpack('=11f',vbo))
        indices = list(struct.iter_unpack('=I',ibo))
        self.check(name+'/independent-index-topology-and-finite-attributes',
                   all(all(math.isfinite(v) for v in vert) for vert in vertices) and
                   all(i[0]<len(vertices) for i in indices) and
                   draw['primitive_count']==draw['expected_triangles']==len(ibo)//12)
        self.check(name+'/actual-draw-shader-time', type(draw['time_uniforms']) is list and
                   all(t['name'] in ('globalTime','legacyTime') and t['value']==4 for t in draw['time_uniforms']))
        actual = draw['local_lights']
        if actual is not None:
            world = facts['local_lights']; sources = world['sources']
            pos = [s['position']+[s['radius']] for s in sources] + [[0,0,0,0]]*(8-len(sources))
            colour = [s['colour']+[s['energy']] for s in sources] + [[0,0,0,0]]*(8-len(sources))
            self.check(name+'/actual-linked-light-uniforms-from-World', actual['domain']=='actual-linked-GL-program-after-production-draw' and
                       actual['count']==world['count']==len(sources) and actual['position_radius']==pos and actual['colour_energy']==colour)
        return {'draw':draw,'vertices':vertices,'indices':[i[0] for i in indices],'sha':sha(vbo),'index_sha':sha(ibo)}

    def draws(self):
        self.draw_observations = {}
        for e, label in [(self.control,'matched-a0')]+[(self.checkpoints[p],p) for p in PHASES]:
            f = e['facts']
            self.check(label+'/bounded-original-draws', type(f['draws']) is list and 1 <= len(f['draws'])<=32)
            self.draw_observations[label]=[self.draw(d,f,label) for d in f['draws']]
            self.check(label+'/main-and-private-linked-PBR-draws', all(any(d['view']==view and d['local_lights'] is not None
                       for d in f['draws']) for view in ('main','reflection')))
            world = f['local_lights']
            self.check(label+'/bounded-light-query-and-sources', type(world['sources']) is list and
                       len(world['sources'])==integer(world['count'],0,8) and integer(world['inspected_sections'],0,27)<=27 and
                       integer(world['inspected_cells'],0,27*4096)<=27*4096 and integer(world['candidates'])>=world['count'] and
                       all(len(vector(s['position'],3))==3 and len(vector(s['colour'],3))==3 and
                           0<numeric(s['radius'])<=12 and numeric(s['energy'])>0 for s in world['sources']))
        if self.mode=='lamp':
            source_maps = []
            for phase in PHASES:
                src=self.checkpoints[phase]['facts']['local_lights']['sources']
                mapped={tuple(s['position']):s for s in src}
                require(len(mapped)==len(src),'duplicate local light source')
                source_maps.append(mapped)
            a,b,c=source_maps
            emitter=(201.5,69.5,-185.5)
            self.check('lamp/actual-target-source-only-removed-and-restored', emitter in a and emitter not in b and c==a and
                       {p:s for p,s in a.items() if p!=emitter}==b)

    def frozen_and_native(self):
        base = self.checkpoints['baseline-a']['facts']
        frozen = ('camera','actors','world_time','animation_time','simulation_delta','physical_window','world_instance','root_instance','scene_instance')
        self.check('identity/live-real-owner-addresses', all(type(base[k]) is str and
                   re.fullmatch(r'0x[0-9a-fA-F]+',base[k]) and int(base[k],16)>0
                   for k in ('world_instance','root_instance','scene_instance')))
        self.raw = {}
        for event,label in [(self.control,'matched-a0')]+[(self.checkpoints[p],p) for p in PHASES]:
            f = event['facts']; planar=f['planar']
            self.check(label+'/actual-scene-and-time-frozen', all(f[k]==base[k] for k in frozen) and
                       f['world_time']==6000 and f['animation_time']==4 and f['simulation_delta']==0 and
                       f['normal_input'] is False and f['input_event_count']==0 and f['frame']==event['frame'] and
                       f['save_directory']==str(self.session/'save') and f['main_readback_gl_error']==0)
            self.check(label+'/actual-physical-window', f['physical_window']==[2560,1440])
            self.target_storage(f['hdr'],(2560,1440),4,label+'/hdr')
            self.target_storage(planar['target'],(1280,720),0,label+'/planar')
            t=planar['target']
            self.check(label+'/authority-timepoints-recorded-without-relabel',
                       f['frame_local_light_revision']==f['local_lights']['revision'] and
                       1<=f['frame_local_light_revision']<=f['frame_input_scene_revision']<=f['later_current_world_visual_revision'] and
                       f['world_visual_revision']==f['later_current_world_visual_revision'])
            self.check(label+'/same-frame-current-native-reflection', planar['frame']==event['frame'] and
                       planar['scene_revision']==f['frame_input_scene_revision'] and integer(planar['update_count'],1)>0 and
                       t['selected'] is True and t['binder_bound'] is True and t['water_sampler_bound'] is True and
                       t['lod_camera_bound'] is True and t['listeners_active'] is False and t['camera_count']==1 and
                       1<=t['private_materials']<=96 and 1<=t['private_passes']<=384)
            actual_view = vector(f['camera']['view_row_major'],16)
            independent_view = list(itertools.chain.from_iterable(view_from_pose(f['camera'])))
            self.check(label+'/actual-main-view-from-camera-pose', all(abs(a-b)<=5e-5 for a,b in zip(actual_view,independent_view)))
            rows=reflected_xyw(f['camera'],numeric(planar['plane_y']),planar['texture_flip'])
            vp=vector(planar['view_projection_row_major'],16)
            self.check(label+'/independent-mirror-projection-xyw', all(abs(rows[r][c]-vp[row*4+c])<=5e-5
                       for r,row in enumerate((0,1,3)) for c in range(4)))
            self.check(label+'/frozen-planar-projection', all(planar[k]==base['planar'][k] for k in
                       ('plane_y','clip_y','texture_flip','view_row_major','projection_row_major','view_projection_row_major')))
            prefix=f['planar_prefix']
            self.check(label+'/exact-evidence-label', prefix==label and f['main_png']==label+'.main.png')
            raw=self.file(self.evidence(prefix+'.native-linear.rgba32f'))
            self.check(label+'/real-float32-RTT-byte-size', len(raw)==1280*720*16)
            stats_key='float-stats:'+sha(raw)
            if stats_key not in self.file_cache:
                floats=array('f');floats.frombytes(raw)
                self.file_cache[stats_key]=(sum(not math.isfinite(v) for v in floats),min(floats),max(floats))
            nonfinite,minimum,maximum=self.file_cache[stats_key]
            self.check(label+'/raw-no-nonfinite-values', nonfinite==0)
            facts_text=self.file(self.evidence(prefix+'.facts.txt'),65536).decode('utf8')
            self.check(label+'/actual-native-readback-text-contract',
                       'raw_format=RGBA_FLOAT32_NATIVE_ENDIAN raw_origin=GL_BOTTOM_LEFT source=actual_native_RGBA16F_RTT' in facts_text and
                       re.search(r'\bframe='+str(event['frame'])+r'\b',facts_text) is not None and
                       re.search(r'\bscene_revision='+str(f['frame_input_scene_revision'])+r'\b',facts_text) is not None and
                       'gl_error_before=0 gl_error_after=0 nonfinite_components=0' in facts_text and
                       'main_water_texture_bound=1' in facts_text)
            png=self.file(self.evidence(f['main_png']))
            self.check(label+'/real-main-window-PNG-dimensions', png[:8]==b'\x89PNG\r\n\x1a\n' and
                       png[12:16]==b'IHDR' and struct.unpack('>II',png[16:24])==(2560,1440))
            self.raw[label]=raw
            self.derived.setdefault('raw_observations',[]).append({'label':label,'minimum':minimum,'maximum':maximum,'sha256':sha(raw)})
        self.check('freeze/matched-A0-and-A-whole-RTT-exact', self.raw['matched-a0']==self.raw['baseline-a'])
        self.check('restore/A-prime-whole-RTT-exact', self.raw['restored-a']==self.raw['baseline-a'])
        updates=[self.control['facts']['planar']['update_count']]+[self.checkpoints[p]['facts']['planar']['update_count'] for p in PHASES]
        self.check('reflection/real-sequential-updates', all(a<b for a,b in zip(updates,updates[1:])))
        self.projection_rows=reflected_xyw(base['camera'],base['planar']['plane_y'],base['planar']['texture_flip'])

    def causality(self):
        base=self.checkpoints['baseline-a']['facts']; plane_y=base['planar']['plane_y']
        points=[]
        for x,y,z in TARGETS[self.mode]:
            lo_y=max(y,plane_y+.03)
            require(lo_y<y+1,'edited cell lies wholly below oblique clipping plane')
            points.extend(itertools.product((x,x+1),(lo_y,y+1),(z,z+1)))
        roi=rectangle(points,self.projection_rows,1280,720)
        a,b,c=(self.raw[p] for p in PHASES)
        count=changed_pixels(a,b,roi,1280)
        self.check('pixels/edited-world-projected-ROI-changes',count>0,{'ROI_GL_bottom_left':roi,'changed_pixels':count})
        mx,my=math.ceil(1280*.01),math.ceil(720*.01)
        control_x=(mx,1280//3-8,2*1280//3-8,1280-mx-16)
        controls=[(x,my,x+16,my+16) for x in control_x]
        # Prospective fixed sky domain, not selected from B's difference map.
        # Every corner ray begins below the clip half-space and points down;
        # no retained World surface can intersect this image domain.
        rotation=quat_rotation(base['camera']['quaternion_wxyz'])
        virtual_eye_y=2*plane_y-base['camera']['eye'][1]
        tangent=math.tan(base['camera']['fov_y_radians']/2)
        directions=[]
        for control in controls:
            rays=[]
            for x,y in itertools.product((control[0]+.5,control[2]-.5),(control[1]+.5,control[3]-.5)):
                ndcx,ndcy=2*x/1280-1,2*y/720-1
                camera_ray=(ndcx*base['camera']['aspect']*tangent,
                            ndcy*tangent*(-1 if base['planar']['texture_flip'] else 1),-1)
                ray_y=-sum(rotation[1][k]*camera_ray[k] for k in range(3))
                rays.append(ray_y)
            directions.append(rays)
        self.check('pixels/four-fixed-sky-control-rays-outside-retained-World',
                   virtual_eye_y<base['planar']['clip_y'] and all(y<0 for rays in directions for y in rays))
        self.derived['control_domain']='four prospective16x16 sky ray domains; no retained World intersection'
        self.derived['control_mirrored_eye_y']=virtual_eye_y
        self.derived['control_ray_world_y_directions']=directions
        self.check('pixels/independent-fixed-controls-outside-target',all(not overlap(control,roi) for control in controls))
        for i,control in enumerate(controls):
            first=(control[1]*1280+control[0])*16
            colour=a[first:first+16]
            self.check('pixels/control'+str(i)+'/actual-A-uniform-sky-domain',
                       all(a[(y*1280+x)*16:(y*1280+x)*16+16]==colour
                           for y in range(control[1],control[3]) for x in range(control[0],control[2])))
            self.check('pixels/control'+str(i)+'/unchanged-A-B-A-prime',changed_pixels(a,b,control,1280)==0 and
                       changed_pixels(a,c,control,1280)==0)
        self.derived['edit_ROI_GL_bottom_left']=roi
        self.derived['fixed_control_ROIs_GL_bottom_left']=controls
        self.derived['edited_ROI_changed_pixels']=count
        if self.mode=='lamp':
            self.lamp_surface(roi)

    def lamp_surface(self, emitter_roi):
        # Fixed authored+actual unchanged StoneBrick front face. Select before
        # native pixels; exclude the removed emitter projection independently.
        corners=[(201,68,-191),(202,68,-191),(202,69,-191),(201,69,-191)]
        roi=rectangle(corners,self.projection_rows,1280,720)
        controls=self.derived['fixed_control_ROIs_GL_bottom_left']
        self.check('lamp/fixed-control-ROIs-exclude-unmodified-receiver',all(not overlap(c,roi) for c in controls))
        count=0;outside=0
        a,b=(self.raw[p] for p in ('baseline-a','edited-b'))
        for y in range(roi[1],roi[3]):
            for x in range(roi[0],roi[2]):
                if emitter_roi[0]<=x<emitter_roi[2] and emitter_roi[1]<=y<emitter_roi[3]:continue
                outside+=1;at=(y*1280+x)*16;count+=a[at:at+16]!=b[at:at+16]
        self.check('lamp/unmodified-receiver-ROI-excludes-emitter-and-changes',outside>0 and count>0,
                   {'ROI_GL_bottom_left':roi,'pixels_outside_emitter':outside,'changed_pixels':count})
        self.derived['lamp_receiver_ROI_GL_bottom_left']=roi
        self.derived['lamp_receiver_changed_pixels_excluding_emitter']=count
        samples=[]
        world_samples=[]
        for phase in PHASES:
            f=self.checkpoints[phase]['facts']
            receiver=f['receiver']
            air=receiver['air_samples']
            self.check(phase+'/lamp-receiver-and-neighbour-cells-unmodified', receiver['cell']==[201,68,-192] and
                       receiver['id']==35 and receiver['metadata']==0 and receiver['face']=='positive-z' and
                       receiver['corners']==[list(c) for c in corners] and len(air)==4 and
                       {tuple(s['position']) for s in air}=={(x,y,-191) for x in (201,202) for y in (68,69)} and
                       all(s['id']==0 and s['metadata']==0 and 0<=integer(s['block'])<=15 and 0<=integer(s['sky'])<=15 for s in air))
            world_samples.append({tuple(s['position']):s['block'] for s in air})
            # A scalar is independently interpolated from original fetched
            # triangles, allowing a legitimate larger greedy rectangle at B.
            values=[]
            for observed in self.draw_observations[phase]:
                d=observed['draw']
                if d['view']!='reflection' or d['local_lights'] is None:continue
                origin=[x*16 for x in d['section']]
                vertices=observed['vertices'];indices=observed['indices']
                for start in range(0,len(indices),3):
                    v=[vertices[i] for i in indices[start:start+3]]
                    p=[[vtx[i]+origin[i] for i in range(3)] for vtx in v]
                    if not all(q[2]==-191 for q in p):continue
                    x,y=201.5,68.5
                    den=(p[1][1]-p[2][1])*(p[0][0]-p[2][0])+(p[2][0]-p[1][0])*(p[0][1]-p[2][1])
                    if den==0:continue
                    u=((p[1][1]-p[2][1])*(x-p[2][0])+(p[2][0]-p[1][0])*(y-p[2][1]))/den
                    w=((p[2][1]-p[0][1])*(x-p[2][0])+(p[0][0]-p[2][0])*(y-p[2][1]))/den
                    t=1-u-w
                    if min(u,w,t)<0:continue
                    values.append(sum(weight*vtx[9] for weight,vtx in zip((u,w,t),v)))
            self.check(phase+'/lamp-actual-receiver-GL-uv2-observed',bool(values) and max(values)-min(values)<=1e-7)
            samples.append(values[0])
        self.check('lamp/real-uploaded-receiver-blockSource-decreases-and-restores',samples[1]<samples[0] and samples[2]==samples[0])
        self.check('lamp/actual-World-receiver-block-light-decreases-and-restores',world_samples[2]==world_samples[0] and
                   all(world_samples[1][p]<=world_samples[0][p] for p in world_samples[0]) and
                   any(world_samples[1][p]<world_samples[0][p] for p in world_samples[0]))
        self.derived['lamp_receiver_actual_GL_uv2_y_plus_one']=samples
        self.derived['lamp_receiver_World_blockLight']=[{str(p):v for p,v in state.items()} for state in world_samples]

    def saved_world(self):
        r=self.run
        final=r['save_clone_final_hashes']
        metadata=self.file(self.session/'save/world.meta',65536)
        self.check('save/actual-final-world-meta-receipt', sha(metadata)==r['save_clone_final_world_meta_sha256'] and
                   metadata.decode('utf8')==r['save_clone_final_world_meta_text'] and final['world.meta']==sha(metadata))
        def world_id(data):
            values=[line[9:] for line in data.decode('utf8').splitlines() if line.startswith('world_id ')]
            require(len(values)==1 and values[0], 'actual unique saved world_id missing')
            return values[0]
        template=Path(r['save_template'])
        template_meta=self.file(template/'world.meta',65536)
        self.check('save/same-actual-world-identity',world_id(metadata)==world_id(template_meta))
        positions=set(self.original_blocks)|{(201,68,-192)}|{(x,y,-191) for x in (201,202) for y in (68,69)}
        self.saved_observations=[]
        chunks={}
        for p in sorted(positions):
            cx,cz=p[0]//16,p[2]//16
            relative=f'chunks/chunk_{cx}_{cz}.hmcchunk'
            if (cx,cz) not in chunks:
                current=self.file(self.session/'save'/relative,32*1024**2)
                original=self.file(template/relative,32*1024**2)
                self.check('save/'+relative+'/actual-final-hash', final.get(relative)==sha(current))
                chunks[cx,cz]=(self.chunk(current,cx,cz),self.chunk(original,cx,cz))
            now,old=chunks[cx,cz]
            at=p[1]*256+(p[2]-cz*16)*16+(p[0]-cx*16)
            require(0<=at<len(now[0]) and at<len(old[0]),'saved target outside actual chunk bounds')
            saved=(now[0][at],now[1][at]);before=(old[0][at],old[1][at])
            expected=self.original_blocks.get(p,(35,0) if p==(201,68,-192) else (0,0))
            self.check('save/cell'+str(p)+'/normal-save-restored-ID-meta', saved==before==expected)
            self.saved_observations.append({'position':p,'before':before,'saved':saved})
        self.derived['actual_saved_cells']=self.saved_observations
        self.derived['actual_saved_world_id']=world_id(metadata)

    @staticmethod
    def chunk(data,cx,cz):
        require(len(data)>=32,'truncated current saved chunk')
        magic,version,x,z,size,sections=struct.unpack_from('<8sIiiII',data)
        require(magic==b'HMCHNK1\0' and version==2 and (x,z)==(cx,cz) and size==16 and 0<sections<=64,
                'invalid current saved chunk identity/header')
        count=sections*4096;at=28+count*2
        require(at+4<=len(data),'truncated saved ID/metadata')
        ids,metas=data[28:28+count],data[28+count:at]
        require(all(v<=44 for v in ids) and all(meta<=3 for block,meta in zip(ids,metas) if 33<=block<=44),
                'unknown current reference block/orientation')
        entities,=struct.unpack_from('<I',data,at);at+=4
        require(entities<=4096,'saved entity count bound')
        seen=set()
        for _ in range(entities):
            require(at+12<=len(data),'truncated saved entity coordinate')
            px,py,pz=struct.unpack_from('<iii',data,at);at+=12
            require(0<=px<16 and 0<=pz<16 and 0<=py<sections*16 and (px,py,pz) not in seen,
                    'saved entity outside chunk or duplicate');seen.add((px,py,pz))
            values=[]
            for maximum in (128,65536):
                require(at+4<=len(data),'truncated saved entity text length')
                length,=struct.unpack_from('<I',data,at);at+=4
                require(length<=maximum and at+length<=len(data),'saved entity text bound')
                values.append(data[at:at+length]);at+=length
            kind=values[0].decode('utf8')
            require({'hellomine:chest':16,'hellomine:furnace':19,'hellomine:crusher':26}.get(kind)==ids[py*256+pz*16+px],
                    'saved entity attachment mismatch')
        require(at==len(data),'unexpected trailing saved chunk bytes')
        return ids,metas

    def run_audit(self):
        self.protocol()
        self.identity()
        self.world()
        self.draws()
        self.frozen_and_native()
        self.causality()
        self.saved_world()


def audit(session,file_cache=None,mutations=None):
    instance=None
    try:
        instance=Audit(session,file_cache,mutations)
        instance.run_audit()
        return instance.report()
    except Exception as error:
        if instance is not None:
            return instance.report(error)
        return {'schema':SCHEMA,'oracle_source_sha256':sha(Path(__file__).read_bytes()),'status':'FAIL','normal_input':False,'ordinary_acceptance_closed':False,
                'check_count':0,'pass_count':0,'fail_count':1,'error':f'{type(error).__name__}: {error}','open':OPEN}


def calibration(session,output):
    """One real complete input + documented fault copies; never synthesize PASS."""
    cache={}
    positive=audit(session,cache)
    require(positive['status']=='PASS_SCOPED_WORLD_EDIT_RENDER','calibration requires an actual complete positive journal')
    output=Path(output).absolute()
    require(not output.exists(),'calibration output must be new')
    output.mkdir(parents=True)
    (output/'actual-positive.json').write_text(json.dumps(positive,ensure_ascii=False,indent=2)+'\n')
    def event_change(kind,phase,action):
        def change(events):
            matches=[e for e in events if e['event']==kind and e['phase']==phase]
            require(len(matches)==1,'fault copy requires unique actual event')
            action(matches[0]);return events
        return change
    def checkpoint(phase,action):
        return {'events':event_change('checkpoint',phase,lambda e:action(e['facts']))}
    def field(mapping,key,value):
        mapping[key]=value
    def selected_field(facts,key,value):
        # Keep duplicate snapshots coherent to reach the independently derived
        # section semantics rather than fail on an unrelated alias mismatch.
        field(facts['sections'][0],key,value)
        field(facts['section_readiness']['sections'][0],key,value)
    saved_position=sorted(TARGETS[positive['mode']])[0]
    saved_x,saved_y,saved_z=saved_position
    saved_cx,saved_cz=saved_x//16,saved_z//16
    saved_relative=f'chunks/chunk_{saved_cx}_{saved_cz}.hmcchunk'
    saved_path=(Path(session).resolve(strict=True)/'save'/saved_relative)
    saved_original=saved_path.read_bytes()
    saved_offset=28+saved_y*256+(saved_z-saved_cz*16)*16+(saved_x-saved_cx*16)
    require(saved_original[saved_offset]==TARGETS[positive['mode']][saved_position][0],
            'saved-target fault requires an actual original target ID')
    saved_bad_id=0 if saved_original[saved_offset]!=0 else 3
    saved_fault=saved_original[:saved_offset]+bytes([saved_bad_id])+saved_original[saved_offset+1:]
    def saved_target_run(run):
        # Match the deliberate fault-copy receipt so the independent binary
        # parser must reject restoration, rather than just a stale file hash.
        run['save_clone_final_hashes'][saved_relative]=sha(saved_fault)
        return run
    def saved_target_bytes(path,data,cache):
        return saved_fault if path==saved_path else data
    faults=[
        ('nonzero-actual-exit', {'run':lambda r:dict(r,child_returncode=1,result='FAILED')}),
        ('timeout', {'run':lambda r:dict(r,external_deadline_exceeded=True)}),
        ('normal-input-scope', {'run':lambda r:dict(r,normal_input=True,input_actions=1)}),
        ('source-protection', {'run':lambda r:dict(r,source_app_all_files_unchanged=False)}),
        ('missing-restored-stage', {'events':lambda e:[x for x in e if x['phase']!='restored-a']}),
        ('duplicate-baseline-stage', {'events':lambda e:e[:4]+[copy.deepcopy(e[3])]+e[4:]}),
        ('sequence-not-one', {'events':lambda e:[dict(x,seq=x['seq']+1) for x in e]}),
        ('missing-matched-control', {'events':lambda e:[x for x in e if x['event']!='matched-control']}),
        ('input-callback',checkpoint('edited-b',lambda f:field(f,'input_event_count',1))),
        ('wrong-world-block',checkpoint('edited-b',lambda f:field(f['blocks'][0],'id',255))),
        ('stale-section-upload',checkpoint('edited-b',lambda f:field(f['sections'][0],'uploaded_revision',f['sections'][0]['live_revision']-1))),
        ('false-offered-state-alias',checkpoint('edited-b',lambda f:selected_field(f,'cpu_ready',True))),
        ('false-revision-current-flag',checkpoint('edited-b',lambda f:selected_field(f,'revision_current',False))),
        ('wrong-global-offered-accounting',checkpoint('edited-b',lambda f:field(f['section_readiness'],'offered_cpu_ready_total',f['section_readiness']['global_cpu_ready_total']+1))),
        ('snapshot-readiness-alias-mismatch',checkpoint('edited-b',lambda f:field(f['section_readiness'],'selected_ready',False))),
        ('fake-camera-freeze',checkpoint('edited-b',lambda f:field(f['camera'],'eye',[f['camera']['eye'][0]+1,*f['camera']['eye'][1:]]))),
        ('unfrozen-animation-time',checkpoint('edited-b',lambda f:field(f,'animation_time',4.5))),
        ('stale-planar-frame',checkpoint('edited-b',lambda f:field(f['planar'],'frame',f['planar']['frame']-1))),
        ('stale-planar-scene-revision',checkpoint('edited-b',lambda f:field(f['planar'],'scene_revision',f['frame_input_scene_revision']-1))),
        ('native-GL-error',checkpoint('edited-b',lambda f:field(f['planar']['target']['native'],'gl_error_after',1282))),
        ('fake-native-size',checkpoint('edited-b',lambda f:field(f['planar']['target']['native']['colour'],'width',640))),
        ('stale-main-uniforms',checkpoint('edited-b',lambda f:next(field(d['local_lights'],'count',d['local_lights']['count']+1)
             for d in f['draws'] if d['view']=='main' and d['local_lights'] is not None))),
        ('stale-private-uniforms',checkpoint('edited-b',lambda f:next(field(d['local_lights'],'count',d['local_lights']['count']+1)
             for d in f['draws'] if d['view']=='reflection' and d['local_lights'] is not None))),
        ('bad-real-VAO-fetch',checkpoint('edited-b',lambda f:field(f['draws'][0]['attributes'][0],'buffer',f['draws'][0]['vbo']+1))),
        ('missing-live-upload-lag',{'events':lambda events:[e for e in events if e['event']!='mutation']}),
        ('bad-real-GPU-bytes', {'bytes':lambda p,b,c:(bytes([b[0]^1])+b[1:]) if p.name=='edited-b-draw0.vbo.bin' else b}),
        ('stale-native-RTT-with-current-metadata', {'bytes':lambda p,b,c:c[str(p.with_name('baseline-a.native-linear.rgba32f'))]
             if p.name=='edited-b.native-linear.rgba32f' else b}),
        ('unfrozen-control-RTT', {'bytes':lambda p,b,c:(bytes([b[0]^1])+b[1:]) if p.name=='matched-a0.native-linear.rgba32f' else b}),
        ('wrong-restored-World-block',checkpoint('restored-a',lambda f:field(f['blocks'][0],'id',255))),
        ('actual-saved-target-not-restored', {'run':saved_target_run,'bytes':saved_target_bytes}),
        ('broken-restored-RTT', {'bytes':lambda p,b,c:(bytes([b[0]^1])+b[1:]) if p.name=='restored-a.native-linear.rgba32f' else b}),
    ]
    control_roi=positive['derived']['fixed_control_ROIs_GL_bottom_left'][0]
    def corner_noise(path,data,cache):
        if path.name!='edited-b.native-linear.rgba32f':return data
        at=(control_roi[1]*1280+control_roi[0])*16
        return data[:at]+bytes([data[at]^1])+data[at+1:]
    faults.append(('nonedit-control-pixel-changes',{'bytes':corner_noise}))
    if positive['mode']=='lamp':
        actual_events=[decode(line) for line in (Path(session)/'edit/journal.jsonl').read_text().splitlines()]
        original_receiver=next(e['facts']['receiver'] for e in actual_events if e['event']=='checkpoint' and e['phase']=='baseline-a')
        faults.append(('stale-World-receiver-block-light',checkpoint('edited-b',lambda f:field(f,'receiver',copy.deepcopy(original_receiver)))))
        receiver_roi=positive['derived']['lamp_receiver_ROI_GL_bottom_left']
        emitter_roi=positive['derived']['edit_ROI_GL_bottom_left']
        def geometry_without_receiver_light(path,data,cache):
            if path.name!='edited-b.native-linear.rgba32f':return data
            original=cache[str(path.with_name('baseline-a.native-linear.rgba32f'))]
            modified=bytearray(data)
            for y in range(receiver_roi[1],receiver_roi[3]):
                for x in range(receiver_roi[0],receiver_roi[2]):
                    if emitter_roi[0]<=x<emitter_roi[2] and emitter_roi[1]<=y<emitter_roi[3]:continue
                    at=(y*1280+x)*16
                    modified[at:at+16]=original[at:at+16]
            return bytes(modified)
        faults.append(('emitter-geometry-only-without-receiver-light-pixels',{'bytes':geometry_without_receiver_light}))
    results=[]
    for name,mutation in faults:
        result=audit(session,cache,mutation)
        path=output/(name+'.json');path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
        rejected=result['status']=='FAIL'
        results.append({'name':name,'expected':'FAIL','actual':result['status'],'rejected':rejected,
                        'error':result.get('error'),'report':str(path),'sha256':sha(path.read_bytes())})
    report={'schema':SCHEMA+'-calibration','oracle_source_sha256':sha(Path(__file__).read_bytes()),'status':'PASS_FAULT_COPY_CALIBRATION' if all(r['rejected'] for r in results) else 'FAIL',
            'normal_input':False,'ordinary_acceptance_closed':False,'actual_positive':str(output/'actual-positive.json'),
            'case_count':1+len(results),'positive_actual_count':1,'fault_copy_count':len(results),
            'rejected_fault_copy_count':sum(r['rejected'] for r in results),'cases':results,
            'scope':'One actual complete journal plus deliberate in-memory fault copies and actual original artifact bytes; no synthetic success or client execution.'}
    (output/'calibration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True,help='New independent report file; never overwrite')
    parser.add_argument('--calibrate',action='store_true',help='Output becomes a new calibration directory from an actual positive only')
    args=parser.parse_args()
    if args.calibrate:
        report=calibration(args.session,args.output)
    else:
        require(not args.output.exists(),'independent output must be new')
        require(args.output.absolute()!=args.session.absolute() and args.output.suffix=='.json','new JSON output required')
        report=audit(args.session)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({key:report.get(key) for key in ('status','mode','check_count','pass_count','fail_count','case_count','error')},ensure_ascii=False))
    return 0 if report['status'] in ('PASS_SCOPED_WORLD_EDIT_RENDER','PASS_FAULT_COPY_CALIBRATION') else 1

if __name__=='__main__':
    try:
        raise SystemExit(main())
    except (Rejected,OSError,KeyError,ValueError,json.JSONDecodeError) as error:
        print(f'[REFERENCE_WORLD_EDIT_ORACLE] {type(error).__name__}: {error}')
        raise SystemExit(2)
