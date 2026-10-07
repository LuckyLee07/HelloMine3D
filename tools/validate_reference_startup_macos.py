#!/usr/bin/env python3
"""Current native asset rejection / HDR fallback probes in independent packages.
Never activates windows, edits a user package, uses PNG readback, or claims
ordinary input. Each launch has an isolated catalogue and bounded frame exit.
"""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def remove_program(text,name):
    start=text.index("fragment_program "+name+" glsl")
    brace=text.index("{",start); depth=0
    for i in range(brace,len(text)):
        if text[i]=="{": depth+=1
        elif text[i]=="}":
            depth-=1
            if not depth: return text[:start]+text[i+1:]
    raise ValueError("Unterminated program")
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    original=args.app.resolve(); output=args.output.resolve()
    output.mkdir(parents=True,exist_ok=False)
    resources=original/"Contents/Resources"
    binary=resources/"bin/HelloMine3D"
    identity=json.loads((resources/"build-identity.json").read_text())
    assert digest(binary)==identity["executable_sha256"]
    baseline_program=(resources/"media/ogre/HelloMine3D.program").read_text()
    cases=[
        ("valid-forced-capability-fallback",None,True,"linear-hdr",0,["active=legacy fallback=1 reason=forced"]),
        ("bad-pbr-code-before-forced-fallback","bad-code",True,"linear-hdr",1,["TerrainSurfaceFragment"]),
        ("missing-one-surface-variant-before-forced-fallback","one-variant",True,"linear-hdr",1,["must declare both receiver variants"]),
        ("missing-AA-auto-binding-before-forced-fallback","aa-auto",True,"linear-hdr",1,["must bind the actual scene texture pixel size"]),
        ("bad-normal-identity-in-legacy-compatibility","normal-hash",False,"legacy",1,["profile/content identity mismatch: normal"]),
        ("older-HDR-resolve-without-AA-uniform","older-resolve",True,"linear-hdr",0,["available=0 enabled=0","active=legacy fallback=1 reason=forced"]),
    ]
    results=[]
    for name,mutation,forced,pipeline,wants_failure,expected in cases:
        case=output/name; app=case/"Probe.app"; case.mkdir()
        shutil.copytree(original,app)
        root=app/"Contents/Resources"; changes=[]
        if mutation=="bad-code":
            path=root/"media/ogre/HelloMine3DTerrain.frag"
            path.write_text(path.read_text()+"\n#ifdef TERRAIN_SURFACE\ninvalid_reference_surface_syntax\n#endif\n");changes=[path]
        elif mutation=="one-variant":
            path=root/"media/ogre/HelloMine3D.program"
            path.write_text(remove_program(baseline_program,"HelloMine3D/TerrainShadowSurfaceFragment"));changes=[path]
        elif mutation=="aa-auto":
            path=root/"media/ogre/HelloMine3DHdr.program"
            source=path.read_text(); marker="        param_named_auto inverseTextureSize inverse_texture_size 0\n"
            assert marker in source;path.write_text(source.replace(marker,"",1));changes=[path]
        elif mutation=="normal-hash":
            path=root/"media/materials/Reference.surface-material"
            lines=path.read_text().splitlines()
            path.write_text("\n".join("normal_hash=0000000000000000" if q.startswith("normal_hash=") else q for q in lines)+"\n");changes=[path]
        elif mutation=="older-resolve":
            # Exact committed first-prototype resources, not a synthetic bypass.
            for rel in ["media/ogre/HelloMine3DHdr.program","media/ogre/HelloMine3DHdrResolve.frag"]:
                path=root/rel
                path.write_bytes(subprocess.check_output(["git","show","552345675265f77f8a30dbf1ae205a52d1abb64f:"+rel]))
                changes.append(path)
        config=root/"bin/config.txt"
        # Retain the packaged current settings schema, including required
        # input/HUD fields. Only scoped renderer/window values are changed.
        overrides={"renderdistance":"1", "directionalshadowquality":"off",
                   "postprocessingquality":"off", "fullscreen":"0",
                   "windowsize":"640 480", "fov":"90",
                   "visualdetail":"compatibility" if mutation=="normal-hash" else "standard",
                   "renderpipeline":pipeline}
        lines=config.read_text().splitlines()
        replaced=set()
        for i,line in enumerate(lines):
            fields=line.split(None,1)
            if fields and fields[0] in overrides:
                lines[i]=fields[0]+" "+overrides[fields[0]];replaced.add(fields[0])
        lines.extend(k+" "+v for k,v in overrides.items() if k not in replaced)
        config.write_text("\n".join(lines)+"\n")
        env={k:v for k,v in os.environ.items() if not k.startswith(("HELLOMINE3D_","HELLO_RENDER_","HELLO_PERF_"))}
        env.update({"HELLOMINE3D_WINDOW_HIDDEN":"1","HELLOMINE3D_CATALOGUE_DIR":str(case/"catalogue"),
                    "HELLOMINE3D_EXIT_AFTER_FRAMES":"8","HELLOMINE3D_STARTUP_ERROR_NO_DIALOG":"1",
                    "HELLOMINE3D_STARTUP_ERROR_REPORT":str(case/"startup-report.txt")})
        if forced: env["HELLOMINE3D_HDR_FALLBACK"]="1"
        start=time.time()
        result=subprocess.run([str(app/"Contents/MacOS/HelloMine3D")],cwd=case,env=env,capture_output=True,text=True,timeout=35)
        (case/"stdout.log").write_text(result.stdout);(case/"stderr.log").write_text(result.stderr)
        actual=result.stdout+"\n"+result.stderr
        rejected_early=(not wants_failure or "active=legacy fallback=1" not in actual)
        ok=(bool(result.returncode)==bool(wants_failure) and all(q in actual for q in expected) and rejected_early)
        entry={"case":name,"status":"PASS" if ok else "FAIL","exit":result.returncode,
               "started_unix":start,"finished_unix":time.time(),"normal_input":False,"hidden":True,
               "pipeline":pipeline,"forced_fallback":forced,"expected":expected,"rejected_before_fallback":rejected_early,
               "executable_sha256":digest(root/"bin/HelloMine3D"),
               "mutated_resources":{str(p.relative_to(root)):digest(p) for p in changes}}
        (case/"fixture.json").write_text(json.dumps(entry,indent=2)+"\n");results.append(entry)
        print("[REFERENCE_NATIVE_STARTUP]",entry["status"],name,"exit="+str(result.returncode),flush=True)
    summary={"status":"PASS" if all(c["status"]=="PASS" for c in results) else "FAIL",
             "source_app":str(original),"executable_sha256":digest(binary),"cases":results,
             "ordinary_input":"NOT_RUN","native_automatic_launches":len(results)}
    (output/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    return summary["status"]!="PASS"
if __name__=="__main__": raise SystemExit(main())
