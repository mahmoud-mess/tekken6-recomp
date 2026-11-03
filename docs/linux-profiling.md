# Linux Vulkan profiling

The baseline port is committed on `linux-port` at `0ddb8e7`. The optional
instrumentation is maintained in the SDK patch and does not change launcher
resolution, timing, or rendering defaults.

## Capture

Build with `tools/build_linux.sh`, then enable instrumentation for a single run:

```sh
TEKKEN6_GPU_PROFILE=/tmp/tekken6-gpu.csv tools/run_linux.sh --log
```

Without this environment variable the instrumentation creates no query pools,
collects no timers, and opens no profile file. Each enabled run overwrites its
chosen output file. Use the same characters, window size, stage position and
settings for comparisons, with gameplay active and the pause menu closed.
Avoid rebuilding while capturing: replacing mapped binaries prevents reliable
CPU symbol resolution.

```sh
python3 tools/analyze_gpu_profile.py /tmp/tekken6-gpu.csv \
  --start-ns START --end-ns END
python3 tools/tests/test_gpu_profile.py
```

START and END are `time.monotonic_ns()` timestamps around a steady gameplay
window. The analyzer can also summarize a whole run when no window is provided,
but that mixes startup, menus, loading, and gameplay.

## Records and limits

`F` rows contain a monotonic timestamp, frame ID, draw attempts, draw handling
wall time, uploaded bytes/chunks, copy commands, hoisted upload bytes, upload
handling wall time, render-pass endings, submissions, submission-fence wait
wall time, and queue acquisition/submission wall time. The CSV header lists units.
CPU-side draw timing includes uploads and other nested work: do not add these
columns together, and do not equate wall time with scheduled CPU execution time.
Render-pass endings are not necessarily render-pass breaks caused by uploads.

`G` rows associate a GPU timestamp span with its owning frame and submission.
The profiler timestamps the start and end of the complete recorded submission;
this includes rendering and copies, but excludes presenter/UI work on other
submissions and queueing before the start timestamp. It does not isolate
individual shaders, effects, or uploads. Results are read only after existing
submission fences complete, without VK_QUERY_RESULT_WAIT_BIT or new GPU waits.
Queue timestamp support and period are checked, timestamp wrap is masked, and
query pools follow command-buffer reuse and teardown. Unsupported queues retain
CPU counters. Query results may arrive later than their `F` row; analysis joins
by frame and excludes incomplete frames from GPU statistics.

Timestamp commands, timers and logging add overhead. Compare against the
uninstrumented baseline and retain raw data, settings and binary hashes.

## Initial measurements

Practice, RX 580 2048SP / Mesa RADV 25.2.8, Ryzen 5 5500, original internal
1024×576 and offscreen 1024×1024, Vulkan framebuffer, FIFO, 60 FPS swap cap.
These are single approximately 20-second samples, not repeated benchmarks.
They predate the [Campaign timing and memory-query fix](scenario-campaign.md):
that baseline used a 1000 Hz guest clock and the legacy maps parser. The current
launcher instead uses a 60 Hz guest clock, no extra swap cap, and direct mapping
queries on supported kernels. Capture again before comparing current performance
against these historical measurements.

| Per-frame metric (mean) | Tunnel Disaster | Lightning Storm |
|---|---:|---:|
| FPS | 60.00 | 53.95 |
| GPU submission spans | 6.48 ms | 6.15 ms |
| Draw handling wall time | 9.52 ms | 13.56 ms |
| Upload handling wall time (nested in draws) | 1.48 ms | 2.43 ms |
| Uploaded bytes | 12.43 MiB | 12.60 MiB |
| Draw attempts | 576.6 | 539.5 |
| Copy commands | 121.0 | 121.9 |
| Render-pass endings | 65.9 | 66.0 |
| Submissions | 1.62 | 2.22 |
| Measured blocking submission-fence wait | 0 ms | 0 ms |

All 1,200 Tunnel Disaster and 1,079 Lightning Storm frame records had complete
GPU timestamp results. Tunnel remained at its 60 FPS cap with instrumentation;
Lightning's uninstrumented comparison was 53.19 FPS. Its worst player-reported
45–48 FPS was not reproduced in this particular window.

The GPU spans are slightly shorter on the slower stage, while CPU-side draw
handling grows by about 4 ms/frame. Upload byte volume and pass counts are nearly
unchanged. This narrows the next investigation to CPU-side draw/upload handling
and serialization, rather than establishing a GPU fill-rate bottleneck. It does
not exclude GPU queueing, driver waits, memory bandwidth or stage-specific guest
work. Stack profiles identify shared-memory upload/memcpy and descriptor updates;
these also occur on the faster stage and are not yet proven causes.

Follow-up Campaign profiling identified expensive memory-protection queries
under a shared runtime lock. The direct-query path and corrected guest clock
restored the reproduced Campaign stage and improved the player's Practice stage
checks. Repeat these captures with the current settings before investigating
remaining draw/upload latency. Forest/water and snow still need matching
instrumented captures.

Raw local captures and additional CPU profiles are in `.local-tests/profiling/`.
They are ignored by Git. The baseline comparison is in that directory's
`REPORT.md`; this instrumented comparison is in `GPU-REPORT.md`.
