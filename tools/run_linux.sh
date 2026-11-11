#!/usr/bin/env bash
# Launch the Linux Vulkan host with Tekken's rendering compatibility settings.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
build_dir=build/linux-amd64
game_dir="$root/game"
rt_path=fbo
fullscreen=false
log=off
legacy_timing=false
guest_vsync=false
frame_limit=0
guest_vblank_hz=60
res=
offscreen=
extra=()
usage() {
  cat <<'HELP'
Usage: tools/run_linux.sh [options] [-- host-options...]
  --fbo / --fsi    Vulkan framebuffer (default) or fragment shader interlock
  --res WxH       Internal resolution, e.g. 768x432 (default: original)
  --offscreen N   Offscreen target size, e.g. 512 or 512x512
  --legacy-timing Restore SDK guest vsync (may reduce gameplay FPS)
  --fullscreen    Start fullscreen
  --log           Write an info log under logs/
  --log-level L   trace|debug|info|warn|error|off
  --build-dir DIR Build directory (default: build/linux-amd64)
  --game DIR      Extracted game folder (default: game/)
Default timing uses a 60 Hz guest clock, fast memory queries and FIFO presentation.
FSI requires Vulkan fragment shader interlock support; use FBO if unavailable.
F3 toggles FPS. Keyboard/controller mappings match the macOS version.
HELP
}
while (($#)); do
  case "$1" in
    --fbo) rt_path=fbo; shift ;;
    --fsi) rt_path=fsi; shift ;;
    --fullscreen) fullscreen=true; shift ;;
    --legacy-timing) legacy_timing=true; guest_vsync=true; frame_limit=0; guest_vblank_hz=0; shift ;;
    --log) log=info; shift ;;
    --res|--offscreen|--log-level|--build-dir|--game)
      [[ $# -ge 2 && -n "$2" ]] || { echo "Missing value for $1" >&2; exit 2; }
      case "$1" in
        --res) res="$2" ;;
        --offscreen) offscreen="$2"; [[ "$offscreen" == *x* ]] || offscreen="${offscreen}x${offscreen}" ;;
        --log-level) log="$2" ;;
        --build-dir) build_dir="$2" ;;
        --game) game_dir="$(realpath -e -- "$2")" ;;
      esac
      shift 2 ;;
    --) shift; extra=("$@"); break ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1 (use -- for host options)" >&2; exit 2 ;;
  esac
done
case "$log" in trace|debug|info|warn|error|off) ;; *) echo "Invalid log level: $log" >&2; exit 2 ;; esac
host="$build_dir/tekken6"
[[ -x "$host" ]] || { echo "Missing $host; run tools/build_linux.sh first." >&2; exit 1; }
[[ -f "$game_dir/Default.xex" || -f "$game_dir/default.xex" ]] || { echo "Missing Default.xex in $game_dir" >&2; exit 1; }
mkdir -p user "cache/linux-$rt_path" logs
export TEKKEN6_YIELD_STARTUP_SPIN=1
export REX_LINUX_FAST_MAP_QUERY=1
[[ -z "$res" ]] || export TEKKEN6_INTERNAL_RESOLUTION="$res"
[[ -z "$offscreen" ]] || export TEKKEN6_OFFSCREEN_RESOLUTION="$offscreen"
log_file=/dev/null
[[ "$log" == off ]] || log_file="$root/logs/tekken6-linux-$(date +%Y%m%d-%H%M%S).log"
exec "$host" \
  --game_data_root="$game_dir" --user_data_root="$root/user" \
  --cache_root="$root/cache/linux-$rt_path" --fullscreen="$fullscreen" \
  --log_file="$log_file" --log_level="$log" \
  --gpu_backend=vulkan --render_target_path_vulkan="$rt_path" \
  --vsync="$guest_vsync" --guest_frame_limit="$frame_limit" \
  --guest_vblank_hz="$guest_vblank_hz" \
  --vulkan_allow_present_mode_immediate="$legacy_timing" \
  --vulkan_allow_present_mode_mailbox="$legacy_timing" \
  --vulkan_allow_present_mode_fifo_relaxed="$legacy_timing" \
  --async_shader_compilation=true --anisotropic_override=0 \
  --execute_unclipped_draw_vs_on_cpu=true --mnk_mode=true "${extra[@]}"
