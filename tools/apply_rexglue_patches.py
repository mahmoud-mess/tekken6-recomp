#!/usr/bin/env python3
"""Apply or refresh the Tekken 6 SDK patch against its upstream revision."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


BASE_REVISION = "0c7b01a0ac0479801757507d80533f662fa0815d"
PATCH_NAME = "0001-tekken6-runtime.patch"


def git(sdk: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(sdk), *args],
        check=check,
        text=True,
        encoding="utf-8",
        errors="surrogateescape",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8", errors="backslashreplace")

    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument(
        "--sdk", type=Path,
        default=Path(__file__).resolve().parents[1] / "third_party/rexglue-sdk",
        help="SDK checkout (default: this repository's submodule)",
    )
    parser.add_argument(
        "--refresh", action="store_true",
        help="Save local SDK source changes to the repository patch",
    )
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    patch = Path(__file__).resolve().parents[1] / "patches/rexglue" / PATCH_NAME

    if not (sdk / ".git").exists():
        print(f"SDK checkout not found: {sdk}", file=sys.stderr)
        return 2
    revision = git(sdk, "rev-parse", "HEAD").stdout.strip()
    if revision != BASE_REVISION:
        print(f"Expected SDK revision {BASE_REVISION}, found {revision}.", file=sys.stderr)
        return 2

    if args.refresh:
        untracked = git(sdk, "ls-files", "--others", "--exclude-standard").stdout.strip()
        if untracked:
            print(
                "Record new SDK source files with git add -N before refreshing:\n" + untracked,
                file=sys.stderr,
            )
            return 2
        delta = git(sdk, "diff", "--binary", "--full-index", BASE_REVISION).stdout
        if not delta:
            print("No SDK source changes to save.", file=sys.stderr)
            return 2
        # Preserve non-UTF-8 source bytes captured by git without changing the patch.
        patch.write_text(delta, encoding="utf-8", errors="surrogateescape")
        print(f"Updated {patch.name}.")
        return 0

    if not patch.is_file():
        print(f"Missing SDK patch: {patch}", file=sys.stderr)
        return 2
    if git(sdk, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
        print("Tekken 6 SDK patch is already applied.")
        return 0
    if git(sdk, "status", "--porcelain", "--untracked-files=no").stdout.strip():
        print("SDK has local changes; refusing to overwrite them.", file=sys.stderr)
        return 2
    git(sdk, "apply", "--check", str(patch))
    # Not `git apply --intent-to-add`: some git versions (Homebrew 2.50) write an
    # index holding only the new files, which leaves every other SDK file
    # staged as deleted. Mark the new files with add -N separately instead.
    git(sdk, "apply", str(patch))
    summary = git(sdk, "apply", "--summary", str(patch)).stdout
    created = [line.split(" ", 4)[4] for line in summary.splitlines()
               if line.startswith(" create mode ")]
    if created:
        git(sdk, "add", "-N", "--", *created)
    print("Applied the Tekken 6 SDK patch.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
