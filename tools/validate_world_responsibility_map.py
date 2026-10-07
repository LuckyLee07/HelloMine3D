#!/usr/bin/env python3
"""Portable check of the AL-A1 public-surface responsibility contract.

Uses the same public declaration, normalization and table rules as the existing
PowerShell checker. This does not claim that the Windows/PowerShell gate ran.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

PUBLIC = r"(?s)\n  public:\r?\n(.*?)\n  private:\r?\n"
HASH = r"<!-- AL-A1-WORLD-API-HASH sha256=([A-F0-9]{64}) -->"
MAP = r"(?s)<!-- AL-A1-WORLD-API-MAP-BEGIN -->(.*?)<!-- AL-A1-WORLD-API-MAP-END -->"
ROW = (r"(?m)^\| `(?P<api>[^`]+)` \| `(?P<concept>Query|Command|Runtime Tick)` \| "
       r"`(?P<responsibility>World Query|World Mutation|Simulation|Streaming|Persistence|Actor|Combat|Progression|Diagnostics)` \|")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inspect(source, document):
    public = re.search(PUBLIC, source)
    require(public is not None, "World.h public API boundary was not found")
    normalized = re.sub(r"[ \t]+(?=\n|$)", "", public[1].replace("\r\n", "\n")).strip()
    actual = hashlib.sha256(normalized.encode("utf8")).hexdigest().upper()
    expected = re.search(HASH, document)
    require(expected is not None, "AL-A1 World API hash marker is missing or malformed")
    require(actual == expected[1], "World.h public surface changed; review map/hash: " + actual)
    table = re.search(MAP, document)
    require(table is not None, "AL-A1 World API map markers are missing")
    rows = list(re.finditer(ROW, table[1]))
    require(rows, "AL-A1 World API map contains no machine-readable rows")
    names = [row["api"] for row in rows]
    require(len(names) == len(set(names)), "World responsibility map duplicates a method")
    stripped = re.sub(r"(?m)//[^\r\n]*", "", public[1])
    declared = set()
    for line in re.finditer(r"(?m)^ {4}(?! )([^\r\n]*\()", stripped):
        identifiers = re.findall(r"([A-Za-z_~][A-Za-z0-9_]*)\s*\(", line[1])
        if identifiers:
            declared.add(identifiers[-1])
    missing, stale = sorted(declared - set(names)), sorted(set(names) - declared)
    require(not missing and not stale, "World responsibility mismatch: missing=" + str(missing) + "; stale=" + str(stale))
    concepts = Counter(row["concept"] for row in rows)
    return {"status": "PASS", "unique_methods": len(declared), "queries": concepts["Query"],
            "commands": concepts["Command"], "runtime_ticks": concepts["Runtime Tick"],
            "public_sha256": actual, "declared_methods": sorted(declared)}


def calibrate(source, document):
    # Copy actual valid inputs; do not synthesize a successful contract surface.
    inspect(source, document)
    line = next(line for line in document.splitlines(True) if re.match(ROW, line))
    corruptions = {
        "wrong-public-hash": (source, re.sub(HASH, "<!-- AL-A1-WORLD-API-HASH sha256=" + "0" * 64 + " -->", document, count=1)),
        "missing-current-method-row": (source, document.replace(line, "", 1)),
        "duplicate-current-method-row": (source, document.replace(line, line + line, 1)),
        "stale-method-row": (source, document.replace("<!-- AL-A1-WORLD-API-MAP-END -->", "| `notAnActualWorldMethod` | `Query` | `Diagnostics` | invalid |\n<!-- AL-A1-WORLD-API-MAP-END -->", 1)),
        "unsupported-concept": (source, document.replace(line, line.replace("`Query`", "`Unsupported`", 1) if "`Query`" in line else line.replace("`Command`", "`Unsupported`", 1), 1)),
        "unreviewed-public-declaration": (source.replace("\n  private:", "\n    void unreviewedWorldEntry();\n  private:", 1), document),
        "missing-public-boundary": (source.replace("  public:", "  protected:", 1), document),
    }
    outcomes = []
    for name, (header, architecture) in corruptions.items():
        try:
            inspect(header, architecture)
        except ValueError as error:
            outcomes.append({"name": name, "rejected": True, "reason": str(error)})
        else:
            outcomes.append({"name": name, "rejected": False})
    require(all(case["rejected"] for case in outcomes), "A public-surface negative calibration was accepted")
    return outcomes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    header = args.root / "src/HelloMine3D/World/World.h"
    architecture = args.root / "docs/current/architecture.md"
    source, document = header.read_text(), architecture.read_text()
    try:
        result = inspect(source, document)
        result.update(schema="hellomine3d-world-api-map-portable-v1", power_shell_execution_claimed=False,
                      header_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),
                      architecture_sha256=hashlib.sha256(architecture.read_bytes()).hexdigest())
        if args.self_test:
            result["actual_input_corruption_calibration"] = calibrate(source, document)
        print(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False))
        return 0
    except ValueError as error:
        print(json.dumps({"schema": "hellomine3d-world-api-map-portable-v1", "status": "FAIL", "error": str(error)}, ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
