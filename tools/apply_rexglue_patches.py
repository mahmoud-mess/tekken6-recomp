#!/usr/bin/env python3
"""Apply the project-owned ReXGlue compatibility patch series."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


BASE_REVISION = "0c7b01a0ac0479801757507d80533f662fa0815d"
PATCHES = (
    "0001-tekken-runtime-support.patch",
    "0002-draw-outcome-tracing.patch",
    "0003-guest-av-context.patch",
    "0004-windowed-frame.patch",
)


def git(sdk: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(sdk), *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--sdk",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "third_party" / "rexglue-sdk",
        help="ReXGlue SDK checkout (default: this repo's submodule)",
    )
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    patch_dir = Path(__file__).resolve().parents[1] / "patches" / "rexglue"

    if not (sdk / ".git").exists():
        print(f"ReXGlue SDK checkout not found: {sdk}", file=sys.stderr)
        return 2
    revision = git(sdk, "rev-parse", "HEAD").stdout.strip()
    if revision != BASE_REVISION:
        print(
            f"Expected ReXGlue base {BASE_REVISION}, found {revision}; refusing to patch a different SDK.",
            file=sys.stderr,
        )
        return 2

    patch_paths = [patch_dir / name for name in PATCHES]
    reverse_checks = [
        git(sdk, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0
        for patch in reversed(patch_paths)
    ]
    if all(reverse_checks):
        print("ReXGlue project patches are already applied.")
        return 0
    if any(reverse_checks):
        print("ReXGlue checkout has a partial patch series; refusing to change it.", file=sys.stderr)
        return 2

    dirty = git(sdk, "status", "--porcelain", "--untracked-files=no").stdout.strip()
    if dirty:
        print("ReXGlue checkout has local tracked changes; refusing to overwrite them.", file=sys.stderr)
        return 2

    for patch in patch_paths:
        if not patch.is_file():
            print(f"Missing project patch: {patch}", file=sys.stderr)
            return 2
        git(sdk, "apply", "--check", str(patch))
        git(sdk, "apply", str(patch))
    print("Applied the pinned ReXGlue compatibility patches.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
