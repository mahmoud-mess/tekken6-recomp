#!/bin/zsh
# The build that "Tekken 6 Builder.app" runs: turns the person's own disc image into
# "Tekken 6.app" on their Mac. Nothing from the game is in the builder; the game's
# code is translated and compiled here from the disc.
#
#   build_game.sh --iso DISC.iso [--arch A] [--apps DIR] [--work DIR] [--res DIR] [--keep-work]
#
#   --arch A     arm64 or x86_64 (default: this Mac's kind)
#   --apps DIR   where Tekken 6.app goes (default /Applications, else ~/Applications)
#   --work DIR   scratch space, deleted afterwards (default ~/Library/Caches/Tekken 6 Builder)
#   --res DIR    the builder's Resources: sources.tar.xz, cmake/, ninja, xdvdfs_tool
#
# Output for the builder window, one per line:
#   STEP n/N text   a new step         PROGRESS a/b   compile progress in this step
#   DONE path       the finished app   NEEDTOOLS      Apple's command line tools are missing
#   ERROR text      what went wrong (exit status 1)
# Everything else is the build log.
set -uo pipefail

res="${0:A:h}"
iso=""
apps="${TEKKEN6_BUILDER_APPS:-}"
arch=""
work="$HOME/Library/Caches/Tekken 6 Builder"
keep_work=false
while (( $# )); do
  case "$1" in
    --iso) iso="${2:A}"; shift 2 ;;
    --apps) apps="${2:A}"; shift 2 ;;
    --arch) arch="$2"; shift 2 ;;
    --work) work="${2:A}"; shift 2 ;;
    --res) res="${2:A}"; shift 2 ;;
    --keep-work) keep_work=true; shift ;;
    *) print "ERROR unknown option $1"; exit 1 ;;
  esac
done

support="$HOME/Library/Application Support/Tekken 6"
game_dir="$support/Tekken 6 Game"
supported_xex="ee138a4fe8d32a0edc131451377f4377041a49626153fa992db24f5420a7f15c"
steps=8
step() { print "STEP $1/$steps $2" }
fail() { print "ERROR $1"; exit 1 }
tools="$work/tools"
export PATH="$tools/cmake/bin:$tools:/usr/bin:/bin:/usr/sbin:/sbin"

step 1 "Checking this Mac"
host_arch="$(uname -m)"
[[ "$(sysctl -n sysctl.proc_translated 2>/dev/null)" == 1 ]] && host_arch=arm64
[[ -n "$arch" ]] || arch="$host_arch"
macos="$(sw_vers -productVersion)"
case "$arch" in
  arm64) platform=mac-arm64; min_macos=13.3; arch_flags=() ;;
  x86_64) platform=mac-amd64; min_macos=15.0; arch_flags=(-DCMAKE_CXX_FLAGS="-msse4.1 -mssse3") ;;
  *) fail "This Mac's processor ($arch) isn't supported." ;;
esac
# only matters when the app is for this Mac
[[ "$arch" != "$host_arch" || "$(print -l "$min_macos" "$macos" | sort -V | head -1)" == "$min_macos" ]] \
  || fail "Tekken 6 needs macOS $min_macos or later on this Mac; it has $macos."
if ! xcode-select -p >/dev/null 2>&1 || ! xcrun --find clang >/dev/null 2>&1; then
  print NEEDTOOLS; exit 3
fi
[[ -f "$iso" ]] || fail "Can't find the disc image $iso."
mkdir -p "$work" "$support" || fail "Can't write to $work."
free_gb=$(( $(df -k "$work" | awk 'NR==2 {print $4}') / 1000000 ))
(( free_gb >= 10 )) || fail "Building needs 10 GB free on this disk; there are $free_gb GB."
mem_gb=$(( $(sysctl -n hw.memsize) / 1000000000 ))
jobs=$(sysctl -n hw.ncpu)
(( jobs > mem_gb * 2 / 3 )) && jobs=$(( mem_gb * 2 / 3 ))
(( jobs < 1 )) && jobs=1
print "arch $arch, macOS $macos, $(sysctl -n hw.ncpu) cores, $mem_gb GB, $jobs build jobs"
# A downloaded builder is quarantined; run its tools from a local copy so macOS
# doesn't stop each one as unverified.
rm -rf "$tools"; mkdir -p "$tools"
cp -R "$res/cmake" "$res/ninja" "$res/xdvdfs_tool" "$tools/" || fail "Can't copy the builder's tools to $work."
xattr -dr com.apple.quarantine "$tools" 2>/dev/null

