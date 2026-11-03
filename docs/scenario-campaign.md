# Scenario Campaign: Linux timing and memory-query fix

The initial Linux launch configuration used `vsync=false` with a separate
60 FPS swap cap. The SDK then generated guest vblank interrupts at 1000 Hz.
Scenario Campaign displayed characters/HUD over a black stage and ran at
roughly 0.1 FPS in the reproduced case.

Thread snapshots ruled out a persistent global-lock deadlock. Locks were
contended during guest write faults, but subsequent snapshots showed a free
lock, an executing guest main thread, and an idle graphics worker. Disabling
async shader compilation or occlusion queries did not recover the scenery.

A fresh launch with a 60 Hz guest clock restored stage scenery but still ran
around 30–34 FPS and felt slow. Removing the extra swap limiter alone did not
fix that. Raising guest vblank to 120 Hz improved gameplay throughput but sped
up cutscenes, even with swaps capped at 60. That is not a suitable default.

CPU profiles also showed `/proc/self/maps` parsing in guest memory fault paths.
Those queries run under a shared runtime lock. `PROCMAP_QUERY`, introduced in
Linux 6.11, queries a single mapping without formatting/parsing the entire map
list. The SDK patch adds an optional path using the kernel's stable 104-byte
UAPI structure, so newer kernels can benefit when built with older headers.
No mapping permissions are cached. Each query opens its own descriptor to avoid
inheriting a parent-process maps descriptor across fork. Unsupported ioctls and
other failures fall back to the legacy parser; an unmapped address remains an
unmapped-address failure.

The protection query benchmark over 512 alternating RO/RW mappings passed on
both paths: RO/RW/no-access, live mprotect changes, unmapped addresses, and fork
isolation. 2,000 queries took approximately 549 ms via the parser and 11 ms via
the ioctl on the test machine. This is a utility benchmark, not an FPS estimate.

## Tested configuration

The user confirmed the same Campaign stage had visible scenery, gameplay near
60 FPS, and normal cutscene speed with:

- `vsync=false` (fast GPU wait polling)
- `guest_vblank_hz=60` (correctly bounded guest clock)
- `guest_frame_limit=0` (no additional swap throttling)
- `REX_LINUX_FAST_MAP_QUERY=1`
- Vulkan framebuffer path with FIFO presentation

The subsequent Practice regression check also stayed near 60 FPS at normal
speed. The user reported steady 60 FPS even on the storm stage and Mystical
Forest, which had previously slowed down. These are player observations;
matching instrumented captures have not yet been repeated with the new settings.

`tools/run_linux.sh` now supplies these settings by default; standalone packages
use the same launcher. Tests were on Zorin OS 18.1, Linux 7.0, RX 580 2048SP,
Mesa RADV 25.2.8. This confirms one reproduced Campaign stage, not the whole
story, every cutscene, every GPU, or legacy-kernel performance. The original
console Campaign FPS target has not been independently established here.

For comparison, `--legacy-timing` restores SDK timing behavior, while
`REX_LINUX_LEGACY_MAP_QUERY=1 tools/run_linux.sh` forces the original map parser.
Older kernels automatically retain that parser and may retain its performance
cost. Rebuild/repackage after updating the scripts and SDK patch:

```sh
tools/build_linux.sh
python3 tools/package_linux.py
```

Regression checks:

```sh
cmake --build build/linux-amd64 --target tekken6_linux_memory_test tekken6_linux_wait_test
build/linux-amd64/tekken6_linux_memory_test
REX_LINUX_FAST_MAP_QUERY=1 build/linux-amd64/tekken6_linux_memory_test
build/linux-amd64/tekken6_linux_wait_test
python3 tools/tests/test_linux_launcher.py
python3 tools/tests/test_gpu_profile.py
```

Raw diagnostics are local under `.local-tests/campaign/`, excluded from Git.
