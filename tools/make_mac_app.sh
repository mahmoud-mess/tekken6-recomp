#!/bin/zsh
# Package a built host (tools/build_mac.sh) as "Tekken 6.app".
#
#   tools/make_mac_app.sh                  # -> build/mac-<arch>/Tekken 6.app
#   tools/make_mac_app.sh --arch x86_64    # the Intel build (default: this Mac's kind)
#   tools/make_mac_app.sh --install        # ...and copy it to /Applications
#   tools/make_mac_app.sh --data-root DIR  # where game/, user/ and cache/ live on this Mac (default: this checkout)
#   tools/make_mac_app.sh --game DIR       # take the disc's files from DIR; the app keeps no build folder
#   tools/make_mac_app.sh --build-dir DIR
#
# The bundle holds the host, its two dylibs, the pinned Vulkan loader and MoltenVK,
# and the ISO extractor; the game stays outside it (7 GB). See the launcher below
# for how it finds the game. The icon is made from the disc's own dashboard art
# (tools/mac_icon.py), so no game art is stored in the repo.
set -euo pipefail

root="${0:A:h:h}"
arch="$(uname -m)"
build_dir=""
data_root="$root"
install=false
game_dir=""

while (( $# )); do
  case "$1" in
    --install) install=true; shift ;;
    --arch) arch="$2"; shift 2 ;;
    --data-root) data_root="${2:A}"; shift 2 ;;
    --build-dir) build_dir="${2:A}"; shift 2 ;;
    --game) game_dir="${2:A}"; shift 2 ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done
case "$arch" in
  arm64) platform=mac-arm64; min_macos=13.3 ;;
  x86_64) platform=mac-amd64; min_macos=15.0 ;;  # see build_mac.sh
  *) echo "unsupported architecture: $arch (arm64 or x86_64)" >&2; exit 1 ;;
esac
[[ -n "$build_dir" ]] || build_dir="$root/build/$platform"

[[ -x "$build_dir/tekken6" ]] || { echo "missing $build_dir/tekken6; run tools/build_mac.sh first" >&2; exit 1; }
if [[ -n "$game_dir" ]]; then data_root=""; else game_dir="$data_root/game"; fi
[[ -f "$game_dir/Default.xex" || -f "$game_dir/default.xex" ]] || { echo "missing $game_dir/Default.xex" >&2; exit 1; }

app="$build_dir/Tekken 6.app"
contents="$app/Contents"
rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Resources"

# The GPU plugin is looked up next to the executable and the runtime through
# @executable_path, so all three sit in MacOS/. The SDK finds the loader and
# MoltenVK under Contents/Resources/vulkan.
cp "$build_dir/tekken6" "$build_dir/librexruntime.dylib" "$build_dir/librexgpu-xenos.dylib" "$build_dir/tools/xdvdfs/xdvdfs_tool" "$contents/MacOS/"
cp -R "$build_dir/vulkan" "$contents/Resources/vulkan"
print -r -- "$data_root" > "$contents/Resources/data-root"
# the code is recompiled from this exact Default.xex: another disc's would crash it
xex="$game_dir/Default.xex"; [[ -f "$xex" ]] || xex="$game_dir/default.xex"
shasum "$xex" | cut -d' ' -f1 > "$contents/Resources/xex-sha1"

cat > "$contents/MacOS/launch" <<'EOF'
#!/bin/zsh
# Finds the game, then starts it with tools/run_mac.sh's default settings.
#
# The game folder (the files from the disc, with Default.xex) can be anywhere; the
# choice is kept in ~/Library/Application Support/Tekken 6/game-location, next to
# the saves (user/) and shader cache (cache/). The first launch asks for it: a game
# folder is used where it is, a disc image (.iso) is copied out to a folder the
# person picks. Holding Option while the app opens asks again. On the Mac that
# built the app, the build's own game/, user/ and cache/ are used.
#   TEKKEN6_DATA=<dir>      use <dir>/game, <dir>/user and <dir>/cache, ask nothing
#   TEKKEN6_SOURCE=<path>   answer the question without dialogs (.iso or game folder)
#   TEKKEN6_SOURCE_DEST=<d> where to copy a disc image to (default: Application Support)
#   TEKKEN6_PREPARE_ONLY=1  set the game up, print its folder and stop
here="${0:A:h}"
res="$here/../Resources"
support="$HOME/Library/Application Support/Tekken 6"
location_file="$support/game-location"
scripted=${TEKKEN6_SOURCE:+1}

