#!/bin/zsh
# Launch Tekken 6 Recomp on macOS. Mirrors the Windows launchers' settings.
#
#   tools/run_mac.sh                 # Vulkan host-framebuffer path (correct on Metal; the default here)
#   tools/run_mac.sh --fsi           # fragment shader interlock path: faster, but shows stale bands on Metal
#   tools/run_mac.sh --res 768x432   # internal 3D resolution: 640x360, 768x432, 1024x576 (default)
#   tools/run_mac.sh --offscreen 512 # 512x512 offscreen target instead of 1024x1024 (softer shadows, faster)
#   tools/run_mac.sh --fullscreen
#   tools/run_mac.sh --log           # keep rexglue logging on (default: off, like Windows)
#   tools/run_mac.sh --log-level trace  # trace|debug|info|warn|error (implies --log)
#   tools/run_mac.sh --build-dir DIR
# Keyboard acts as a controller (mnk_mode): Return/X=Start, Space=A, Backspace=B, L=X, P=Y,
# WASD=left stick, Shift+arrows=d-pad, 1/3=shoulders, Q/E=triggers. F3 toggles the FPS overlay.
# execute_unclipped_draw_vs_on_cpu restores Xenia's default (rexglue-sdk PR 513): without it,
# Tekken's depth-only clears with an 8192 scissor claim all of EDRAM and wrap over the scene
# colour buffer, which left Practice mode with a black scene and fights with black hit frames.
# Anything after -- is passed to the host executable.
set -euo pipefail

root="${0:A:h:h}"
cd "$root"

arch="$(uname -m)"
[[ "$arch" == arm64 ]] && platform=mac-arm64 || platform=mac-amd64
build_dir="build/$platform"
rt_path=fbo
res=""
offscreen=""
fullscreen=false
log=off
extra=()

while (( $# )); do
  case "$1" in
    --fbo) rt_path=fbo; shift ;;
    --fsi) rt_path=fsi; shift ;;
    --res) res="$2"; shift 2 ;;
    --offscreen) offscreen="$2"; [[ "$offscreen" == *x* ]] || offscreen="${offscreen}x${offscreen}"; shift 2 ;;
    --fullscreen) fullscreen=true; shift ;;
    --log) log=info; shift ;;
    --log-level) log="$2"; shift 2 ;;
    --build-dir) build_dir="$2"; shift 2 ;;
    --) shift; extra=("$@"); break ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "unknown option: $1 (use -- to pass host flags)" >&2; exit 1 ;;
  esac
done

host="$build_dir/tekken6"
[[ -x "$host" ]] || { echo "missing $host; run tools/build_mac.sh first" >&2; exit 1; }
[[ -f game/Default.xex || -f game/default.xex ]] || { echo "missing game/Default.xex" >&2; exit 1; }

mkdir -p user "cache/$rt_path" logs

export TEKKEN6_YIELD_STARTUP_SPIN=1
[[ -n "$res" ]] && export TEKKEN6_INTERNAL_RESOLUTION="$res"
[[ -n "$offscreen" ]] && export TEKKEN6_OFFSCREEN_RESOLUTION="$offscreen"

log_file=/dev/null
[[ "$log" != off ]] && log_file="logs/tekken6-$(date +%Y%m%d-%H%M%S).log"

exec "$host" \
  --game_data_root="$root/game" \
  --user_data_root="$root/user" \
  --cache_root="$root/cache/$rt_path" \
  --fullscreen="$fullscreen" \
  --log_file="$log_file" --log_level="$log" \
  --gpu_backend=vulkan --render_target_path_vulkan="$rt_path" \
  --async_shader_compilation=false --anisotropic_override=0 \
  --execute_unclipped_draw_vs_on_cpu=true \
  --mnk_mode=true \
  "${extra[@]}"
