#!/usr/bin/env python3
"""Package only the Linux builder, ISO tool, scripts and notices for release."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/linux-amd64')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--repository', help='HTTPS source repository; defaults to fork/origin remote')
    args = parser.parse_args()
    version = (ROOT / 'VERSION.txt').read_text().strip()
    name = f'Tekken6-Builder-Linux-{version}'
    output = (args.output or ROOT / f'build/{name}.tar.xz').resolve()
    build = args.build_dir.resolve()
    binaries = {'tekken6_builder': build / 'tekken6_builder',
                'xdvdfs_tool': build / 'tools/xdvdfs/xdvdfs_tool'}
    for path in binaries.values():
        if not path.is_file():
            parser.error(f'Missing tool: {path}. Build targets tekken6_builder and xdvdfs_tool first.')
    repository = args.repository
    if not repository:
        remotes = git('remote').splitlines()
        repository = git('remote', 'get-url', 'fork' if 'fork' in remotes else 'origin')
        if repository.startswith('git@github.com:'):
            repository = 'https://github.com/' + repository.split(':', 1)[1]
    if not re.fullmatch(r'https://github\.com/[\w.-]+/[\w.-]+(?:\.git)?', repository):
        parser.error('--repository must be a public HTTPS GitHub repository URL without credentials.')
    revision = git('rev-parse', 'HEAD')
    epoch = int(git('show', '-s', '--format=%ct', 'HEAD'))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='tekken6-builder-package-') as temporary:
        package = Path(temporary) / name
        (package / 'bin').mkdir(parents=True)
        (package / 'builder').mkdir()
        (package / 'licenses').mkdir()
        for filename, source in binaries.items():
            shutil.copy2(source, package / 'bin' / filename)
            (package / 'bin' / filename).chmod(0o755)
        shutil.copy2(ROOT / 'tools/build_from_iso_linux.py', package / 'builder/build_from_iso_linux.py')
        shutil.copy2(ROOT / 'README.md', package / 'README.md')
        shutil.copy2(ROOT / 'third_party/rexglue-sdk/thirdparty/sdl3/LICENSE.txt',
                     package / 'licenses/SDL3.txt')
        (package / 'Build.sh').write_text('''#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if (($#)); then
  exec python3 "$here/builder/build_from_iso_linux.py" --builder-root "$here" "$@"
fi
exec "$here/bin/tekken6_builder"
''')
        (package / 'Build.sh').chmod(0o755)
        hashes = {filename: hashlib.sha256((package / 'bin' / filename).read_bytes()).hexdigest()
                  for filename in binaries}
        (package / 'build-info.json').write_text(json.dumps({
            'version': version, 'source_repository': repository,
            'source_revision': revision, 'binary_sha256': hashes,
            'contains_game_data': False, 'contains_game_executable': False,
        }, indent=2) + '\n')
        (package / 'START-HERE.txt').write_text(
            'TEKKEN 6 LINUX ISO BUILDER\n\n'
            'Run ./Build.sh, select your own supported Xbox 360 ISO and an output folder,\n'
            'then choose Check setup or Build game. Build options are optional.\n'
            'No game assets, generated guest code, game executables or saves are included.\n'
            'Requires Internet to fetch the pinned source and SDK dependencies, Python 3,\n'
            'Clang 19, CMake 3.25+, Ninja, Git and the development libraries in README.md.\n'
            'The GUI uses SDL3; a desktop file-dialog service such as Zenity is needed.\n'
            'The prebuilt tools require x86-64 Linux and compatible system libraries.\n'
            'Allow 25 GB beyond your ISO. Sources and intermediate build files are cached\n'
            'under $XDG_CACHE_HOME/tekken6-builder (default ~/.cache/tekken6-builder).\n\n'
            'Terminal mode: ./Build.sh --iso /path/to/Tekken6.iso --output /path/to/game\n'
            'Check only: ./Build.sh --iso /path/to/Tekken6.iso --check\n'
            'Options: --jobs 2 --fma auto|on|off. Run ./Play.sh in your generated app folder.\n')

        def metadata(entry):
            entry.uid = entry.gid = 0
            entry.uname = entry.gname = ''
            entry.mtime = epoch
            return entry

        # Stage an explicit allowlist only; never traverse the local app/game tree.
        # Atomic replacement also keeps a previous release archive intact on failure.
        fd, pending = tempfile.mkstemp(prefix='.builder-', dir=output.parent)
        os.close(fd)
        try:
            with tarfile.open(pending, 'w:xz') as archive:
                archive.add(package, arcname=name, filter=metadata)
            os.replace(pending, output)
            output.chmod(0o644)
        finally:
            if os.path.exists(pending):
                os.unlink(pending)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_name(output.name + '.sha256').write_text(f'{checksum}  {output.name}\n')
    print(f'Builder only: {output}')
    print(f'SHA-256: {checksum}')


if __name__ == '__main__':
    main()
