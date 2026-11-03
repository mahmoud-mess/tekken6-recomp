#!/usr/bin/env python3
"""Summarize TEKKEN6_GPU_PROFILE CSV without adding waits to the game.

Select a steady gameplay window with --start-ns / --end-ns (monotonic ns).
CPU draw/upload timings are nested and must not be added together. GPU spans
cover complete submissions, not individual shaders; they exclude queueing
before the first timestamp. Only frames with all submission results are used
for GPU statistics. Requires no third-party Python packages.
"""
import argparse
import csv
import json
import statistics
from pathlib import Path

FIELDS = ('time_ns', 'frame', 'draw_attempts', 'draw_cpu_ns', 'upload_bytes',
          'upload_ranges', 'copy_commands', 'hoisted_bytes', 'upload_cpu_ns',
          'pass_ends', 'submits', 'fence_wait_ns', 'queue_submit_cpu_ns')


def summarize(path, start=0, end=2**64-1):
    frames, gpu = [], {}
    with Path(path).open() as stream:
        for row in csv.reader(stream):
            if not row or row[0].startswith('#'):
                continue
            if row[0] == 'F' and len(row) == len(FIELDS)+1:
                frame = dict(zip(FIELDS, map(int, row[1:])))
                if start <= frame['time_ns'] <= end:
                    frames.append(frame)
            elif row[0] == 'G' and len(row) == 5:
                gpu.setdefault(int(row[2]), {})[int(row[3])] = float(row[4])
    if not frames:
        raise ValueError('No frame records in the selected window')
    def stats(values):
        values = sorted(values)
        return {'mean': statistics.mean(values), 'median': statistics.median(values),
                'p95': values[int(.95*(len(values)-1))]}
    result = {'frames': len(frames), 'start_ns': frames[0]['time_ns'],
              'end_ns': frames[-1]['time_ns'], 'per_frame': {}}
    if len(frames) > 1:
        gaps = [(b['time_ns']-a['time_ns'])/1e6 for a, b in zip(frames, frames[1:])]
        result['frame_interval_ms'] = stats(gaps)
        result['mean_fps'] = 1000/statistics.mean(gaps)
    for key in FIELDS[2:]:
        result['per_frame'][key] = stats([f[key] for f in frames])
    complete = [sum(gpu[f['frame']].values())/1e6 for f in frames
                if f['frame'] in gpu and len(gpu[f['frame']]) == f['submits']]
    result['gpu_complete_frames'] = len(complete)
    if complete:
        result['gpu_submission_spans_ms'] = stats(complete)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--start-ns', type=int, default=0)
    parser.add_argument('--end-ns', type=int, default=2**64-1)
    args = parser.parse_args()
    try:
        result = summarize(args.csv, args.start_ns, args.end_ns)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