step 2 "Reading your disc image"
probe="$work/probe"; rm -rf "$probe"; mkdir -p "$probe"
xdvdfs_tool extract "$iso" Default.xex "$probe/Default.xex" >/dev/null 2>&1 \
  || fail "That file isn't an Xbox 360 disc image (or it can't be read)."
[[ "$(shasum -a 256 "$probe/Default.xex" | cut -d' ' -f1)" == "$supported_xex" ]] \
  || fail "That disc isn't the Tekken 6 this builder supports: Tekken 6 for the Xbox 360, USA/Europe, Rev 1."
rm -rf "$probe"

step 3 "Copying the game out of the disc image"
if [[ -f "$game_dir/Default.xex" && "$(shasum -a 256 "$game_dir/Default.xex" | cut -d' ' -f1)" == "$supported_xex" ]]; then
  print "already there: $game_dir"
else
  free_gb=$(( $(df -k "$support" | awk 'NR==2 {print $4}') / 1000000 ))
  (( free_gb >= 8 )) || fail "The game needs 8 GB free in $support; there are $free_gb GB."
  rm -rf "$game_dir.part"; mkdir -p "$game_dir.part"
  xdvdfs_tool extract-all "$iso" "$game_dir.part" || true
  [[ -f "$game_dir.part/Default.xex" ]] || fail "Copying the game out of the disc image failed."
  rm -rf "$game_dir"; mv "$game_dir.part" "$game_dir"
fi
print -r -- "$game_dir" > "$support/game-location"

step 4 "Unpacking the source code"
src="$work/src"; build="$work/build"
rm -rf "$src" "$build"; mkdir -p "$src"
tar -xJf "$res/sources.tar.xz" -C "$src" || fail "The builder's source archive is damaged; download the builder again."
ln -s "$game_dir" "$src/game"
# The SDK's CMake checks each submodule for a .git; the archive has the files without
# git (like the Windows builder, mark them).
for d in "$src/third_party/rexglue-sdk" "$src/third_party/rexglue-sdk/thirdparty/"*(/); do mkdir -p "$d/.git"; done

# Runs a build step, turning ninja's "[a/b]" lines into PROGRESS lines.
build_targets() {
  cmake --build "$build" --parallel "$jobs" --target "$@" 2>&1 | while IFS= read -r line; do
    [[ "$line" =~ '^\[([0-9]+)/([0-9]+)\]' ]] && print "PROGRESS $match[1]/$match[2]"
    print -r -- "$line"
  done
  return ${pipestatus[1]}
}
configure() {
  cmake -S "$src" -B "$build" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$tools/ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(xcrun --find clang)" -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" "${arch_flags[@]}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$min_macos" \
    -DREXGLUE_USE_VULKAN=ON \
    -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF \
    -DREXGLUE_RECOMP_DEBUG_INFO=none
}

step 5 "Building the tools (about a quarter of the time)"
configure || fail "Setting up the build failed. The log has the details."
build_targets rexglue rexruntime rexgpu-xenos xdvdfs_tool || fail "Building the tools failed. The log has the details."

step 6 "Translating the game's code"
build_targets tekken6_codegen || fail "Translating the game's code failed. The log has the details."
configure >/dev/null || fail "Setting up the build failed. The log has the details."

step 7 "Compiling the game (the long part)"
build_targets tekken6 || fail "Compiling the game failed. The log has the details."

step 8 "Making Tekken 6.app"
"$src/tools/make_mac_app.sh" --arch "$arch" --build-dir "$build" --game "$game_dir" || fail "Making the app failed. The log has the details."
if [[ -z "$apps" ]]; then
  apps=/Applications; [[ -w "$apps" ]] || apps="$HOME/Applications"
fi
mkdir -p "$apps" || fail "Can't write to $apps."
rm -rf "$apps/Tekken 6.app"
mv "$build/Tekken 6.app" "$apps/" || fail "Can't put the app in $apps."
$keep_work || rm -rf "$work"
print -r -- "DONE $apps/Tekken 6.app"
