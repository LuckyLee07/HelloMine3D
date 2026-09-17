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


def resource_manifest_entries(path):
    lines = path.read_text().splitlines()
    if not lines or lines[0] != "# HelloMine3D resource manifest v1":
        raise ValueError(f"Invalid resource manifest header: {path}")
    entries = [line for line in lines if line.strip() and not line.startswith("#")]
    if entries != sorted(set(entries)):
        raise ValueError("Resource manifest entries must be unique and sorted")
    for entry in entries:
        category, separator, relative = entry.partition("|")
        resource = Path(relative)
        if (not separator or not re.fullmatch(r"[a-z][a-z-]*", category)
                or not relative or resource.is_absolute() or ".." in resource.parts):
            raise ValueError(f"Invalid resource manifest line: {entry}")
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="New .app path, or an existing workbench with --refresh-existing")
    parser.add_argument("--refresh-existing", action="store_true",
                        help="Refresh a verified, unaccepted workbench app in place")
    parser.add_argument("--configuration", choices=("Debug", "Release"),
                        default="Release", help="Configuration actually built by caller")
    parser.add_argument("--binary", type=Path,
                        help="Explicit client output for an isolated concurrent build")
    parser.add_argument("--bundle-id",
                        help="Stable project bundle identity; retained on subsequent refreshes")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.absolute()
    binary = args.binary.resolve(strict=True) if args.binary else root / "bin/HelloMine3D"
    if sys.platform != "darwin" or output.suffix != ".app":
        parser.error("Run on macOS and choose a .app output path")
    if args.refresh_existing != output.exists():
        parser.error("Use --refresh-existing only with an existing workbench .app")
    old_paths = set()
    bundle_id = args.bundle_id or "local.hellomine3d.macos-goal"
    if not re.fullmatch(r"local\.hellomine3d\.[a-z0-9-]+", bundle_id):
        parser.error("Bundle id must be a local.hellomine3d project identity")
    if args.refresh_existing:
        inventory = output / "Contents/Resources/distribution-sha256.txt"
        metadata = json.loads((output / "Contents/Resources/build-identity.json").read_text())
        if metadata.get("acceptance") != "NOT_RUN" or metadata.get("signed_or_notarized"):
            parser.error("Only an unaccepted, unsigned workbench package may be refreshed")
        with (output / "Contents/Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        previous_bundle = metadata.get("bundle_id", "local.hellomine3d.macos-goal")
        if info.get("CFBundleIdentifier") != previous_bundle:
            parser.error("Existing package has a different bundle identity")
        if not args.bundle_id:
            bundle_id = previous_bundle
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
    for line in resource_manifest_entries(manifest):
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
                       "CFBundleIdentifier": bundle_id,
                       "CFBundleName": output.stem,
                       "CFBundlePackageType": "APPL",
                       "CFBundleVersion": "1",
                       "NSHighResolutionCapable": True}, stream)
    managed_paths.add((contents / "Info.plist").relative_to(output))
    # git diff excludes new files; include their identities in a source receipt.
    source_entries = []
    for source in sorted((root / "src/HelloMine3D").rglob("*")):
        if source.is_file() and source.suffix in (".h", ".cpp", ".m", ".mm"):
            source_entries.append(f"{digest(source)}  {source.relative_to(root).as_posix()}")
    source_receipt = package / "source-tree-sha256.txt"
    source_receipt.write_text("\n".join(source_entries) + "\n")
    managed_paths.add(source_receipt.relative_to(output))
    metadata = {
        "platform": "macOS", "configuration": args.configuration,
        "bundle_id": bundle_id,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "tracked_diff_sha256": hashlib.sha256(subprocess.check_output(
            ["git", "diff", "HEAD", "--"], cwd=root)).hexdigest(),
        "executable_sha256": digest(binary), "file_identity": identity.strip(),
        "linkage": linkage.splitlines()[1:],
        "resource_manifest_sha256": digest(manifest),
        "source_file_count": len(source_entries),
        "source_manifest_sha256": digest(source_receipt),
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
