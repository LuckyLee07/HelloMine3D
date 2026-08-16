#!/usr/bin/env python3
"""Validate the exact generated Xcode workspace/project-reference graph."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Callable


DEFAULT_CONTRACT = pathlib.Path(__file__).with_name(
    "xcode-project-graph-contract-v1.json"
)
PBX_ID = r"[A-F0-9]{24}"
SECTION_TEMPLATE = "/* {boundary} {name} section */"
GROUP_OBJECT_START = re.compile(
    rf"^\s*({PBX_ID})(?: /\*.*\*/)? = \{{$"
)
GROUP_OBJECT_END = re.compile(r"^\s*\};$")
GROUP_CHILD = re.compile(rf"^\s*({PBX_ID})(?: /\*.*\*/)?,$")
PROJECT_REF = re.compile(rf"ProjectRef = ({PBX_ID})")
FILE_REFERENCE = re.compile(
    rf"^\s*({PBX_ID})(?: /\*.*\*/)? = "
    r"\{isa = PBXFileReference;",
    re.MULTILINE,
)


class GraphError(RuntimeError):
    pass


@dataclass(frozen=True)
class GraphContract:
    workspace: str
    projects: tuple[str, ...]


@dataclass(frozen=True)
class GraphStats:
    projects: int
    groups: int
    memberships: int
    project_refs: int


def _require_relative_path(value: object, label: str, suffix: str) -> str:
    if not isinstance(value, str) or not value or "\\" in value:
        raise GraphError(f"contract has invalid {label}")
    path = pathlib.PurePosixPath(value)
    if (
        path.is_absolute()
        or value != path.as_posix()
        or any(part in {"", ".", ".."} for part in path.parts)
        or not value.endswith(suffix)
    ):
        raise GraphError(f"contract has invalid {label}='{value}'")
    return value


def load_contract(path: pathlib.Path) -> GraphContract:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GraphError(f"cannot load graph contract {path}: {error}") from error
    if not isinstance(raw, dict) or raw.get("contract_version") != 1:
        raise GraphError("graph contract must use contract_version=1")
    workspace = _require_relative_path(
        raw.get("workspace"), "workspace", ".xcworkspacedata"
    )
    raw_projects = raw.get("projects")
    if not isinstance(raw_projects, list) or not raw_projects:
        raise GraphError("graph contract projects must be a non-empty list")
    projects = tuple(
        _require_relative_path(value, f"projects[{index}]", ".xcodeproj")
        for index, value in enumerate(raw_projects)
    )
    if len(projects) != len(set(projects)):
        raise GraphError("graph contract projects contain duplicates")
    if list(projects) != sorted(projects):
        raise GraphError("graph contract projects must be sorted")
    expected_count = raw.get("expected_project_count")
    if (
        isinstance(expected_count, bool)
        or not isinstance(expected_count, int)
        or expected_count != len(projects)
    ):
        raise GraphError(
            "graph contract expected_project_count does not match projects"
        )
    return GraphContract(workspace=workspace, projects=projects)


def _format_inventory_difference(
    label: str, expected: set[str], actual: set[str]
) -> str:
    parts: list[str] = []
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing:
        parts.append("missing=" + ",".join(missing))
    if unexpected:
        parts.append("unexpected=" + ",".join(unexpected))
    return f"{label} mismatch: " + " ".join(parts)


def _read_workspace_projects(path: pathlib.Path) -> tuple[str, ...]:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        raise GraphError(f"cannot parse workspace {path}: {error}") from error
    projects: list[str] = []
    for file_ref in root.iter("FileRef"):
        location = file_ref.attrib.get("location", "")
        if not location.startswith("group:") or not location.endswith(
            ".xcodeproj"
        ):
            raise GraphError(
                f"workspace has invalid FileRef location='{location}'"
            )
        projects.append(location[len("group:") :])
    if len(projects) != len(set(projects)):
        raise GraphError("workspace contains duplicate FileRef entries")
    return tuple(projects)


def _extract_section(text: str, name: str) -> str:
    begin = SECTION_TEMPLATE.format(boundary="Begin", name=name)
    end = SECTION_TEMPLATE.format(boundary="End", name=name)
    if text.count(begin) != 1 or text.count(end) != 1:
        raise GraphError(f"PBX project has invalid {name} section markers")
    start = text.index(begin) + len(begin)
    finish = text.index(end, start)
    return text[start:finish]


def _parse_groups(
    project_label: str, text: str
) -> tuple[dict[str, tuple[str, ...]], dict[str, tuple[str, ...]]]:
    section = _extract_section(text, "PBXGroup")
    groups: dict[str, tuple[str, ...]] = {}
    current_id: str | None = None
    current_lines: list[str] = []
    for line in section.splitlines():
        if current_id is None:
            start = GROUP_OBJECT_START.match(line)
            if start:
                current_id = start.group(1)
                current_lines = []
            elif line.strip():
                raise GraphError(
                    f"{project_label}: malformed PBXGroup line '{line.strip()}'"
                )
            continue
        if GROUP_OBJECT_END.match(line):
            body = "\n".join(current_lines)
            if "isa = PBXGroup;" not in body:
                raise GraphError(
                    f"{project_label}: group {current_id} has invalid isa"
                )
            children_match = re.search(
                r"children = \((.*?)\);", body, re.DOTALL
            )
            if children_match is None:
                raise GraphError(
                    f"{project_label}: group {current_id} has no children list"
                )
            children: list[str] = []
            for child_line in children_match.group(1).splitlines():
                if not child_line.strip():
                    continue
                child = GROUP_CHILD.match(child_line)
                if child is None:
                    raise GraphError(
                        f"{project_label}: malformed group child "
                        f"'{child_line.strip()}'"
                    )
                children.append(child.group(1))
            if len(children) != len(set(children)):
                raise GraphError(
                    f"{project_label}: group {current_id} contains duplicate children"
                )
            groups[current_id] = tuple(children)
            current_id = None
            current_lines = []
        else:
            current_lines.append(line)
    if current_id is not None:
        raise GraphError(f"{project_label}: unterminated PBXGroup {current_id}")

    memberships: dict[str, list[str]] = {}
    for group_id, children in groups.items():
        for child_id in children:
            memberships.setdefault(child_id, []).append(group_id)
    repeated = {
        child_id: parents
        for child_id, parents in memberships.items()
        if len(parents) > 1
    }
    if repeated:
        child_id = sorted(repeated)[0]
        raise GraphError(
            f"{project_label}: child {child_id} belongs to multiple PBXGroups "
            + ",".join(sorted(repeated[child_id]))
        )
    return groups, {
        child_id: tuple(parents) for child_id, parents in memberships.items()
    }


def _parse_project_refs(project_label: str, text: str) -> tuple[str, ...]:
    blocks = re.findall(
        r"projectReferences = \((.*?)\);\s*projectRoot =",
        text,
        re.DOTALL,
    )
    if len(blocks) > 1:
        raise GraphError(f"{project_label}: multiple projectReferences blocks")
    if not blocks:
        return ()
    refs = tuple(PROJECT_REF.findall(blocks[0]))
    if len(refs) != len(set(refs)):
        raise GraphError(f"{project_label}: duplicate ProjectRef entries")
    return refs


def _validate_pbx_project(path: pathlib.Path, label: str) -> GraphStats:
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise GraphError(f"cannot read PBX project {path}: {error}") from error
    groups, memberships = _parse_groups(label, text)
    refs = _parse_project_refs(label, text)
    file_reference_section = _extract_section(text, "PBXFileReference")
    file_references = set(FILE_REFERENCE.findall(file_reference_section))
    for ref in refs:
        if ref not in file_references:
            raise GraphError(
                f"{label}: ProjectRef {ref} is not a PBXFileReference"
            )
        parents = memberships.get(ref, ())
        if len(parents) != 1:
            raise GraphError(
                f"{label}: ProjectRef {ref} must belong to exactly one PBXGroup"
            )
    return GraphStats(
        projects=1,
        groups=len(groups),
        memberships=len(memberships),
        project_refs=len(refs),
    )


def validate_graph(
    build_dir: pathlib.Path, contract: GraphContract
) -> GraphStats:
    build_dir = build_dir.resolve()
    workspace_path = build_dir / pathlib.PurePosixPath(contract.workspace)
    workspace_projects = _read_workspace_projects(workspace_path)
    expected = set(contract.projects)
    actual_workspace = set(workspace_projects)
    if expected != actual_workspace:
        raise GraphError(
            _format_inventory_difference(
                "workspace project inventory", expected, actual_workspace
            )
        )

    project_files = sorted(build_dir.rglob("project.pbxproj"))
    actual_projects: dict[str, pathlib.Path] = {}
    for project_file in project_files:
        project_dir = project_file.parent
        relative = project_dir.relative_to(build_dir).as_posix()
        if project_dir.suffix != ".xcodeproj":
            raise GraphError(
                f"project.pbxproj is outside an xcodeproj directory: {relative}"
            )
        if relative in actual_projects:
            raise GraphError(f"duplicate on-disk project path: {relative}")
        actual_projects[relative] = project_file
    actual = set(actual_projects)
    if expected != actual:
        raise GraphError(
            _format_inventory_difference(
                "on-disk project inventory", expected, actual
            )
        )

    groups = 0
    memberships = 0
    project_refs = 0
    for relative in contract.projects:
        stats = _validate_pbx_project(actual_projects[relative], relative)
        groups += stats.groups
        memberships += stats.memberships
        project_refs += stats.project_refs
    return GraphStats(
        projects=len(actual_projects),
        groups=groups,
        memberships=memberships,
        project_refs=project_refs,
    )


FIXTURE_PROJECT = "App/App.xcodeproj"
FIXTURE_REF = "BBBBBBBBBBBBBBBBBBBBBBBB"


def _fixture_pbx(
    *,
    project_refs: int = 1,
    group_children: int = 1,
    second_group: bool = False,
    declare_ref: bool = True,
) -> str:
    declaration = (
        f"\t\t{FIXTURE_REF} /* Dependency.xcodeproj */ = "
        "{isa = PBXFileReference; lastKnownFileType = "
        '"wrapper.pb-project"; path = Dependency.xcodeproj; '
        "sourceTree = SOURCE_ROOT; };\n"
        if declare_ref
        else ""
    )
    children = "".join(
        f"\t\t\t\t{FIXTURE_REF} /* Dependency.xcodeproj */,\n"
        for _ in range(group_children)
    )
    extra_group = ""
    if second_group:
        extra_group = f"""
