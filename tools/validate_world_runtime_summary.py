#!/usr/bin/env python3
"""Validate the full WorldRuntime log, including every emitted assertion.

WorldRuntimeSmokeMain.cpp check() increments g_checkCount once per PASS/FAIL
line, then main() emits checks/failures and a final status. The current full
route includes caseHdrConfig, caseReferenceShapes and caseArchitecturalKit.
Its exact 4465 checks were established by Debug/Release-full-world.log in
water-boundary-clip-r2 after 1201 boundary-clipping checks; previous 3264-check
full logs and focused architectural runs must not satisfy this current gate.
New suite checks require an explicit count update with full-run evidence.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import sys

FULL_CHECKS = 4465
START = "[VALIDATION] world runtime smoke starting"
PREFIX = "[VALIDATION]"
SUMMARY = re.compile(r"\[VALIDATION\] checks=([0-9]+) failures=([0-9]+)")
STATUS = re.compile(r"\[VALIDATION\] status=(PASS|FAIL)")
ASSERTION = re.compile(r"\[VALIDATION\] (PASS|FAIL) (.+)")


class InvalidSummary(ValueError):
    pass


def validate(text: str) -> int:
    if "\x00" in text:
        raise InvalidSummary("NUL byte in validation output")
    lines = [line for line in text.splitlines() if line]
    if not lines or lines[0] != START or lines.count(START) != 1:
        raise InvalidSummary("missing or duplicate WorldRuntime start")
    summaries = [SUMMARY.fullmatch(line) for line in lines]
    summaries = [match for match in summaries if match]
    statuses = [STATUS.fullmatch(line) for line in lines]
    statuses = [match for match in statuses if match]
    if len(summaries) != 1 or len(statuses) != 1:
        raise InvalidSummary("require exactly one checks summary and status")
    if lines[-2:] != [summaries[0].group(0), statuses[0].group(0)]:
        raise InvalidSummary("summary and status must be the final two lines")
    assertions = []
    for line in lines:
        match = ASSERTION.fullmatch(line)
        if match:
            assertions.append(match)
        elif PREFIX in line and line not in (
            START, summaries[0].group(0), statuses[0].group(0)
        ):
            raise InvalidSummary("unexpected or malformed validation output: " + line)
    checks, failures = map(int, summaries[0].groups())
    actual_failures = sum(match[1] == "FAIL" for match in assertions)
    if checks != len(assertions) or failures != actual_failures:
        raise InvalidSummary(
            f"summary disagrees with assertions: declared={checks}/{failures} "
            f"observed={len(assertions)}/{actual_failures}"
        )
    if checks != FULL_CHECKS:
        raise InvalidSummary(f"full scope requires {FULL_CHECKS} checks, got {checks}")
    if failures or statuses[0][1] != "PASS":
        raise InvalidSummary(f"full WorldRuntime failed: failures={failures}")
    return checks


def self_test(calibration_dir: Path | None, calibration_full_log: Path | None) -> int:
    good = "\n".join([START] + [
        f"[VALIDATION] PASS fixture/{index}" for index in range(FULL_CHECKS)
    ] + [f"[VALIDATION] checks={FULL_CHECKS} failures=0", "[VALIDATION] status=PASS", ""])
    cases: list[tuple[str, str, bool]] = [("synthetic-full", good, True)]
    if calibration_dir is not None:
        for filename, expected in (
            ("Release-full-world-r4.log", False),
            ("Release-full-world-r2.log", False),
            ("Release-architectural-world-r2.log", False),
            ("Release-architectural-world-r1.log", False),
            ("Release-full-world-r3.log", False),
        ):
            path = calibration_dir / filename
            source = path.read_text(encoding="utf-8")
            cases.append((filename, source, expected))
            print(f"[WORLD_SUMMARY_CALIBRATION] source={path} "
                  f"sha256={hashlib.sha256(path.read_bytes()).hexdigest()}")
        failed = (calibration_dir / "Release-architectural-world-r1.log").read_text(encoding="utf-8")
        cases.append(("actual-49-failures-forged-PASS",
                      failed.replace("checks=3108 failures=49", "checks=3108 failures=0")
                            .replace("status=FAIL", "status=PASS"), False))
        focused = (calibration_dir / "Release-architectural-world-r2.log").read_text(encoding="utf-8")
        cases.append(("actual-focus-forged-full-count",
                      focused.replace("checks=221 failures=0", f"checks={FULL_CHECKS} failures=0"), False))
    if calibration_full_log is not None:
        # Mutate actual current passing evidence, never relabel an older suite.
        good = calibration_full_log.read_text(encoding="utf-8")
        cases.append(("actual-current-full", good, True))
        print(f"[WORLD_SUMMARY_CALIBRATION] source={calibration_full_log} "
              f"sha256={hashlib.sha256(calibration_full_log.read_bytes()).hexdigest()}")
    summary = f"[VALIDATION] checks={FULL_CHECKS} failures=0"
    cases += [
        ("missing-summary", good.replace(summary + "\n", ""), False),
        ("missing-status", good.replace("[VALIDATION] status=PASS\n", ""), False),
        ("missing-start", good.replace(START + "\n", ""), False),
        ("duplicate-summary", good + summary + "\n", False),
        ("duplicate-status", good + "[VALIDATION] status=PASS\n", False),
        ("duplicate-whole-run", good + good, False),
        ("wrong-full-count", good.replace(summary, "[VALIDATION] checks=221 failures=0"), False),
        ("substring-false-pass", good.replace(summary, "prefix " + summary), False),
        ("count-suffix-false-pass", good.replace(summary, summary + " garbage"), False),
        ("status-suffix-false-pass", good.replace("status=PASS", "status=PASS trailing"), False),
        ("false-zero-failures", good.replace("[VALIDATION] PASS ", "[VALIDATION] FAIL ", 1), False),
        ("false-pass-status", good.replace("failures=0", "failures=1"), False),
        ("explicit-fail-status", good.replace("status=PASS", "status=FAIL"), False),
        ("unhandled-exception", good.replace(summary, "[VALIDATION] unhandled exception: injected\n" + summary), False),
        ("trailing-output", good + "extra output\n", False),
        ("nul-byte", good + "\x00", False),
        ("summary-only-false-pass", START + "\n" + summary + "\n[VALIDATION] status=PASS\n", False),
    ]
    failures = 0
    for name, source, expected in cases:
        try:
            validate(source)
            actual = True
        except InvalidSummary:
            actual = False
        failures += actual != expected
        print(f"[WORLD_SUMMARY_CALIBRATION] case={name} "
              f"expected={'ACCEPT' if expected else 'REJECT'} "
              f"actual={'ACCEPT' if actual else 'REJECT'} "
              f"status={'PASS' if actual == expected else 'FAIL'}")
    print(f"[WORLD_SUMMARY_CALIBRATION] checks={len(cases)} failures={failures} "
          f"status={'PASS' if failures == 0 else 'FAIL'}")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--calibration-dir", type=Path)
    parser.add_argument("--calibration-full-log", type=Path)
    args = parser.parse_args()
    if (args.calibration_dir is not None or args.calibration_full_log is not None) and not args.self_test:
        parser.error("calibration inputs require --self-test")
    if args.log is None and not args.self_test:
        parser.error("provide a WorldRuntime log or --self-test")
    try:
        if args.self_test and self_test(args.calibration_dir, args.calibration_full_log):
            return 1
        if args.log is not None:
            checks = validate(args.log.read_text(encoding="utf-8"))
            print(f"[WORLD_SUMMARY] status=PASS scope=full checks={checks} failures=0 log={args.log}")
        return 0
    except (InvalidSummary, OSError, UnicodeError) as error:
        print(f"[WORLD_SUMMARY] status=FAIL reason={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
