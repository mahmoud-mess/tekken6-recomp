#!/bin/zsh
# macOS build of Tekken 6 Recomp: Apple Silicon (macOS 13.3+) or Intel (macOS 15+).
#
# Builds the pinned ReXGlue SDK, its codegen tool, the Xenos GPU plugin (Vulkan
# over MoltenVK), code generated from your own Default.xex, and the host app.
# Either Mac can build either version: --arch picks it (default: this Mac's).
#
#   tools/build_mac.sh --iso /path/to/Tekken6.iso      # extract + build
#   tools/build_mac.sh --game /path/to/extracted/game  # copy + build
#   tools/build_mac.sh                                 # rebuild (game/ present)
#
# Options:
#   --arch A          arm64 (Apple Silicon) or x86_64 (Intel)
#   --build-dir DIR   default build/mac-<arch>
#   --jobs N          default: all CPU cores
#   --debug           RelWithDebInfo instead of Release
#   --sdk-only        stop after the SDK tools (no game data needed)
#
# Then run the game with tools/run_mac.sh.
set -euo pipefail

root="${0:A:h:h}"
cd "$root"

arch="$(uname -m)"
build_dir=""
jobs="$(sysctl -n hw.ncpu)"
build_type=Release
iso=""
game_src=""
sdk_only=0

while (( $# )); do
  case "$1" in
    --iso) iso="$2"; shift 2 ;;
    --game) game_src="$2"; shift 2 ;;
    --arch) arch="$2"; shift 2 ;;
    --build-dir) build_dir="$2"; shift 2 ;;
    --jobs) jobs="$2"; shift 2 ;;
    --debug) build_type=RelWithDebInfo; shift ;;
    --sdk-only) sdk_only=1; shift ;;
    -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done

case "$arch" in
  arm64) platform=mac-arm64; min_macos=13.3; arch_flags=() ;;
  # on macOS 13 the AMD driver draws the characters black (the motion-blur pass)
  x86_64) platform=mac-amd64; min_macos=15.0; arch_flags=(-DCMAKE_CXX_FLAGS="-msse4.1 -mssse3") ;;
  *) echo "unsupported architecture: $arch (arm64 or x86_64)" >&2; exit 1 ;;
esac
[[ -n "$build_dir" ]] || build_dir="build/$platform"

say() { print -P "%F{cyan}==>%f $*"; }

for tool in cmake ninja clang++ python3; do
  command -v "$tool" >/dev/null || { echo "missing $tool (brew install cmake ninja; Xcode command line tools)" >&2; exit 1; }
done

say "SDK submodule and Tekken patch"
git submodule update --init --recursive
python3 tools/apply_rexglue_patches.py

say "Configure $platform ($build_type) in $build_dir"
cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_OSX_ARCHITECTURES="$arch" "${arch_flags[@]}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$min_macos" \
  -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF \
  -DREXGLUE_RECOMP_DEBUG_INFO=none

say "SDK tools (codegen, runtime, Xenos plugin, ISO extractor)"
cmake --build "$build_dir" --target rexglue rexruntime rexgpu-xenos xdvdfs_tool --parallel "$jobs"
(( sdk_only )) && { say "SDK built: third_party/rexglue-sdk/out/$platform"; exit 0; }

xdvdfs="$build_dir/tools/xdvdfs/xdvdfs_tool"
if [[ -n "$iso" ]]; then
  [[ -f "$iso" ]] || { echo "ISO not found: $iso" >&2; exit 1; }
  say "Extract ISO into game/"
  rm -rf game
  mkdir -p game
  "$xdvdfs" extract-all "$iso" game
elif [[ -n "$game_src" ]]; then
  say "Copy game data into game/"
  rm -rf game
  cp -R "$game_src" game
fi

xex="$(find game -maxdepth 1 -iname 'default.xex' | head -1 || true)"
if [[ -z "$xex" ]]; then
  echo "game/Default.xex is missing. Pass --iso or --game, or use --sdk-only." >&2
  exit 1
fi
# The manifest names game/Default.xex; XDVDFS images may use lowercase.
[[ "$xex" == "game/Default.xex" ]] || ln -sf "${xex:t}" game/Default.xex

supported="ee138a4fe8d32a0edc131451377f4377041a49626153fa992db24f5420a7f15c"
actual="$(shasum -a 256 "$xex" | cut -d' ' -f1)"
if [[ "$actual" == "$supported" ]]; then
  say "Default.xex matches Tekken 6 (Europe/USA) Rev 1"
else
  say "WARNING: Default.xex SHA-256 $actual is not the tested Europe/USA Rev 1 image; codegen may fail"
fi

if [[ ! -f generated/default/sources.cmake ]]; then
  say "Codegen from Default.xex"
  cmake --build "$build_dir" --target tekken6_codegen --parallel "$jobs"
  # The generated source list only exists now; configure again so the
  # recompiled-code library joins the build graph before the host link.
  cmake -S . -B "$build_dir"
fi

say "Host build (long: tens of thousands of generated functions)"
cmake --build "$build_dir" --target tekken6 --parallel "$jobs"

say "Done. Run: tools/run_mac.sh, or package it: tools/make_mac_app.sh --arch $arch"
