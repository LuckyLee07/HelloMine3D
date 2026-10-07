#!/usr/bin/env python3
"""Strict independent oracle for actual complete inactive Water samplers.

The old inactive width/height/format zeros were unqueried initialization values,
not actual GL image storage. New facts must explicitly establish query domains.

Never launches/render a client. Calibration must derive from an actual COMPLETE
journal; it does not construct a mock positive or relax pressure failures.
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
import sys
import zlib
import reference_world_edit_oracle as shared

require, integer, numeric, vector = shared.require, shared.integer, shared.numeric, shared.vector
PHASES=('above-a','collar','below','above-return')
DOMAIN='hellomine3d-reference-water-fallback-sampler-independent-oracle-v1'
FALLBACK_TUS='HelloMine3D.PlanarWater.CompleteSamplerFallback'
FACT_VERSION='complete-2d-sampler-v1'
MAX_BYTES=256*1024**2
f32=lambda n: struct.unpack('<f',struct.pack('<f',n))[0]
PLANE=f32(66.9)
OPEN=['ordinary_input_water_route','continuous_60Hz_transition_quality',
      'other_resident_water_plane_switch','full_goal_acceptance']

def png(data):
    require(data[:8]==b'\x89PNG\r\n\x1a\n','actual PNG signature missing')
    offset=8;ihdr=None;compressed=bytearray();finished=False
    while offset+12<=len(data):
        length=struct.unpack_from('>I',data,offset)[0];tag=data[offset+4:offset+8]
        require(length<=32*1024**2 and offset+12+length<=len(data),'PNG chunk bound')
        body=data[offset+8:offset+8+length];crc=struct.unpack_from('>I',data,offset+8+length)[0]
        require(zlib.crc32(tag+body)&0xffffffff==crc,'PNG CRC mismatch')
        if tag==b'IHDR':require(ihdr is None and length==13,'PNG duplicate/header');ihdr=struct.unpack('>IIBBBBB',body)
        if tag==b'IDAT':compressed.extend(body)
        offset+=12+length
        if tag==b'IEND':require(length==0 and offset==len(data),'PNG EOF');finished=True;break
    require(finished and ihdr is not None and compressed,'PNG incomplete')
    w,h,depth,colour,compression,filtering,interlace=ihdr
    require(depth==8 and colour in (2,6) and compression==filtering==interlace==0,'PNG actual 8bit RGB(A) required')
    raw=zlib.decompress(compressed)
    require(len(raw)==h*(1+w*(3 if colour==2 else 4)),'PNG complete pixel bytes differ')
    return [w,h]

class Audit:
    def __init__(self, output, exit_code, mutation=None, cache=None, runtime_log=None):
        self.output=Path(output).resolve(strict=True);self.exit=integer(exit_code,-255,255)
        self.cache={} if cache is None else cache;self.inputs={};self.checks=[]
        self.mutation=mutation or {}
        self.summary=self.read_json('summary.json')
        lines=self.file('journal.jsonl').decode().splitlines()
        require(1<=len(lines)<=32 and all(lines),'journal exact bounded records')
        self.events=[shared.decode(line) for line in lines]
        if 'summary' in self.mutation:self.summary=self.mutation['summary'](copy.deepcopy(self.summary))
        if 'events' in self.mutation:self.events=self.mutation['events'](copy.deepcopy(self.events))
        shared.finite_tree(self.summary);shared.finite_tree(self.events)
        self.runtime_log=Path(runtime_log) if runtime_log is not None else self.output.parent/'client.log'
        require(not self.runtime_log.is_symlink() and self.runtime_log.is_file() and 0<self.runtime_log.stat().st_size<=32*1024**2,'actual bounded native runtime stdout required')
        raw=self.runtime_log.read_bytes()
        if 'runtime_log' in self.mutation:raw=self.mutation['runtime_log'](raw)
        self.inputs[str(self.runtime_log.resolve())]={'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)}
        self.runtime_log_bytes=raw
    def check(self,key,value):
        self.checks.append({'name':key,'status':'PASS' if value else 'FAIL'});require(value,key)
    def file(self,name):
        require(type(name) is str and len(Path(name).parts)==1 and name not in ('.','..'),'immediate evidence path only')
        p=self.output/name
        require(not p.is_symlink() and p.is_file() and 0<p.stat().st_size<=32*1024**2,'real bounded regular evidence required: '+name)
        if name not in self.cache:self.cache[name]=p.read_bytes()
        data=self.cache[name]
        if 'bytes' in self.mutation:data=self.mutation['bytes'](name,data)
        self.inputs[str(p)]={'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
        return data
    def read_json(self,name):
        v=shared.decode(self.file(name));require(type(v) is dict,'actual JSON object');return v
    def storage(self,target,w,h,samples):
        n=target['native'];
        for key in ('target_count','depth_count','depth_pool','width','height'):integer(target[key])
        for key in ('resolve_fbo','draw_fbo','gl_error_before','gl_error_after'):integer(n[key])
        for attachment in ('colour','resolved','depth','stencil'):
            for key in ('kind','object','format','width','height','samples'):integer(n[attachment][key])
        self.check('native/complete/GL0',n['complete'] is True and n['gl_error_before']==0 and n['gl_error_after']==0)
        r=n['resolved'];self.check('native/resolved-half-float',r['kind']==5890 and integer(r['object'])>0 and r['format']==34842 and [r['width'],r['height'],r['samples']]==[w,h,0])
        c=n['colour'];d=n['depth'];self.check('native/colour-and-depth',c['format']==34842 and integer(c['object'])>0 and [c['width'],c['height'],c['samples']]==[w,h,samples] and d['kind']==36161 and integer(d['object'])>0 and [d['width'],d['height'],d['samples']]==[w,h,samples])
        self.check('native/component-owned-depth',target['target_count']==1 and target['depth_count']==1 and target['owned_depth_attached'] is True and target['depth_pool']==0 and [target['width'],target['height']]==[w,h])
    def phase(self,label,f,index,previous):
        frame=integer(f['frame'],0,2047);self.check(label+'/frame-monotone',previous is None or frame>previous['frame'])
        identity=f['identity'];self.check(label+'/same-actual-identity', all(type(identity[k]) is str and identity[k] not in ('','0','0x0') for k in ('root_instance','scene_instance','window_instance','world_instance','world_id','save_directory')) and (previous is None or identity==previous['identity']))
        integer(identity['seed'],0);integer(identity['terrain_generation_version'],0)
        self.check(label+'/physical-window',f['physical_window']==[2560,1440])
        self.check(label+'/normal-update',numeric(f['simulation_delta'])>0 and integer(f['warm_frames'],12,511)>=12 and integer(f['production_teleports'],12,2048)>0 and (previous is None or numeric(f['world_time'])>=numeric(previous['world_time'])))
        player=vector(f['player']['position'],3);eye=vector(f['main_camera']['eye'],3);logic=vector(f['logic_camera']['position'],3);requested=vector(f['requested_player'],3)
        self.check(label+'/production-player-logic-main',abs(player[0]-194.5)<1e-5 and abs(player[2]+182.5)<1e-5 and max(abs(a-b) for a,b in zip(eye,logic))<1e-5 and requested[0]==194.5 and requested[2]==-182.5)
        # Each frame's real teleport resets velocity and previous=requested.
        # The normal 20Hz fixed tick advances current position, while Camera
        # follows the previous/current interpolation. With dt<.05, the scheduler
        # remainder permits at most one tick. Do not claim actual alpha was read.
        velocity=vector(f['player']['velocity'],3)
        stationary=player==requested and velocity==[0,0,0]
        post_tick_y=f32(requested[1]+f32(-2*f32(.05)))
        single_tick=(0<numeric(f['simulation_delta'])<.05 and velocity==[0,-2,0]
            and player[0]==requested[0] and player[2]==requested[2] and player[1]==post_tick_y)
        self.check(label+'/actual-reset-or-single-gravity-tick',stationary or single_tick)
        lower=f32(player[1]+f32(.6));upper=f32(requested[1]+f32(.6))
        self.check(label+'/actual-camera-interpolation-segment',
            (stationary and abs(eye[1]-player[1]-.6)<1e-5)
            or (single_tick and lower-1e-5<=eye[1]<=upper+1e-5))
        self.check(label+'/actual-camera-rotation',vector(f['player']['rotation'],3)==[5,45,0] and vector(f['logic_camera']['rotation'],3)==[5,45,0])
        self.check(label+'/actual-plane',f['selected_plane_y']==PLANE and f['eye_plane_delta']==f32(eye[1]-PLANE))
        m=f['medium'];cell=vector(m['cell'],3)
        self.check(label+'/World-medium-domain',m['observation_domain']=='actual-World.getBlock-main-eye' and cell==[math.floor(x) for x in eye] and type(m['camera_underwater']) is bool and m['camera_underwater']==(integer(m['id'],0,255)==7))
        integer(m['metadata'],0,255);integer(m['above_id'],0,255)
        underwater=m['camera_underwater'];active=index in (0,3);delta=f['eye_plane_delta']
        expected=not underwater and delta>.15 if active else (not underwater and 0<delta<=.15 if index==1 else underwater and delta<0)
        self.check(label+'/derived-phase',expected)
        depth=f32(cell[1]+1-eye[1]) if underwater else 0
        immersion=(1 if m['above_id']==7 else min(1,max(0,f32(f32(depth-f32(.05))/f32(.95))))) if underwater else 0
        self.check(label+'/actual-depth-immersion',m['surface_depth']==depth and m['immersion']==immersion)
        column=f['water_column'];self.check(label+'/unchanged-resident-column',len(column)==3 and {tuple(v['position']) for v in column}=={(194,y,-183) for y in (64,65,66)} and all(v['id']==7 and v['metadata']==0 and v['observed_frame']==frame and v['observation']=='blocking-World.getBlock-find-only-nonAir' for v in column))
        for item in column:
            integer(item['id'],1,255);integer(item['metadata'],0,255);integer(item['observed_frame'])
        revision=integer(f['frame_input_scene_revision']);self.check(label+'/input-clock-separation',integer(f['later_current_world_visual_revision'])>=revision)
        self.storage(f['hdr'],2560,1440,4);self.check(label+'/HDR-active',f['hdr']['active'] is True)
        p=f['planar'];t=p['target'];self.check(label+'/actual-input-clock',p['frame']==frame and p['scene_revision']==revision and p['input_camera_underwater'] is underwater and p['input_linear_hdr'] is True and p['input_enabled'] is True and p['selected'] is True and p['selected_plane_y']==PLANE)
        self.check(label+'/bounded-component',integer(t['camera_count'])==1 and integer(t['target_count'])<=1 and integer(t['depth_count'])<=1 and integer(t['private_materials'])<=96 and integer(t['private_passes'])<=384 and t['listeners_active'] is False and t['observer_failures']==0)
        pas=p['pass'];draw=f['draw'];u=draw['uniforms'];sampler=draw['sampler'];floor=integer(f['update_floor'])
        numeric(pas['enabled'])
        for name,value in u.items():numeric(value)
        integer(draw['gl_error'])
        for key in ('width','height','format'):integer(sampler[key])
        self.check(label+'/actual-pass-domain',pas['observation_domain']=='actual-Ogre-Water-pass-parameter-and-TUS' and pas['tus_name']=='planarReflection')
        self.check(label+'/actual-driver-domain',draw['observation_domain']=='actual-driver-after-native-Water-draw' and draw['frame']==frame and integer(draw['program'])>0 and draw['program_linked'] is True and draw['attached_production_stages'] is True and integer(draw['primitive_count'])>0 and draw['vertex_program']=='HelloMine3D/WaterVertex' and draw['fragment_program']=='HelloMine3D/WaterFragment' and draw['state_restored'] is True and draw['gl_error']==0)
        self.check(label+'/actual-transparent-state',draw['blend']=={'enabled':True,'src_rgb':770,'dst_rgb':771,'depth_write':False})
        self.check(label+'/actual-linear-uniform',numeric(u['linearHdrMode'])==1 and numeric(u['planarReflectionPlaneY'])==PLANE and numeric(u['waterDetailStrength'])>0 and numeric(u['globalTime'])>=0 and numeric(u['fogDensity'])>0)
        self.check(label+'/sampler-domain',type(sampler['sampling_enabled']) is bool and integer(sampler['unit'],0,31)>=0 and integer(sampler['location'])>=0 and integer(sampler['texture'])>=0)
        self.check(label+'/complete-observation-version',sampler['observation_version']==FACT_VERSION and sampler['binding_observed'] is True and sampler['storage_observed'] is True and sampler['sampler_type_observed'] is True and sampler['sampler_parameters_observed'] is True)
        self.check(label+'/actual-linked-sampler-type',integer(sampler['actual_sampler_type'])==35678 and integer(sampler['actual_sampler_array_size'])==1)
        self.check(label+'/engine-placeholder-identity-domain',sampler['fallback_identity_domain']=='actual-GL3PlusTextureManager.getWarningTextureID' and integer(sampler['expected_fallback_texture_id'])>0 and sampler['fallback_texture_valid'] is True and integer(sampler['sampler_object'])>=0 and sampler['sampler_object_valid'] is True)
        for key in ('base_level','max_level','texture_min_filter','texture_mag_filter','min_filter','mag_filter'):integer(sampler[key])
        q=draw['query_state']
        for key in ('active_texture_before','active_texture_after','original_2d_binding_before','original_2d_binding_after','sampled_2d_binding_before','sampled_2d_binding_after','sampler_object_binding_before','sampler_object_binding_after'):integer(q[key])
        self.check(label+'/query-state-actually-restored',integer(draw['gl_error_before'])==0 and q['active_texture_before']>=33984 and q['active_texture_before']==q['active_texture_after'] and q['original_2d_binding_before']==q['original_2d_binding_after'] and q['sampled_2d_binding_before']==q['sampled_2d_binding_after']==sampler['texture'] and q['sampler_object_binding_before']==q['sampler_object_binding_after']==sampler['sampler_object'])
        self.check(label+'/owned-fallback-name-and-bound',pas['fallback_tus_name']==FALLBACK_TUS and integer(pas['fallback_tus_count'],0,2048)<=1 and integer(pas['fallback_warning_texture_id'])==sampler['expected_fallback_texture_id'] and integer(pas['fallback_sampler_index_actual'])==sampler['unit'])

        count=integer(p['update_count']);self.check(label+'/target-update-consistency',t['update_count']==count and p['active'] is active and t['active'] is active and (previous is None or floor==previous['planar']['update_count']))
        if active:
            self.check(label+'/active-no-fallback-TUS',pas['fallback_tus_present'] is False and pas['fallback_tus_owned'] is False and pas['fallback_tus_count']==0 and pas['fallback_tus_index']==-1 and pas['fallback_water_sampler_bound'] is False and pas['fallback_pass_instance']=='' and pas['fallback_texture_is_blank'] is False)
            self.check(label+'/new-same-frame-RTT',p['reason']=='rendered' and p['last_rendered_frame']==frame and count>floor and integer(p['colour_batches'])>0 and 0<=integer(p['shadow_updates'])<=1)
            self.storage(t,1280,720,0)
            self.check(label+'/native-sampler-actual-attachment',u['planarReflectionEnabled']==1 and pas['enabled']==1 and pas['tus_present'] is True and pas['tus_matches_target'] is True and integer(pas['tus_index'])>=0 and pas['tus_texture_name']==t['texture_name'] and t['water_sampler_bound'] is True and sampler['sampling_enabled'] is True and [sampler['texture'],sampler['width'],sampler['height'],sampler['format']]==[t['native']['resolved']['object'],1280,720,34842])
            raw=self.file(label+'.native-linear.rgba32f');self.check(label+'/actual-raw-budget',len(raw)==1280*720*16)
            floats=array('f');floats.frombytes(raw);self.check(label+'/actual-raw-finite',len(floats)==1280*720*4 and all(math.isfinite(v) for v in floats))
            self.check(label+'/actual-raw-signal',max(floats)>min(floats))
            facts=self.file(label+'.facts.txt').decode();values=dict(re.findall(r'([a-z_]+)=([^\s]+)',facts))
            self.check(label+'/readback-facts',values['frame']==str(frame) and values['width']=='1280' and values['height']=='720' and values['raw_bytes']==str(len(raw)) and values['gl_error_before']==values['gl_error_after']==values['nonfinite_components']=='0' and values['main_water_texture_bound']=='1' and values['colour_object']==str(t['native']['resolved']['object']))
            self.check(label+'/preview-full-pixels',png(self.file(label+'.camera-preview.png'))==[1280,720])
        else:
            self.check(label+'/no-RTT-update',p['reason']=='underwater-or-surface-crossing' and count==floor and integer(p['last_rendered_frame'])<frame and p['colour_batches']==p['shadow_updates']==0)
            self.check(label+'/actual-disabled-pass-and-driver',pas['enabled']==0 and pas['tus_present'] is False and pas['tus_index']==-1 and pas['tus_texture_name']=='' and pas['tus_matches_target'] is False and t['water_sampler_bound'] is False and u['planarReflectionEnabled']==0 and sampler['sampling_enabled'] is False)
            self.check(label+'/inactive-owned-complete-fallback-TUS',pas['fallback_tus_present'] is True and pas['fallback_tus_owned'] is True and pas['fallback_tus_count']==1 and integer(pas['fallback_tus_index'])==sampler['unit'] and pas['fallback_texture_is_blank'] is True and pas['fallback_water_sampler_bound'] is True and type(pas['fallback_pass_instance']) is str and pas['fallback_pass_instance'] not in ('','0','0x0'))
            self.check(label+'/inactive-exact-complete-placeholder-storage',integer(sampler['texture'])==sampler['expected_fallback_texture_id'] and [sampler['width'],sampler['height'],sampler['format']]==[8,8,32849] and sampler['base_level']==sampler['max_level']==0 and sampler['texture_min_filter']==sampler['texture_mag_filter']==sampler['min_filter']==sampler['mag_filter']==9729)
            # The named reflection TUS is absent and its uniform remains0.
            # The actual complete engine placeholder is present but unconsumed;
            # unlike old helper zeros, all storage/filter values were queried.
            if t['target_count']:
                self.storage(t,1280,720,0)
            self.check(label+'/inactive-no-readback',not any((self.output/(label+suffix)).exists() for suffix in ('.native-linear.rgba32f','.camera-preview.png','.facts.txt')))
        self.check(label+'/main-full-pixels',png(self.file(label+'.main.png'))==[2560,1440])
        return f
    def run(self):
        s=self.summary
        self.check('actual-process-exit',self.exit==0)
        self.check('actual-summary-complete',s['schema']=='hellomine3d-reference-water-transition-summary-v1' and s['status']=='COMPLETE' and s['completed_phases']==4 and s['records']==16 and s['normal_save'] is True and s['native_fault']=='' and s['normal_simulation'] is True and s['normal_input'] is False and s['input_event_count']==0 and s['other_resident_plane_transition']=='NOT_RUN_NO_DECLARED_SECOND_RESIDENT_LEVEL')
        self.check('summary/hard-bound',integer(s['frame'],0,2047)>=0 and 0<=numeric(s['elapsed_ms'])<30000)
        restoration=s['restoration'];self.check('actual-origin-restored',restoration['production_teleport'] is True and restoration['actual_player']==restoration['original_player'] and restoration['actual_rotation']==restoration['original_rotation'] and vector(restoration['actual_rotation'],3)==[5,45,0])
        expected=[(phase,event) for phase in PHASES for event in ('begin','observation','checkpoint','end')]
        self.check('journal/exact-protocol',len(self.events)==16 and [(e['phase'],e['event']) for e in self.events]==expected)
        previous=None
        for i,phase in enumerate(PHASES):
            records=self.events[4*i:4*i+4];begin,observation,checkpoint,end=records
            for j,e in enumerate(records,4*i+1):
                self.check('record/'+str(j),e['schema']=='hellomine3d-reference-water-transition-journal-v1' and e['sequence']==j and type(e['normal_input']) is bool and e['normal_input'] is False and e['input_event_count']==0 and integer(e['frame'],0,2047)>=0 and numeric(e['elapsed_ms'])<30000)
            self.check(phase+'/phase-bounds',checkpoint['frame']-begin['frame']<512 and checkpoint['frame']-begin['frame']>=11 and checkpoint['elapsed_ms']-begin['elapsed_ms']<8000 and checkpoint['frame']==observation['frame']==end['frame'] and checkpoint['facts']==observation['facts'] and checkpoint['facts']['frame']==checkpoint['frame'])
            self.check(phase+'/actual-teleport-begin',begin['facts']['production_teleport'] is True and vector(begin['facts']['requested_player'],3)==checkpoint['facts']['requested_player'])
            previous=self.phase(phase,checkpoint['facts'],i,previous)
        self.check('final-frame-summary',s['frame']==previous['frame'])
        self.check('actual-world-clock-progressed',previous['world_time']>self.events[2]['facts']['world_time'])
        densities=[self.events[4*i+2]['facts']['draw']['uniforms']['fogDensity'] for i in range(4)]
        self.check('actual-linked-Water-medium-fog-response',densities[2]>max(densities[0],densities[1],densities[3]))
        files=list(self.output.iterdir());self.check('whole-output-bound',len(files)<=32 and all(p.is_file() and not p.is_symlink() for p in files) and sum(p.stat().st_size for p in files)<=MAX_BYTES)
        exact={'journal.jsonl','summary.json',*(phase+'.main.png' for phase in PHASES),*(phase+suffix for phase in (PHASES[0],PHASES[3]) for suffix in ('.native-linear.rgba32f','.camera-preview.png','.facts.txt'))}
        self.check('only-declared-captures',{p.name for p in files}==exact)
        meta=self.output.parent/'save/world.meta'
        self.check('owned-saved-metadata-kind',meta.is_file() and not any(p.is_symlink() for p in (meta,*meta.parents)) and meta.stat().st_size<65536)
        metadata=meta.read_bytes();self.inputs[str(meta)]={'sha256':hashlib.sha256(metadata).hexdigest(),'bytes':len(metadata)}
        ids=[line[9:] for line in metadata.decode().splitlines() if line.startswith('world_id ')]
        self.check('actual-World-id-matches-normal-save',ids==[previous['identity']['world_id']])
        releases=[]
        for line in self.runtime_log_bytes.decode('utf-8').splitlines():
            if '[PLANAR_SAMPLER_FALLBACK_RELEASE]' not in line:continue
            m=re.fullmatch(r'\[PLANAR_SAMPLER_FALLBACK_RELEASE\] owned_tus=(\d+) pass_bound=(\d+) removed=(\d+) failures=(\d+) manager_texture_owned=(\d+)',line)
            self.check('fallback-cleanup/exact-certificate',m is not None)
            releases.append([int(v) for v in m.groups()])
        self.check('fallback-cleanup/one-cycle-owned-TUS-released',len(releases)==1 and releases[0][0]==releases[0][1]==releases[0][3]==releases[0][4]==0 and 1<=releases[0][2]<=2048)
        return {'schema':DOMAIN,'status':'PASS','checks':len(self.checks),'failures':0,'checks_detail':self.checks,'inputs':self.inputs,'scope':'same Root actual complete inactive 2D sampler/TUS and active native RTT, normal simulation, owned teleport input0; engine texture is not owned or deleted by Planar','ordinary_acceptance':'NOT_RUN','open':OPEN,'old_inactive_zero_domain':'NOT_QUERIED_INITIAL_VALUES_NOT_ACTUAL_GL_STORAGE','warning_cause_or_pixel_harmlessness':'NOT_PROVEN'}

def rejected_result(a,error):
    return {'schema':DOMAIN,'status':'FAIL','checks':len(a.checks) if a else 0,'failures':1,'reason':str(error),'checks_detail':a.checks if a else [],'inputs':a.inputs if a else {},'ordinary_acceptance':'NOT_RUN','open':OPEN}

def evaluate(output,exit_code,mutation=None,cache=None,runtime_log=None):
    a=None
    try:a=Audit(output,exit_code,mutation,cache,runtime_log);return a.run(),a
    except (shared.Rejected,KeyError,ValueError,TypeError,OSError,zlib.error,struct.error) as e:return rejected_result(a,e),a

def native_fault_facts(a):
    require(a is not None and a.exit==1 and a.summary['status']=='FAILED' and a.summary['native_fault']=='retain-water-binding','actual deliberate failed process required')
    checkpoints=[e for e in a.events if e['event']=='checkpoint'];observations=[e for e in a.events if e['event']=='observation' and e['phase']=='collar']
    require(len(checkpoints)==1 and checkpoints[0]['phase']=='above-a' and len(observations)==1,'actual A then collar observation required')
    before=checkpoints[0]['facts'];f=observations[0]['facts'];p=f['planar'];draw=f['draw'];pas=p['pass']
    require(f['identity']==before['identity'] and f['medium']['camera_underwater'] is False and 0<f['eye_plane_delta']<=.15,'same-Root actual dry collar required')
    require(p['active'] is False and p['reason']=='underwater-or-surface-crossing' and p['update_count']==before['planar']['update_count']==f['update_floor'] and p['last_rendered_frame']<f['frame'],'actual inactive unchanged old RTT required')
    require(pas['enabled']==1 and pas['tus_present'] is True and pas['tus_matches_target'] is True and draw['uniforms']['planarReflectionEnabled']==1 and draw['sampler']['sampling_enabled'] is True and draw['program_linked'] is True and draw['frame']==f['frame'] and draw['gl_error']==0,'actual retained live pass+driver fault required')
    sampler=draw['sampler'];q=draw['query_state']
    require(sampler['observation_version']==FACT_VERSION and sampler['binding_observed'] is True and sampler['storage_observed'] is True and sampler['sampler_type_observed'] is True and sampler['actual_sampler_type']==35678 and sampler['actual_sampler_array_size']==1,'actual new-version queried sampler fault required')
    require(pas['fallback_tus_present'] is False and pas['fallback_tus_owned'] is False and pas['fallback_tus_count']==0 and pas['fallback_water_sampler_bound'] is False and sampler['texture']==p['target']['native']['resolved']['object'],'actual retained reflection instead of complete fallback required')
    require(draw['gl_error_before']==0 and draw['state_restored'] is True and q['active_texture_before']==q['active_texture_after'] and q['sampled_2d_binding_before']==q['sampled_2d_binding_after']==sampler['texture'],'actual negative native query state required')
    return {'status':'EXPECTED_NATIVE_FAULT_REJECTED','actual_exit_code':1,'same_identity':True,'frame':f['frame'],'update_count':p['update_count'],'actual_pass_enabled':pas['enabled'],'actual_tus_present':pas['tus_present'],'actual_driver_enabled':draw['uniforms']['planarReflectionEnabled'],'old_RTT_readback_not_substituted':True,'positive_status':'FAIL'}

def fault_events(fn):
    def mutate(events):
        for e in events:
            if e['phase']=='collar' and e['event'] in ('observation','checkpoint'):fn(e['facts'])
        return events
    return mutate

def calibrate(output,cache,runtime_log=None):
    # Each copy derives solely from the actual full positive. No epsilon and no
    # synthesized positive. Pixel files stay original except explicit corruption.
    def alter_summary(k,v):
        def mutate(s):s[k]=v;return s
        return mutate
    faults={
      'false-complete':{'summary':alter_summary('completed_phases',3)},
      'normal-input':{'summary':alter_summary('normal_input',True)},
      'legacy-domain':{'events':fault_events(lambda f:f['draw']['uniforms'].__setitem__('linearHdrMode',0))},
      'wrong-world-medium':{'events':fault_events(lambda f:f['medium'].__setitem__('camera_underwater',True))},
      'collar-request-not-actual':{'events':fault_events(lambda f:f.__setitem__('eye_plane_delta',.16))},
      'retained-pass-TUS':{'events':fault_events(lambda f:f['planar']['pass'].__setitem__('tus_present',True))},
      'retained-GL-enable':{'events':fault_events(lambda f:f['draw']['uniforms'].__setitem__('planarReflectionEnabled',1))},
      'inactive-sampler-consumed':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('sampling_enabled',True))},
      'inactive-update':{'events':fault_events(lambda f:f['planar'].__setitem__('update_count',f['update_floor']+1))},
      'stale-RTT-labelled-current':{'events':fault_events(lambda f:f['planar'].__setitem__('last_rendered_frame',f['frame']))},
      'cold-root-replacement':{'events':fault_events(lambda f:f['identity'].__setitem__('root_instance','0x1234'))},
      'World-water-meta':{'events':fault_events(lambda f:f['water_column'][0].__setitem__('metadata',3))},
      'non-current-input':{'events':fault_events(lambda f:f['planar'].__setitem__('scene_revision',f['frame_input_scene_revision']-1))},
      'fake-driver-domain':{'events':fault_events(lambda f:f['draw'].__setitem__('observation_domain','submitted-only'))},
      'unlinked-program':{'events':fault_events(lambda f:f['draw'].__setitem__('program_linked',False))},
      'zero-primitives':{'events':fault_events(lambda f:f['draw'].__setitem__('primitive_count',0))},
      'GL-error':{'events':fault_events(lambda f:f['draw'].__setitem__('gl_error',1280))},
      'depth-write-change':{'events':fault_events(lambda f:f['draw']['blend'].__setitem__('depth_write',True))},
      'normal-simulation-zero':{'events':fault_events(lambda f:f.__setitem__('simulation_delta',0))},
      'bool-frame-counter':{'summary':alter_summary('frame',True)},
      'no-actual-save':{'summary':alter_summary('normal_save',False)},
      'raw-nonfinite':{'bytes':lambda n,b:struct.pack('<f',float('nan'))+b[4:] if n=='above-return.native-linear.rgba32f' else b},
      'PNG-corrupt':{'bytes':lambda n,b:b[:-1]+bytes([b[-1]^1]) if n=='below.main.png' else b},
    }
    baseline,_=evaluate(output,0,cache=cache,runtime_log=runtime_log)
    require(baseline['status']=='PASS','actual calibration baseline changed or incomplete')
    expected_gates={
      'false-complete':'actual-summary-complete','normal-input':'actual-summary-complete','legacy-domain':'actual-linear-uniform',
      'wrong-world-medium':'World-medium-domain','collar-request-not-actual':'actual-plane','retained-pass-TUS':'actual-disabled-pass-and-driver',
      'retained-GL-enable':'actual-disabled-pass-and-driver','inactive-sampler-consumed':'actual-disabled-pass-and-driver',
      'inactive-update':'target-update-consistency','stale-RTT-labelled-current':'no-RTT-update','cold-root-replacement':'same-actual-identity',
      'World-water-meta':'unchanged-resident-column','non-current-input':'actual-input-clock','fake-driver-domain':'actual-driver-domain',
      'unlinked-program':'actual-driver-domain','zero-primitives':'actual-driver-domain','GL-error':'actual-driver-domain',
      'depth-write-change':'actual-transparent-state','normal-simulation-zero':'normal-update','bool-frame-counter':'invalid/bounded integer',
      'no-actual-save':'actual-summary-complete','raw-nonfinite':'actual-raw-finite','PNG-corrupt':'PNG CRC mismatch'}
    faults.update({
      'old-unqueried-domain':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('storage_observed',False))},
      'wrong-sampler-type':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('actual_sampler_type',35679))},
      'wrong-manager-id':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('fallback_identity_domain','requested-token'))},
      'sampler-state-not-restored':{'events':fault_events(lambda f:f['draw']['query_state'].__setitem__('sampled_2d_binding_after',0))},
      'unowned-fallback':{'events':fault_events(lambda f:f['planar']['pass'].__setitem__('fallback_tus_owned',False))},
      'duplicate-fallback':{'events':fault_events(lambda f:f['planar']['pass'].__setitem__('fallback_tus_count',2))},
      'wrong-placeholder-width':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('width',7))},
      'wrong-placeholder-format':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('format',34842))},
      'mipmap-filter':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('min_filter',9987))},
      'wrong-max-level':{'events':fault_events(lambda f:f['draw']['sampler'].__setitem__('max_level',1))},
      'missing-owned-cleanup':{'runtime_log':lambda b:b'\n'.join(l for l in b.splitlines() if b'[PLANAR_SAMPLER_FALLBACK_RELEASE]' not in l)+b'\n'},
    })
    expected_gates.update({
      'old-unqueried-domain':'complete-observation-version','wrong-sampler-type':'actual-linked-sampler-type',
      'wrong-manager-id':'engine-placeholder-identity-domain','sampler-state-not-restored':'query-state-actually-restored',
      'unowned-fallback':'inactive-owned-complete-fallback-TUS','duplicate-fallback':'owned-fallback-name-and-bound',
      'wrong-placeholder-width':'inactive-exact-complete-placeholder-storage','wrong-placeholder-format':'inactive-exact-complete-placeholder-storage',
      'mipmap-filter':'inactive-exact-complete-placeholder-storage','wrong-max-level':'inactive-exact-complete-placeholder-storage',
      'missing-owned-cleanup':'fallback-cleanup/one-cycle-owned-TUS-released',
    })
    # Two pose faults mutate only paired above-a facts from the same actual
    # positive, preserving the raw files and all 35 existing negative cases.
    def above_pose_events(fn):
        def mutate(events):
            for event in events:
                if event['phase']=='above-a' and event['event'] in ('observation','checkpoint'):
                    fn(event['facts'])
            return events
        return mutate
    def outside_interpolation(f):
        y=f32(f['requested_player'][1]+f32(.6)+.2)
        f['main_camera']['eye'][1]=y;f['logic_camera']['position'][1]=y
    def wrong_single_tick(f):
        f['player']['velocity']=[0,-4,0]
        f['player']['position'][1]=f32(f['requested_player'][1]+f32(-4*f32(.05)))
    faults.update({
      'above-eye-and-logic-outside-interpolation':{'events':above_pose_events(outside_interpolation)},
      'above-wrong-single-tick-velocity-and-displacement':{'events':above_pose_events(wrong_single_tick)},
    })
    expected_gates.update({
      'above-eye-and-logic-outside-interpolation':'actual-camera-interpolation-segment',
      'above-wrong-single-tick-velocity-and-displacement':'actual-reset-or-single-gravity-tick',
    })
    results=[]
    for name,mutation in faults.items():
        result,_=evaluate(output,0,mutation,cache,runtime_log);results.append({'case':name,'expected':'FAIL','actual':result['status'],'expected_gate':expected_gates[name],'expected_gate_reached':expected_gates[name] in result.get('reason',''),'reason':result.get('reason')})
    result,_=evaluate(output,1,cache=cache,runtime_log=runtime_log);results.append({'case':'nonzero-process-exit','expected':'FAIL','actual':result['status'],'expected_gate_reached':result.get('reason')=='actual-process-exit','reason':result.get('reason')})
    return {'schema':DOMAIN+'-calibration','source':'actual COMPLETE positive + strictly mutated evidence copies','status':'PASS' if all(r['actual']=='FAIL' and r['expected_gate_reached'] for r in results) else 'FAIL','positive':1,'negative_cases':len(results),'results':results}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True,type=Path);p.add_argument('--process-exit-code',required=True,type=int);p.add_argument('--result',required=True,type=Path);p.add_argument('--calibration',type=Path);p.add_argument('--runtime-log',type=Path,help='Actual native stdout after normal shutdown; default output-parent/client.log')
    args=p.parse_args()
    output=args.output.resolve(strict=True)
    for path in (args.result,args.calibration):
        if path is not None:require(output not in (path.resolve(),*path.resolve().parents),'oracle outputs must be outside immutable native evidence')
    result,a=evaluate(args.output,args.process_exit_code,runtime_log=args.runtime_log)
    if result['status']=='FAIL' and a is not None and a.summary.get('native_fault')=='retain-water-binding':
        try:result['native_negative_control']=native_fault_facts(a)
        except (shared.Rejected,KeyError,TypeError,ValueError) as e:result['native_negative_control']={'status':'NOT_ESTABLISHED','reason':str(e)}
    args.result.parent.mkdir(parents=True,exist_ok=True);require(not args.result.exists(),'new oracle result required');args.result.write_text(json.dumps(result,indent=2)+'\n')
    if args.calibration:
        require(result['status']=='PASS','calibration requires actual native positive');require(not args.calibration.exists(),'new calibration output required');args.calibration.write_text(json.dumps(calibrate(args.output,a.cache,args.runtime_log),indent=2)+'\n')
    print(json.dumps({'status':result['status'],'checks':result['checks'],'failures':result['failures'],'reason':result.get('reason'),'result':str(args.result)}));return 0 if result['status']=='PASS' else 1
if __name__=='__main__':sys.exit(main())