say() {  # message to the person: a dialog, or stderr when scripted
  if [[ -n "$scripted" ]]; then print -u2 -- "$1"
  else osascript - "$1" <<<'on run argv
display alert "Tekken 6" message (item 1 of argv) as critical
end run' >/dev/null; fi
}
game_xex() { for x in "$1/Default.xex" "$1/default.xex"; do [[ -f "$x" ]] && { print -- "$x"; return 0 }; done; return 1 }
right_xex() { [[ "$(shasum "$1" | cut -d' ' -f1)" == "$(<"$res/xex-sha1")" ]] }
option_held() {
  [[ "$(osascript -l JavaScript -e 'ObjC.import("AppKit"); ($.NSEvent.modifierFlags & $.NSEventModifierFlagOption) ? "1" : "0"' 2>/dev/null)" == 1 ]]
}
wrong="That isn't the Tekken 6 this app was made from. It needs Tekken 6 for the Xbox 360 (USA/Europe, Rev 1)."

# Checks a game folder and remembers it.
use_folder() {
  local xex
  xex="$(game_xex "$1")" || { say "There's no Default.xex in $1. Pick the folder with the files from the disc."; return 1; }
  right_xex "$xex" || { say "$wrong"; return 1; }
  mkdir -p "$support" && print -r -- "${1:A}" > "$location_file"
  game_dir="${1:A}"
}

# Copies a disc image out to <dest>/Tekken 6 Game and remembers that folder.
use_image() {
  local iso="$1" dest="$2/Tekken 6 Game" tmp="$2/Tekken 6 Game.part" free
  [[ -f "$iso" ]] || { say "Can't find $iso."; return 1; }
  mkdir -p "$2" || { say "Can't write to $2."; return 1; }
  free=$(df -k "$2" | awk 'NR==2 {print $4}')
  (( free > 7500000 )) || { say "The game needs 7.5 GB free in $2."; return 1; }
  rm -rf "$tmp"; mkdir -p "$tmp"
  "$here/xdvdfs_tool" extract "$iso" Default.xex "$tmp/Default.xex" >/dev/null 2>&1 \
    || { rm -rf "$tmp"; say "Can't read $iso as an Xbox 360 disc image."; return 1; }
  right_xex "$tmp/Default.xex" || { rm -rf "$tmp"; say "$wrong"; return 1; }
  rm "$tmp/Default.xex"
  [[ -n "$scripted" ]] || osascript -e 'display notification "Copying the game out of your disc image. This takes about a minute." with title "Tekken 6"' >/dev/null
  "$here/xdvdfs_tool" extract-all "$iso" "$tmp" >/dev/null 2>&1 || true
  game_xex "$tmp" >/dev/null || { rm -rf "$tmp"; say "Copying the game out of $iso failed."; return 1; }
  rm -rf "$dest"; mv "$tmp" "$dest"
  use_folder "$dest"
}

# Asks where the game is until it gets an answer that works, or the person cancels.
ask() {
  local intro="$1" choice picked dest
  while true; do
    choice="$(osascript - "$intro" <<<'on run argv
set r to display dialog (item 1 of argv) & "Choose your Tekken 6 disc image (.iso), or a folder with the files from the disc (the folder with Default.xex)." with title "Tekken 6" buttons {"Cancel", "Game Folder…", "Disc Image…"} default button 3 cancel button 1
return button returned of r
end run' 2>/dev/null)" || exit 0
    if [[ "$choice" == Game* ]]; then
      picked="$(osascript -e 'POSIX path of (choose folder with prompt "Choose the folder with the Tekken 6 game files (it has Default.xex in it).")' 2>/dev/null)" || continue
      use_folder "${picked%/}" && return 0
    else
      picked="$(osascript -e 'POSIX path of (choose file with prompt "Choose your Tekken 6 disc image." of type {"iso", "public.disk-image"})' 2>/dev/null)" || continue
      dest="$(osascript - "$support" <<<'on run argv
set r to display dialog "The game will be copied out of the disc image (7 GB). Where should it go?" with title "Tekken 6" buttons {"Cancel", "Somewhere Else…", "Application Support"} default button 3 cancel button 1
if button returned of r is "Application Support" then return item 1 of argv
return POSIX path of (choose folder with prompt "Choose where to put the Tekken 6 game files.")
end run' 2>/dev/null)" || continue
      use_image "$picked" "${dest%/}" && return 0
    fi
  done
}

