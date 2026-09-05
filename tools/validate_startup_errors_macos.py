#!/usr/bin/env python3
"""Run the frozen Windows startup-negative fixtures with macOS report semantics.

Reads only literal fixture tables from validate_startup_errors.ps1 (never
executes PowerShell). Fixture counts and diagnostics must match; a format
change fails closed. Windows MessageBoxW validation remains platform-specific.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--exe", type=Path)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This runner checks macOS stderr-only startup reports")
    root = Path(__file__).resolve().parents[1]
    exe = (args.exe or root / "bin/HelloMine3D").resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    specification = (root / "tools/validate_startup_errors.ps1").read_text()
    missing_table = specification.split("$requirements = @(", 1)[1].split(
        "foreach ($missing", 1)[0]
    missing = re.findall(
        r'Category = "([^"\n]+)"\s+RelativePath = "([^"\n]+)"\s+'
        r'DiagnosticPath = "([^"\n]+)"', missing_table)
    invalid_table = specification.split("$invalidOverrides = @(", 1)[1].split(
        "foreach ($invalid", 1)[0]
    invalid = re.findall(
        r'Name = "([^"\n]+)"\s+LogicalPath = "([^"\n]+)"\s+'
        r'Content = @"\n(.*?)\n"@\s+Expected = "([^"\n]+)"',
        invalid_table, re.S)
    if len(missing) != 10 or len(invalid) != 5:
        raise ValueError("Frozen startup fixture tables changed; inspect before running")
    results = []

    def run_case(name, case, resource_root, expected, pack=None):
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith(("HELLOMINE3D_", "HELLO_PERF_"))}
        environment.update({"HELLOMINE3D_ROOT": str(resource_root),
                            "HELLOMINE3D_WINDOW_HIDDEN": "1",
                            "HELLOMINE3D_STARTUP_ERROR_REPORT": str(case / "report.txt"),
                            "HELLOMINE3D_STARTUP_ERROR_NO_DIALOG": "1"})
        if pack is not None:
            environment["HELLOMINE3D_RESOURCE_PACKS"] = str(pack)
        result = subprocess.run([str(exe)], cwd=exe.parent, env=environment,
                                capture_output=True, text=True, timeout=30)
        (case / "stdout.log").write_text(result.stdout)
        (case / "stderr.log").write_text(result.stderr)
        report = (case / "report.txt").read_text() if (case / "report.txt").exists() else ""
        passed = (result.returncode != 0 and
                  all(token in result.stderr and token in report for token in expected) and
                  "ui=stderr-only" in report and "dialog_suppressed=true" in report)
        results.append({"case": name, "exit": result.returncode,
                        "result": "PASS" if passed else "FAIL"})
        print(f"[MACOS_STARTUP] {results[-1]['result']} case={name} exit={result.returncode}",
              flush=True)

    for category, _, diagnostic in missing:
        case = output / category
        (case / "media").mkdir(parents=True)
        (case / "bin").mkdir()
        manifest = ["runtime-template|bin/resource-packs.txt"]
        for present_category, relative, logical in missing:
            manifest.append(f"{present_category}|{logical}")
            if present_category != category:
                target = case / logical
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(root / relative.replace("\\", "/"), target)
        (case / "media/resource-manifest.txt").write_text(
            "# HelloMine3D resource manifest v1\n\n" +
            "\n".join(sorted(manifest)) + "\n")
        shutil.copy2(root / "bin/resource-packs.txt", case / "bin/resource-packs.txt")
        run_case(category, case, case,
                 [f"Missing or empty effective {category} resource", diagnostic])

    for name, relative, content, expected in invalid:
        case = output / name
        pack = case / "pack"
        target = pack / relative.replace("\\", "/")
        target.parent.mkdir(parents=True)
        (pack / "pack.meta").write_text(
            f"# HelloMine3D resource pack v1\nname={name}\nformat=1\n")
        target.write_text(content.lstrip())
        run_case(name, case, root, [expected], pack)

    summary = {"platform": "macOS", "cases": results,
               "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
               "fixture_spec_sha256": hashlib.sha256(specification.encode()).hexdigest()}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    passed = all(result["result"] == "PASS" for result in results)
    print(f"[MACOS_STARTUP] status={'PASS' if passed else 'FAIL'} cases={len(results)}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
