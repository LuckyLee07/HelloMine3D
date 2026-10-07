#!/usr/bin/env python3
"""Observe actual resident chunk departure and storage return in an owned client.

The client moves its real Player through the production WorldManager route,
with normal simulation. This runner records native execution and preservation;
only the independent oracle can judge residency, retirement and current draw.
Ordinary gameplay and performance remain separate.
"""
import argparse
from datetime import datetime, timezone
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
from run_reference_world_edit_macos import current_source_matches
from run_reference_render_lifecycle_macos import (
    SETTINGS, certify_loaded_executable, digest, file_inventory, game_census,
    reject_symlinks, require, write_record,
)

MAGIC = "HelloMine3D owned reference residency session v1\n"
PREFIXES = ("HELLOMINE3D_", "HELLO_RENDER_", "HELLO_PERF_", "HELLO_VISUAL_")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--save-template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="Entirely new owned session; never reused")
    parser.add_argument("--fault", choices=("skip-target-retirement",))
    parser.add_argument("--busy-host-engineering", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, choices=range(55, 121), default=90)
    args = parser.parse_args()
    require(platform.system() == "Darwin", "This actual client observation requires macOS")
    app, template, output = (p.absolute() for p in (args.app, args.save_template, args.output))
    require(app.suffix.lower() == ".app" and app.is_dir(), "A verified current .app is required")
    require(template.is_dir() and (template / "world.meta").is_file(), "Existing real save required")
    reject_symlinks(app)
    reject_symlinks(template)
    require(not any(p.is_symlink() for p in (output, *output.parents)), "Output symlink forbidden")
    require(not any(p.suffix.lower() == ".app" for p in (output, *output.parents)),
            "Output may not be inside any app, including published packages")
    require(not output.exists(), "Session output must be new")
    app, template, output = app.resolve(strict=True), template.resolve(strict=True), output.resolve()
    # resolve() preserves spelling on case-insensitive macOS volumes. Physical
    # identity must reject aliases before mkdir/copytree can pollute an input.
    require(not any(parent.exists() and (parent.samefile(app) or parent.samefile(template))
                    for parent in (output, *output.parents)),
            "Output physically overlaps package/template through an existing ancestor")
    require(not (output.is_relative_to(app) or app.is_relative_to(output)
                 or output.is_relative_to(template) or template.is_relative_to(output)),
            "Output must not overlap package/template")
    distribution, _ = verified_package_entries(app)
    identity = json.loads((app / "Contents/Resources/build-identity.json").read_text())
    binary = app / "Contents/Resources/bin/HelloMine3D"
    require(digest(binary) == identity["executable_sha256"], "Package executable identity changed")
    binary_bytes = binary.read_bytes()
    require(all(token in binary_bytes for token in (
                b"HELLOMINE3D_REFERENCE_RESIDENCY_PROBE",
                b"hellomine3d-reference-residency-summary-v1",
                b"hellomine3d-reference-residency-journal-v1")),
            "Client lacks actual residency output capability; do not launch old packages")
    require(identity.get("capabilities", {}).get("hidden_window_no_activate_v1") is True,
            "Non-activating hidden window support required")
    require(identity["source_manifest_sha256"] == digest(app / "Contents/Resources/source-tree-sha256.txt")
            and identity["resource_manifest_sha256"] == digest(app / "Contents/Resources/media/resource-manifest.txt"),
            "Packaged source/resource receipt changed")
    source_certificate = current_source_matches(app)
    with (app / "Contents/Info.plist").open("rb") as stream:
        bundle_id = plistlib.load(stream)["CFBundleIdentifier"]
    require(bundle_id == identity["bundle_id"], "Bundle identifier differs from identity")
    source_before, template_before = file_inventory(app), file_inventory(template)
    output.mkdir(parents=True)
    record_path = output / "run.json"
    record = {
        "schema": "hellomine3d-reference-residency-run-v1",
        "evidence_type": "DEVELOPER_DIAGNOSTIC",
        "normal_input": False, "input_actions": 0, "native_fault_requested": args.fault,
        "host_mode": "BUSY_HOST_ENGINEERING" if args.busy_host_engineering else "IDLE_GAME_PREFLIGHT",
        "performance_isolation_claimed": False, "source_app": str(app), "source_bundle_id": bundle_id,
        "package_identity": identity, "current_source_certificate": source_certificate,
        "source_file_hashes_before": source_before, "save_template": str(template),
        "template_file_hashes_before": template_before, "session": str(output),
        "launch_count": 0, "process_deadline_seconds": args.timeout_seconds,
        "game_censuses": [], "result": "PREPARING", "started_utc": datetime.now(timezone.utc).isoformat(),
        "limits": [
            "Player-residency/render-engineering observation; no ordinary input/menu/playability acceptance.",
            "Private saved-world clone; exact original metadata and bytes protected outside that clone.",
            "Real Player teleport and native readbacks do not constitute ordinary walking or performance.",
            "Process census is periodic; other processes are never selected or signalled.",
        ],
    }
    write_record(record_path, record)
    child = None
    try:
        record["game_censuses"].append(game_census(output, "before"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "Another game is present; refuse and preserve its window")
        runtime = output / "Runtime.app"
        clone_verified_package(app, runtime)
        shutil.copytree(template, output / "save")
        initial = file_inventory(output / "save")
        require(initial == template_before, "Initial save clone differs from template")
        record["save_clone_initial_hashes"] = initial
        (output / ".hellomine3d-reference-residency-owned").write_text(MAGIC)
        resources = runtime / "Contents/Resources"
        (resources / "bin/config.txt").write_text(SETTINGS)
        diagnostic = {
            "HELLOMINE3D_ROOT": str(resources), "HELLOMINE3D_WINDOW_HIDDEN": "1",
            "HELLOMINE3D_REFERENCE_RESIDENCY_PROBE": "1",
            "HELLOMINE3D_REFERENCE_RESIDENCY_DIR": str(output / "residency"),
            "HELLOMINE3D_SAVE_DIR": str(output / "save"),
            "HELLOMINE3D_CATALOGUE_DIR": str(output / "catalogue"),
            "HELLOMINE3D_PLAYER_POSITION": "195.5 68.02 -176.2",
            "HELLOMINE3D_PLAYER_ROTATION": "5 45 0",
            "HELLOMINE3D_WORLD_TIME": "6000", "HELLOMINE3D_SEED": "42",
            "HELLOMINE3D_SHOW_DEBUG_INFO": "0", "HELLOMINE3D_MSAA4": "1",
            "HELLO_RENDER_CAPTURE": "1", "HELLO_RENDER_CAPTURE_DIR": str(output / "residency"),
            "HELLO_RENDER_CAPTURE_MS": "60000", "HELLO_RENDER_CAPTURE_EXIT": "0",
            "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000", "HELLO_PERF_CAPTURE": "0",
        }
        if args.fault:
            diagnostic["HELLOMINE3D_REFERENCE_RESIDENCY_FAULT"] = args.fault
        environment = {k: v for k, v in os.environ.items() if not k.startswith(PREFIXES)}
        environment.update(diagnostic)
        command = [str(resources / "bin/HelloMine3D")]
        record.update({"runtime_app": str(runtime), "command": command, "environment": diagnostic,
                       "removed_inherited_diagnostic_environment_names": sorted(k for k in os.environ if k.startswith(PREFIXES)),
                       "settings": SETTINGS, "settings_sha256_before_launch": digest(resources / "bin/config.txt"),
                       "original_distribution_sha256": digest(distribution)})
        record["game_censuses"].append(game_census(output, "before-launch"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "A game appeared before launch; refuse without interacting")
        with (output / "client.log").open("w") as stdout, (output / "client-stderr.log").open("w") as stderr:
            child = subprocess.Popen(command, cwd=resources / "bin", env=environment,
                                     stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
            record.update({"launch_count": 1, "game_pid": child.pid, "result": "RUNNING",
                           "launch_unix": time.time(), "actual_executable": command[0]})
            write_record(record_path, record)
            deadline = time.monotonic() + args.timeout_seconds
            record["loaded_executable"] = certify_loaded_executable(child, resources / "bin/HelloMine3D", output)
            require(record["loaded_executable"]["executable_sha256"] == identity["executable_sha256"],
                    "Loaded copied executable differs from verified package")
            next_census, count = time.monotonic(), 0
            while child.poll() is None:
                if time.monotonic() >= deadline:
                    record["external_deadline_exceeded"] = True
                    raise TimeoutError("Owned Residency diagnostic exceeded external deadline")
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
        require(child.returncode == 0, f"Native Residency diagnostic exit {child.returncode}")
        summary = output / "residency/summary.json"
        require(summary.is_file(), "Missing actual residency summary")
        record["native_summary"] = json.loads(summary.read_text())
        record["native_summary_sha256"] = digest(summary)
        require(record["native_summary"].get("status") == "COMPLETE", "Native Residency phases did not complete")
        record["result"] = "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE"
    except (Exception, KeyboardInterrupt, SystemExit) as error:
        record["result"] = "FAILED"
        record["error"] = f"{type(error).__name__}: {error}"
    finally:
        # Only this verified Popen handle is ever signalled; no PID guessing.
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
            record["child_signal"] = -child.returncode if child.returncode is not None and child.returncode < 0 else None
        record["source_app_all_files_unchanged"] = file_inventory(app) == source_before
        record["save_template_all_files_unchanged"] = file_inventory(template) == template_before
        if not (record["source_app_all_files_unchanged"] and record["save_template_all_files_unchanged"]):
            record["result"] = "FAILED"
            record["preservation_error"] = "Source package or template changed"
        saved_clone = output / "save"
        if saved_clone.is_dir():
            record["save_clone_final_hashes"] = file_inventory(saved_clone)
            metadata = saved_clone / "world.meta"
            if metadata.is_file():
                record["save_clone_final_world_meta_sha256"] = digest(metadata)
                record["save_clone_final_world_meta_text"] = metadata.read_text()
        record["finished_utc"] = datetime.now(timezone.utc).isoformat()
        record["finished_unix"] = time.time()
        write_record(record_path, record)
    print(json.dumps({k: record.get(k) for k in ("result", "launch_count", "game_pid",
                      "child_returncode", "error", "source_app_all_files_unchanged", "save_template_all_files_unchanged")}, ensure_ascii=False))
    print(record_path)
    return 0 if record["result"] == "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        print(f"[REFERENCE_RESIDENCY_RUNNER] {type(error).__name__}: {error}")
        raise SystemExit(2)
