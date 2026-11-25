#!/bin/sh
# Downloads wgpu-native for the Android app (android/, docs/android.md) into third_party/wgpu-android-<arch> and checks the
# SHA-256 of each file before unpacking: aarch64 for phones and tablets, x86_64 for the Android emulator on a PC.
# The same release as scripts/fetch_wgpu.sh. Safe to run again: a folder that is already there is kept.
set -e
version=v29.0.1.1
here=$(cd "$(dirname "$0")/.." && pwd)
. "$here/scripts/fetch_lib.sh"
for arch in aarch64 x86_64; do
  case "$arch" in
    aarch64) sha256=721741f1b05a20c1738166bedf7a5efb2ba4b382da689526d3fc33de22bdd573 ;;   # from the release page, 2026-11-24
    x86_64)  sha256=ef16fc0644bf0e308a39ac4516742da8e22d8c201d3a542cc5baf533d272c491 ;;
  esac
  target="$here/third_party/wgpu-android-$arch"
  if [ -f "$target/lib/libwgpu_native.a" ]; then echo "wgpu-native $version for Android $arch: already in $target"; continue; fi
  mkdir -p "$target"
  url="https://github.com/gfx-rs/wgpu-native/releases/download/$version/wgpu-android-$arch-release.zip"
  fetch_download "wgpu-native Android $arch" "$target" wgpu.zip "$url" "$sha256" || exit 1
  got=$(fetch_sha256 "$target/wgpu.zip")
  if [ "$got" != "$sha256" ]; then rm -f "$target/wgpu.zip"; echo "wgpu-native Android $arch: wrong SHA-256 $got (expected $sha256)" >&2; exit 1; fi
  if command -v unzip > /dev/null; then unzip -o -q "$target/wgpu.zip" -d "$target"; else (cd "$target" && tar -xf wgpu.zip); fi
  rm "$target/wgpu.zip"
  echo "wgpu-native $version for Android $arch in $target"
done
