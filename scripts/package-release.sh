#!/bin/sh
set -eu
version=$(sed -n 's/^VERSION := //p' Makefile)
root=$(pwd)
[ -n "$version" ] && [ -f Kobo.tgz ] && [ -f src/libnm.so ] || { echo 'Build the Qt 6 package first.' >&2; exit 1; }
mkdir -p "dist/$version"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for firmware in 5.18.270971 6.0.279225; do
  mkdir -p "$tmp/$firmware"
  cp Kobo.tgz "$tmp/$firmware/Kobo.tgz"
  if [ "$firmware" = 6.0.279225 ]; then
    cp dist/firmware-6.0.279225/cloud-shortcuts.conf "$tmp/$firmware/cloud-shortcuts.conf"
  fi
  (cd "$tmp/$firmware" && sha256sum * > SHA256SUMS)
  name="Nickelite-$version-firmware-$firmware.zip"
  (cd "$tmp/$firmware" && zip -q "$root/dist/$version/$name" ./*)
done
(cd dist/kfmon && sha256sum -c SHA256SUMS)
cp dist/kfmon/*.zip "dist/$version/"
(cd "dist/$version" && sha256sum *.zip > SHA256SUMS)
