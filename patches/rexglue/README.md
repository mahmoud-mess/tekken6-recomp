# Tekken 6 SDK patch

The SDK submodule uses the public upstream tag `nightly-20260904-0c7b01a0`
(`0c7b01a0ac0479801757507d80533f662fa0815d`).
`0001-tekken6-runtime.patch` contains the title's runtime compatibility,
rendering, ROV specialization and crash-report support. Linux additions preserve
sub-millisecond POSIX wait durations (avoiding busy polling) and add the optional
`guest_frame_limit` setting. The Linux launcher uses fast GPU wait polling with
an independent 60 Hz guest vblank clock. Linux memory protection queries
optionally use PROCMAP_QUERY on
supported kernels, with a legacy parser fallback. These changes were tested in
Scenario Campaign to restore scenery, near-60 FPS gameplay, and normal cutscenes.
Linux guest name/sign-in queries use the system account username; the profile's
storage name and XUID stay unchanged so existing saves remain accessible.

Presentation startup logs identify the SDL native surface, unavailable drawable
sizes, unusable swapchain extents and missing presentation queues. SDL show/expose
events retry transient connection failures even if the pixel size is unchanged.
For a blank Linux window, rebuild with `tools/build_linux.sh --jobs 6` (reuse the
existing extracted game), then run `tools/run_linux.sh --fullscreen --log`.
Keep the startup log through the first swapchain message or connection failure.
To compare the X11/XWayland path, run
`tools/run_linux.sh --fullscreen --log -- --video_driver=x11`.

From the project root:

```sh
git submodule update --init --recursive
python3 tools/apply_rexglue_patches.py
```

SDK source edits are maintained in the patch. After editing the SDK, save them
before committing the parent repository:

```sh
# For any new SDK source files:
git -C third_party/rexglue-sdk add -N path/to/new/source.cpp
python3 tools/apply_rexglue_patches.py --refresh
```

Keep the submodule revision pinned to the upstream tag. Its applied working-tree
changes are excluded from the parent repository's status; the patch is the
versioned source of those changes. The helper checks the base revision and
refuses to overwrite an SDK with conflicting edits.

Optional Vulkan counters and asynchronous submission timestamps are enabled by
`TEKKEN6_GPU_PROFILE`; see [Linux progress and runtime options](../../README.md#linux-progress-and-runtime-options).
