"""Run with python3 tools/tests/test_linux_launcher.py (no game data needed)."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

LAUNCHER = Path(__file__).resolve().parents[1] / 'run_linux.sh'


class LauncherTests(unittest.TestCase):
    def test_default_and_legacy_timing(self):
        with tempfile.TemporaryDirectory(prefix='tekken launcher ') as directory:
            root = Path(directory)
            game = root / 'game data'
            game.mkdir()
            (game / 'Default.xex').write_bytes(b'test fixture')
            host = root / 'tekken6'
            host.write_text('#!/usr/bin/env python3\nimport sys,os,json\n'
                            'print(json.dumps({"args":sys.argv[1:],"fast":os.getenv("REX_LINUX_FAST_MAP_QUERY")}))\n')
            host.chmod(0o755)
            for legacy in (False, True):
                args = [str(LAUNCHER), '--build-dir', str(root), '--game', str(game)]
                if legacy:
                    args.append('--legacy-timing')
                args += ['--', '--custom=value with spaces']
                result = subprocess.run(args, check=True, text=True, capture_output=True, cwd='/tmp')
                value = json.loads(result.stdout)
                self.assertEqual(value['fast'], '1')
                for flag in ('--guest_vblank_hz='+('0' if legacy else '60'),
                             '--guest_frame_limit=0', '--vsync='+str(legacy).lower(),
                             '--vulkan_allow_present_mode_immediate='+str(legacy).lower(),
                             '--custom=value with spaces'):
                    self.assertEqual(value['args'].count(flag), 1)


if __name__ == '__main__':
    unittest.main()
