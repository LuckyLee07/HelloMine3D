#!/usr/bin/env python3
"""Package an already verified macOS Release client without source-tree assets.

The caller must build Release first. This tool records identity, checks linked
libraries and copies the frozen resource manifest; it does not claim gameplay
acceptance or infer build configuration from an executable's filename.
"""

import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="New .app path, or an existing workbench with --refresh-existing")
    parser.add_argument("--refresh-existing", action="store_true",
                        help="Refresh a verified, unaccepted workbench app in place")
    parser.add_argument("--configuration", choices=("Debug", "Release"),
                        default="Release", help="Configuration actually built by caller")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.absolute()
    binary = root / "bin/HelloMine3D"
    if sys.platform != "darwin" or output.suffix != ".app":
        parser.error("Run on macOS and choose a .app output path")
    if args.refresh_existing != output.exists():
        parser.error("Use --refresh-existing only with an existing workbench .app")
    old_paths = set()
    if args.refresh_existing:
        inventory = output / "Contents/Resources/distribution-sha256.txt"
        metadata = json.loads((output / "Contents/Resources/build-identity.json").read_text())
        if metadata.get("acceptance") != "NOT_RUN" or metadata.get("signed_or_notarized"):
            parser.error("Only an unaccepted, unsigned workbench package may be refreshed")
        with (output / "Contents/Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        if info.get("CFBundleIdentifier") != "local.hellomine3d.macos-goal":
            parser.error("Existing package has a different bundle identity")
        for entry in inventory.read_text().splitlines():
            expected, relative = entry.split("  ", 1)
            path = Path(relative)
            if path.is_absolute() or ".." in path.parts:
                raise ValueError(f"Unsafe inventory path: {relative}")
            if digest(output / path) != expected:
                raise ValueError(f"Existing package changed: {relative}")
            old_paths.add(path)
    identity = subprocess.check_output(["file", str(binary)], text=True)
    if "Mach-O" not in identity:
        parser.error("Client must be a built Mach-O executable")
    linkage = subprocess.check_output(["otool", "-L", str(binary)], text=True)
    for line in linkage.splitlines()[1:]:
        dependency = line.strip().split(" (", 1)[0]
        if not dependency.startswith(("/System/Library/", "/usr/lib/")):
            parser.error(f"Non-system dependency needs explicit packaging: {dependency}")

    contents = output / "Contents"
    package = contents / "Resources"
    files = {}
    manifest = root / "media/resource-manifest.txt"
    for line in manifest.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        _, separator, relative = line.partition("|")
        path = Path(relative)
        if not separator or path.is_absolute() or ".." in path.parts:
            raise ValueError(f"Invalid resource manifest line: {line}")
        files[relative] = root / path
    files.update({"media/resource-manifest.txt": manifest,
                  "README.md": root / "README.md", "bin/HelloMine3D": binary})
    # Reuse the distribution's explicit third-party notice list.
    windows_packager = (root / "tools/package_windows_release.ps1").read_text()
    notice_block = windows_packager.split("$notices = [ordered]@{", 1)[1].split("}", 1)[0]
    notices = re.findall(r'"([^"\n]+)"\s*=\s*"([^"\n]+)"', notice_block)
    if not notices:
        raise ValueError("Distribution notice list is missing")
    for target, source in notices:
        files[f"notices/{target}"] = root / source.replace("\\", "/")
    managed_paths = set()
    for relative, source in files.items():
        if not source.is_file() or source.stat().st_size == 0:
            raise ValueError(f"Missing or empty package input: {source}")
        target = package / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        managed_paths.add(target.relative_to(output))

    launcher = contents / "MacOS/HelloMine3D"
    launcher.parent.mkdir(parents=True, exist_ok=True)
    launcher.write_text('''#!/bin/bash
set -euo pipefail
package_root="$(cd "$(dirname "$0")/../Resources" && pwd)"
export HELLOMINE3D_ROOT="$package_root"
cd "$package_root/bin"
exec ./HelloMine3D "$@"
''')
    launcher.chmod(0o755)
    managed_paths.add(launcher.relative_to(output))
    with (contents / "Info.plist").open("wb") as stream:
        plistlib.dump({"CFBundleExecutable": "HelloMine3D",
                       "CFBundleIdentifier": "local.hellomine3d.macos-goal",
                       "CFBundleName": "HelloMine3D macOS Goal",
                       "CFBundlePackageType": "APPL",
                       "CFBundleVersion": "1",
                       "NSHighResolutionCapable": True}, stream)
    managed_paths.add((contents / "Info.plist").relative_to(output))
    metadata = {
        "platform": "macOS", "configuration": args.configuration,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "tracked_diff_sha256": hashlib.sha256(subprocess.check_output(
            ["git", "diff", "HEAD", "--"], cwd=root)).hexdigest(),
        "executable_sha256": digest(binary), "file_identity": identity.strip(),
        "linkage": linkage.splitlines()[1:],
        "resource_manifest_sha256": digest(manifest),
        "source_tree_required": False,
        "acceptance": "NOT_RUN", "signed_or_notarized": False,
    }
    (package / "build-identity.json").write_text(json.dumps(metadata, indent=2) + "\n")
    managed_paths.add((package / "build-identity.json").relative_to(output))
    for path in old_paths - managed_paths:
        (output / path).unlink()
    inventory = []
    for path in sorted(managed_paths):
        inventory.append(f"{digest(output / path)}  {path.as_posix()}")
    (package / "distribution-sha256.txt").write_text("\n".join(inventory) + "\n")
    print(f"[MACOS_PACKAGE] mode={'REFRESH' if args.refresh_existing else 'NEW'} files={len(inventory)} executable_sha256={digest(binary)}")
    print(output)


if __name__ == "__main__":
    main()
