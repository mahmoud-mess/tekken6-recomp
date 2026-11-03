"""Run with python3 tools/tests/test_gpu_profile.py."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'gpu_profile', Path(__file__).resolve().parents[1] / 'analyze_gpu_profile.py')
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.csv = Path(self.temp.name) / 'profile.csv'
        self.csv.write_text(
            '# schema comment\n'
            'F,1000000,1,2,3,4,5,6,7,8,9,2,11,12\n'
            'G,9,1,1,1000000\nG,10,1,2,2000000\n'
            'F,2000000,2,2,3,4,5,6,7,8,9,2,11,12\n'
            'G,11,2,3,500000\n')

    def test_gpu_submissions_aggregate_by_owner_frame(self):
        result = profile.summarize(self.csv)
        self.assertEqual(result['frames'], 2)
        self.assertEqual(result['gpu_complete_frames'], 1)
        self.assertEqual(result['gpu_submission_spans_ms']['mean'], 3)
        self.assertEqual(result['mean_fps'], 1000)

    def test_window_filters_frames_and_excludes_incomplete_gpu_data(self):
        result = profile.summarize(self.csv, 1500000, 2500000)
        self.assertEqual(result['frames'], 1)
        self.assertEqual(result['gpu_complete_frames'], 0)
        self.assertNotIn('gpu_submission_spans_ms', result)
        self.assertNotIn('mean_fps', result)

    def test_cpu_counters_work_without_timestamp_support(self):
        self.csv.write_text('F,1000000,1,2,3,4,5,6,7,8,9,2,11,12\n')
        result = profile.summarize(self.csv)
        self.assertEqual(result['per_frame']['upload_bytes']['mean'], 4)
        self.assertEqual(result['gpu_complete_frames'], 0)

    def test_empty_window_is_reported(self):
        with self.assertRaises(ValueError):
            profile.summarize(self.csv, 3000000, 4000000)


if __name__ == '__main__':
    unittest.main()
