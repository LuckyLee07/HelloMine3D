#!/usr/bin/env python3
"""Independent audit of real resident departure, GPU retirement and storage return.

Reads actual owned observations. Never launches a client or constructs a success.
The original lifecycle/edit oracles and ordinary input acceptance are separate.
"""
import argparse
from array import array
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import time
import reference_world_edit_oracle as shared

require, integer, numeric, vector = shared.require, shared.integer, shared.numeric, shared.vector
decode, sha, finite_tree = shared.decode, shared.sha, shared.finite_tree
PHASES = ('resident-a', 'departed-b', 'returned-a')
SECTIONS = {(12, 4, -12), (12, 4, -13)}
CHUNKS = {(12, -12), (12, -13)}
WITNESS_SECTIONS = {(14, 4, -11), (14, 4, -12)}
WITNESS_CELLS = {(225,67,-175):(4,2), (225,72,-177):(5,2)}
CELLS = {(201,69,-186):(44,0), (194,66,-183):(7,0),
         (199,68,-196):(35,0), (196,66,-184):(33,2)}
SCHEMA = 'hellomine3d-reference-residency-independent-oracle-v1'
OPEN = ['ordinary_input', 'ordinary_walking_unload_return', 'ordinary_settings_restart',
        'ordinary_save_reopen', 'full_goal_exit_conditions']
MAX_JSON, MAX_BUFFER, MAX_DIAG = 8*1024**2, 16*1024**2, 256*1024**2

def world_id(data):
    values = [s[9:] for s in data.decode('utf8').splitlines() if s.startswith('world_id ')]
    require(len(values)==1 and values[0], 'actual unique world_id missing')
    return values[0]

def metadata_number(data,key):
    values=[line[len(key)+1:] for line in data.decode('utf8').splitlines() if line.startswith(key+' ')]
    require(len(values)==1 and re.fullmatch(r'[0-9]+',values[0]),'unique numeric world.meta field required: '+key)
    return integer(int(values[0]),0,2**64-1)

def location(value, n=3):
    return tuple(integer(x, -2**30, 2**30) for x in vector(value,n))

def tracked(value):
    p = location(value)
    return (p[0],p[2]) in CHUNKS

