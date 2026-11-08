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

## Linux (native x86-64, Vulkan)

The Linux port uses the same runtime and title compatibility patches as macOS.
You need your own Xbox 360 Tekken 6 Europe/USA Rev 1 ISO, a Vulkan-capable
GPU/driver, and about 25 GB of free space beyond the ISO for sources, build
files and the locally generated app. Release packages contain builder tools
only: no ISO, game assets, generated guest code, game executable or saves.

### Dependencies

On Ubuntu 24.04 or a compatible distribution:

```sh
sudo apt-get install clang-19 lld-19 cmake ninja-build python3 git pkg-config \
  libasound2-dev libpulse-dev libx11-xcb-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libwayland-dev libdecor-0-dev libudev-dev libdrm-dev libgbm-dev \
  libegl1-mesa-dev nasm zenity
```

Clang 19 and C++23 `std::expected` support are required. The SDL builder uses
native file dialogs; Zenity supplies them on desktops without a dialog portal.
Building the game also needs Internet access to fetch the pinned source and
SDK submodules. The prebuilt builder requires compatible x86-64 Linux system
libraries; it is not a complete distribution-independent runtime.

### ISO builder

Unpack a Linux **builder** archive and run `./Build.sh`. The SDL interface asks
for your own ISO and an output folder. **Check setup** checks the build tools
and supported game version without compiling. **Build game** fetches the pinned
source and dependencies, extracts your ISO locally, compiles the game, and
creates your personal app folder. **Build options** lets you choose the number
of compiler jobs and enable or disable FMA3. Defaults are two jobs and FMA3
only when supported by the current CPU. Turning FMA3 off permits use on CPUs
without that extension.

The same builder supports terminal use:

```sh
./Build.sh --iso /path/to/Tekken6.iso --check
./Build.sh --iso /path/to/Tekken6.iso --output /path/to/Tekken6-Linux --jobs 2 --fma auto
```

Sources, extracted data, intermediate files and build logs stay under
`$XDG_CACHE_HOME/tekken6-builder` (normally `~/.cache/tekken6-builder`). They are
kept to allow later builds to reuse completed work. The output app folder
contains `Play.sh`; run it after a successful local build. Saves and shader
caches stay inside that folder. Keep your generated game files private; the
release packager below distributes only the builder tools.

To create a builder archive from source, initialize the SDK and configure CMake:

```sh
git submodule update --init --recursive
python3 tools/apply_rexglue_patches.py
cmake -S . -B build/linux-amd64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_CXX_FLAGS=-mssse3 -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
cmake --build build/linux-amd64 --target tekken6_builder xdvdfs_tool --parallel 2
python3 tools/package_linux_builder.py
```

This creates `build/Tekken6-Builder-Linux-<version>.tar.xz` and its SHA-256
checksum. It includes the SDL builder, ISO inspection/extraction tool, Python
driver, README, source revision manifest and SDL license. It never reads the
local `game/`, `generated/default/`, app folders, saves or caches. Only the two
tool targets are needed to prepare a release; no game is compiled or packaged.
The source commit recorded in the manifest must be pushed to the selected
repository before distributing the builder. `--repository URL` selects another
public GitHub source repository; the default is the `fork` remote, or `origin`.

### Building directly from a checkout

```sh
tools/build_linux.sh --iso /path/to/Tekken6.iso --jobs 2
tools/run_linux.sh --log
```

Without `--iso`, the script asks for an ISO when no extracted game is present.
Extraction requires an empty or absent `game/` directory. Subsequent builds use
`tools/build_linux.sh` without `--iso`. `--game DIR` can link an existing game
folder on the first build; `--fma on|off` controls FMA3. `--sdk-only` builds the
SDK, rendering plugin and extractor without game data. A personal app folder
can be generated after a local game build with `python3 tools/package_linux.py`;
this app folder is not a release artifact.

### Linux progress and runtime options

Practice has been checked near 60 FPS at the original internal resolution,
with normal gameplay speed and audio, on Zorin OS 18.1, Linux 7.0, Ryzen 5 5500,
RX 580 2048SP and Mesa RADV 25.2.8. Follow-up player checks also reported steady
60 FPS on the storm stage and Mystical Forest, which previously slowed down.
These are player observations, not repeated benchmarks. Snow/Manji Valley and
other remaining stages still need follow-up checks.

Scenario Campaign initially showed characters over a black stage and nearly
stalled. A separate 60 Hz guest clock restored scenery; direct Linux mapping
queries reduced CPU time spent parsing `/proc/self/maps` under the runtime
lock. In the reproduced stage, gameplay then ran near 60 FPS and cutscenes
played at normal speed. This confirms one stage, not the complete campaign.

