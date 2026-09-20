#!/usr/bin/env python3
"""Freeze regional ecology sites from the complete eight-seed planning survey."""
import argparse,csv,hashlib,json
from collections import defaultdict
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('samples',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
groups=defaultdict(dict)
for raw in csv.DictReader(a.samples.open()):
 r={k:float(v) if k in ('grove','snowline') else int(v) for k,v in raw.items()}
 groups[(r['seed'],r['region'])][(r['x'],r['z'])]=r
sites=[]
for (seed,region),points in sorted(groups.items()):
 remaining=set(points);components=[]
 while remaining:
  start=min(remaining);remaining.remove(start);stack=[start];component=[]
  while stack:
   xy=stack.pop();component.append(xy)
   for neighbor in [(xy[0]+28,xy[1]),(xy[0]-28,xy[1]),(xy[0],xy[1]+28),(xy[0],xy[1]-28)]:
    if neighbor in remaining:remaining.remove(neighbor);stack.append(neighbor)
  components.append(component)
 component=max(components,key=lambda c:(len(c),tuple(-n for n in min(c))))
 cx=sum(x for x,z in component)/len(component);cz=sum(z for x,z in component)/len(component)
 candidates=[xy for xy in component if points[xy]['height']>=64] or component
 xy=min(candidates,key=lambda xy:((xy[0]-cx)**2+(xy[1]-cz)**2,xy))
 row=points[xy].copy();row['component_samples']=len(component);sites.append(row)
assert len(sites)==72, len(sites)
a.output.write_text(json.dumps({'schema':1,'terrain_version':18,'samples_sha256':hashlib.sha256(a.samples.read_bytes()).hexdigest(),'selection':'Largest connected regional component; nearest dry sample to centroid; complete eight-seed survey; fixed before visual review','sites':sites},ensure_ascii=False,indent=2)+'\n')
