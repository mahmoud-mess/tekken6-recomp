#!/usr/bin/env python3
"""Create a standalone Linux app folder from the native build and your game data."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BINARIES = ('tekken6', 'librexruntime.so', 'librexgpu-xenos.so')


def copy_changed(source, destination):
    source, destination = Path(source), Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file():
        old, new = destination.stat(), source.stat()
        if old.st_size == new.st_size and old.st_mtime_ns == new.st_mtime_ns:
            return str(destination)
    # Replacing the inode also preserves any already-running executable/library.
    fd, name = tempfile.mkstemp(prefix='.package-', dir=destination.parent)
    os.close(fd)
    try:
        shutil.copy2(source, name)
        os.replace(name, destination)
    finally:
        if os.path.exists(name):
            os.unlink(name)
    return str(destination)


def package(build, game, output, include_saves=False):
    build, game, output = build.resolve(), game.resolve(), output.resolve()
    # Reject overlapping trees before copying anything.
    if (output == ROOT or output == build or output in build.parents or
            output == game or game in output.parents or output in game.parents):
        raise ValueError('Output must be separate from the repository, build inputs and game tree')
    for name in BINARIES:
        if not (build / name).is_file():
            raise ValueError(f'Missing {build / name}; run tools/build_linux.sh first or use --build')
    if not any((game / name).is_file() for name in ('Default.xex', 'default.xex')):
        raise ValueError(f'Missing Default.xex in {game}')
    output.mkdir(parents=True, exist_ok=True)
    for name in BINARIES:
        copy_changed(build / name, output / 'bin' / name)
    copy_changed(ROOT / 'tools/run_linux.sh', output / 'tools/run_linux.sh')
    print('Copying game data (unchanged files are reused)...', flush=True)
    shutil.copytree(game, output / 'game', dirs_exist_ok=True, copy_function=copy_changed)
    launcher = output / 'Play.sh'
    launcher.write_text('''#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$here/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$here/tools/run_linux.sh" --build-dir "$here/bin" "$@"
''')
    launcher.chmod(0o755)
    (output / 'bin/tekken6').chmod(0o755)
    (output / 'tools/run_linux.sh').chmod(0o755)
    if include_saves and (ROOT / 'user').is_dir() and not (output / 'user').exists():
        shutil.copytree(ROOT / 'user', output / 'user', copy_function=copy_changed)
    (output / 'README.txt').write_text(
        'Tekken 6 — Linux\n\nRun ./Play.sh (or ./Play.sh --fullscreen).\n'
        'Move this entire folder together. Saves/cache stay inside it.\n'
        'Requires compatible Linux system libraries and a working Vulkan driver.\n'
        'Tested on Ubuntu 24.04-based Zorin OS 18.1.\n'
        'Scenario Campaign has been tested in one stage; some stages may run below 60 FPS.\n')
    manifest = {'version': (ROOT / 'VERSION.txt').read_text().strip(),
                'binary_sha256': {name: hashlib.sha256((output / 'bin' / name).read_bytes()).hexdigest()
                                  for name in BINARIES}}
    (output / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return launcher


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/linux-amd64')
    parser.add_argument('--game-root', type=Path, default=ROOT / 'game')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/Tekken6-Linux')
    parser.add_argument('--build', action='store_true', help='Build/rebuild before packaging')
    parser.add_argument('--include-saves', action='store_true',
                        help='Copy repository saves if the app folder has no user/ directory yet')
    args = parser.parse_args()
    try:
        if args.build:
            subprocess.run([str(ROOT / 'tools/build_linux.sh'), '--build-dir',
                            str(args.build_dir.resolve())], cwd=ROOT, check=True)
        launcher = package(args.build_dir, args.game_root, args.output, args.include_saves)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Packaging failed: {error}\n')
    print(f'Ready: {launcher}')


if __name__ == '__main__':
    main()
