# Tekken 6 Recomp

Static recompiler project for the Xbox 360 release of Tekken 6. Game data and
generated code come from a user-provided XEX and are kept out of version control.

## Windows build

For a Linux-hosted Windows build, use an extracted LLVM-MinGW distribution.
Initialize the SDK submodule and apply the repository patch before configuring
the native SDK. Generate guest code from your own game XEX, then configure the
Windows build:

```sh
git submodule update --init --recursive
python3 tools/apply_rexglue_patches.py

cmake -S third_party/rexglue-sdk -B build/sdk-native -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-mssse3 \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
cmake --build build/sdk-native --target rexglue --parallel 3
third_party/rexglue-sdk/out/linux-amd64/rexglue codegen tekken6_manifest.toml

cmake -S . -B build/windows-llvm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/windows-clang.cmake \
  -DLLVM_MINGW_ROOT=/path/to/extracted/llvm-mingw \
  -DCMAKE_BUILD_TYPE=Release -DTEKKEN6_USE_PREGENERATED_CODE=ON \
  -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF \
  -DREXGLUE_RECOMP_DEBUG_INFO=none
cmake --build build/windows-llvm --target tekken6 tekken6_low_overhead \
  tekken6_late_depth tekken6_fused_resolve tekken6_vulkan \
  tekken6_vulkan_fbo --parallel 2

python3 tools/package_windows.py --build-dir build/windows-llvm \
  --game-root /path/to/game --output /path/to/windows-package \
  --llvm-mingw-root /path/to/extracted/llvm-mingw
```

The package tool checks PE imports, stages required compiler runtime DLLs, and
writes a `build-info.json` manifest with source revisions and binary hashes.
SDK changes are maintained in the [project patch](patches/rexglue/README.md).
CMake defaults to `Release` when no build type is selected. Generated guest
code uses FMA3 by default for faster explicit fused operations; the resulting
executable requires an FMA3-capable CPU. Set `-DTEKKEN6_ENABLE_FMA3=OFF` for a
portable build.

## macOS build (Apple Silicon or Intel)

macOS is a supported ReXGlue host platform: the Xenos GPU runs through the
SDK's Vulkan backend on a pinned MoltenVK, windowing goes through SDL3/Cocoa,
and the whole toolchain builds natively, so there is no cross-compiler step.
The host-framebuffer (FBO) render path is the default on macOS: it renders
fights correctly on Metal. The fragment shader interlock (FSI) path runs faster
but shows stale EDRAM bands in resolved textures (floor reflections, menu
blur) under MoltenVK, so it is opt-in. There is no D3D12 on macOS.

Requirements: Xcode command line tools (AppleClang 16+), `brew install cmake ninja`.

```sh
tools/build_mac.sh --iso /path/to/your/Tekken6.iso   # or --game /path/to/extracted
tools/run_mac.sh                                      # host framebuffers (default)
tools/run_mac.sh --fsi                                # interlock path (faster, stale bands)
tools/run_mac.sh --res 768x432 --offscreen 512        # lighter internal targets
```

The script extracts the ISO into `game/`, builds the SDK and its codegen tool,
generates guest code from your XEX, and links `build/mac-<arch>/tekken6` with
`librexruntime`, the `librexgpu-xenos` plugin and a `vulkan/` folder holding the
pinned Vulkan loader and MoltenVK next to it. Saves go to `user/`, shader
caches to `cache/<path>/`. `tools/run_mac.sh --log` keeps logging on; pass host
flags after `--`. FMA3 and SSSE3 flags only apply on x86-64 hosts; on arm64 the
generated code's fused operations lower to NEON directly.

Three SDK bugs fixed in the carried patch were needed on a POSIX host: a lost
wakeup in the signal-and-wait/auto-reset event emulation (the game hung on its
first frame fence), a manual-reset SetEvent that did not release threads already
waiting when a ResetEvent followed at once (the game's mutex-plus-event
condition variables notify that way; Practice mode hung at character select
two times in three), and the SPIR-V translator reading the texture-fetch
exponent bias from the wrong fetch-constant word (every post-processed scene
saturated to white; rexglue-sdk issue 456). After changing anything in the
shader translator, delete `cache/` so stale translated shaders are not reused.

One rendering bug needed no SDK change at all. Tekken clears depth every frame
with unclipped draws under an 8192-high scissor; with the SDK's default
`execute_unclipped_draw_vs_on_cpu=false` (Xenia's default is true, see
rexglue-sdk PR 513) the render target cache sizes those claims from the
scissor, so a tiny target at the end of EDRAM claims all of it, wraps past
tile 2047 and takes over the scene colour buffer at tile 0. The contents then
round-trip through a 4x MSAA depth target and come back black: Practice mode
showed only the HUD, and fights flashed black between attacks. `run_mac.sh`
passes `--execute_unclipped_draw_vs_on_cpu=true`, which sizes the claims from
the actual vertices, and the scene is intact on both paths.

For hangs: run with `REX_DEBUG_DUMP_TRIGGER=/some/path` in the environment and
create that file while the game is stuck. The runtime prints every event,
mutex and semaphore with its host state and what each guest thread is waiting
on, to stderr and the log.

