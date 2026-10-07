#!/usr/bin/env python3
"""Deterministically export coherent sRGB colour / linear normal / linear RME arrays.
Normal relief is independently authored in surface-authoring.json, never inferred
from light or shadow in the generated albedo. All mip levels are explicit.
"""
import argparse, hashlib, json, struct
from pathlib import Path
import numpy as np
from PIL import Image
from build_warm_texture_array import build, fnv64, bytes_rgba
from reference_material_source import ART, ROOT

def reduce(values, edge):
    return np.stack([np.asarray(Image.fromarray(values[:,:,i], "F").resize(
        (edge,edge),Image.Resampling.BOX), dtype=np.float32)
        for i in range(values.shape[2])],axis=2)

def normal_from_height(spec, edge=64):
    y,x=np.mgrid[0:edge,0:edge].astype(np.float32)
    u,v=(x+.5)/edge,(y+.5)/edge
    c,r=spec["columns"],spec["courses"]
    h=np.zeros((edge,edge),dtype=np.float32)
    if c:
        # Periodic authoring coordinates and central wrapped differences.
        if r:
            phase = u*c + (np.floor(v*r) % 2)*.5
            vertical=np.abs(np.sin(np.pi*phase))
            horizontal=np.abs(np.sin(np.pi*v*r))
            h=np.minimum(np.clip(vertical/.12,0,1),np.clip(horizontal/.10,0,1))
        else:
            h=np.clip(np.abs(np.sin(np.pi*u*c))/.10,0,1)
            h += .04*np.sin(2*np.pi*(u*19 + .08*np.sin(v*2*np.pi)))
    h *= spec["relief"]
    du=(np.roll(h,-1,axis=1)-np.roll(h,1,axis=1))*edge*.5
    dv=(np.roll(h,-1,axis=0)-np.roll(h,1,axis=0))*edge*.5
    n=np.stack((-du,-dv,np.ones_like(h)),axis=2)
    return n/np.linalg.norm(n,axis=2,keepdims=True)

def pack(images):
    payload=b"".join(bytes_rgba(v) for v in images)
    return struct.pack("<8sIIIIIQ",b"HMTARRAY",1,64,256,7,len(payload),fnv64(payload))+payload

def build_surface():
    spec=json.loads((ART/"surface-authoring.json").read_text())["materials"]
    normal=np.zeros((256,64,64,3),dtype=np.float32); normal[:,:,:,2]=1
    surface=np.zeros((256,64,64,4),dtype=np.float32)
    surface[:,:,:,0]=.96;surface[:,:,:,3]=1
    for value in spec.values():
        layer=value["layer"]
        normal[layer]=normal_from_height(value)
        surface[layer,:,:,:3]=value["roughness"],value["metalness"],value["emission"]
    # Existing stone roads and plank faces receive the same material family,
    # retaining all original identity, colour pixels, AO and block light.
    for destination,source in ((3,145),(23,145),(21,147),(4,147),(5,147),(137,145)):
        normal[destination]=normal[source]
        surface[destination]=surface[source]
    normal_mips=[];surface_mips=[];stats=[]
    for mip in range(7):
        edge=64>>mip
        min_len=1.;min_rough=1.
        for layer in range(256):
            average=reduce(normal[layer],edge) if mip else normal[layer]
            length=np.linalg.norm(average,axis=2,keepdims=True)
            n=average/np.maximum(length,1e-8)
            normal_mips.append(np.concatenate((n*.5+.5,np.ones((edge,edge,1))),axis=2))
            data=reduce(surface[layer],edge) if mip else surface[layer].copy()
            # Sub-pixel normal variance broadens roughness to prevent specular sparkle.
            data[:,:,0]=np.sqrt(np.clip(data[:,:,0]**2+(1-length[:,:,0]),.04,1))
            surface_mips.append(data)
            min_len=min(min_len,float(np.min(np.linalg.norm(n,axis=2))))
            min_rough=min(min_rough,float(data[:,:,0].min()))
        stats.append(dict(mip=mip,edge=edge,normal_min_length=min_len,min_roughness=min_rough))
    return pack(normal_mips),pack(surface_mips),stats

def main(check=False):
    colour,colour_report=build(64)
    authoring=json.loads((ART/"surface-authoring.json").read_text())
    colour_aliases=authoring.get("colour_aliases",{})
    if colour_aliases:
        mapped=bytearray(colour)
        offset=36
        for mip in range(7):
            layer_bytes=(64>>mip)**2*4
            for destination,source in colour_aliases.items():
                destination=int(destination)
                if not 0<=destination<256 or type(source) is not int or not 0<=source<256:
                    raise ValueError("Invalid authored colour layer alias")
                mapped[offset+destination*layer_bytes:offset+(destination+1)*layer_bytes]=\
                    colour[offset+source*layer_bytes:offset+(source+1)*layer_bytes]
            offset+=256*layer_bytes
        struct.pack_into("<Q",mapped,28,fnv64(mapped[36:]))
        colour=bytes(mapped)
    normal,surface,stats=build_surface()
    files={"colour":colour,"normal":normal,"surface":surface}
    records={}
    for name,data in files.items():
        target=ROOT/("media/textures/Reference"+name.title()+"64.hmt")
        if check:
            assert target.read_bytes()==data, "non-deterministic export: "+str(target)
        else: target.write_bytes(data)
        records[name]=dict(path=target.relative_to(ROOT).as_posix(),
                           fnv64="%016x"%struct.unpack_from("<Q",data,28)[0],
                           sha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
    lines=["# HelloMine3D reference surface profile v1",
           "edge=64","mips=7"]
    for name,record in records.items():
        lines += [name+"_texture="+record["path"],name+"_hash="+record["fnv64"]]
    profile="\n".join(lines)+"\n"
    target=ROOT/"media/materials/Reference.surface-material"
    if check: assert target.read_text()==profile
    else: target.write_text(profile)
    report=dict(version=1,colour="sRGB RGBA8; hardware decode, linear-light mip",
                normal="linear XYZ encoded [0,1], mip normalized",
                surface="linear roughness/metalness/emission/1; variance-adjusted roughness mip",
                records=records,mips=stats,semantics=colour_report["semantics"],colour_aliases=colour_aliases,
                source_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ART.glob("*source.png")},
                authoring_sha256=hashlib.sha256((ART/"surface-authoring.json").read_bytes()).hexdigest(),
                channels_total_bytes=sum(v["bytes"] for v in records.values()))
    if not check: (ART/"surface-build.json").write_text(json.dumps(report,indent=2)+"\n")
    print("[REFERENCE_SURFACE] PASS deterministic=%d bytes=%d channels=3 edge=64 layers=256 mips=7"%(
          check,report["channels_total_bytes"]))
if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--check",action="store_true")
    main(parser.parse_args().check)