class Audit:
    def __init__(self, session, cache=None, mutations=None):
        self.session = Path(session).resolve(strict=True)
        self.output = self.session/'residency'
        self.cache = cache if cache is not None else {}
        self.mutations = mutations or {}
        self.checks, self.inputs, self.derived = [], {}, {}
        self.started = time.monotonic()
        self.run = self.json(self.session/'run.json')
        self.summary = self.json(self.output/'summary.json')
        lines = self.file(self.output/'journal.jsonl',MAX_JSON).decode('utf8').splitlines()
        require(1<=len(lines)<=128 and all(lines), 'journal outside128-record bound/blank')
        self.events = [decode(line) for line in lines]
        if 'run' in self.mutations: self.run=self.mutations['run'](copy.deepcopy(self.run))
        if 'summary' in self.mutations: self.summary=self.mutations['summary'](copy.deepcopy(self.summary))
        if 'events' in self.mutations: self.events=self.mutations['events'](copy.deepcopy(self.events))
        finite_tree(self.run); finite_tree(self.summary); finite_tree(self.events)
        self.partial = [e for e in self.events if e.get('event') in ('checkpoint','departed-observation','failure','gpu-retired')]

    def file(self,path,maximum=32*1024**2):
        p=Path(path)
        require(p.is_absolute() and '..' not in p.parts and
                all(not a.is_symlink() for a in (p,*p.parents)), 'safe non-symlink evidence path required')
        p=p.resolve(strict=True)
        require(p.is_file() and 0<p.stat().st_size<=maximum, 'actual file outside bound: '+str(p))
        key=str(p)
        if key not in self.cache:self.cache[key]=p.read_bytes()
        data=self.cache[key]
        if 'bytes' in self.mutations:data=self.mutations['bytes'](p,data,self.cache)
        self.inputs[key]={'bytes':len(data),'sha256':sha(data)}
        return data

    def json(self,path):
        v=decode(self.file(path,MAX_JSON));require(type(v) is dict,'JSON object required');return v

    def evidence(self,name):
        require(type(name) is str and name and len(name)<=4096 and
                len(Path(name).parts)==1 and not Path(name).is_absolute() and name not in ('.','..'),
                'immediate residency evidence child required')
        return self.output/name

    def check(self,name,condition,detail=None):
        require(time.monotonic()-self.started<=60,'independent audit exceeds60s')
        self.checks.append({'name':name,'status':'PASS' if condition else 'FAIL',
                            **({'detail':detail} if detail is not None else {})})
        require(condition,name)

    def protocol(self):
        r,s=self.run,self.summary
        self.check('run/actual-natural-success', r['schema']=='hellomine3d-reference-residency-run-v1' and
                   r['launch_count']==1 and type(r['child_returncode']) is int and r['child_returncode']==0 and
                   r.get('child_signal') is None and not r.get('external_deadline_exceeded',False) and
                   not r.get('owned_child_cleanup_required',False) and
                   r['result']=='NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE')
        self.check('scope/hidden-zero-input-nonperf', r['evidence_type']=='DEVELOPER_DIAGNOSTIC' and
                   r['normal_input'] is False and r['input_actions']==0 and
                   r['native_fault_requested'] is None and r['performance_isolation_claimed'] is False)
        self.check('summary/real-three-phases-two-readbacks-tail', s['schema']=='hellomine3d-reference-residency-summary-v1' and
                   s['status']=='COMPLETE' and s['completed_phases']==3 and s['tail_complete'] is True and
                   s['normal_input'] is False and s['input_event_count']==0 and
                   s['normal_simulation_advances'] is True and s['readbacks']==2 and s['fault']=='' and s['reason']=='')
        self.check('summary/fixed-bounds', integer(s['frames'],0,4095)<4096 and
                   0<=numeric(s['elapsed_ms'])<45000)
        self.check('journal/starts-one-contiguous', [e['sequence'] for e in self.events]==list(range(1,len(self.events)+1)))
        kinds={'header','begin','checkpoint','end','teleport','chunk-saved','chunk-loaded','chunk-unloaded',
               'gpu-retired','component-release','departed-observation','normal-save','world-cleared',
               'components-destroyed','root-shutdown'}
        self.check('journal/schemas-observation-order-and-scope',
                   all(e['schema']=='hellomine3d-reference-residency-journal-v1' and
                       e['event'] in kinds and e['phase'] in (*PHASES,'complete') and
                       e['normal_input'] is False and e['input_event_count']==0 and
                       0<=integer(e['frame'])<4096 and 0<=numeric(e['elapsed_ms'])<45000
                       for e in self.events) and
                   all(a['frame']<=b['frame'] and a['elapsed_ms']<=b['elapsed_ms']
                       for a,b in zip(self.events,self.events[1:])))
        self.check('journal/one-header-first', self.events[0]['event']=='header' and
                   sum(e['event']=='header' for e in self.events)==1)
        h=self.events[0]['snapshot']
        self.check('header/normal-simulation-and-budgets', h['normal_input'] is False and
                   h['normal_simulation_advances'] is True and h['readback_budget']==2 and
                   h['maximum_frames']==4096 and h['maximum_seconds']==45 and
                   h['phase_maximum_frames']==1024 and h['phase_maximum_seconds']==12 and
                   h['maximum_journal_records']==128 and h['tracked_event_ring']==32 and
                   h['bindings_bound']==16 and h['native_object_bound']==64 and h['draw_facts_bound']==32 and
                   h['total_file_bytes_bound']==MAX_DIAG and h['fault']=='')
        self.begins,self.ends,self.checkpoints={},{},{}
        expected=[(kind,p) for p in PHASES for kind in ('begin','checkpoint','end')]
        self.check('journal/one-ordered-group-per-phase',
                   [(e['event'],e['phase']) for e in self.events if e['event'] in ('begin','checkpoint','end')]==expected)
        for e in self.events:
            if e['event']=='begin':self.begins[e['phase']]=e
            if e['event']=='end':self.ends[e['phase']]=e
            if e['event']=='checkpoint':self.checkpoints[e['phase']]=e
        for p in PHASES:
            b,c=self.begins[p],self.checkpoints[p]
            self.check(p+'/real-warm-frames-and-deadline', 12<=c['frame']-b['frame']<1024 and
                       c['elapsed_ms']-b['elapsed_ms']<12000)
        self.actual={p:(self.checkpoints[p]['snapshot']['actual'] if p!='departed-b'
                        else self.checkpoints[p]['snapshot']) for p in PHASES}
        self.check('files/bounded-diagnostic-inventory',
                   all(p.is_file() and not p.is_symlink() for p in self.output.iterdir()) and
                   len(list(self.output.iterdir()))<=300 and
                   sum(p.stat().st_size for p in self.output.iterdir())<=MAX_DIAG)

    def identity(self):
        r=self.run;ident,cert,loaded=r['package_identity'],r['current_source_certificate'],r['loaded_executable']
        runtime=self.session/'Runtime.app'
        self.check('identity/original-source-template-protected', r['source_app_all_files_unchanged'] is True and
                   r['save_template_all_files_unchanged'] is True and
                   r['save_clone_initial_hashes']==r['template_file_hashes_before'])
        self.check('identity/current-source-full-membership-certificate', cert['all_entries_current_exact'] is True and
                   cert['matching_entries']==ident['source_file_count'] and integer(cert['matching_entries'],1,10000)>0 and
                   cert['receipt_sha256']==ident['source_manifest_sha256'])
        self.check('identity/owned-runtime-command', Path(r['session']).resolve()==self.session and
                   Path(r['runtime_app'])==runtime and r['command']==[str(runtime/'Contents/Resources/bin/HelloMine3D')])
        exe=self.file(runtime/'Contents/Resources/bin/HelloMine3D',128*1024**2)
        self.check('identity/real-loaded-binary', loaded['verified'] is True and
                   loaded['executable_sha256']==ident['executable_sha256']==sha(exe))
        receipt=self.file(Path(loaded['receipt']),MAX_JSON);lr=decode(receipt)
        self.check('identity/actual-lsof-PID-and-path', sha(receipt)==loaded['receipt_sha256'] and
                   lr['pid']==integer(r['game_pid'],1,2**31) and lr['expected']==r['command'][0] and
                   lr['executable_sha256']==sha(exe) and any(a.get('returncode')==0 and
                   re.search(r'^p'+str(r['game_pid'])+r'$',a.get('stdout',''),re.M) and
                   re.search(r'^n'+re.escape(r['command'][0])+r'$',a.get('stdout',''),re.M)
                   for a in lr['attempts']))
        src=self.file(runtime/'Contents/Resources/source-tree-sha256.txt',MAX_JSON)
        resources=self.file(runtime/'Contents/Resources/media/resource-manifest.txt',MAX_JSON)
        self.check('identity/real-source-resource-receipts',sha(src)==ident['source_manifest_sha256'] and
                   sha(resources)==ident['resource_manifest_sha256'])
        entries=[line.split('  ',1) for line in src.decode('utf8').splitlines()]
        self.check('identity/source-receipt-unique-members',len(entries)==ident['source_file_count'] and
                   all(len(x)==2 and re.fullmatch('[0-9a-f]{64}',x[0]) and x[1].startswith('src/') for x in entries) and
                   len({x[1] for x in entries})==len(entries))
        self.check('identity/real-owned-marker',self.file(self.session/'.hellomine3d-reference-residency-owned',256)==
                   b'HelloMine3D owned reference residency session v1\n')
        wanted={'HELLOMINE3D_REFERENCE_RESIDENCY_PROBE':'1','HELLOMINE3D_REFERENCE_RESIDENCY_DIR':str(self.output),
                'HELLOMINE3D_ROOT':str(runtime/'Contents/Resources'),'HELLOMINE3D_SAVE_DIR':str(self.session/'save'),
                'HELLOMINE3D_CATALOGUE_DIR':str(self.session/'catalogue'),'HELLOMINE3D_WINDOW_HIDDEN':'1',
                'HELLO_RENDER_CAPTURE':'1','HELLO_RENDER_CAPTURE_DIR':str(self.output),'HELLO_RENDER_CAPTURE_MS':'60000',
                'HELLO_RENDER_CAPTURE_EXIT':'0','HELLO_RENDER_CAPTURE_MAX_DELTA_MS':'5000','HELLO_PERF_CAPTURE':'0','HELLOMINE3D_MSAA4':'1',
                'HELLOMINE3D_PLAYER_POSITION':'195.5 68.02 -176.2','HELLOMINE3D_PLAYER_ROTATION':'5 45 0',
                'HELLOMINE3D_WORLD_TIME':'6000','HELLOMINE3D_SEED':'42'}
        self.check('identity/effective-owned-environment',all(r['environment'].get(k)==v for k,v in wanted.items()))
        original_meta=self.file(Path(r['save_template'])/'world.meta',65536)
        self.original_world_id=world_id(original_meta)
        self.original_seed=metadata_number(original_meta,'seed')
        self.original_generation=metadata_number(original_meta,'terrain_generation_version')
        self.template_cells={}
        for cx,cz in CHUNKS:
            rel=f'chunks/chunk_{cx}_{cz}.hmcchunk'
            data=self.file(Path(r['save_template'])/rel)
            self.check('template/'+rel+'/bound-original-receipt',sha(data)==r['template_file_hashes_before'][rel])
            ids,metas=shared.Audit.chunk(data,cx,cz)
            for p,wanted_cell in CELLS.items():
                if (p[0]//16,p[2]//16)==(cx,cz):
                    at=p[1]*256+(p[2]%16)*16+p[0]%16
                    self.template_cells[p]=(ids[at],metas[at])
        self.check('template/independent-r9-ID-meta',self.template_cells==CELLS)
        self.template_witness_cells={}
        for cx,cz in {(p[0]//16,p[2]//16) for p in WITNESS_CELLS}:
            rel=f'chunks/chunk_{cx}_{cz}.hmcchunk'
            data=self.file(Path(r['save_template'])/rel)
            self.check('template/witness/'+rel+'/bound-original-receipt',
                       sha(data)==r['template_file_hashes_before'][rel])
            ids,metas=shared.Audit.chunk(data,cx,cz)
            for p in WITNESS_CELLS:
                if (p[0]//16,p[2]//16)==(cx,cz):
                    at=p[1]*256+(p[2]%16)*16+p[0]%16
                    self.template_witness_cells[p]=(ids[at],metas[at])
        self.check('template/independent-fixed-tree-witness-ID-meta',
                   self.template_witness_cells==WITNESS_CELLS)

    def world(self):
        identities=[self.actual[p]['identity'] for p in PHASES]
        a=identities[0]
        for p,i in zip(PHASES,identities):
            self.check(p+'/same-live-World-Root-Scene-window',i==a and
                       all(type(i[k]) is str and re.fullmatch(r'0x[0-9a-fA-F]+',i[k]) and int(i[k],16)>0
                           for k in ('root_instance','scene_instance','window_instance','world_instance')) and
                       i['world_id']==i['actual_world_id']==i['disk_world_id']==self.original_world_id and
                       i['actual_seed']==self.original_seed and
                       i['actual_terrain_generation_version']==self.original_generation and
                       i['save_directory']==str(self.session/'save'))
            f=self.actual[p]
            self.check(p+'/normal-real-simulation-delta',0<numeric(f['simulation_delta'])<=5)
            self.check(p+'/authority-frame-not-later-revision',1<=integer(f['frame_input_scene_revision'])<=
                       integer(f['later_current_world_visual_revision']) and
                       f['local_lights']['revision']<=f['frame_input_scene_revision'])
            counts=f['chunk_event_counts']
            self.check(p+'/bounded-event-aggregate',set(counts)=={'loaded','unloaded','saved'} and
                       all(integer(x,0,2**32-1)>=0 for x in counts.values()))
            total=integer(f['global_cpu_ready_total']);deferred=integer(f['global_cpu_ready_deferred'],0,total)
            offered=integer(f['offered_cpu_ready_total'],0,8)
            self.check(p+'/background-budget-not-fake-current',offered==total-deferred)
            columns=f['columns']
            self.check(p+'/actual-resident-only-column-observations',len(columns)==2 and
                       {location(c['position'],2) for c in columns}=={(201,-186),(199,-196)} and
                       all(c['known'] is (p!='departed-b') for c in columns))
            self.check(p+'/complete-same-frame-column-observation-domain',
                       f['column_observation_available'] is True and
                       all(c['observation']=='World.observeSurfaceMap-try-lock-complete-batch' and
                           integer(c['observed_frame'],1)==self.checkpoints[p]['frame']
                           for c in columns))
        times=[numeric(self.actual[p]['world_time']) for p in PHASES]
        self.check('simulation/normal-world-time-advanced',times[0]<=times[1]<=times[2] and times[2]>times[0])
        player=[self.actual[p]['player'] for p in PHASES]
        a,b,c=[vector(x['position'],3) for x in player]
        self.check('movement/real-player-departed-and-returned',b[0]==a[0]+192 and b[2]==a[2] and
                   c[0]==a[0] and c[2]==a[2] and player[2]['rotation']==player[0]['rotation'] and
                   player[2]['interpolation_epoch']>player[0]['interpolation_epoch'])
        for p,pos in zip(PHASES,(a,b,c)):
            logic=vector(self.actual[p]['logic_camera']['position'],3)
            self.check(p+'/real-logic-camera-demand-followed-player',abs(logic[0]-pos[0])<=0.01 and
                       abs(logic[2]-pos[2])<=0.01)
        self.original_incarnations={}
        for p in PHASES:
            f=self.actual[p];chunks=f['target_chunks']
            self.check(p+'/exact-two-target-numeric-copies',{location(t['coord'],2) for t in chunks}==CHUNKS and len(chunks)==2)
            if p=='departed-b':
                self.check(p+'/actual-production-data-absent',all(t['present'] is False and t['data_residency']==0 and
                           t['incarnation'] is None and t['cells']==[] and t['observation']=='chunk-unloaded' for t in chunks))
                self.check(p+'/target-cells-not-queried',f['cells']==[])
            else:
                for t in chunks:
                    cp=location(t['coord'],2)
                    self.check(p+'/'+str(cp)+'/real-resident-data',t['present'] is True and t['data_residency']==4 and
                               integer(t['incarnation'],1)>0)
                    observed=shared.blocks_map(t['cells'])
                    expected={k:v for k,v in CELLS.items() if (k[0]//16,k[2]//16)==cp}
                    self.check(p+'/'+str(cp)+'/locked-copy-target-cells',observed==expected)
                    if p=='resident-a':self.original_incarnations[cp]=t['incarnation']
                    else:self.check(p+'/'+str(cp)+'/storage-return-new-incarnation',
                                    t['from_storage'] is True and t['observation'] in ('chunk-loaded','chunk-saved') and
                                    t['incarnation']!=self.original_incarnations[cp])
                self.check(p+'/actual-current-World-ID-meta',shared.blocks_map(f['cells'])==CELLS)
            sections=f['sections'];locs={location(s['location']) for s in sections}
            if p=='departed-b':
                self.check(p+'/no-current-target-sections',sections==[] and f['sections_ready'] is False)
                cache=f['cache']
                self.check(p+'/actual-target-render-cache-empty',
                           all(cache[k]==[] for k in ('target_live_sections','target_render_states',
                               'target_section_visuals','target_batches','target_dirty_batches')) and
                           all(not re.fullmatch(r'12_[0-9]+_-(12|13)',key) for key in cache['render_state_keys']))
                self.check(p+'/actual-current-origin-access-zero',f['render_object_access']['frame']==self.checkpoints[p]['frame'] and
                           f['render_object_access']['origin_main_accesses']==f['render_object_access']['origin_reflection_accesses']==0 and
                           integer(f['render_object_access']['main_callbacks'],1)>0 and
                           integer(f['render_object_access']['reflection_callbacks'])>=0 and
                           f['render_object_access']['domain']=='production-RenderObjectListener-before-native-draw')
                self.check(p+'/no-live-original-native-object',f['native_objects']==[])
                self.check(p+'/no-stale-draw-list',f['draws']==[])
            else:
                self.check(p+'/independently-current-selected-upload',locs==SECTIONS and len(sections)==2 and
                           f['sections_ready'] is True and all(s['gpu_resident'] is True and
                           s['offered_cpu_ready'] is False and type(s['uploaded_revision']) is int and
                           s['uploaded_revision']==integer(s['live_revision'],1,2**32-1) for s in sections))
        teleports=[e for e in self.events if e['event']=='teleport']
        self.check('movement/actual-two-accepted-production-teleports',
                   len(teleports)==2 and [e['snapshot']['direction'] for e in teleports]==['depart','return'] and
                   all(e['snapshot']['accepted'] is True and e['snapshot']['requested']==e['snapshot']['actual_player'] for e in teleports))
        for cp in CHUNKS:
            unloaded=[e for e in self.events if e['event']=='chunk-unloaded' and location(e['snapshot']['target']['coord'],2)==cp]
            loaded=[e for e in self.events if e['event']=='chunk-loaded' and location(e['snapshot']['target']['coord'],2)==cp and
                    e['sequence']>self.checkpoints['departed-b']['sequence']]
            self.check('events/'+str(cp)+'/real-erase-then-storage-load',len(unloaded)>=1 and len(loaded)>=1 and
                       unloaded[0]['sequence']<self.checkpoints['departed-b']['sequence']<loaded[-1]['sequence'] and
                       unloaded[0]['snapshot']['target']['present'] is False and loaded[-1]['snapshot']['target']['from_storage'] is True)
        self.derived['target_incarnations_before']={str(k):v for k,v in self.original_incarnations.items()}
        self.derived['actual_world_time_sequence']=times

    def witness(self):
        # Fixed before these cases, from independent original chunk decoding.
        # This proves two declared tree sections, never a whole viewport.
        records={}
        for p in ('resident-a','returned-a'):
            f=self.actual[p];items=f['view_witness_sections']
            self.check(p+'/fixed-two-current-witness-sections',type(items) is list and
                       len(items)==2 and {location(x['location']) for x in items}==WITNESS_SECTIONS and
                       f['view_witness_ready'] is True and all(x['gpu_resident'] is True and
                       x['offered_cpu_ready'] is False and type(x['uploaded_revision']) is int and
                       x['uploaded_revision']==integer(x['live_revision'],1,2**32-1) for x in items))
            cpframe=self.checkpoints[p]['frame'];cells={}
            for x in items:
                loc=location(x['location']);sample=(225,67,-175) if loc[2]==-11 else (225,72,-177)
                self.check(p+'/'+str(loc)+'/actual-upload-serial-frame',
                           integer(x['upload_serial'],1)>0 and
                           0<=integer(x['uploaded_frame'])<=cpframe)
                self.check(p+'/'+str(loc)+'/known-current-witness-cell',
                           len(x['cells'])==1 and x['cells'][0]['known'] is True and
                           x['cells'][0]['observation']=='blocking-World.getBlock-find-only-nonAir' and
                           integer(x['cells'][0]['observed_frame'],1)==cpframe and
                           shared.blocks_map(x['cells'])=={sample:self.template_witness_cells[sample]})
                self.check(p+'/'+str(loc)+'/actual-clean-current-witness-incarnation',
                           x['mesh_state']=='Clean' and integer(x['incarnation'],1)>0)
                self.check(p+'/'+str(loc)+'/captured-source-current-incarnation',
                           x['upload_source_incarnation']==x['incarnation'] and
                           x['upload_source'] in ('cpu-ready','retained-clean-replay') and
                           x['upload_domain'] in ('startup','frame'))
                self.witness_upload_ledger(x,p,loc)
                cells.update(shared.blocks_map(x['cells']))
            self.check(p+'/independent-witness-cells-match-original',cells==self.template_witness_cells)
            records[p]=items
        far=self.actual['departed-b'];items=far['view_witness_sections']
        self.check('departed-b/witness-cells-not-forced-or-read',type(items) is list and len(items)<=2 and
                   len({location(x['location']) for x in items})==len(items) and
                   all(location(x['location']) in WITNESS_SECTIONS and x['cells']==[] for x in items) and
                   type(far['view_witness_ready']) is bool)
        returned=self.actual['returned-a'];movement=next(e for e in self.events
            if e['event']=='teleport' and e['snapshot']['direction']=='return')
        floor=integer(returned['return_upload_serial_floor']);frame=integer(returned['return_movement_frame'])
        self.check('returned-a/actual-return-upload-boundary',
                   floor==integer(movement['snapshot']['return_upload_serial_floor']) and
                   frame==integer(movement['snapshot']['return_movement_frame'])==movement['frame'] and
                   self.checkpoints['departed-b']['frame']<=frame<self.checkpoints['returned-a']['frame'] and
                   floor>=max(x['upload_serial'] for x in records['resident-a']))
        self.check('returned-a/fresh-witness-upload-after-actual-movement',
                   all(integer(x['return_upload_serial_floor'])==floor and
                       integer(x['upload_serial'],1)>floor and integer(x['uploaded_frame'])>frame
                       for x in records['returned-a']))
        tree=next(x for x in records['returned-a'] if location(x['location'])==(14,4,-11))
        before_tree=next(x for x in records['resident-a'] if location(x['location'])==(14,4,-11))
        self.check('returned-a/actual-retained-clean-tree-recovery',
                   tree['upload_source']=='retained-clean-replay' and tree['upload_source_mesh_state']=='Clean' and
                   tree['incarnation']==before_tree['incarnation'])
        self.derived['fixed_view_witness_sections']=[list(x) for x in sorted(WITNESS_SECTIONS)]
        self.derived['fixed_view_witness_cells']={str(k):v for k,v in self.template_witness_cells.items()}
        self.derived['return_witness_upload_boundary']={'serial_floor':floor,'movement_frame':frame,
            'uploads':records['returned-a'],'scope':'Only two predefined visible-tree sections; other background and actors remain unproven.'}

    def witness_upload_ledger(self,x,p,loc):
        name=p+'/'+str(loc)
        if x['upload_domain']=='startup':
            self.check(name+'/startup-source-domain',p=='resident-a' and x['uploaded_frame']==0 and
                       x['upload_frame_facts'] is None and x['upload_source']=='cpu-ready' and
                       x['upload_source_mesh_state'] in ('CpuReady','Clean'))
            return
        f=x['upload_frame_facts']
        self.check(name+'/actual-frame-source-domain',type(f) is dict and f['frame']==x['uploaded_frame'] and
                   x['upload_source_mesh_state']==('Clean' if x['upload_source']=='retained-clean-replay' else 'CpuReady'))
        def records(key,state):
            items=f[key]
            self.check(name+'/'+key+'/bounded-exact-source-records',type(items) is list and len(items)<=8 and
                       len({location(a['location']) for a in items})==len(items) and
                       all(integer(a['revision'],1,2**32-1)>0 and integer(a['incarnation'],1)>0 and
                           a['mesh_state']==state for a in items))
            return {location(a['location']):(a['revision'],a['incarnation'],a['mesh_state']) for a in items}
        normal=records('cpu_ready_offered','CpuReady')
        requested=records('retained_clean_requested','Clean')
        copied=records('retained_clean_copied','Clean')
        uploaded_normal=records('cpu_ready_uploaded','CpuReady')
        uploaded_replay=records('retained_clean_uploaded','Clean')
        self.check(name+'/same-frame-normal-replay-budget',
                   integer(f['normal_count'])==len(normal) and integer(f['replay_count'])==len(copied) and
                   integer(f['combined_count'])==len(normal)+len(copied)<=integer(f['maximum_count'])==8 and
                   len(copied)<=8-len(normal) and not (set(normal)&set(copied)))
        self.check(name+'/replay-is-exact-requested-current-clean-copy',
                   all(a in requested and v==requested[a] for a,v in copied.items()) and
                   uploaded_normal==normal and uploaded_replay==copied)
        accepted=f['accepted_uploads'];parts=normal|copied
        self.check(name+'/actual-accepted-records-match-source-parts',type(accepted) is list and
                   len(accepted)==len(parts) and len({location(a['location']) for a in accepted})==len(accepted) and
                   {location(a['location']):(a['revision'],a['incarnation'],a['mesh_state']) for a in accepted}==parts and
                   all(type(a['accepted_current']) is bool and type(a['has_visual']) is bool for a in accepted))
        self.check(name+'/accepted-current-state-is-not-an-offered-alias',
                   all(not a['accepted_current'] or (a['current_revision']==a['revision'] and
                       a['current_incarnation']==a['incarnation'] and a['current_mesh_state']=='Clean')
                       for a in accepted))
        source=copied if x['upload_source']=='retained-clean-replay' else normal
        desired=(x['uploaded_revision'],x['upload_source_incarnation'],x['upload_source_mesh_state'])
        self.check(name+'/actual-source-upload-ledger',source.get(loc)==desired)
        accepted_source=next(a for a in accepted if location(a['location'])==loc)
        self.check(name+'/real-current-accepted-visual',accepted_source['accepted_current'] is True and
                   accepted_source['has_visual'] is True and accepted_source['current_revision']==x['live_revision'] and
                   accepted_source['current_incarnation']==x['incarnation'] and accepted_source['current_mesh_state']=='Clean')

    def draw(self,d,f,p,n):
        name=p+'/draw'+str(n)
        self.check(name+'/actual-native-original-draw',d['view'] in ('main','reflection') and
                   d['frame']==self.checkpoints[p]['frame'] and d['program_linked'] is True and
                   d['production_attached_shaders'] is True and d['gl_is_buffer'] is True and
                   all(integer(d[k],1)>0 for k in ('program','vao','vbo','ibo')) and
                   d['gl_error']==0 and d['state_restored'] is True)
        attrs=d['attributes'];expected={'vertex':(3,0),'uv0':(2,12),'uv1':(2,20),'uv2':(3,28),'uv3':(1,40)}
        self.check(name+'/real-44B-VAO-inputs',len(attrs)==len({x['name'] for x in attrs}) and
                   {x['name'] for x in attrs}>={'vertex','uv2'} and all(x['name'] in expected and
                   x['enabled'] is True and x['buffer']==d['vbo'] and x['type']==5126 and
                   (x['size'],x['pointer_offset'])==expected[x['name']] and x['stride']==44 and
                   x['normalized'] is False and x['integer'] is False and x['divisor']==0 for x in attrs))
        v=self.file(self.evidence(d['vbo_file']),MAX_BUFFER);i=self.file(self.evidence(d['ibo_file']),MAX_BUFFER)
        cv=self.file(self.evidence(d['cpu_vbo_file']),MAX_BUFFER);ci=self.file(self.evidence(d['cpu_ibo_file']),MAX_BUFFER)
        self.check(name+'/actual-CPU-GL-byte-equivalence',v==cv and i==ci and len(v)==d['vertex_bytes'] and
                   len(i)==d['index_bytes'] and len(v)%44==0 and len(i)%12==0 and d['cpu_gpu_bytes_equal'] is True)
        vertices=list(struct.iter_unpack('=11f',v));indices=[x[0] for x in struct.iter_unpack('=I',i)]
        self.check(name+'/independent-real-triangle-index-bounds',vertices and indices and
                   all(all(math.isfinite(x) for x in row) for row in vertices) and
                   all(x<len(vertices) for x in indices) and d['primitive_count']==d['expected_triangles']==len(indices)//3)
        sections={location(x['location']):x for x in f['sections']}
        sources={location(x) for x in d['source_sections']}
        self.check(name+'/real-source-current-upload',bool(sources&SECTIONS) and
                   d['uploaded_revision']==sections[next(iter(sources&SECTIONS))]['live_revision'] and
                   integer(d['upload_serial'],1)>0)
        if d['local_lights'] is not None:
            actual=d['local_lights'];src=f['local_lights']['sources']
            pos=[s['position']+[s['radius']] for s in src]+[[0,0,0,0]]*(8-len(src))
            col=[s['colour']+[s['energy']] for s in src]+[[0,0,0,0]]*(8-len(src))
            self.check(name+'/actual-linked-main-private-light-parameters',
                       actual['domain']=='actual-linked-GL-program-after-production-draw' and
                       actual['count']==len(src)==f['local_lights']['count'] and
                       actual['position_radius']==pos and actual['colour_energy']==col)
        self.check(name+'/normal-finite-shader-animation-time',type(d['time_uniforms']) is list and
                   all(t['name'] in ('globalTime','legacyTime') and numeric(t['value'])>=0 for t in d['time_uniforms']))
        return {'draw':d,'vertices':vertices,'indices':indices}

    def graphics(self):
        self.observed={}
        self.native_before=set()
        for p in ('resident-a','returned-a'):
            e=self.checkpoints[p];wrapper=e['snapshot'];f=self.actual[p]
            native=f['native_objects']
            keys=[(o['name'],o['upload_serial'],o['vbo'],o['ibo']) for o in native]
            self.check(p+'/real-bounded-original-native-inventory',1<=len(native)<=64 and
                       len(keys)==len(set(keys)) and len({o['upload_serial'] for o in native})==len(native) and
                       all(tracked(o['origin']) and o['vbo_alive'] is True and o['ibo_alive'] is True and
                           integer(o['upload_serial'],1)>0 and integer(o['vbo'],1)>0 and integer(o['ibo'],1)>0 and
                           0<integer(o['vertex_bytes'])<=MAX_BUFFER and
                           0<integer(o['index_bytes'])<=MAX_BUFFER for o in native))
            if p=='resident-a':self.native_before=set(keys)
            self.check(p+'/bounded-real-native-draw-list',1<=len(f['draws'])<=32)
            self.check(p+'/observed-draws-belong-to-real-native-inventory',
                       {(d['object_name'],d['upload_serial'],d['vbo'],d['ibo']) for d in f['draws']}<=set(keys))
            self.observed[p]=[self.draw(d,f,p,n) for n,d in enumerate(f['draws'])]
            self.check(p+'/both-targets-main-and-private-PBR-submission',
                       all(any(view==d['view'] and target in {location(x) for x in d['source_sections']} and
                       d['local_lights'] is not None for d in f['draws'])
                       for target in SECTIONS for view in ('main','reflection')))
            lights=f['local_lights']
            self.check(p+'/resident-bounded-local-light-query',len(lights['sources'])==integer(lights['count'],0,8) and
                       integer(lights['inspected_sections'],0,27)<=27 and
                       integer(lights['inspected_cells'],0,27*4096)<=27*4096 and
                       any(s['position']==[201.5,69.5,-185.5] for s in lights['sources']) and
                       all(0<numeric(s['radius'])<=12 and numeric(s['energy'])>0 for s in lights['sources']))
            shared.Audit.target_storage(self,f['hdr'],(2560,1440),4,p+'/HDR')
            planar=f['planar'];target=planar['target']
            shared.Audit.target_storage(self,target,(1280,720),0,p+'/planar')
            self.check(p+'/actual-same-frame-native-reflection',wrapper['frame']==e['frame']==planar['frame'] and
                       planar['scene_revision']==f['frame_input_scene_revision'] and
                       target['selected'] is True and target['binder_bound'] is True and target['water_sampler_bound'] is True and
                       target['lod_camera_bound'] is True and target['listeners_active'] is False and target['camera_count']==1 and
                       1<=target['private_materials']<=96 and 1<=target['private_passes']<=384)
            rows=shared.reflected_xyw(f['main_camera'],numeric(planar['plane_y']),planar['texture_flip'])
            vp=vector(planar['view_projection_row_major'],16)
            self.check(p+'/independent-mirror-projection',all(abs(rows[r][c]-vp[row*4+c])<=5e-5
                       for r,row in enumerate((0,1,3)) for c in range(4)))
            self.check(p+'/readback-original-label',wrapper['planar_prefix']==p and wrapper['main_png']==p+'.main.png' and
                       wrapper['main_readback_gl_error']==0)
            raw=self.file(self.evidence(p+'.native-linear.rgba32f'))
            self.check(p+'/native-raw-size',len(raw)==1280*720*16)
            floats=array('f');floats.frombytes(raw)
            self.check(p+'/raw-real-radiance-finite',all(math.isfinite(x) for x in floats))
            txt=self.file(self.evidence(p+'.facts.txt'),65536).decode('utf8')
            self.check(p+'/actual-native-readback-contract',
                       'raw_format=RGBA_FLOAT32_NATIVE_ENDIAN raw_origin=GL_BOTTOM_LEFT source=actual_native_RGBA16F_RTT' in txt and
                       re.search(r'\bframe='+str(e['frame'])+r'\b',txt) and
                       re.search(r'\bscene_revision='+str(f['frame_input_scene_revision'])+r'\b',txt) and
                       'gl_error_before=0 gl_error_after=0 nonfinite_components=0' in txt and 'main_water_texture_bound=1' in txt)
            png=self.file(self.evidence(wrapper['main_png']))
            preview=self.file(self.evidence(p+'.camera-preview.png'))
            self.check(p+'/real-main-and-RTT-preview-dimensions',
                       all(data[:8]==b'\x89PNG\r\n\x1a\n' and data[12:16]==b'IHDR' and
                       struct.unpack('>II',data[16:24])==dims for data,dims in
                       ((png,(2560,1440)),(preview,(1280,720)))))
            clear=vector(planar['target_clear_linear_rgba'],4)
            half_clear=tuple(struct.unpack('e',struct.pack('e',x))[0] for x in clear)
            observations=[]
            for label,point in (('receiver',(201.5,68.5,-191)),('wall',(199.5,68.5,-195))):
                corners=[(point[0]+dx,point[1]+dy,point[2]) for dx,dy in ((-.5,-.5),(.5,-.5),(.5,.5),(-.5,.5))]
                roi=shared.rectangle(corners,rows,1280,720,padding=0)
                nonclear=sum(struct.unpack_from('=4f',raw,(y*1280+x)*16)!=half_clear
                             for y in range(roi[1],roi[3]) for x in range(roi[0],roi[2]))
                covered=False
                for o in self.observed[p]:
                    d=o['draw']
                    if d['view']!='reflection' or d['local_lights'] is None:continue
                    origin=[x*16 for x in d['section']]
                    for at in range(0,len(o['indices']),3):
                        tri=[[o['vertices'][index][k]+origin[k] for k in range(3)]
                             for index in o['indices'][at:at+3]]
                        if not all(v[2]==point[2] for v in tri):continue
                        a,b,c=tri
                        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
                        if not den:continue
                        u=((b[1]-c[1])*(point[0]-c[0])+(c[0]-b[0])*(point[1]-c[1]))/den
                        w=((c[1]-a[1])*(point[0]-c[0])+(a[0]-c[0])*(point[1]-c[1]))/den
                        if min(u,w,1-u-w)>=0:covered=True;break
                    if covered:break
                self.check(p+'/'+label+'/actual-reflected-GPU-surface-and-nonclear-ROI',covered and nonclear>0,
                           {'ROI_GL_bottom_left':roi,'nonclear_pixels':nonclear,
                            'scope':'Submitted current target geometry and nonclear projected region; exact per-face occlusion remains visual review.'})
                observations.append({'surface':label,'roi':roi,'nonclear_pixels':nonclear})
            self.derived[p+'/native_RTT']={'sha256':sha(raw),'minimum':min(floats),'maximum':max(floats),'projected_surfaces':observations,
                                         'whole_image_equivalence_required':False}
        a=self.actual['resident-a']['draws'];returned=self.actual['returned-a']['draws']
        self.check('return/actual-new-upload-objects',min(d['upload_serial'] for d in returned)>max(d['upload_serial'] for d in a))
        start,end=self.checkpoints['resident-a']['sequence'],self.checkpoints['departed-b']['sequence']
        gaps=[e['snapshot'] for e in self.events if e['event']=='gpu-retired' and start<e['sequence']<end]
        self.check('retirement/actual-immediate-gaps-exist',bool(gaps))
        for n,g in enumerate(gaps):
            self.check('retirement/gap'+str(n)+'/native-old-storage-dead-before-allocate',
                       g['vbo_alive'] is False and g['ibo_alive'] is False and g['gl_error']==0 and
                       g['observation']=='immediate-before-next-allocate' and
                       integer(g['vbo'],1)>0 and integer(g['ibo'],1)>0)
        wanted=self.native_before
        actual={(g['name'],g['upload_serial'],g['vbo'],g['ibo']) for g in gaps}
        self.check('retirement/every-original-native-target-buffer-retired',wanted<=actual)
        far=self.actual['departed-b']['lifecycle']['planar']
        self.check('far/private-cache-remains-bounded',integer(far['private_materials'],0,96)<=96 and integer(far['private_passes'],0,384)<=384)
        self.derived['retired_original_native_objects']=len(wanted)
        self.derived['actual_native_retirement_gaps']=len(gaps)

    def allocated_native(self,facts,dimensions,samples,label):
        # Retiring allocated storage may already be inactive/selection-cleared.
        # Activity is a runtime draw condition, never a substitute for storage.
        w,h=dimensions
        self.check(label+'/allocated-native-target',type(facts['active']) is bool and
                   facts['width']==w and facts['height']==h and
                   w*h<=(8294400 if samples else 2073600) and
                   facts['target_count']==facts['depth_count']==1 and
                   facts['owned_depth_attached'] is True and facts['depth_pool']==0 and
                   facts['observer_failures']==0 and integer(facts['generation'],1)>0)
        n=facts['native']
        self.check(label+'/complete-real-FBO-no-GL-errors',n['complete'] is True and
                   integer(n['resolve_fbo'],1)>0 and integer(n['draw_fbo'],1)>0 and
                   n['gl_error_before']==n['gl_error_after']==0)
        for slot,count in (('colour',samples),('resolved',0),('depth',samples)):
            a=n[slot]
            kind=5890 if slot=='resolved' or (slot=='colour' and not samples) else 36161
            self.check(label+'/'+slot+'-real-storage',a['kind']==kind and integer(a['object'],1)>0 and
                       a['width']==w and a['height']==h and a['samples']==count and
                       (a['format']==34842 if slot!='depth' else a['format'] in (33189,33190,33191,35056,36012,36013)))
        a=n['stencil']
        self.check(label+'/stencil-storage-or-absence',a['kind']==0 and a['object']==0 or
                   a['kind']==36161 and a['object']==n['depth']['object'] and
                   a['width']==w and a['height']==h and a['samples']==samples)
        self.check(label+'/same-draw-resolve-FBO-for-no-MSAA',samples>0 or n['resolve_fbo']==n['draw_fbo'])

    def tail(self):
        tail=('normal-save','world-cleared','components-destroyed','root-shutdown')
        seq=[]
        for kind in tail:
            records=[e for e in self.events if e['event']==kind]
            self.check('tail/'+kind+'/exactly-one-actual-event',len(records)==1 and records[0]['phase']=='complete')
            seq.append(records[0]['sequence'])
        self.check('tail/normal-ordered-after-return',self.ends['returned-a']['sequence']<seq[0]<seq[1]<seq[2]<seq[3] and
                   seq[-1]==self.events[-1]['sequence'])
        saved=next(e['snapshot'] for e in self.events if e['event']=='normal-save')
        self.check('tail/real-production-save-return',saved['saved'] is True and shared.blocks_map(saved['cells'])==CELLS)
        cleared=next(e['snapshot'] for e in self.events if e['event']=='world-cleared')
        self.check('tail/real-world-worker-owner-and-caches-cleared',cleared['normal_close_save_and_join_returned'] is True and
                   cleared['world_present'] is False and cleared['sandbox_present'] is False and
                   cleared['loader_lifetime_owner_present'] is False and
                   all(cleared['cache'][k]==0 for k in ('sections','batches','dirty_batches','render_states',
                       'last_live_sections','material_identity_revisions','local_lights','empty_section_upload_identities')) and
                   cleared['cache']['dynamic_shadow_off'] is True)
        self.check('tail/reflection-owned-resources-reset',cleared['planar']['active'] is False and
                   all(cleared['planar'][k]==0 for k in ('target_count','depth_count','private_materials','private_passes')) and
                   cleared['planar']['camera_count']==1)
        destroyed=next(e['snapshot'] for e in self.events if e['event']=='components-destroyed')
        self.check('tail/components-before-live-Scene-Root',destroyed['root_alive'] is True and destroyed['scene_alive'] is True and
                   destroyed['hdr_component'] is False and destroyed['planar_component'] is False and destroyed['scene_camera_count']==1 and
                   all(destroyed[k]['target_count']==destroyed[k]['depth_count']==0 for k in ('hdr','planar')))
        closed=next(e['snapshot'] for e in self.events if e['event']=='root-shutdown')
        self.check('tail/root-Scene-null-without-invalid-GL-query',closed['root_alive'] is False and closed['scene_alive'] is False and
                   closed['managers_available'] is False and closed['manager'] is None and closed['scene_camera_count']==0)
        releases=[e['snapshot'] for e in self.events if e['event']=='component-release']
        self.check('tail/actual-component-native-release-observations',bool(releases))
        self.check('tail/required-component-release-owner-domains',
                   {'hdr-target','planar-target','planar-materials','planar-camera'}<={r['owner'] for r in releases})
        for n,r in enumerate(releases):
            facts=r['facts'];before,after=facts['before'],facts['after'];owner=r['owner']
            prefix='component-release/'+str(n)+'/'+owner
            self.check(prefix+'/runtime-failure-not-hidden',r['runtime_pass'] is True)
            if owner=='planar-camera':
                self.check(prefix+'/actual-owned-camera-dead',before['camera_name']==self.actual['resident-a']['planar']['target']['camera_name'] and
                           before['camera_name']==self.actual['returned-a']['planar']['target']['camera_name'] and
                           after['camera_name_alive'] is False)
                continue
            self.check(prefix+'/known-owner',owner in ('hdr-target','planar-target','planar-materials'))
            expected_objects,expected_names=set(),set()
            if owner=='planar-materials':
                names=before['material_names']
                self.check(prefix+'/actual-owned-material-set',1<=len(names)<=96 and len(names)==len(set(names)) and
                           len(names)==before['private_materials'])
                expected_names={('material',name) for name in names}
            else:
                self.allocated_native(before,(2560,1440) if owner=='hdr-target' else (1280,720),
                                      4 if owner=='hdr-target' else 0,prefix+'/before')
                native=before['native']
                expected_objects={(36160,integer(native[key],1)) for key in ('resolve_fbo','draw_fbo')}
                for slot in ('colour','resolved','depth','stencil'):
                    a=native[slot]
                    if a['kind']:expected_objects.add((a['kind'],integer(a['object'],1)))
                self.check(prefix+'/actual-owned-texture-name',type(before['texture_name']) is str and bool(before['texture_name']))
                expected_names={('texture',before['texture_name'])}
                if owner=='hdr-target':
                    prior=self.actual['returned-a']['hdr']
                    self.check(prefix+'/exact-current-HDR-allocation',before['generation']==prior['generation'] and
                               before['texture_name']==prior['texture_name'] and before['native']==prior['native'])
                    self.check(prefix+'/compiled-operations-and-camera-restored',facts['compiled_operations_drained'] is True and
                               facts['camera_viewport_restored'] is True)
            ids=[(x['kind'],x['id']) for x in after['objects']]
            names=[(x['kind'],x['name']) for x in after['manager_names']]
            self.check(prefix+'/exact-old-native-object-set-dead',len(ids)==len(set(ids)) and set(ids)==expected_objects and
                       all(x['alive'] is False for x in after['objects']))
            self.check(prefix+'/exact-old-manager-name-set-absent',len(names)==len(set(names)) and set(names)==expected_names and
                       all(x['alive'] is False for x in after['manager_names']))
            self.check(prefix+'/native-queries-GL0',after['gl_error_before']==after['gl_error_after']==0)
        r=self.run;metadata=self.file(self.session/'save/world.meta',65536)
        self.check('save/independent-final-world-identity',world_id(metadata)==self.original_world_id and
                   metadata_number(metadata,'seed')==self.original_seed and
                   metadata_number(metadata,'terrain_generation_version')==self.original_generation and
                   sha(metadata)==r['save_clone_final_world_meta_sha256'] and metadata.decode('utf8')==r['save_clone_final_world_meta_text'] and
                   r['save_clone_final_hashes']['world.meta']==sha(metadata))
        cells={}
        for cx,cz in CHUNKS:
            rel=f'chunks/chunk_{cx}_{cz}.hmcchunk';data=self.file(self.session/'save'/rel)
            self.check('save/'+rel+'/actual-final-receipt',sha(data)==r['save_clone_final_hashes'][rel])
            ids,metas=shared.Audit.chunk(data,cx,cz)
            for p in CELLS:
                if (p[0]//16,p[2]//16)==(cx,cz):
                    at=p[1]*256+(p[2]%16)*16+p[0]%16;cells[p]=(ids[at],metas[at])
        self.check('save/actual-clone-target-ID-meta-preserved',cells==self.template_cells)
        self.derived['normal_saved_target_cells']={str(k):v for k,v in cells.items()}

    def report(self,error=None):
        return {'schema':SCHEMA,'status':'FAIL' if error else 'PASS_SCOPED_RESIDENCY_RENDER',
                'oracle_source_sha256':sha(Path(__file__).read_bytes()),
                'shared_parser_native_fact_helpers_sha256':sha(Path(shared.__file__).read_bytes()),
                'normal_input':False,'ordinary_acceptance_closed':False,'performance_isolation_claimed':False,
                'scope':'Actual same-World Player demand, data eviction/storage return, current original GPU/draw, resource retirement and two predefined tree-section witnesses; normal time, no full viewport or pixel equivalence claim.',
                'check_count':len(self.checks),'pass_count':sum(x['status']=='PASS' for x in self.checks),
                'fail_count':max(int(bool(error)),sum(x['status']=='FAIL' for x in self.checks)),
                'checks':self.checks,'inputs':self.inputs,'derived':self.derived,'open':OPEN,
                **({'error':str(error),'partial_native_observations':self.partial} if error else {})}

    def execute(self):
        self.protocol();self.identity();self.world();self.witness();self.graphics();self.tail()

def audit(session,cache=None,mutations=None):
    obj=None
    try:
        obj=Audit(session,cache,mutations);obj.execute();return obj.report()
    except Exception as e:
        return obj.report(e) if obj else {'schema':SCHEMA,'status':'FAIL','normal_input':False,
            'ordinary_acceptance_closed':False,'error':str(e),'check_count':0,'pass_count':0,'fail_count':1,'open':OPEN}

def calibrate(session,destination):
    destination=Path(destination);require(not destination.exists(),'calibration output must be new')
    cache={};positive=audit(session,cache)
    require(positive['status']=='PASS_SCOPED_RESIDENCY_RENDER','actual complete positive required before fault copies')
    source=Audit(session,cache);source.execute()
    destination.mkdir(parents=True)
    (destination/'actual-positive.json').write_text(json.dumps(positive,ensure_ascii=False,indent=2,allow_nan=False)+'\n')
    def change(phase,fn):
        def apply(events):
            target=next(e for e in events if e['event']=='checkpoint' and e['phase']==phase)
            facts=target['snapshot']['actual'] if phase!='departed-b' else target['snapshot']
            fn(facts);return events
        return {'events':apply}
    def rewrite(fn):
        def apply(events):
            events=fn(events)
            for i,e in enumerate(events,1):e['sequence']=i
            return events
        return {'events':apply}
    def event_change(kind,fn):
        def apply(events):
            fn(next(e['snapshot'] for e in events if e['event']==kind));return events
        return {'events':apply}
    def setval(obj,key,value):obj[key]=value
    original_inc=source.actual['resident-a']['target_chunks'][0]['incarnation']
    returned_file=source.actual['returned-a']['draws'][0]['vbo_file']
    def changed_witness(f,key,value):
        next(x for x in f['view_witness_sections'] if location(x['location'])==(14,4,-11))[key]=value
    def overbudget_witness(f):
        next(x for x in f['view_witness_sections'] if location(x['location'])==(14,4,-11))['upload_frame_facts']['combined_count']=9
    saved_rel='chunks/chunk_12_-12.hmcchunk'
    saved_path=Path(session).resolve()/'save'/saved_rel
    saved_raw=source.file(saved_path)
    corrupt_saved=bytearray(saved_raw)
    cell=(201,69,-186);offset=28+cell[1]*256+(cell[2]%16)*16+cell[0]%16
    corrupt_saved[offset]=0;corrupt_saved=bytes(corrupt_saved)
    def corrupt_saved_receipt(run):
        run['save_clone_final_hashes'][saved_rel]=sha(corrupt_saved);return run
    def empty_component(events):
        target=next(e for e in events if e['event']=='component-release' and e['snapshot']['owner']=='hdr-target')
        target['snapshot']['facts']['after']['objects']=[];return events
    faults=[
        ('missing-stage','journal/one-ordered-group-per-phase',rewrite(lambda e:[x for x in e if not (x['event']=='checkpoint' and x['phase']=='departed-b')])),
        ('duplicate-stage','journal/one-ordered-group-per-phase',rewrite(lambda e:e[:1]+[copy.deepcopy(next(x for x in e if x['event']=='begin'))]+e[1:])),
        ('native-exit1','run/actual-natural-success',{'run':lambda r:dict(r,child_returncode=1)}),
        ('false-ordinary','scope/hidden-zero-input-nonperf',{'run':lambda r:dict(r,normal_input=True,input_actions=1)}),
        ('global-timeout','summary/fixed-bounds',{'summary':lambda s:dict(s,elapsed_ms=45000)}),
        ('render-camera-only','movement/real-player-departed-and-returned',change('departed-b',lambda f:setval(f['player']['position'],0,f['player']['position'][0]-192))),
        ('target-data-retained','departed-b/actual-production-data-absent',change('departed-b',lambda f:setval(f['target_chunks'][0],'present',True))),
        ('target-column-known','departed-b/actual-resident-only-column-observations',change('departed-b',lambda f:setval(f['columns'][0],'known',True))),
        ('false-witness-observation-domain','/known-current-witness-cell',change('returned-a',lambda f:setval(f['view_witness_sections'][0]['cells'][0],'observation','copied-template-only'))),
        ('stale-complete-column-observation-frame','returned-a/complete-same-frame-column-observation-domain',change('returned-a',lambda f:setval(f['columns'][0],'observed_frame',f['columns'][0]['observed_frame']-1))),
        ('retained-origin-renderstate','departed-b/actual-target-render-cache-empty',change('departed-b',lambda f:f['cache']['render_state_keys'].append('12_4_-12'))),
        ('origin-render-access','departed-b/actual-current-origin-access-zero',change('departed-b',lambda f:setval(f['render_object_access'],'origin_reflection_accesses',1))),
        ('retained-original-native','departed-b/no-live-original-native-object',change('departed-b',lambda f:setval(f,'native_objects',copy.deepcopy(source.actual['resident-a']['native_objects'])))),
        ('no-actual-unload-event','/real-erase-then-storage-load',rewrite(lambda e:[x for x in e if x['event']!='chunk-unloaded'])),
        ('no-retirement-gap','retirement/actual-immediate-gaps-exist',rewrite(lambda e:[x for x in e if x['event']!='gpu-retired'])),
        ('old-native-buffer-alive','/native-old-storage-dead-before-allocate',{'events':lambda e:[dict(x,snapshot=dict(x['snapshot'],vbo_alive=True)) if x['event']=='gpu-retired' else x for x in e]}),
        ('return-not-storage','/storage-return-new-incarnation',change('returned-a',lambda f:setval(f['target_chunks'][0],'from_storage',False))),
        ('return-old-incarnation','/storage-return-new-incarnation',change('returned-a',lambda f:setval(f['target_chunks'][0],'incarnation',original_inc))),
        ('wrong-World-identity','returned-a/same-live-World-Root-Scene-window',change('returned-a',lambda f:setval(f['identity'],'actual_world_id','wrong-world'))),
        ('wrong-actual-seed','returned-a/same-live-World-Root-Scene-window',change('returned-a',lambda f:setval(f['identity'],'actual_seed',43))),
        ('stale-upload','returned-a/independently-current-selected-upload',change('returned-a',lambda f:setval(f['sections'][0],'uploaded_revision',f['sections'][0]['live_revision']-1))),
        ('offered-current-target','returned-a/independently-current-selected-upload',change('returned-a',lambda f:setval(f['sections'][0],'offered_cpu_ready',True))),
        ('wrong-returned-cell','returned-a/actual-current-World-ID-meta',change('returned-a',lambda f:setval(f['cells'][0],'id',0))),
        ('missing-view-witness','returned-a/fixed-two-current-witness-sections',change('returned-a',lambda f:setval(f,'view_witness_sections',f['view_witness_sections'][:1]))),
        ('stale-view-witness-upload','returned-a/fresh-witness-upload-after-actual-movement',change('returned-a',lambda f:setval(f['view_witness_sections'][0],'upload_serial',f['return_upload_serial_floor']))),
        ('dirty-view-witness-state','/actual-clean-current-witness-incarnation',change('returned-a',lambda f:changed_witness(f,'mesh_state','Dirty'))),
        ('wrong-view-witness-upload-incarnation','/captured-source-current-incarnation',change('returned-a',lambda f:changed_witness(f,'upload_source_incarnation',0))),
        ('combined-normal-replay-overbudget','/same-frame-normal-replay-budget',change('returned-a',overbudget_witness)),
        ('wrong-native-FBO-size','/colour-real-storage',change('returned-a',lambda f:setval(f['planar']['target']['native']['colour'],'width',640))),
        ('native-GL-error','/complete-real-FBO-no-GL-errors',change('returned-a',lambda f:setval(f['hdr']['native'],'gl_error_after',1282))),
        ('stale-RTT-frame','returned-a/actual-same-frame-native-reflection',change('returned-a',lambda f:setval(f['planar'],'frame',f['planar']['frame']-1))),
        ('bad-real-VAO','/real-44B-VAO-inputs',change('returned-a',lambda f:setval(f['draws'][0]['attributes'][0],'buffer',0))),
        ('missing-private-draw','returned-a/both-targets-main-and-private-PBR-submission',change('returned-a',lambda f:setval(f,'draws',[d for d in f['draws'] if d['view']!='reflection']))),
        ('stale-linked-lights','/actual-linked-main-private-light-parameters',change('returned-a',lambda f:next(setval(d['local_lights'],'count',9) for d in f['draws'] if d['local_lights'] is not None))),
        ('bad-original-GPU-bytes','/actual-CPU-GL-byte-equivalence',{'bytes':lambda p,b,c:(bytes([b[0]^1])+b[1:]) if p.name==returned_file else b}),
        ('nonfinite-native-RTT','returned-a/raw-real-radiance-finite',{'bytes':lambda p,b,c:struct.pack('=f',float('nan'))+b[4:] if p.name=='returned-a.native-linear.rgba32f' else b}),
        ('missing-tail-root','tail/root-shutdown/exactly-one-actual-event',rewrite(lambda e:[x for x in e if x['event']!='root-shutdown'])),
        ('retained-clear-cache','tail/real-world-worker-owner-and-caches-cleared',event_change('world-cleared',lambda f:setval(f['cache'],'sections',1))),
        ('retained-empty-mesh-identity-cache','tail/real-world-worker-owner-and-caches-cleared',event_change('world-cleared',lambda f:setval(f['cache'],'empty_section_upload_identities',1))),
        ('empty-claimed-native-release','/exact-old-native-object-set-dead',{'events':empty_component}),
        ('saved-target-corruption-coherent-receipt','save/actual-clone-target-ID-meta-preserved',{'run':corrupt_saved_receipt,'bytes':lambda p,b,c:corrupt_saved if p==saved_path else b}),
    ]
    results=[]
    for name,expected,mutation in faults:
        result=audit(session,cache,mutation);path=destination/(name+'.json')
        path.write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n')
        rejected=result['status']=='FAIL' and expected in result.get('error','')
        results.append({'name':name,'expected':'FAIL at '+expected,'actual':result['status'],
                        'rejected_at_expected_gate':rejected,'error':result.get('error'),
                        'report':str(path),'sha256':sha(path.read_bytes())})
    report={'schema':SCHEMA+'-calibration','status':'PASS_FAULT_COPY_CALIBRATION' if all(x['rejected_at_expected_gate'] for x in results) else 'FAIL',
            'normal_input':False,'ordinary_acceptance_closed':False,'case_count':1+len(results),'positive_actual_count':1,
            'fault_copy_count':len(results),'rejected_fault_copy_count':sum(x['rejected_at_expected_gate'] for x in results),'cases':results,
            'scope':'One actual complete native journal plus deliberate in-memory fault copies with specified semantic rejection gates; zero client execution.'}
    (destination/'calibration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n');return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--session',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--calibrate',action='store_true');args=p.parse_args()
    if args.calibrate:report=calibrate(args.session,args.output)
    else:
        require(not args.output.exists() and args.output.suffix=='.json','new output JSON required')
        report=audit(args.session);args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','check_count','pass_count','fail_count','case_count','rejected_fault_copy_count')},ensure_ascii=False))
    return 0 if report['status'].startswith('PASS_') else 1

if __name__=='__main__':
    raise SystemExit(main())
