#!/usr/bin/env python3
"""Build locally from the user's ISO; distributed bundles contain tools only."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

XEX_SHA256 = 'ee138a4fe8d32a0edc131451377f4377041a49626153fa992db24f5420a7f15c'


def run(args, *, cwd=None, log=None):
    print('> ' + ' '.join(map(str, args)), flush=True)
    process = subprocess.Popen(list(map(str, args)), cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, errors='replace')
    for line in process.stdout:
        print(line.rstrip(), flush=True)
        if log:
            log.write(line)
            log.flush()
    if process.wait():
        raise RuntimeError(f'Command failed ({process.returncode}): {args[0]}')


def cpu_has_fma():
    return 'fma' in Path('/proc/cpuinfo').read_text().split()


def inspect_iso(extractor, iso):
    print('STEP Checking your ISO (no game compilation)', flush=True)
    with tempfile.TemporaryDirectory(prefix='tekken6-iso-check-') as temporary:
        root = Path(temporary)
        run([extractor, 'list', iso, root / 'files.txt'])
        names = [line.split('\t', 1)[1].strip().replace('\\', '/')
                 for line in (root / 'files.txt').read_text().splitlines() if '\t' in line]
        xex = next((name for name in names if name.lower() == 'default.xex'), None)
        if not xex:
            raise RuntimeError('The ISO has no Default.xex at its root.')
        run([extractor, 'extract', iso, xex, root / 'Default.xex'])
        if hashlib.sha256((root / 'Default.xex').read_bytes()).hexdigest() != XEX_SHA256:
            raise RuntimeError('Unsupported disc. Use Tekken 6 Xbox 360 Europe/USA Rev 1.')
    print('Supported Tekken 6 Europe/USA Rev 1 ISO verified.', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path)
    parser.add_argument('--output', type=Path, default=Path.home() / 'Games/Tekken6-Linux')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--fma', choices=('auto', 'on', 'off'), default='auto')
    parser.add_argument('--check', action='store_true', help='Check tools and ISO without building')
    parser.add_argument('--builder-root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    if not args.iso:
        if not sys.stdin.isatty():
            parser.error('Select your ISO in the builder, or pass --iso FILE.')
        args.iso = Path(input('Path to your Tekken 6 Xbox 360 ISO: ').strip())
    iso = args.iso.expanduser().resolve()
    output = args.output.expanduser().resolve()
    builder = args.builder_root.resolve()
    if not iso.is_file():
        raise RuntimeError(f'ISO not found: {iso}')
    if not 1 <= args.jobs <= 128:
        raise RuntimeError('Parallel jobs must be between 1 and 128.')
    fma = cpu_has_fma() if args.fma == 'auto' else args.fma == 'on'
    if fma and not cpu_has_fma():
        raise RuntimeError('This CPU does not support FMA3. Turn the FMA3 option off.')
    missing = [tool for tool in ('git', 'cmake', 'ninja', 'clang-19', 'clang++-19',
                                 'nasm', 'pkg-config') if not shutil.which(tool)]
    if missing:
        raise RuntimeError('Missing build tools: ' + ', '.join(missing) +
                           '. Install the dependencies listed in README.md.')
    cmake_version = subprocess.check_output(['cmake', '--version'], text=True)
    version = re.search(r'cmake version (\d+)\.(\d+)', cmake_version)
    if not version or tuple(map(int, version.groups())) < (3, 25):
        raise RuntimeError('CMake 3.25 or newer is required.')
    extractor = builder / 'bin/xdvdfs_tool'
    if not extractor.is_file():
        raise RuntimeError(f'Missing ISO tool: {extractor}. Keep the builder folder together.')
    inspect_iso(extractor, iso)
    print(f'Options: {args.jobs} jobs; FMA3 {"on" if fma else "off"}; output {output}', flush=True)
    if args.check:
        print('STEP Checks passed. No game assets were extracted into the output and no game was built.', flush=True)
        return
    metadata = json.loads((builder / 'build-info.json').read_text())
    revision, repository = metadata['source_revision'], metadata['source_repository']
    if output == Path('/') or output == Path.home() or builder == output or builder in output.parents:
        raise RuntimeError('Choose a separate game output folder, outside the builder.')
    if output.exists() and any(output.iterdir()) and not (output / 'build-info.json').is_file():
        raise RuntimeError('Output must be empty or a previously generated Tekken 6 app folder.')
    identity = hashlib.sha256(str(output).encode()).hexdigest()[:12]
    cache = Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache'))
    if not cache.is_absolute():
        cache = Path.home() / '.cache'
    work = cache / 'tekken6-builder' / (identity + '-' + revision[:12])
    work.mkdir(parents=True, exist_ok=True)
    project = work / 'project'
    print(f'Build log: {work / "build.log"}', flush=True)
    with (work / 'build.log').open('a') as log:
        print('STEP Fetching the pinned source and SDK dependencies', flush=True)
        if not project.exists():
            run(['git', 'clone', '--no-checkout', repository, project], log=log)
        run(['git', '-C', project, 'fetch', 'origin', revision], log=log)
        run(['git', '-C', project, 'checkout', '--detach', revision], log=log)
        run(['git', '-C', project, 'submodule', 'update', '--init', '--recursive'], log=log)
        game = project / 'game'
        stamp = work / 'game-source.json'
        source_id = {'size': iso.stat().st_size, 'mtime_ns': iso.stat().st_mtime_ns,
                     'path': str(iso), 'xex_sha256': XEX_SHA256}
        if game.exists():
            if not stamp.is_file() or json.loads(stamp.read_text()) != source_id:
                raise RuntimeError('Workspace contains different game data. Select a new output folder.')
        else:
            print('STEP Extracting your game data locally', flush=True)
            temporary_game = project / 'game.extracting'
            # This staging directory belongs exclusively to the builder.
            if temporary_game.exists():
                shutil.rmtree(temporary_game)
            run([extractor, 'extract-all', iso, temporary_game], log=log)
            stamp.write_text(json.dumps(source_id))
            temporary_game.rename(game)
        print('STEP Building the native Linux game (this can take several minutes)', flush=True)
        run(['bash', project / 'tools/build_linux.sh', '--jobs', args.jobs,
             '--fma', 'on' if fma else 'off'], cwd=project, log=log)
        print('STEP Creating your local app folder', flush=True)
        run([sys.executable, project / 'tools/package_linux.py', '--output', output],
            cwd=project, log=log)
    print(f'STEP Complete. Run {output / "Play.sh"}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f'ERROR: {error}', file=sys.stderr, flush=True)
        sys.exit(1)
