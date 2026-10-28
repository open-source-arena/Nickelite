#!/bin/sh
set -eu

repo=Sojaner/NickelMenuNg
version=1.0.0
mount=${1:?Usage: install.sh /path/to/KOBOeReader [versioned-package.zip]}
[ -f "$mount/.kobo/version" ] || { echo 'Kobo storage is not mounted at that path.' >&2; exit 1; }
firmware=$(cut -d, -f3 "$mount/.kobo/version" | tr -d '\r\n')
case "$firmware" in
  5.18.270971|6.0.279225) ;;
  *) echo "No tested package for firmware $firmware" >&2; exit 1 ;;
esac
asset="Nickelite-${version}-firmware-${firmware}.zip"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
if [ "${2:-}" ]; then
  cp "$2" "$tmp/$asset"
else
  command -v gh >/dev/null || { echo 'Install GitHub CLI (gh) and authenticate for the private repository.' >&2; exit 1; }
  gh release download "v$version" -R "$repo" -p "$asset" -p SHA256SUMS -D "$tmp"
  (cd "$tmp" && if command -v sha256sum >/dev/null; then sha256sum -c --ignore-missing SHA256SUMS; else shasum -a 256 -c SHA256SUMS | grep "$asset: OK"; fi)
fi
unzip -q "$tmp/$asset" -d "$tmp/package"
[ -f "$tmp/package/Kobo.tgz" ] && [ -f "$tmp/package/SHA256SUMS" ] || { echo 'Package is incomplete.' >&2; exit 1; }
(cd "$tmp/package" && if command -v sha256sum >/dev/null; then sha256sum -c SHA256SUMS; else shasum -a 256 -c SHA256SUMS; fi)
[ ! -e "$mount/.adds/nm/uninstall" ] || { echo 'Remove .adds/nm/uninstall before installing.' >&2; exit 1; }
mkdir -p "$mount/.adds/nm"
if [ -f "$tmp/package/cloud-shortcuts.conf" ] && [ ! -e "$mount/.adds/nm/cloud-shortcuts.conf" ]; then
  cloud_found=false
  for config in "$mount/.adds/nm/"*; do
    [ -f "$config" ] || continue
    [ "${config##*/}" = doc ] && continue
    if grep -Eq 'nickel_open[[:space:]]*:[[:space:]]*library:(gdrive|dropbox)' "$config"; then
      cloud_found=true
      break
    fi
  done
  if [ "$cloud_found" = false ]; then
    cp "$tmp/package/cloud-shortcuts.conf" "$mount/.adds/nm/cloud-shortcuts.conf"
  fi
fi
cp "$tmp/package/Kobo.tgz" "$mount/.kobo/Kobo.tgz"
sync
echo "Staged Nickelite $version for Kobo firmware $firmware. Safely eject, then unplug the Kobo."
