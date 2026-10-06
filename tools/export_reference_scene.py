#!/usr/bin/env python3
"""Export an editable, bounded World-only scene into deterministic C++ regions."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import time

LEGACY = ['Air','Grass','Dirt','Stone','OakBark','OakLeaf','Sand','Water','Cactus','Rose','TallGrass','DeadShrub','CoalOre','IronOre','Glass','GlassBorderless','Chest','WheatCrop','Workbench','Furnace','WaystoneCore','Torch','OakPlank','Cobblestone','OakDoorClosed','OakDoorOpen','Crusher','Snow','Gravel','Clay','ForestFloor','MossStone','Silt']
KIT = ['StoneStep','StoneWindowFrame','StoneBrick','StoneSlab','StoneCornice','ClayTileStep','ClayTileEave','TimberBeam','TimberRailing','StoneWindowSill','StonePlanter','Lantern']
BLOCKS = LEGACY + KIT


def regions(source):
    result=[]
    for operation in source['operations']:
        box=operation['box']
        x0,y0,z0,x1,y1,z1=box
        block,yaw=operation['block'],operation['yaw']
        if not operation.get('exclude_preserved_groves'):
            result.append((box,block,yaw));continue
        # Preserve an entire native column including soil and tree ownership.
        for z in range(z0,z1+1):
            start=None
            for x in range(x0,x1+2):
                keep=x<=x1 and not any(a<=x<=c and b<=z<=d for a,b,c,d in source['preserved_groves'])
                if keep and start is None:start=x
                if not keep and start is not None:
                    result.append(([start,y0,z,x-1,y1,z],block,yaw));start=None
    return result


def validate(source):
    if source['version']!=2 or source['origin']!=[208,66,-192] or source['bounds']!=[-24,-4,-24,23,20,23]:
        raise ValueError('Scene v2 has one frozen 48m site and fixed origin')
    if source['edit_budget']!=72000 or source['chunk_budget']!=16:
        raise ValueError('Scene edit and resident chunk budgets are frozen')
    if not 1<=len(source['operations'])<=1024:raise ValueError('Operation budget exceeded')
    for operation in source['operations']:
        box=operation['box'];block=operation['block'];yaw=operation['yaw']
        if len(box)!=6 or any(type(v) is not int for v in box):raise ValueError('Integer six-coordinate regions required')
        if block not in BLOCKS or type(yaw) is not int or not 0<=yaw<4 or (block not in KIT and yaw!=0):raise ValueError('Invalid stable block or metadata')
        if any(box[a]>box[a+3] or box[a]<source['bounds'][a] or box[a+3]>source['bounds'][a+3] for a in range(3)):raise ValueError('Edit escapes fixed scene bounds: '+operation['group'])
    edits=regions(source);cells={};writes=0
    for box,block,yaw in edits:
        x0,y0,z0,x1,y1,z1=box
        writes+=(x1-x0+1)*(y1-y0+1)*(z1-z0+1)
        for y in range(y0,y1+1):
            for z in range(z0,z1+1):
                for x in range(x0,x1+1):cells[x,y,z]=(block,yaw)
    if writes>source['edit_budget']:raise ValueError('World edit budget exceeded')
    trees=source.get('trees',[])
    if not 1<=len(trees)<=4:raise ValueError('One to four bounded planted trees required')
    names=set()
    for tree in trees:
        name=tree['name'];root=tree['root']
        if not isinstance(name,str) or not name or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in name) or name in names:raise ValueError('Unique plain tree name required')
        names.add(name)
        if len(root)!=3 or any(type(v) is not int for v in root):raise ValueError('Integer local tree root required')
        if tree['kind'] not in ['Oak','Birch'] or tree['origin_type']!='ordinary_planted':raise ValueError('Planted tree kind/source is invalid')
        if type(tree['seed']) is not int or not 0<=tree['seed']<=2147483647 or type(tree['stature']) is not int or not 0<=tree['stature']<=2:raise ValueError('Tree seed/stature is invalid')
        # Production maximum radius/height and 1024 raw planned writes remain
        # conservative limits. Runtime validates the complete actual visitor
        # sequence against future scene blocks, bodies and resident chunks
        # before any scene/tree writes; geometry is not reimplemented here.
        if not (-24<=root[0]-6 and root[0]+6<=23 and -24<=root[2]-6 and root[2]+6<=23 and 1<=root[1] and root[1]+15<=20):raise ValueError('Tree projection escapes fixed scene bounds')
        if cells.get((root[0],root[1]-1,root[2]),('Air',0))[0] not in ['Grass','Dirt','ForestFloor']:raise ValueError('Planted root needs authored soil support')
    if writes+len(trees)*1024>source['edit_budget']:raise ValueError('Scene and worst-case tree budget exceeded')
    if not 1<=len(source.get('weather_shell_regions',[]))<=32:raise ValueError('Bounded weather shell regions required')
    for region in source['weather_shell_regions']:
        box=region['box'];expected=(region['block'],region['yaw'])
        if len(box)!=6 or any(type(v) is not int for v in box) or any(box[a]>box[a+3] or box[a]<source['bounds'][a] or box[a+3]>source['bounds'][a+3] for a in range(3)):raise ValueError('Weather shell region escapes scene bounds')
        for y in range(box[1],box[4]+1):
            for z in range(box[2],box[5]+1):
                for x in range(box[0],box[3]+1):
                    if cells.get((x,y,z))!=expected:raise ValueError('Weather shell is open at '+str((x,y,z)))
    for cell in source['required_cells']:
        if cells.get(tuple(cell['at']))!=(cell['block'],cell['yaw']):raise ValueError('Required authoritative cell missing: '+str(cell))
    for cell in source['clearance_cells']:
        if cells.get(tuple(cell),('Air',0))[0]!='Air':raise ValueError('Body/door/bridge clearance failed: '+str(cell))
    used={block for block,yaw in cells.values()}
    if not set(KIT)<=used:raise ValueError('Both layouts must exercise the entire architectural kit')
    if len(source['buildings'])!=2 or source['buildings'][0]['roof_ridge']==source['buildings'][1]['roof_ridge'] or source['buildings'][0]['floors']==source['buildings'][1]['floors']:
        raise ValueError('Second layout must differ in storeys and roof axis')
    names=[v['name'] for v in source['views']]
    if len(set(names))!=len(names) or not {'street','interior','details','water','second'}<=set(names):raise ValueError('Named ordinary-height views missing')
    # Count only surviving source geometry, with existing kit cached face budgets.
    counts=collections.Counter(block for block,yaw in cells.values() if block!='Air')
    return edits,cells,writes,counts


def output(source,edits,cells):
    text=['// Generated by tools/export_reference_scene.py; edit media/sources/reference-visual-scene-v2.json.','constexpr SceneEdit kSceneEdits[] = {']
    for box,block,yaw in edits:text.append('    {'+','.join(map(str,box))+',BlockId::'+block+','+str(yaw)+'},')
    text+=['};','constexpr SceneView kSceneViews[] = {']
    for view in source['views']:
        def values(key):return ','.join(format(float(v),'.6g')+'f' if float(v)%1 else str(int(v))+'.f' for v in view[key])
        text.append('    {"'+view['name']+'",{'+values('position')+'},{'+values('rotation')+'}},')
    text+=['};','constexpr SceneCell kSceneRequiredCells[] = {']
    for cell in source['required_cells']:text.append('    {'+','.join(map(str,cell['at']))+',BlockId::'+cell['block']+','+str(cell['yaw'])+'},')
    text+=['};','constexpr SceneCell kSceneClearanceCells[] = {']
    for cell in source['clearance_cells']:text.append('    {'+','.join(map(str,cell))+',BlockId::Air,0},')
    text+=['};','constexpr SceneEdit kSceneWeatherShellRegions[] = {']
    for region in source['weather_shell_regions']:
        text.append('    {'+','.join(map(str,region['box']))+',BlockId::'+region['block']+','+str(region['yaw'])+'},')
    text+=['};','constexpr SceneCell kSceneWeatherRoofCells[] = {']
    for box in source['flat_roof_regions']:
        for y in range(box[1],box[4]+1):
            for z in range(box[2],box[5]+1):
                for x in range(box[0],box[3]+1):
                    block,yaw=cells[x,y,z]
                    text.append('    {'+','.join(map(str,[x,y,z]))+',BlockId::'+block+','+str(yaw)+'},')
    text+=['};','constexpr SceneTree kSceneTrees[] = {']
    for tree in source['trees']:
        text.append('    {'+json.dumps(tree['name'])+',{'+','.join(map(str,tree['root']))+'},AdventureTreeKind::'+tree['kind']+','+str(tree['seed'])+','+str(tree['stature'])+'},')
    base_writes=sum((b[3]-b[0]+1)*(b[4]-b[1]+1)*(b[5]-b[2]+1) for b,block,yaw in edits)
    text+=['};',f'constexpr std::size_t kSceneEditBudget = {source["edit_budget"]}u;',f'constexpr std::size_t kSceneBaseEditAttempts = {base_writes}u;',f'constexpr std::size_t kSceneRegionCount = {len(edits)}u;','']
    return '\n'.join(text)


def preview(source,cells,path):
    palette={'Water':'#668b93','Grass':'#6c8251','Cobblestone':'#a5a18c','StoneStep':'#d4cdb6','StoneBrick':'#d4cdb6','ClayTileStep':'#af624a','ClayTileEave':'#af624a','OakPlank':'#957354','TimberBeam':'#76533a','TimberRailing':'#76533a','StonePlanter':'#d4cdb6','Rose':'#aa5575','TallGrass':'#698147','Lantern':'#e5b65a'}
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="900" height="850" viewBox="0 0 900 850">','<rect width="900" height="850" fill="#f3f0e8"/>','<text x="40" y="30" font-family="sans-serif" font-size="18">48m reference town v2 · authoring plan, not gameplay evidence</text>']
    columns={}
    for (x,y,z),(block,yaw) in cells.items():
        if block!='Air' and ((x,z) not in columns or y>columns[x,z][0]):columns[x,z]=(y,block)
    for z in range(-24,24):
        for x in range(-24,24):
            block=columns.get((x,z),(0,'native'))[1]
            colour=palette.get(block,'#718457' if block=='native' else '#b9b29e')
            svg.append(f'<rect x="{40+(x+24)*15}" y="{55+(z+24)*15}" width="15" height="15" fill="{colour}" stroke="#eee9db" stroke-width=".3"/>')
    for building in source['buildings']:
        x0,z0,x1,z1=building['bounds']
        svg.append(f'<text x="{40+(x0+24)*15}" y="{45+(z0+24)*15}" font-family="sans-serif" font-size="12">{building["name"]}</text>')
    for view in source['views']:
        x,y,z=view['position'];px=40+(x+24)*15;pz=55+(z+24)*15
        svg.append(f'<circle cx="{px}" cy="{pz}" r="5" fill="#1c3e53"/><text x="{px+7}" y="{pz}" font-family="sans-serif" font-size="11">{view["name"]}</text>')
    svg+=['<text x="40" y="815" font-family="sans-serif" font-size="13">Same twelve parts · two floors + balcony / single floor + gallery · two ordinary planted trees; preserved native columns at north corners</text>','</svg>']
    path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(svg)+'\n')


def main():
    start=time.monotonic();parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--check',action='store_true');parser.add_argument('--preview',type=Path);parser.add_argument('--record',type=Path)
    args=parser.parse_args();path=args.root/'media/sources/reference-visual-scene-v2.json';source=json.loads(path.read_text())
    edits,cells,writes,counts=validate(source)
    kit=json.loads((args.root/'media/sources/architectural-kit-v1.json').read_text())
    shapes={p['name']:p['boxes'] for p in kit['parts']}
    def boxes_for(block,yaw):
        result=[]
        for box in shapes.get(block,[{'bounds':[0,0,0,8,8,8],'collidable':True}]):
            low=list(box['bounds'][:3]);high=list(box['bounds'][3:])
            for rotation in range(yaw):low[0],high[0],low[2],high[2]=8-high[2],8-low[2],low[0],high[0]
            result.append((low,high))
        return result
    weather_cells=set()
    for region in source['weather_shell_regions']:
        filled=set()
        for low,high in boxes_for(region['block'],region['yaw']):
            for y in range(low[1],high[1]):
                for z in range(low[2],high[2]):
                    for x in range(low[0],high[0]):filled.add((x,y,z))
        if len(filled)!=512:raise ValueError('Weather shell needs actual full-cell geometry')
        box=region['box']
        weather_cells.update((x,y,z) for y in range(box[1],box[4]+1) for z in range(box[2],box[5]+1) for x in range(box[0],box[3]+1))
    roof_cells=set()
    for box in source['flat_roof_regions']:
        for y in range(box[1],box[4]+1):
            for z in range(box[2],box[5]+1):
                for x in range(box[0],box[3]+1):
                    block,yaw=cells[x,y,z];bottom=set()
                    for low,high in boxes_for(block,yaw):
                        if low[1]!=0:continue
                        bottom.update((u,v) for u in range(low[0],high[0]) for v in range(low[2],high[2]))
                    if len(bottom)!=64:raise ValueError('Flat roof does not meet the wall top at '+str((x,y,z)))
                    roof_cells.add((x,y,z))
    roof_joints=0
    for profile in source['roof_profiles']:
        axis=profile['axis'];other=2 if axis==0 else 0;rows={}
        if axis not in [0,2]:raise ValueError('Roof profile needs a horizontal axis')
        for operation in source['operations']:
            if operation['group']!=profile['group']:continue
            box=operation['box']
            if not box[other]<=profile['fixed_coordinate']<=box[other+3]:continue
            for t in range(box[axis],box[axis+3]+1):
                xyz=[0,box[1],0];xyz[axis]=t;xyz[other]=profile['fixed_coordinate']
                if cells.get(tuple(xyz))!=(operation['block'],operation['yaw']):raise ValueError('Roof profile source was overwritten')
                rows[t]=(box[1],operation['block'],operation['yaw'])
        if len(rows)<3 or max(rows)-min(rows)+1!=len(rows):raise ValueError('Roof profile is incomplete')
        def edge_intervals(row,edge):
            y,block,yaw=row;result=[]
            for low,high in boxes_for(block,yaw):
                if low[axis]<=edge<=high[axis] and low[other]<=4<=high[other]:result.append((y+low[1]/8,y+high[1]/8))
            return result
        for t in range(min(rows),max(rows)):
            a=edge_intervals(rows[t],8);b=edge_intervals(rows[t+1],0)
            if not any(max(lo,other_lo)<=min(hi,other_hi)+1e-6 for lo,hi in a for other_lo,other_hi in b):raise ValueError('Roof has an open geometric seam at '+profile['name']+' '+str(t+1))
            roof_joints+=1
    # Independently inspect the authored standing body, using source boxes and
    # their exact right-angle rotation, rather than renderer camera clearance.
    def body_clear(position):
        px,py,pz=position;minimum=[px-.3,py-1,pz-.3];maximum=[px+.3,py+1,pz+.3]
        for (x,y,z),(block,yaw) in cells.items():
            if block in ['Air','Water','Rose','TallGrass','DeadShrub','WheatCrop']:continue
            boxes=shapes.get(block,[{'bounds':[0,0,0,8,8,8],'collidable':True}])
            for box in boxes:
                if not box['collidable']:continue
                low=[v/8 for v in box['bounds'][:3]];high=[v/8 for v in box['bounds'][3:]]
                for rotation in range(yaw):low[0],high[0],low[2],high[2]=1-high[2],1-low[2],low[0],high[0]
                origin=[x,y,z]
                if all(maximum[a]>origin[a]+low[a]+.0001 and minimum[a]<origin[a]+high[a]-.0001 for a in range(3)):return False
        return True
    for view in source['views']:
        if not body_clear(view['position']):raise ValueError('Authored player body is blocked at '+view['name'])
    text=output(source,edits,cells)
    generated=args.root/'src/HelloMine3D/Diagnostics/ReferenceVisualSceneData.inc';drift=not generated.exists() or generated.read_text()!=text
    if args.check and drift:raise ValueError('Generated scene drift; export source first')
    if not args.check:generated.write_text(text)
    if args.preview:preview(source,cells,args.preview)
    summary={'status':'PASS','version':2,'mode':'check' if args.check else 'export','regions':len(edits),'world_edit_attempts':writes,'edit_budget':source['edit_budget'],'resident_chunks':16,'ordinary_planted_tree_specs':len(source['trees']),'tree_max_planned_attempts':len(source['trees'])*1024,'total_reserved_edit_attempts':writes+len(source['trees'])*1024,'occupied_cells':sum(counts.values()),'full_weather_shell_cells':len(weather_cells),'flat_roof_bottom_coverage_cells':len(roof_cells),'closed_roof_joints':roof_joints,'kit_parts':12,'player_views_with_clear_body':len(source['views']),'surviving_block_counts':dict(sorted(counts.items())),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'generated_sha256':hashlib.sha256(text.encode()).hexdigest(),'export_elapsed_seconds':round(time.monotonic()-start,6)}
    if args.record:args.record.parent.mkdir(parents=True,exist_ok=True);args.record.write_text(json.dumps(summary,indent=2)+'\n')
    print('[REFERENCE-SCENE-EXPORT] '+json.dumps(summary,sort_keys=True));return 0

if __name__=='__main__':raise SystemExit(main())