Rendering notes specific to MoltenVK: Metal cannot disable primitive restart
for strip topologies and MoltenVK fails pipeline creation when asked to, which
silently drops those draws (a black 3D scene behind a working HUD). The carried
patch forces restart on for strip pipelines on Apple, in both the guest pipeline
cache and the presentation pipeline. Keep the integer ownership-transfer formats:
without them the SDK decides transfers cannot be bit-exact and switches to the
FSI path on its own. The `window_high_pixel_density` cvar can be set to `false`
to present at 1x on a Retina display. The first run with an empty `cache/` looks
frozen or black for 10-20 s while shaders compile.

Performance on Apple Silicon is limited by render pass count, not pixels:
lowering the internal resolution changes nothing. Every Metal render pass on
a tile-based GPU loads and stores its attachments, and the SDK used to end the
open pass for each upload of guest memory that changed since the last draw
(vertex data, mostly), then restart it. In a fight that cut the shadow-map
pass into about 25 pieces and the scene pass into about 45, roughly 125 render
passes a frame. The carried patch records such an upload ahead of the open
pass instead whenever no draw already in that pass reads the uploaded pages,
which leaves about 56 passes a frame. `--vulkan_hoist_uploads=false` restores
the old behaviour for comparison.

Measured on an M1 Max with the game's own count of frames that reach the
screen (the F3 overlay counts frames drawn, which overstates it), 90 s of
random input in Practice on City After Dark and in an Arcade fight, one run
each: about 21 frames shown per second before, with as many drawn and
dropped, and about 60 after. A Metal System Trace with the change puts the GPU
at about 10.5 ms of the 16.7 ms frame in that Practice fight. Heavier moments
still come close to the budget, so an occasional dropped frame remains. An M5
Max runs fights at a locked 60 either way. Measure with the screen unlocked,
nothing else heavy running, and the full-power charger: an M1 Max on a small
charger drains and throttles under this load.

The Intel build compiles and starts, but on an AMD Radeon Pro 580 (macOS 13)
the GPU hangs about half a minute in, at the main menu, with or without the
changes above; that is still open.

## Portable ISO builder

The release can include a self-contained `Tekken 6 Builder.exe` beside its
`builder` folder. Players extract the complete release, choose their own ISO
with the Windows file picker, select one or more renderer checkboxes, and pick
an output folder. The D3D12 ROV option is checked by default and is labeled as
the widest-compatibility choice. No compiler installation or network access is
needed on the player's PC. The ISO and its game files are never included in the
builder release.

To assemble that release folder, first build the Windows GUI and XDVDFS helper,
then provide LLVM-MinGW 20260908 MSVCRT x64 (Clang 23.1.1), portable CMake,
Ninja, and the Python embeddable package. The bundled compiler's fixed
capabilities are supplied by `cmake/windows-portable-capabilities.cmake` so
players do not repeat the compiler feature probes.

```sh
cmake --build build/windows-llvm --target tekken6_builder xdvdfs_tool --parallel "$(nproc)"
python3 tools/package_portable_builder.py \
  --builder-exe build/windows-llvm/tekken6_builder.exe \
  --llvm-mingw-root /path/to/llvm-mingw \
  --cmake-root /path/to/cmake \
  --ninja /path/to/ninja.exe \
  --python-root /path/to/python-embeddable \
  --xdvdfs-tool build/windows-llvm/tools/xdvdfs/xdvdfs_tool.exe \
  --output build/Tekken6-Builder-Windows
```

Release versions are set in `VERSION.txt`; the builder window, Windows file details,
and package manifests use that version. For 0.9.4, archive the complete
`build/Tekken6-Builder-Windows` folder as `Tekken6-Builder-Windows-0.9.4.zip`.
Keep `Tekken 6 Builder.exe` and its `builder` folder together. The folder contains
the pinned project sources and build tools; the game itself is extracted from the player's
ISO when they run the builder. Project sources stay in one archive to reduce the
number of files in the initial unzip; the builder unpacks them automatically
when the first build starts.

Game compilation uses the available CPU threads, limited by available memory.
After a successful build, temporary sources, extracted game copies, and compiler
outputs are deleted automatically. The activity indicator keeps animating while
the build runs, and the Build button stays disabled after completion.

Press **F3** in game to show or hide the FPS and frame-time overlay. It reports
guest frame swaps independently of the host's overlay or presentation rate.
Routine logs are disabled in the Windows launcher. Unhandled crashes write
`crash.txt` and `crash.dmp` under `crashes/<run>/`.

The startup chooser offers internal 3D resolutions from 640x360 through the
original 1024x576. It also enables a 512x512 size for the 1024x1024 offscreen
target by default; this can soften shadows. Uncheck the option to keep the
original offscreen size.

## Rendering paths

`tekken6.exe` selects D3D12 ROV. The runtime includes no-blend shader
specialization, ROV sample-count variants, and direct RGBA8 packing for known
targets. Unsupported states retain the general shader path.
Late-depth and fused RGBA8 resolves are available through their optional
launchers.

`tekken6_vulkan.exe` selects Vulkan fragment shader interlock (FSI). It requires
a Vulkan device and driver that expose fragment shader interlock. The explicit
request fails if the extension is unavailable. `tekken6_vulkan_fbo.exe` selects
the experimental framebuffer path, which does not require interlock.

The launcher keeps user saves, shader caches, and crash reports in the package
directory. Game files and generated recompilation sources are local inputs and
must not be committed.
