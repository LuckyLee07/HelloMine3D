#!/usr/bin/env python3
"""Freeze representative connected regions from a production terrain survey."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Refusing to replace a frozen selection")
    seeds = {}
    with args.samples.open() as stream:
        for row in csv.DictReader(stream):
            if row["set"] == "macro":
                seeds.setdefault(int(row["seed"]), {})[
                    (int(row["x"]), int(row["z"]))] = row
    regions = []
    for seed, cells in seeds.items():
        for biome in (0, 6, 7, 1, 3, 4):
            remaining = {p for p, r in cells.items() if int(r["biome"]) == biome}
            components = []
            while remaining:
                pending = [min(remaining)]
                remaining.remove(pending[0])
                component = []
                while pending:
                    x, z = pending.pop()
                    component.append((x, z))
                    for neighbour in ((x+32, z), (x-32, z), (x, z+32), (x, z-32)):
                        if neighbour in remaining:
                            remaining.remove(neighbour)
                            pending.append(neighbour)
                components.append(component)
            if not components:
                raise ValueError(f"Missing biome {biome} in seed {seed}")
            component = max(components, key=lambda c: (len(c), -min(c)[0], -min(c)[1]))
            cx = sum(p[0] for p in component) / len(component)
            cz = sum(p[1] for p in component) / len(component)
            x, z = min(component, key=lambda p: ((p[0]-cx)**2+(p[1]-cz)**2, p))
            regions.append(dict(seed=seed, biome=biome, component_cells=len(component),
                                x=x, z=z, height=int(cells[(x, z)]["height"])))
    result = dict(selection="Largest four-connected biome component on the 32-block macro grid; member nearest centroid; lexicographic ties",
                  source=str(args.samples.resolve()),
                  source_sha256=hashlib.sha256(args.samples.read_bytes()).hexdigest(),
                  regions=regions)
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
