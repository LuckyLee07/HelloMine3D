#!/usr/bin/env python3
"""Run a bounded hidden production render-resource lifecycle diagnostic.

This uses a verified package copy and two separate copied save directories.
It never sends input and never proves normal menu/gameplay acceptance.
The in-client probe owns a 45-second/4096-frame bound; this runner supplies
an independent process deadline, including startup and synchronous saves.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import resource
import shutil
import subprocess
import time

from capture_visual_macos import clone_verified_package, verified_package_entries

MAGIC = "HelloMine3D owned render lifecycle session v1\n"
PREFIXES = ("HELLOMINE3D_", "HELLO_RENDER_", "HELLO_PERF_")
SETTINGS = """settings_version 12
renderdistance 3
directionalshadowquality medium
postprocessingquality off
fullscreen 0
windowsize 1280 720
fov 90
uiscale 1.0
locale zh-CN
audiocaptions 1
actionhints 1
sprintmode hold
sneakmode hold
feedbackintensity full
mouse_break_attack primary
mouse_use secondary
mouse_place secondary
mouse_guard secondary
seed random
visualdetail standard
minimaprange 128
cameraperspective first
renderpipeline linear-hdr
"""


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_inventory(root):
    return {p.relative_to(root).as_posix(): digest(p)
            for p in sorted(root.rglob("*")) if p.is_file()}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def reject_symlinks(path):
    require(not any(p.is_symlink() for p in (path, *path.parents)),
            f"Symlink path is not allowed: {path}")
    require(not any(p.is_symlink() for p in path.rglob("*")),
            f"Symlink input tree is not allowed: {path}")


def write_record(path, record):
    path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n")


def game_census(output, label):
    result = subprocess.run(["/bin/ps", "-ax", "-o", "pid=,comm="],
                            capture_output=True, text=True, check=False, timeout=2)
    text_path = output / f"process-{label}.txt"
    text_path.write_text(result.stdout)
    (output / f"process-{label}.stderr.txt").write_text(result.stderr)
    require(result.returncode == 0, f"Process inventory failed: {label}")
    games = []
    for line in result.stdout.splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) == 2 and Path(fields[1]).name == "HelloMine3D":
            games.append({"pid": int(fields[0]), "comm": fields[1]})
    return {"label": label, "utc": datetime.now(timezone.utc).isoformat(),
            "games": games, "full_list": str(text_path),
            "full_list_sha256": digest(text_path)}


def certify_loaded_executable(child, expected, output):
    """Certify the owned process loaded this copied binary, without input."""
    limit = time.monotonic() + 3
    attempts = []
    while child.poll() is None and time.monotonic() < limit:
        try:
            result = subprocess.run(
                ["/usr/sbin/lsof", "-a", "-p", str(child.pid), "-d", "txt", "-Fn"],
                capture_output=True, text=True, check=False, timeout=2)
            attempts.append({"returncode": result.returncode, "stdout": result.stdout,
                             "stderr": result.stderr})
            paths = [line[1:] for line in result.stdout.splitlines() if line.startswith("n")]
            if result.returncode == 0 and any(Path(path).resolve() == expected.resolve()
                                              for path in paths):
                receipt = output / "loaded-executable.json"
                write_record(receipt, {"pid": child.pid, "expected": str(expected),
                                      "executable_sha256": digest(expected), "attempts": attempts})
                return {"receipt": str(receipt), "receipt_sha256": digest(receipt),
                        "executable_sha256": digest(expected), "verified": True}
        except subprocess.TimeoutExpired:
            attempts.append({"timeout_seconds": 2})
        time.sleep(0.1)
    write_record(output / "loaded-executable.json",
                 {"pid": child.pid, "expected": str(expected), "attempts": attempts,
                  "verified": False})
    raise ValueError("Could not certify the owned child loaded the copied current executable")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--save-template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="Entirely new owned session directory; never reused")
    parser.add_argument("--busy-host-engineering", action="store_true",
                        help="Explicit process-local resource case with other games recorded; never isolated performance")
    parser.add_argument("--fault", choices=("delayed-hdr-drain",),
                        help="Explicit hidden native negative control; expected to fail")
    parser.add_argument("--timeout-seconds", type=int, choices=range(55, 121),
                        default=90, help="External deadline including startup/save (55..120)")
    args = parser.parse_args()
    require(platform.system() == "Darwin", "This native diagnostic requires macOS")
    app = args.app.absolute()
    template = args.save_template.absolute()
    output = args.output.absolute()
    require(app.suffix == ".app" and app.is_dir(), "A current verified .app is required")
    require(template.is_dir() and (template / "world.meta").is_file(),
            "A real saved world template is required")
    reject_symlinks(app)
    reject_symlinks(template)
    require(not any(p.is_symlink() for p in (output, *output.parents)),
            "Session path may not contain symlinks")
    require(not output.exists(), "Session output must not exist")
    app = app.resolve(strict=True)
    template = template.resolve(strict=True)
    output = output.resolve()
    require(not (output.is_relative_to(app) or app.is_relative_to(output)
                 or output.is_relative_to(template) or template.is_relative_to(output)),
            "Output must not overlap package or save template")
    inventory, entries = verified_package_entries(app)
    identity = json.loads((app / "Contents/Resources/build-identity.json").read_text())
    binary = app / "Contents/Resources/bin/HelloMine3D"
    require(digest(binary) == identity["executable_sha256"], "Package executable identity changed")
    require(b"HELLOMINE3D_RENDER_LIFECYCLE_PROBE" in binary.read_bytes(),
            "Client has no render lifecycle probe; do not launch older packages")
    require(identity.get("capabilities", {}).get("hidden_window_no_activate_v1") is True,
            "Client must support hidden non-activating diagnostics")
    require(identity["source_manifest_sha256"] == digest(app / "Contents/Resources/source-tree-sha256.txt")
            and identity["resource_manifest_sha256"] == digest(app / "Contents/Resources/media/resource-manifest.txt"),
            "Source/resource receipt identity changed")
    with (app / "Contents/Info.plist").open("rb") as stream:
        bundle_id = plistlib.load(stream)["CFBundleIdentifier"]
    require(bundle_id == identity["bundle_id"], "Bundle identifier differs from build identity")
    source_before = file_inventory(app)
    template_before = file_inventory(template)
    output.mkdir(parents=True)
    record_path = output / "run.json"
    record = {"schema": 1, "evidence_type": "DEVELOPER_DIAGNOSTIC",
              "normal_input": False, "native_fault_requested": args.fault,
              "host_mode": "BUSY_HOST_ENGINEERING" if args.busy_host_engineering else "IDLE_GAME_PREFLIGHT",
              "performance_isolation_claimed": False, "source_app": str(app), "source_bundle_id": bundle_id,
              "package_identity": identity, "source_file_hashes_before": source_before,
              "save_template": str(template), "template_file_hashes_before": template_before,
              "session": str(output), "launch_count": 0, "input_actions": 0,
              "process_deadline_seconds": args.timeout_seconds, "game_censuses": [],
              "result": "PREPARING", "started_utc": datetime.now(timezone.utc).isoformat(),
              "limits": ["No normal input/menu acceptance or settings-restart claim.",
                         "Two save clones preserve template bytes and original world id; distinct paths/runtime instances are recorded.",
                         "Five-second process census is not a continuous process-spawn trace.",
                         "Finite resource observations do not prove a long session is leak-free."]}
    write_record(record_path, record)
    child = None
    try:
        record["game_censuses"].append(game_census(output, "before"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "Another game is present; refuse before launching and preserve its window")
        runtime = output / "Runtime.app"
        clone_verified_package(app, runtime)
        record["save_clone_initial_hashes"] = {}
        for name in ("save-a", "save-b"):
            shutil.copytree(template, output / name)
            initial = file_inventory(output / name)
            require(initial == template_before, f"Initial save clone differs from template: {name}")
            record["save_clone_initial_hashes"][name] = initial
        (output / ".hellomine3d-render-lifecycle-owned").write_text(MAGIC)
        resources = runtime / "Contents/Resources"
        (resources / "bin/config.txt").write_text(SETTINGS)
        environment = {name: value for name, value in os.environ.items()
                       if not name.startswith(PREFIXES)}
        removed = sorted(name for name in os.environ if name.startswith(PREFIXES))
        diagnostic = {
            "HELLOMINE3D_ROOT": str(resources),
            "HELLOMINE3D_WINDOW_HIDDEN": "1",
            "HELLOMINE3D_RENDER_LIFECYCLE_PROBE": "1",
            "HELLOMINE3D_RENDER_LIFECYCLE_DIR": str(output / "lifecycle"),
            "HELLOMINE3D_SAVE_DIR": str(output / "save-a"),
            "HELLOMINE3D_LIFECYCLE_SAVE_B": str(output / "save-b"),
            "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
            "HELLOMINE3D_PLAYER_POSITION": "191.5 68.02 -176.2",
            "HELLOMINE3D_PLAYER_ROTATION": "5 45 0",
            "HELLOMINE3D_WORLD_TIME": "6000",
            "HELLOMINE3D_SEED": "42",
            "HELLOMINE3D_SHOW_DEBUG_INFO": "0",
            "HELLOMINE3D_MSAA4": "1",
            "HELLO_RENDER_CAPTURE": "0",
        }
        if args.fault:
            diagnostic["HELLOMINE3D_LIFECYCLE_FAULT"] = args.fault
        environment.update(diagnostic)
        command = [str(resources / "bin/HelloMine3D")]
        record.update({"runtime_app": str(runtime), "command": command,
                       "environment": diagnostic,
                       "removed_inherited_diagnostic_environment_names": removed,
                       "settings": SETTINGS, "settings_path": str(resources / "bin/config.txt"),
                       "settings_sha256_before_launch": digest(resources / "bin/config.txt"),
                       "original_distribution_sha256": digest(inventory)})
        record["game_censuses"].append(game_census(output, "before-launch"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "A game appeared before launch; preserve it and refuse")
        with (output / "client.log").open("w") as stdout, \
                (output / "client-stderr.log").open("w") as stderr:
            child = subprocess.Popen(command, cwd=resources / "bin", env=environment,
                                     stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
            record["other_game_present_before_launch"] = bool(record["game_censuses"][-1]["games"])
            record.update({"launch_count": 1, "game_pid": child.pid, "result": "RUNNING",
                           "launch_unix": time.time(), "actual_executable": command[0]})
            write_record(record_path, record)
            deadline = time.monotonic() + args.timeout_seconds
            record["loaded_executable"] = certify_loaded_executable(
                child, resources / "bin/HelloMine3D", output)
            require(record["loaded_executable"]["executable_sha256"] == identity["executable_sha256"],
                    "Loaded copied executable differs from the verified package")
            write_record(record_path, record)
            next_census = time.monotonic()
            count = 0
            while child.poll() is None:
                if time.monotonic() >= deadline:
                    record["external_deadline_exceeded"] = True
                    child.terminate()
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait(timeout=5)
                    raise TimeoutError("Owned diagnostic exceeded external deadline")
                if time.monotonic() >= next_census:
                    count += 1
                    census = game_census(output, f"during-{count:02d}")
                    record["game_censuses"].append(census)
                    if any(p["pid"] != child.pid for p in census["games"]):
                        record["other_game_observed_during_run"] = True
                    next_census = time.monotonic() + 5
                    write_record(record_path, record)
                time.sleep(0.2)
            record.update({"child_returncode": child.returncode,
                           "child_signal": -child.returncode if child.returncode < 0 else None,
                           "peak_waited_children_rss_bytes": int(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)})
        record["game_censuses"].append(game_census(output, "after"))
        require(child.returncode == 0, f"Native diagnostic exit {child.returncode}")
        summary = output / "lifecycle/summary.json"
        require(summary.is_file(), "Missing lifecycle summary after process exit")
        record["native_summary"] = json.loads(summary.read_text())
        record["native_summary_sha256"] = digest(summary)
        require(record["native_summary"].get("status") == "COMPLETE",
                "Native ownership/stage summary did not complete")
        record["result"] = "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE"
    except (Exception, KeyboardInterrupt, SystemExit) as error:
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
        record["result"] = "FAILED"
        record["error"] = f"{type(error).__name__}: {error}"
        if child is not None:
            record["child_returncode"] = child.returncode
            record["child_signal"] = -child.returncode if child.returncode is not None and child.returncode < 0 else None
    finally:
        # Even a runner interruption/error must not leave the process we own alive.
        # This Popen handle is the only process we ever signal.
        if child is not None and child.poll() is None:
            record["result"] = "FAILED"
            record["owned_child_cleanup_required"] = True
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
        if child is not None:
            record["child_returncode"] = child.returncode
            record["child_signal"] = (-child.returncode if child.returncode is not None
                                      and child.returncode < 0 else None)
        record["source_app_all_files_unchanged"] = file_inventory(app) == source_before
        record["save_template_all_files_unchanged"] = file_inventory(template) == template_before
        if not (record["source_app_all_files_unchanged"] and record["save_template_all_files_unchanged"]):
            record["result"] = "FAILED"
            record["preservation_error"] = "Source package or save template changed"
        record["finished_utc"] = datetime.now(timezone.utc).isoformat()
        record["finished_unix"] = time.time()
        write_record(record_path, record)
    print(json.dumps({k: record.get(k) for k in
                      ("result", "launch_count", "game_pid", "child_returncode", "error",
                       "source_app_all_files_unchanged", "save_template_all_files_unchanged")}, ensure_ascii=False))
    print(record_path)
    return 0 if record["result"] == "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        print(f"[RENDER_LIFECYCLE_RUNNER] {type(error).__name__}: {error}")
        raise SystemExit(2)
