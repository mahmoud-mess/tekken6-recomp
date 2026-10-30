#!/bin/zsh
# Package "Tekken 6 Builder.app": the Mac counterpart of the Windows builder. It holds
# this project's source code (no game files, no generated code), CMake, Ninja and the
# disc extractor; people open it, pick their own disc image, and it builds
# Tekken 6.app on their Mac (tools/mac_builder/build_game.sh).
#
#   tools/package_mac_builder.sh --cmake DIR --ninja FILE [--out DIR] [--sign ID [--notarize PROFILE]]
#
#   --cmake DIR   CMake.app/Contents from the official macOS universal release
#                 (cmake-4.4.4-macos-universal.tar.gz from cmake.org or GitHub)
#   --ninja FILE  ninja from the official ninja-mac.zip release (universal)
#   --out DIR     default build/mac-builder
#   --sign ID     sign with this Developer ID Application certificate (default: ad-hoc,
#                 so people have to click Open Anyway)
#   --notarize P  send it to Apple with the notarytool keychain profile P and staple the
#                 approval, so it opens like any downloaded app
#
# Needs both Mac builds of xdvdfs_tool (tools/build_mac.sh --arch arm64 and
# --arch x86_64, or --sdk-only for each), so the builder runs on either kind of Mac.
set -euo pipefail

root="${0:A:h:h}"
cd "$root"
cmake_dir=""
ninja=""
out="$root/build/mac-builder"
sign_id=""
notary=""
while (( $# )); do
  case "$1" in
    --cmake) cmake_dir="${2:A}"; shift 2 ;;
    --ninja) ninja="${2:A}"; shift 2 ;;
    --out) out="${2:A}"; shift 2 ;;
    --sign) sign_id="$2"; shift 2 ;;
    --notarize) notary="$2"; shift 2 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done
[[ -x "$cmake_dir/bin/cmake" && -d "$cmake_dir/share" ]] || { echo "--cmake must be CMake.app/Contents (with bin/cmake)" >&2; exit 1; }
[[ -x "$ninja" ]] || { echo "--ninja must be the ninja binary" >&2; exit 1; }
[[ -z "$notary" || -n "$sign_id" ]] || { echo "--notarize needs --sign" >&2; exit 1; }
for a in arm64 amd64; do
  [[ -x "build/mac-$a/tools/xdvdfs/xdvdfs_tool" ]] || { echo "missing build/mac-$a/tools/xdvdfs/xdvdfs_tool" >&2; exit 1; }
done

app="$out/Tekken 6 Builder.app"
contents="$app/Contents"
res="$contents/Resources"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
rm -rf "$app"
mkdir -p "$contents/MacOS" "$res/cmake/bin"

echo "Source archive"
# Every file git knows about, here and in the submodules (the SDK with the Tekken
# patch applied), nothing it ignores: so no game files and no generated code.
git ls-files --cached --others --exclude-standard > "$tmp/files"
git submodule foreach --recursive --quiet \
  'git ls-files --cached --others --exclude-standard | sed "s|^|$displaypath/|"' >> "$tmp/files"
# gitlinks (the submodule entries themselves) are directories; the files list covers them
while IFS= read -r f; do [[ -f "$f" || -L "$f" ]] && print -r -- "$f"; done < "$tmp/files" > "$tmp/list"
if grep -E '^(game|user|cache|build|logs)/|^generated/default/[^.]|\.(iso|xex)$' "$tmp/list"; then
  echo "refusing to package game data or generated code (listed above)" >&2; exit 1
fi
tar -cf - -n -T "$tmp/list" | xz -T0 -6 > "$res/sources.tar.xz"
echo "  $(wc -l < "$tmp/list" | tr -d ' ') files, $(du -h "$res/sources.tar.xz" | cut -f1)"

echo "Tools"
cp "$cmake_dir/bin/cmake" "$res/cmake/bin/"
cp -R "$cmake_dir/share" "$res/cmake/share"
rm -rf "$res/cmake/share/"{aclocal,bash-completion,emacs,vim}
cp "$ninja" "$res/ninja"
lipo -create build/mac-arm64/tools/xdvdfs/xdvdfs_tool build/mac-amd64/tools/xdvdfs/xdvdfs_tool -output "$res/xdvdfs_tool"
cp tools/mac_builder/build_game.sh "$res/"
# License texts that CMake's and Ninja's licenses ask to ship with them, and the SDK's
mkdir -p "$res/licenses/cmake"
(cd "$cmake_dir/doc/cmake" && find . -type f ! -path './html/*' \( -iname 'LICENSE*' -o -iname 'COPYING*' \
  -o -iname 'NOTICE*' -o -iname 'Copyright*' \) | cpio -pdm --quiet "$res/licenses/cmake")
cp tools/mac_builder/ninja-COPYING "$res/licenses/ninja-COPYING"
cp third_party/rexglue-sdk/LICENSE "$res/licenses/rexglue-sdk-LICENSE"
chmod +x "$res/build_game.sh" "$res/ninja" "$res/xdvdfs_tool" "$res/cmake/bin/cmake"

echo "Builder window"
for a in arm64 x86_64; do
  swiftc -O -parse-as-library -target "$a-apple-macos13.3" tools/mac_builder/Builder.swift -o "$tmp/builder-$a"
done
lipo -create "$tmp/builder-arm64" "$tmp/builder-x86_64" -output "$contents/MacOS/Tekken 6 Builder"

version="$(tr -d '[:space:]' < VERSION.txt 2>/dev/null || echo 1.0)"
cat > "$contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Tekken 6 Builder</string>
  <key>CFBundleDisplayName</key><string>Tekken 6 Builder</string>
  <key>CFBundleIdentifier</key><string>com.tekken6recomp.mac.builder</string>
  <key>CFBundleExecutable</key><string>Tekken 6 Builder</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$version</string>
  <key>CFBundleVersion</key><string>$version</string>
  <key>LSMinimumSystemVersion</key><string>13.3</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

# Sign our own binaries, then the bundle. CMake keeps Kitware's Developer ID signature.
# Notarization needs the hardened runtime and a timestamp on each.
if [[ -n "$sign_id" ]]; then sign=(--sign "$sign_id" --options runtime --timestamp); else sign=(--sign -); fi
for f in "$res/ninja" "$res/xdvdfs_tool" "$contents/MacOS/Tekken 6 Builder"; do
  codesign --force "${sign[@]}" "$f"
done
codesign --force "${sign[@]}" "$app"
codesign --verify --deep --strict "$app"
zip_app() { (cd "$out" && rm -f "Tekken 6 Builder.zip" && ditto -c -k --keepParent "Tekken 6 Builder.app" "Tekken 6 Builder.zip") }
zip_app
if [[ -n "$notary" ]]; then
  echo "Notarizing (a few minutes)"
  xcrun notarytool submit "$out/Tekken 6 Builder.zip" --keychain-profile "$notary" --wait | tee "$tmp/notary"
  grep -q "status: Accepted" "$tmp/notary" || { echo "Apple didn't accept it; see: xcrun notarytool log <id> --keychain-profile $notary" >&2; exit 1; }
  xcrun stapler staple "$app"
  zip_app
fi
echo "Built: $app"
echo "Zip:   $out/Tekken 6 Builder.zip ($(du -h "$out/Tekken 6 Builder.zip" | cut -f1))"
