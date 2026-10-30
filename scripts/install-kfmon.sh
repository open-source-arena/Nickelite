#!/bin/sh
set -eu

repo=open-source-arena/Nickelite
version=1.4.0
asset=KFMon-Nickelite-Install.zip
mount=${1:?Usage: install.sh --kfmon /path/to/KOBOeReader [install-package.zip]}
package=${2:-}
[ -f "$mount/.kobo/version" ] || { echo 'Kobo storage is not mounted at that path.' >&2; exit 1; }
firmware=$(cut -d, -f3 "$mount/.kobo/version" | tr -d '\r\n')
case "$firmware" in
  5.18.270971|6.0.279225) ;;
  *) echo "No tested KFMon updater for firmware $firmware" >&2; exit 1 ;;
esac
for pending in "$mount/.kobo/Kobo.tgz" "$mount/.kobo/KoboRoot.tgz" "$mount/.kobo/update.tar" "$mount/.kobo/update.tar.tmp"; do
  [ ! -e "$pending" ] || { echo "Another updater is staged at $pending. Let it finish before choosing KFMon." >&2; exit 1; }
done
command -v gh >/dev/null || { echo 'Install GitHub CLI (gh) to download the signed-by-checksum public release asset.' >&2; exit 1; }
command -v unzip >/dev/null || { echo 'Install unzip to inspect the KFMon release package.' >&2; exit 1; }
tmp=$(mktemp -d)
stage=
trap 'rm -rf "$tmp"; [ -z "$stage" ] || rm -f "$stage"' EXIT HUP INT TERM
if [ -n "$package" ]; then
  cp "$package" "$tmp/$asset"
  gh release download "v$version" -R "$repo" -p SHA256SUMS -D "$tmp"
else
  gh release download "v$version" -R "$repo" -p "$asset" -p SHA256SUMS -D "$tmp"
fi
zip="$tmp/$asset"
[ -f "$zip" ] || { echo 'KFMon release package is missing.' >&2; exit 1; }
expected=$(awk -v name="$asset" '$2 == name { print $1 }' "$tmp/SHA256SUMS")
[ -n "$expected" ] || { echo 'KFMon release checksum is missing.' >&2; exit 1; }
if command -v sha256sum >/dev/null; then
  actual=$(sha256sum "$zip" | awk '{print $1}')
else
  actual=$(shasum -a 256 "$zip" | awk '{print $1}')
fi
[ "$actual" = "$expected" ] || { echo 'KFMon release checksum mismatch.' >&2; exit 1; }
members=$(unzip -Z1 "$zip")
[ "$members" = '.kobo/update.tar' ] || { echo 'KFMon package must contain only .kobo/update.tar.' >&2; exit 1; }
stage="$mount/.kobo/update.tar.tmp"
unzip -p "$zip" .kobo/update.tar > "$stage"
[ -s "$stage" ] || { echo 'KFMon updater is empty.' >&2; exit 1; }
mv "$stage" "$mount/.kobo/update.tar"
sync
echo "Staged KFMon $version updater for firmware $firmware. Safely eject and unplug the Kobo."
echo 'This is a separate updater cycle: do not stage it alongside Nickelite .kobo/Kobo.tgz.'