if [[ -n "${TEKKEN6_DATA:-}" ]]; then
  game_dir="$TEKKEN6_DATA/game"; user_dir="$TEKKEN6_DATA/user"; cache_dir="$TEKKEN6_DATA/cache/fbo"
  game_xex "$game_dir" >/dev/null || { say "There's no Default.xex in $game_dir."; exit 1; }
else
  user_dir="$support/user"; cache_dir="$support/cache/fbo"
  build_root="$(<"$res/data-root")"; [[ -n "$build_root" ]] || build_root=/nonexistent
  game_dir=""
  if [[ -n "$scripted" ]]; then
    if [[ -d "$TEKKEN6_SOURCE" ]]; then use_folder "$TEKKEN6_SOURCE" || exit 1
    else use_image "$TEKKEN6_SOURCE" "${TEKKEN6_SOURCE_DEST:-$support}" || exit 1; fi
  elif option_held; then
    ask "" || exit 0
  elif [[ -f "$location_file" ]]; then
    saved="$(<"$location_file")"
    if game_xex "$saved" >/dev/null; then game_dir="$saved"
    else ask "Tekken 6 was in $saved, but it isn't there now. If it's on a drive, connect it and try again, or choose it again.

" || exit 0; fi
  elif game_xex "$build_root/game" >/dev/null; then
    # the Mac that built the app: share the build's game, saves and cache
    game_dir="$build_root/game"; user_dir="$build_root/user"; cache_dir="$build_root/cache/fbo"
  else
    ask "" || exit 0
  fi
fi
[[ -n "${TEKKEN6_PREPARE_ONLY:-}" ]] && { print -r -- "$game_dir"; exit 0; }

mkdir -p "$user_dir" "$cache_dir"
export TEKKEN6_YIELD_STARTUP_SPIN=1
exec "$here/tekken6" \
  --game_data_root="$game_dir" \
  --user_data_root="$user_dir" \
  --cache_root="$cache_dir" \
  --fullscreen=false \
  --log_file=/dev/null --log_level=off \
  --gpu_backend=vulkan --render_target_path_vulkan=fbo \
  --async_shader_compilation=true --anisotropic_override=0 \
  --execute_unclipped_draw_vs_on_cpu=true \
  --mnk_mode=true \
  "$@"
EOF
chmod +x "$contents/MacOS/launch"

cat > "$contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Tekken 6</string>
  <key>CFBundleDisplayName</key><string>Tekken 6</string>
  <key>CFBundleIdentifier</key><string>com.tekken6recomp.mac</string>
  <key>CFBundleExecutable</key><string>launch</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>$min_macos</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.action-games</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>GCSupportsControllerUserInteraction</key><true/>
</dict>
</plist>
EOF

icon_tmp="$(mktemp -d)"
trap 'rm -rf "$icon_tmp"' EXIT
python3 "$root/tools/mac_icon.py" "$game_dir/nxeart" "$icon_tmp/icon.png"
mkdir "$icon_tmp/AppIcon.iconset"
for sz in 16 32 128 256 512; do
  sips -z $sz $sz "$icon_tmp/icon.png" --out "$icon_tmp/AppIcon.iconset/icon_${sz}x${sz}.png" >/dev/null
  sips -z $((sz * 2)) $((sz * 2)) "$icon_tmp/icon.png" --out "$icon_tmp/AppIcon.iconset/icon_${sz}x${sz}@2x.png" >/dev/null
done
iconutil -c icns "$icon_tmp/AppIcon.iconset" -o "$contents/Resources/AppIcon.icns"

# Ad-hoc sign the nested code first, then the bundle.
for f in "$contents"/MacOS/*.dylib "$contents"/Resources/vulkan/lib/*.dylib "$contents/MacOS/tekken6" "$contents/MacOS/xdvdfs_tool"; do
  codesign --force --sign - "$f" 2>/dev/null
done
codesign --force --sign - "$app" 2>/dev/null
echo "Built: $app"

if $install; then
  rm -rf "/Applications/Tekken 6.app"
  cp -R "$app" /Applications/
  touch "/Applications/Tekken 6.app"
  echo "Installed: /Applications/Tekken 6.app"
fi
