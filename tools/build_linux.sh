#!/usr/bin/env bash
# Build the native x86-64 Linux Vulkan host from your own Xbox 360 game files.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
build_dir=build/linux-amd64
jobs=2
iso=
game_src=
sdk_only=0
usage() {
  cat <<'HELP'
Usage: tools/build_linux.sh [options]
  --iso FILE       Extract an ISO into game/ (must be empty or absent)
  --game DIR       Link an existing game folder as game/ (must be absent)
  --build-dir DIR  Build directory (default: build/linux-amd64)
  --jobs N         Parallel compiler processes (default: 2 to limit memory use)
  --sdk-only       Build SDK, runtime, GPU plugin and extractor without game data
Requires CMake 3.25+, Ninja, Python 3, Clang 19+ and Linux development libraries.
CC/CXX may select another Clang version. Then launch with tools/run_linux.sh.
HELP
}
while (($#)); do
  case "$1" in
    --iso|--game|--build-dir|--jobs)
      [[ $# -ge 2 && -n "$2" ]] || { echo "Missing value for $1" >&2; exit 2; }
      case "$1" in
        --iso) iso="$(realpath -e -- "$2")" ;;
        --game) game_src="$(realpath -e -- "$2")" ;;
        --build-dir) build_dir="$2" ;;
        --jobs) jobs="$2" ;;
      esac
      shift 2 ;;
    --sdk-only) sdk_only=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || { echo "This script currently targets x86-64 Linux." >&2; exit 2; }
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo "--jobs must be a positive integer" >&2; exit 2; }
[[ -z "$iso" || -z "$game_src" ]] || { echo "Choose --iso or --game" >&2; exit 2; }
cc="${CC:-clang-19}"
cxx="${CXX:-clang++-19}"
for tool in cmake ninja python3 "$cc" "$cxx"; do
  command -v "$tool" >/dev/null || { echo "Missing $tool; see README.md Linux dependencies." >&2; exit 1; }
done
if [[ -n "$game_src" ]]; then
  [[ ! -e game && ! -L game ]] || { echo "game/ already exists; omit --game to reuse it." >&2; exit 1; }
  [[ -f "$game_src/Default.xex" || -f "$game_src/default.xex" ]] || { echo "No Default.xex in $game_src" >&2; exit 1; }
  ln -s -- "$game_src" game
fi
if [[ -n "$iso" && -d game && -n "$(ls -A game)" ]]; then
  echo "game/ is nonempty; omit --iso to reuse it." >&2; exit 1
fi

# Avoid writing Git metadata when all pinned dependencies are already checked out.
if git submodule status --recursive | grep '^[+-]' >/dev/null; then
  git submodule update --init --recursive
fi
python3 tools/apply_rexglue_patches.py
cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
  -DCMAKE_CXX_FLAGS=-mssse3 \
  -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF \
  -DREXGLUE_RECOMP_DEBUG_INFO=none
cmake --build "$build_dir" --target rexglue rexruntime rexgpu-xenos xdvdfs_tool --parallel "$jobs"
((sdk_only)) && exit 0
if [[ -n "$iso" ]]; then
  "$build_dir/tools/xdvdfs/xdvdfs_tool" extract-all "$iso" game
fi
# Normalize only the manifest path; retain the original disc filename.
if [[ ! -f game/Default.xex && -f game/default.xex ]]; then
  ln -s default.xex game/Default.xex
fi
[[ -f game/Default.xex ]] || { echo "Missing game/Default.xex; use --iso FILE or --game DIR." >&2; exit 1; }
actual="$(sha256sum game/Default.xex)"
if [[ "${actual%% *}" != ee138a4fe8d32a0edc131451377f4377041a49626153fa992db24f5420a7f15c ]]; then
  echo "Warning: XEX differs from the tested Europe/USA Rev 1 image: ${actual%% *}" >&2
fi
cmake --build "$build_dir" --target tekken6_codegen --parallel "$jobs"
# Codegen creates the guest source list; configure again before compiling it.
cmake -S . -B "$build_dir"
cmake --build "$build_dir" --target tekken6 --parallel "$jobs"
echo "Built $build_dir/tekken6. Launch with tools/run_linux.sh --build-dir '$build_dir'."
