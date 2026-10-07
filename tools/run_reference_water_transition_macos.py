#!/usr/bin/env python3
"""Record four bounded hidden production Water-medium/reflection transitions.

Only an owned verified runtime and saved-world clone are launched. The client
uses production teleports with normal simulation, not input. Native completion
is pending the independent oracle; a requested fault remains a FAILED run.
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

MAGIC = "HelloMine3D owned reference water transition session v1\n"
PREFIXES = ("HELLOMINE3D_", "HELLO_RENDER_", "HELLO_PERF_", "HELLO_VISUAL_")
PROTECTED_RELATIVE = (
    "build/reference-visual-v1/HelloMine3D Reference Visual.app",
    "build/reference-visual-goal/GoalWorkbench.app",
    "build/reference-visual-goal/WorkbenchCurrent.app",
    "build/reference-visual-goal/HelloMine3D Reference Complete.app",
    *(f"build/reference-visual-goal/HelloMine3D Reference Complete v{v}.app" for v in range(3, 9)),
)


def utc():
    return datetime.now(timezone.utc).isoformat()


def checked_inventory(path):
    reject_symlinks(path)
    return file_inventory(path)


def launch_environment(resources, session, fault=None, inherited=None):
    diagnostic = {
        "HELLOMINE3D_ROOT": str(resources),
        "HELLOMINE3D_WINDOW_HIDDEN": "1",
        "HELLOMINE3D_REFERENCE_WATER_PROBE": "1",
        "HELLOMINE3D_REFERENCE_WATER_DIR": str(session / "water"),
        "HELLOMINE3D_SAVE_DIR": str(session / "save"),
        "HELLOMINE3D_CATALOGUE_DIR": str(session / "catalogue"),
        "HELLOMINE3D_MSAA4": "1",
        "HELLOMINE3D_SEED": "42",
        "HELLOMINE3D_WORLD_TIME": "6000",
        "HELLOMINE3D_PLAYER_POSITION": "195.5 68.02 -176.2",
        "HELLOMINE3D_PLAYER_ROTATION": "5 45 0",
        "HELLO_RENDER_CAPTURE": "1",
        "HELLO_RENDER_CAPTURE_DIR": str(session / "water"),
        "HELLO_RENDER_CAPTURE_MS": "60000",
        "HELLO_RENDER_CAPTURE_EXIT": "0",
        "HELLO_RENDER_CAPTURE_MAX_DELTA_MS": "5000",
    }
    if fault:
        require(fault == "retain-water-binding", "Unknown native Water fault")
        diagnostic["HELLOMINE3D_REFERENCE_WATER_FAULT"] = fault
    inherited = os.environ if inherited is None else inherited
    environment = {k: v for k, v in inherited.items() if not k.startswith(PREFIXES)}
    environment.update(diagnostic)
    removed = sorted(k for k in inherited if k.startswith(PREFIXES))
    return environment, diagnostic, removed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--save-template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="Entirely new owned session; never reused")
    parser.add_argument("--fault", choices=("retain-water-binding",))
    parser.add_argument("--busy-host-engineering", action="store_true")
    parser.add_argument("--timeout-seconds", type=int, choices=range(35, 61), default=45)
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
    require(not any(parent.exists() and (parent.samefile(app) or parent.samefile(template))
                    for parent in (output, *output.parents)),
            "Output physically overlaps package/template through an existing ancestor")
    require(not (output.is_relative_to(app) or app.is_relative_to(output)
                 or output.is_relative_to(template) or template.is_relative_to(output)),
            "Output must not overlap package/template")
    distribution, entries = verified_package_entries(app)
    identity = json.loads((app / "Contents/Resources/build-identity.json").read_text())
    binary = app / "Contents/Resources/bin/HelloMine3D"
    require(digest(binary) == identity["executable_sha256"], "Package executable identity changed")
    binary_bytes = binary.read_bytes()
    require(all(token in binary_bytes for token in (
                b"HELLOMINE3D_REFERENCE_WATER_PROBE",
                b"hellomine3d-reference-water-transition-summary-v1",
                b"hellomine3d-reference-water-transition-journal-v1")),
            "Client lacks actual Water transition output capability; do not launch old packages")
    require(identity.get("capabilities", {}).get("hidden_window_no_activate_v1") is True,
            "Non-activating hidden window support required")
    require(identity["source_manifest_sha256"] == digest(app / "Contents/Resources/source-tree-sha256.txt")
            and identity["resource_manifest_sha256"] == digest(app / "Contents/Resources/media/resource-manifest.txt"),
            "Packaged source/resource receipt changed")
    source_certificate = current_source_matches(app)
    with (app / "Contents/Info.plist").open("rb") as stream:
        bundle_id = plistlib.load(stream)["CFBundleIdentifier"]
    require(bundle_id == identity["bundle_id"], "Bundle identifier differs from identity")
    source_before, template_before = checked_inventory(app), checked_inventory(template)
    managed = [path.as_posix() for path, _ in entries]
    require(len(source_before) == 168 and len(managed) == 167 and len(set(managed)) == 167
            and set(source_before) == set(managed) | {distribution.relative_to(app).as_posix()},
            "Expected complete 168-file source package and 167 managed entries required")
    require(len(template_before) == 56, "Expected complete 56-file save template required")
    root = Path(__file__).resolve().parents[1]
    protected_before = {}
    for relative in PROTECTED_RELATIVE:
        published = root / relative
        require(published.is_dir(), "Expected published app missing: " + str(published))
        protected_before[str(published)] = checked_inventory(published)
        require(not (output.is_relative_to(published) or published.is_relative_to(output)),
                "Output overlaps a protected published app")
        require(not any(parent.exists() and parent.samefile(published)
                        for parent in (output, *output.parents)),
                "Output physically overlaps a protected published app")
    output.mkdir(parents=True)
    record_path = output / "run.json"
    record = {
        "schema": "hellomine3d-reference-water-transition-run-v1",
        "evidence_type": "DEVELOPER_DIAGNOSTIC",
        "normal_input": False, "input_actions": 0, "ordinary_acceptance": "NOT_RUN",
        "native_fault_requested": args.fault,
        "host_mode": "BUSY_HOST_ENGINEERING" if args.busy_host_engineering else "IDLE_GAME_PREFLIGHT",
        "performance_isolation_claimed": False, "source_app": str(app), "source_bundle_id": bundle_id,
        "package_identity": identity, "current_source_certificate": source_certificate,
        "source_file_hashes_before": source_before, "save_template": str(template),
        "template_file_hashes_before": template_before, "protected_file_hashes_before": protected_before,
        "session": str(output), "launch_count": 0, "process_deadline_seconds": args.timeout_seconds,
        "game_censuses": [], "result": "PREPARING", "started_utc": utc(),
        "script_sha256": digest(Path(__file__)),
        "natural_exit": False, "forced_cleanup": False, "external_deadline_exceeded": False,
        "limits": [
            "Production-teleport/normal-simulation Water engineering observation; no ordinary input or walking acceptance.",
            "Private saved-world clone; source package/template and ten published app trees protected.",
            "Process census is periodic; foreign processes are never selected or signalled.",
            "Native completion is pending the independent oracle; requested faults stay FAILED.",
            "macOS startup failure is stderr-only; no extra dialog/report environment keys admitted.",
        ],
    }
    write_record(record_path, record)
    child = None
    launch_clock = None
    try:
        record["game_censuses"].append(game_census(output, "before"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "Another game is present; refuse and preserve its window")
        runtime = output / "Runtime.app"
        clone_verified_package(app, runtime)
        record["runtime_clone_initial_hashes"] = checked_inventory(runtime)
        require(record["runtime_clone_initial_hashes"] == source_before, "168-file runtime clone differs")
        shutil.copytree(template, output / "save")
        initial = checked_inventory(output / "save")
        require(initial == template_before, "Initial save clone differs from template")
        record["save_clone_initial_hashes"] = initial
        (output / ".hellomine3d-reference-water-transition-owned").write_text(MAGIC)
        resources = runtime / "Contents/Resources"
        (resources / "bin/config.txt").write_text(SETTINGS)
        environment, diagnostic, removed = launch_environment(resources, output, args.fault)
        command = [str(resources / "bin/HelloMine3D")]
        record.update({"runtime_app": str(runtime), "command": command,
                       "cwd": str(resources / "bin"), "environment": diagnostic,
                       "removed_inherited_diagnostic_environment_names": removed,
                       "settings": SETTINGS, "settings_sha256_before_launch": digest(resources / "bin/config.txt"),
                       "original_distribution_sha256": digest(distribution)})
        record["game_censuses"].append(game_census(output, "before-launch"))
        require(args.busy_host_engineering or not record["game_censuses"][-1]["games"],
                "A game appeared before launch; refuse without interacting")
        with (output / "client.log").open("x") as stdout, (output / "client-stderr.log").open("x") as stderr:
            launch_clock = time.monotonic()
            child = subprocess.Popen(command, cwd=resources / "bin", env=environment,
                                     stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
            record.update({"launch_count": 1, "game_pid": child.pid, "result": "RUNNING",
                           "launch_unix": time.time(), "actual_executable": command[0]})
            write_record(record_path, record)
            deadline = launch_clock + args.timeout_seconds
            record["loaded_executable"] = certify_loaded_executable(child, resources / "bin/HelloMine3D", output)
            require(record["loaded_executable"]["executable_sha256"] == identity["executable_sha256"],
                    "Loaded copied executable differs from verified package")
            next_census, count = time.monotonic(), 0
            while child.poll() is None:
                if time.monotonic() >= deadline:
                    record["external_deadline_exceeded"] = True
                    raise TimeoutError("Owned Water transition exceeded external deadline")
                if time.monotonic() >= next_census:
                    count += 1
                    census = game_census(output, f"during-{count:02d}")
                    record["game_censuses"].append(census)
                    if any(p["pid"] != child.pid for p in census["games"]):
                        record["other_game_observed_during_run"] = True
                    next_census = time.monotonic() + 5
                    write_record(record_path, record)
                time.sleep(0.2)
            if time.monotonic() >= deadline:
                record["external_deadline_exceeded"] = True
                raise TimeoutError("Owned Water transition exited after external deadline")
            record.update({"child_returncode": child.returncode,
                           "natural_exit": child.returncode >= 0,
                           "child_signal": -child.returncode if child.returncode < 0 else None,
                           "peak_waited_children_rss_bytes": int(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)})
        record["game_censuses"].append(game_census(output, "after"))
        require(child.returncode == 0, f"Native Water transition exit {child.returncode}")
        summary = output / "water/summary.json"
        require(summary.is_file(), "Missing actual Water transition summary")
        record["native_summary"] = json.loads(summary.read_text())
        require(record["native_summary"].get("schema") == "hellomine3d-reference-water-transition-summary-v1"
                and record["native_summary"].get("status") == "COMPLETE",
                "Native Water transition phases did not complete")
        require(args.fault is None, "Requested native fault must not be classified as completion")
        record["result"] = "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE"
    except (Exception, KeyboardInterrupt, SystemExit) as error:
        record["result"] = "FAILED"
        record["error"] = f"{type(error).__name__}: {error}"
    finally:
        if child is not None and child.poll() is None:
            record["result"] = "FAILED"
            record["forced_cleanup"] = True
            record["owned_child_cleanup_required"] = True
            try:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired) as error:
                record["owned_cleanup_error"] = f"{type(error).__name__}: {error}"
        if child is not None:
            code = child.poll()
            record["child_returncode"] = code
            record["child_signal"] = -code if code is not None and code < 0 else None
            record["natural_exit"] = code is not None and code >= 0 and not record["forced_cleanup"]
        if launch_clock is not None:
            record["process_wall_seconds"] = time.monotonic() - launch_clock
        for name, original, path in (("source_app", source_before, app),
                                     ("save_template", template_before, template)):
            try:
                actual = checked_inventory(path)
                record[name + "_file_hashes_after"] = actual
                record[name + "_all_files_unchanged"] = actual == original
                if actual != original:
                    record["result"] = "FAILED"
            except Exception as error:
                record["result"] = "FAILED"
                record[name + "_preservation_error"] = f"{type(error).__name__}: {error}"
        try:
            after = {name: checked_inventory(Path(name)) for name in protected_before}
            record["protected_file_hashes_after"] = after
            record["protected_trees_unchanged"] = after == protected_before
            if after != protected_before:
                record["result"] = "FAILED"
        except Exception as error:
            record["result"] = "FAILED"
            record["protected_preservation_error"] = f"{type(error).__name__}: {error}"
        try:
            record["current_source_certificate_after"] = current_source_matches(app)
        except Exception as error:
            record["result"] = "FAILED"
            record["current_source_after_error"] = f"{type(error).__name__}: {error}"
        # Preserve actual partial outputs even for exit1, timeout or missing journal.
        try:
            native = output / "water"
            record["native_file_hashes_after"] = checked_inventory(native) if native.exists() else {}
            summary = native / "summary.json"
            if summary.is_file():
                record["native_summary_sha256"] = digest(summary)
                try:
                    record["native_summary"] = json.loads(summary.read_text())
                except (ValueError, OSError) as error:
                    record["result"] = "FAILED"
                    record["native_summary_read_error"] = f"{type(error).__name__}: {error}"
            save = output / "save"
            if save.exists():
                record["save_clone_final_hashes"] = checked_inventory(save)
                meta = save / "world.meta"
                if meta.is_file():
                    record["save_clone_final_world_meta_sha256"] = digest(meta)
                    record["save_clone_final_world_meta_text"] = meta.read_text()
            runtime = output / "Runtime.app"
            if runtime.exists():
                final = checked_inventory(runtime)
                record["runtime_final_hashes"] = final
                record["runtime_original_168_files_unchanged"] = all(final.get(k) == v for k, v in source_before.items())
                if not record["runtime_original_168_files_unchanged"]:
                    record["result"] = "FAILED"
            for name in ("client.log", "client-stderr.log"):
                path = output / name
                if path.is_file():
                    record[name + "_sha256"] = digest(path)
            reject_symlinks(output)
            record["owned_file_hashes_after_except_run_json"] = {
                k: v for k, v in file_inventory(output).items() if k != "run.json"}
        except Exception as error:
            record["result"] = "FAILED"
            record["owned_artifact_inventory_error"] = f"{type(error).__name__}: {error}"
        record["finished_utc"] = utc()
        record["finished_unix"] = time.time()
        write_record(record_path, record)
    print(json.dumps({k: record.get(k) for k in ("result", "launch_count", "game_pid",
                      "child_returncode", "natural_exit", "forced_cleanup", "error",
                      "source_app_all_files_unchanged", "save_template_all_files_unchanged",
                      "protected_trees_unchanged")}, ensure_ascii=False))
    print(record_path)
    return 0 if record["result"] == "NATIVE_COMPLETED_PENDING_INDEPENDENT_ORACLE" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        print(f"[REFERENCE_WATER_TRANSITION_RUNNER] {type(error).__name__}: {error}")
        raise SystemExit(2)