The launcher now uses a 60 Hz guest clock, fast GPU wait polling and FIFO
presentation, without an additional guest swap limiter. `--legacy-timing`
restores the SDK timing for comparison. `PROCMAP_QUERY` is used on supported
Linux 6.11+ kernels; older kernels fall back to the parser and may perform worse.
`REX_LINUX_LEGACY_MAP_QUERY=1` forces that parser for comparison. The SDK patch
also fixes busy polling during fractional-millisecond POSIX waits.

Vulkan framebuffer rendering is the default. `--fsi` requires fragment shader
interlock; it selected llvmpipe software rendering on the test GPU and is not
validated there as a hardware alternative. `--res 768x432`, `--offscreen 512`,
`--fullscreen` and `--log` are available. Arguments after `--` go directly to
the runtime. F3 toggles FPS; controls match the table below and game controllers
use SDL3. The player name follows the Linux account username, within the guest
name limit, while the saved profile identity stays the same. Online multiplayer
is not included in this port.

Optional profiling can be enabled with
`TEKKEN6_GPU_PROFILE=/tmp/tekken6-gpu.csv tools/run_linux.sh --log`; analyze it
with `python3 tools/analyze_gpu_profile.py /tmp/tekken6-gpu.csv`. Compare the same
characters, stage, window size and active gameplay interval. CPU wall times
include nested work, and GPU submission spans exclude presenter work and prior
queueing; these counters are not a complete GPU bottleneck diagnosis. Initial
Tunnel Disaster/Lightning Storm captures used older timing and mapping queries,
so repeat captures with current defaults before comparing performance.

## macOS

### What you need

- A Mac with Apple Silicon and macOS 13.3 or later, or an Intel Mac with
  macOS 15 or later
- Your own disc image (`.iso`) of Tekken 6 for the Xbox 360, USA/Europe, Rev 1
- About 20 GB of free space

### Install

1. Download `Tekken6-Builder-macOS-<version>.zip` from Releases, unzip it and open
   `Tekken 6 Builder`.
2. Click **Choose Disc Image** and pick your `.iso`. If it asks for Apple's
   Command Line Tools, click **Install**, then **Try Again**.
3. Wait for it to finish (a few minutes), then click **Open Tekken 6**.

Tekken 6 is now in Applications. The game files, saves and shader cache are in
`~/Library/Application Support/Tekken 6`, so you can delete the disc image.

The first launch shows a black screen for 10 to 20 seconds while shaders are
prepared.

### Controls

| Xbox 360 | Keyboard |
|---|---|
| Start | Return |
| A, B, X, Y | Space, Backspace, L, P |
| Left stick | W A S D |
| D-pad | Shift + arrow keys |
| LB, RB / LT, RT | 1, 3 / Q, E |

In a fight, L, P, ; and ' are left punch, right punch, left kick and right
kick. F3 shows the frame rate. Game controllers work too.

### Using a game folder somewhere else

To play from a game folder you already have (the one with `Default.xex`, on any
drive), hold Option while opening Tekken 6 and choose it. The app remembers it.

### Building from source

Needs Apple's Command Line Tools and `brew install cmake ninja`.

```sh
tools/build_mac.sh --iso /path/to/Tekken6.iso   # add --arch x86_64 for Intel
tools/make_mac_app.sh --install                  # puts Tekken 6.app in /Applications
```

`tools/run_mac.sh` starts it from the terminal: `--game DIR` for the game
folder, `--fullscreen`, `--res 768x432` for a lower 3D resolution, `--log` to
write a log to `logs/`. `tools/package_mac_builder.sh` makes the builder
(`--sign` and `--notarize` for a Developer ID signed release).

### What changed for macOS

All in the [SDK patch](patches/rexglue/README.md):

- Fixed two timing bugs in the Xbox event emulation that froze the first frame.
- Converts a primitive type Metal can't draw.
- Fixed a shader converter value that washed every scene out white.
- Groups graphics work into fewer passes (M1 Max: 21 fps before, 60 after).
- Paces frames evenly to the display (`--vulkan_present_pacing=false` turns it
  off).
- Intel Macs don't use Metal argument buffers, which hung the AMD GPU.
- Removed the white square where the Xbox shows the gamer picture.

Intel needs macOS 15 because the macOS 13 AMD driver draws the game's motion
blur wrong and the characters come out black.

For developers: `--execute_unclipped_draw_vs_on_cpu=true` (set by the launchers)
is needed for Practice mode and fights to render. After changing the shader
converter, delete `cache/`. If the game hangs, set `REX_DEBUG_DUMP_TRIGGER=/some/file`
and create that file to print what every thread is waiting on.

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
