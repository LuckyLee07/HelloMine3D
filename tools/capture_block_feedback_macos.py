#!/usr/bin/env python3
"""Capture block feedback in a verified disposable macOS package.

The fixture sends timed holds/releases through the real Sandbox mining path.
It is developer diagnostic evidence, not normal-input gameplay acceptance.
Performance mode uses an unchanged natural viewpoint without readback/fixtures.
"""
import argparse
import json
from pathlib import Path
import platform
import subprocess
import time

from capture_visual_macos import clone_verified_package, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scene", choices=("stone", "flower", "grass", "crop", "door"), default="stone")
    parser.add_argument("--intensity", choices=("off", "reduced", "full"), default="full")
    parser.add_argument("--shadow", choices=("off", "high"), default="off")
    parser.add_argument("--detail", choices=("standard", "compatibility"), default="standard")
    parser.add_argument("--performance", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("macOS required")
    output = args.output.absolute()
    if output.exists():
        parser.error("Output must be new; failures are retained")
    output.mkdir(parents=True)
    app = args.app.resolve(strict=True)
    clone = output / "Runtime.app"
    clone_verified_package(app, clone)
    root = clone / "Contents/Resources"
    identity = json.loads((root / "build-identity.json").read_text())
    assert digest(root / "bin/HelloMine3D") == identity["executable_sha256"]
    settings = f"""settings_version 9
visualdetail {args.detail}
renderdistance 8
directionalshadowquality {args.shadow}
postprocessingquality off
fullscreen 0
windowsize 1280 720
fov 70
uiscale 0.85
locale zh-CN
audiocaptions 0
actionhints 0
sprintmode hold
sneakmode hold
feedbackintensity {args.intensity}
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
"""
    (root / "bin/config.txt").write_text(settings)
    environment = {
        "HELLOMINE3D_ROOT": str(root),
        "HELLOMINE3D_SAVE_DIR": str(output / "save"),
        "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
        "HELLOMINE3D_SEED": "20260807",
        "HELLOMINE3D_PLAYER_POSITION": "1056.5 106 928.5" if args.performance else "1056 160 928",
        "HELLOMINE3D_PLAYER_ROTATION": "40 0 0" if args.performance else "0 0 0",
        "HELLOMINE3D_WORLD_TIME": "6000",
        "HELLOMINE3D_SHOW_DEBUG_INFO": "0",
    }
    frames = list(range(1000, 7000, 100))
    if args.performance:
        environment.update({
            "HELLO_RENDER_CAPTURE": "0", "HELLO_PERF_CAPTURE": "1",
            "HELLO_PERF_CAPTURE_DIR": str(output / "performance"),
            "HELLO_PERF_CAPTURE_WARMUP_MS": "5000",
            "HELLO_PERF_CAPTURE_DURATION_MS": "30000",
            "HELLO_PERF_CAPTURE_EXIT": "1",
        })
    else:
        environment.update({
            "HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE": args.scene,
            "HELLO_PERF_CAPTURE": "0", "HELLO_RENDER_CAPTURE": "1",
            "HELLO_RENDER_CAPTURE_DIR": str(output / "frames"),
            "HELLO_RENDER_CAPTURE_FORMAT": "bmp",
            "HELLO_RENDER_CAPTURE_MS": ",".join(map(str, frames)),
            "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000",
            "HELLO_RENDER_CAPTURE_EXIT": "1",
        })
    command = ["/usr/bin/open", "-n", "-W", "--stdout", str(output / "client.log"),
               "--stderr", str(output / "client-stderr.log")]
    for key, value in environment.items():
        command.extend(["--env", f"{key}={value}"])
    command.append(str(clone))
    record = {"evidence_type": "DEVELOPER_DIAGNOSTIC", "normal_input": False,
              "source_app": str(app), "package_identity": identity,
              "settings": settings, "environment": environment,
              "platform": platform.platform(), "command": command,
              "started_unix": time.time(), "result": "RUNNING"}
    record_path = output / "capture.json"
    record_path.write_text(json.dumps(record, indent=2) + "\n")
    try:
        subprocess.run(command, check=True, timeout=100)
        if args.performance:
            artifacts = [output / "performance/frames.csv", output / "performance/summary.txt"]
        else:
            artifacts = sorted((output / "frames").glob("*.bmp"))
            if len(artifacts) != len(frames):
                raise RuntimeError(f"Expected {len(frames)} frames; got {len(artifacts)}")
        for path in artifacts:
            if not path.is_file() or not path.stat().st_size:
                raise RuntimeError(f"Missing artifact: {path}")
        if (output / "client-stderr.log").stat().st_size:
            raise RuntimeError("Client reported stderr; inspect before accepting capture")
        record["artifacts"] = {str(p.relative_to(output)): digest(p) for p in artifacts}
        record["world_metadata"] = (output / "save/world.meta").read_text()
        record["result"] = "CAPTURED"
    except Exception as error:
        record["result"] = "FAIL"
        record["error"] = str(error)
        raise
    finally:
        record["finished_unix"] = time.time()
        record_path.write_text(json.dumps(record, indent=2) + "\n")
    print(record_path)


if __name__ == "__main__":
    main()