\t\tDDDDDDDDDDDDDDDDDDDDDDDD /* Other */ = {{
\t\t\tisa = PBXGroup;
\t\t\tchildren = (
\t\t\t\t{FIXTURE_REF} /* Dependency.xcodeproj */,
\t\t\t);
\t\t\tname = Other;
\t\t\tsourceTree = "<group>";
\t\t}};
"""
    references = "".join(
        f"""
\t\t\t\t{{
\t\t\t\t\tProjectRef = {FIXTURE_REF} /* Dependency.xcodeproj */;
\t\t\t\t}},
"""
        for _ in range(project_refs)
    )
    return f"""// !$*UTF8*$!
/* Begin PBXFileReference section */
{declaration}/* End PBXFileReference section */

/* Begin PBXGroup section */
\t\tAAAAAAAAAAAAAAAAAAAAAAAA /* Projects */ = {{
\t\t\tisa = PBXGroup;
\t\t\tchildren = (
{children}\t\t\t);
\t\t\tname = Projects;
\t\t\tsourceTree = "<group>";
\t\t}};
{extra_group}/* End PBXGroup section */

/* Begin PBXProject section */
\t\tCCCCCCCCCCCCCCCCCCCCCCCC /* Project object */ = {{
\t\t\tisa = PBXProject;
\t\t\tprojectReferences = (
{references}\t\t\t);
\t\t\tprojectRoot = "";
\t\t}};
/* End PBXProject section */
"""


def _write_fixture(
    root: pathlib.Path,
    *,
    workspace_project: str = FIXTURE_PROJECT,
    pbx: str | None = None,
) -> tuple[pathlib.Path, pathlib.Path]:
    contract_path = root / "contract.json"
    contract_path.write_text(
        json.dumps(
            {
                "contract_version": 1,
                "workspace": "App.xcworkspace/contents.xcworkspacedata",
                "expected_project_count": 1,
                "projects": [FIXTURE_PROJECT],
            }
        ),
        encoding="utf-8",
    )
    workspace = root / "App.xcworkspace" / "contents.xcworkspacedata"
    workspace.parent.mkdir(parents=True)
    workspace.write_text(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Workspace version=\"1.0\">\n"
        f"  <FileRef location=\"group:{workspace_project}\"/>\n"
        "</Workspace>\n",
        encoding="utf-8",
    )
    project_file = root / FIXTURE_PROJECT / "project.pbxproj"
    project_file.parent.mkdir(parents=True)
    project_file.write_text(
        _fixture_pbx() if pbx is None else pbx, encoding="utf-8"
    )
    return contract_path, project_file


def _unexpected_project(root: pathlib.Path) -> None:
    stale = root / "Stale/Stale.xcodeproj/project.pbxproj"
    stale.parent.mkdir(parents=True)
    stale.write_text(_fixture_pbx(), encoding="utf-8")


def _invalid_contract(
    root: pathlib.Path, *, count: int = 1, project: str = FIXTURE_PROJECT
) -> tuple[pathlib.Path, pathlib.Path]:
    contract_path, project_file = _write_fixture(root)
    contract_path.write_text(
        json.dumps(
            {
                "contract_version": 1,
                "workspace": "App.xcworkspace/contents.xcworkspacedata",
                "expected_project_count": count,
                "projects": [project],
            }
        ),
        encoding="utf-8",
    )
    return contract_path, project_file


def run_self_tests() -> int:
    cases: tuple[
        tuple[str, str, Callable[[pathlib.Path], tuple[pathlib.Path, pathlib.Path]]],
        ...,
    ] = (
        ("valid", "PASS", lambda root: _write_fixture(root)),
        (
            "workspace-inventory",
            "workspace project inventory mismatch",
            lambda root: _write_fixture(
                root, workspace_project="Missing/Missing.xcodeproj"
            ),
        ),
        (
            "duplicate-project-ref",
            "duplicate ProjectRef entries",
            lambda root: _write_fixture(
                root, pbx=_fixture_pbx(project_refs=2)
            ),
        ),
        (
            "duplicate-group-child",
            "contains duplicate children",
            lambda root: _write_fixture(
                root, pbx=_fixture_pbx(group_children=2)
            ),
        ),
        (
            "multiple-group-membership",
            "belongs to multiple PBXGroups",
            lambda root: _write_fixture(
                root, pbx=_fixture_pbx(second_group=True)
            ),
        ),
        (
            "missing-file-reference",
            "is not a PBXFileReference",
            lambda root: _write_fixture(
                root, pbx=_fixture_pbx(declare_ref=False)
            ),
        ),
        (
            "contract-count",
            "expected_project_count does not match",
            lambda root: _invalid_contract(root, count=2),
        ),
        (
            "contract-traversal",
            "invalid projects[0]",
            lambda root: _invalid_contract(
                root, project="../App/App.xcodeproj"
            ),
        ),
        (
            "unexpected-project",
            "on-disk project inventory mismatch",
            lambda root: _write_fixture(root),
        ),
    )
    for name, expected, setup in cases:
        with tempfile.TemporaryDirectory(prefix="hellomine-xcode-graph-") as temp:
            root = pathlib.Path(temp)
            contract_path, _ = setup(root)
            if name == "unexpected-project":
                _unexpected_project(root)
            actual = "PASS"
            try:
                validate_graph(root, load_contract(contract_path))
            except GraphError as error:
                actual = str(error)
            passed = actual == "PASS" if expected == "PASS" else expected in actual
            print(
                f"[XCODE_GRAPH_VERIFY] case={name} "
                f"expected={'PASS' if expected == 'PASS' else 'INVALID'} "
                f"status={'PASS' if passed else 'FAIL'}"
            )
            if not passed:
                raise GraphError(
                    f"self-test '{name}' expected '{expected}' actual '{actual}'"
                )
    print(f"[XCODE_GRAPH_VERIFY] status=PASS cases={len(cases)}")
    return len(cases)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument(
        "--contract", type=pathlib.Path, default=DEFAULT_CONTRACT
    )
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            run_self_tests()
        if args.build_dir is None:
            if not args.self_test:
                raise GraphError("--build-dir is required unless --self-test is used")
            return 0
        contract = load_contract(args.contract.resolve())
        stats = validate_graph(args.build_dir, contract)
        print(
            "[XCODE_GRAPH] status=PASS "
            f"projects={stats.projects} groups={stats.groups} "
            f"memberships={stats.memberships} project_refs={stats.project_refs}"
        )
        return 0
    except GraphError as error:
        print(f"[XCODE_GRAPH] status=INVALID reason={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
