#!/usr/bin/env python3
"""Freeze representative region sites from production planner survey output."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def select(source):
    groups = {}
    with source.open(newline="") as stream:
        for row in csv.DictReader(stream):
            row = {key: int(value) for key, value in row.items()}
            region = row["region"]
            if row["seed"] not in (0, 42, 20260807):
                continue
            if region < 7 and not row["core"]:
                continue
            groups.setdefault((row["seed"], region), {})[row["x"], row["z"]] = row
    sites = []
    for seed in (0, 42, 20260807):
        for region in range(9):
            rows = groups.get((seed, region), {})
            remaining = set(rows)
            components = []
            while remaining:
                start = min(remaining)
                remaining.remove(start)
                component, stack = [], [start]
                while stack:
                    x, z = stack.pop()
                    component.append((x, z))
                    for p in ((x-32, z), (x+32, z), (x, z-32), (x, z+32)):
                        if p in remaining:
                            remaining.remove(p)
                            stack.append(p)
                components.append(component)
            if not components:
                raise ValueError(f"missing seed {seed} region {region}")
            chosen = min(components, key=lambda c: (-len(c), min(c)))
            cx = sum(p[0] for p in chosen) / len(chosen)
            cz = sum(p[1] for p in chosen) / len(chosen)
            point = min(chosen, key=lambda p: ((p[0]-cx)**2+(p[1]-cz)**2, p))
            row = rows[point]
            sites.append({"seed": seed, "region": region, "x": point[0], "z": point[1],
                          "height": row["height"], "component_samples": len(chosen),
                          "camera_rotation": [-8, 35, 0], "world_time": 6000})
    return {"schema": 1, "terrain_version": 16,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "selection": "largest core component, nearest sample to centroid, deterministic ties",
            "scope": "planning sites fixed before visual capture; no visual or gameplay PASS",
            "sites": sites}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("survey", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output already exists")
    args.output.write_text(json.dumps(select(args.survey), ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
