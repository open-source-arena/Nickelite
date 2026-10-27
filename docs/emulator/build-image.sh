#!/bin/sh
# Build an emulator image from the pinned vendor firmware; no Kobo files enter Git.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=${1:?usage: build-image.sh WORK_DIRECTORY EXPERIMENTAL_KOBO_TGZ [QEMU_ARM_STATIC]}
plugin=${2:?usage: build-image.sh WORK_DIRECTORY EXPERIMENTAL_KOBO_TGZ [QEMU_ARM_STATIC]}
qemu=${3:-/usr/bin/qemu-arm-static}
firmware=${NM_FIRMWARE_VERSION:-6.0.274403}
case "$firmware" in
    6.0.274403)
        firmware_url=https://ereaderfiles.kobo.com/firmwares/kobo12/Sep2026/qt6-update-6.0.274403.zip
        firmware_hash=31d1462b3c7ae9f850c04b752b654404407cc4605c50885318a876ddb3b9ce47
        rootfs_hash=6d401357e39891c48c99011f76fe5c8f6b852c497707a8765566674521db864c
        libnickel_hash=b22059eb126d355312f69abb9d1cafc72f0cf54ed4ffe5607eadda2d65670df7
        ;;
    6.0.279225)
        firmware_url=https://ereaderfiles.kobo.com/firmwares/kobo12/Oct2026/qt6-update-6.0.279225.zip
        firmware_hash=48b777179a79f68a5d1216e2a8ae20936f1bfacb6c5595351c9a6104b058f2ac
        rootfs_hash=0b77560f6d8c4eda0a446e087281a9ec05d8b9374f0654f0864682ef17650865
        libnickel_hash=82c877261652bec2a7d601b1ac3007f48bd6f3b39dd1950b19d0869cb29a7a44
        ;;
    *) echo "unsupported firmware: $firmware" >&2; exit 1 ;;
esac
image="nickelmenung-simulator-$firmware:latest"

for command in curl unzip tar zstd debugfs sha256sum docker; do
    command -v "$command" >/dev/null || { echo "missing $command" >&2; exit 1; }
done
test -x "$qemu" || { echo "missing static ARM interpreter: $qemu" >&2; exit 1; }
test -f "$plugin" || { echo "missing plugin archive: $plugin" >&2; exit 1; }
test "$(tar -tzf "$plugin")" = imageformats/libnm.so || {
    echo 'plugin archive must contain only imageformats/libnm.so' >&2; exit 1;
}
test -f "$repo/docs/emulator/libqvnc-repaint.so" || {
    echo 'build docs/emulator/libqvnc-repaint.so first; see README.md' >&2; exit 1;
}
mkdir -p "$work"
work=$(CDPATH= cd -- "$work" && pwd)
test ! -e "$work/rootfs" || { echo "refusing to overwrite $work/rootfs" >&2; exit 1; }
test ! -e "$work/rootfs.raw" || { echo "refusing to overwrite $work/rootfs.raw" >&2; exit 1; }

if [ ! -f "$work/firmware.zip" ]; then
    curl -fL "$firmware_url" -o "$work/firmware.zip"
fi
printf '%s  %s\n' "$firmware_hash" "$work/firmware.zip" | sha256sum -c -
unzip -p "$work/firmware.zip" update.tar > "$work/update.tar"
tar -xOf "$work/update.tar" rootfs.img > "$work/rootfs.img"
printf '%s  %s\n' "$rootfs_hash" "$work/rootfs.img" | sha256sum -c -
zstd -d -c "$work/rootfs.img" > "$work/rootfs.raw"
mkdir "$work/rootfs"
# Unprivileged debugfs emits one harmless chown warning per extracted file.
debugfs -R "rdump / $work/rootfs" "$work/rootfs.raw" 2> "$work/debugfs.log"
test -f "$work/rootfs/usr/local/Kobo/libnickel.so.1.0.0"
printf '%s  %s\n' "$libnickel_hash" \
    "$work/rootfs/usr/local/Kobo/libnickel.so.1.0.0" | sha256sum -c -

cp "$qemu" "$work/rootfs/qemu-arm-static"
cp "$repo/docs/emulator/start-guest.sh" "$work/rootfs/emulator-start.sh"
mkdir -p "$work/rootfs/usr/lib/plugins/platforms"
cp "$repo/docs/emulator/libqvnc-repaint.so" \
    "$work/rootfs/usr/lib/plugins/platforms/libqvnc.so"
tar -xzf "$plugin" -C "$work/rootfs/usr/local/Kobo"
cat > "$work/rootfs/emulator-offline.conf" <<'EOF'
[ApplicationPreferences]
CurrentLocale=en_US
SideloadedMode=false
EOF

tar -C "$work/rootfs" -cf - . | docker import \
    --change 'ENV PRODUCT=spaKoboBW PLATFORM=MT8113T-ntx INTERFACE=wlan0 WIFI_MODULE=wlan_drv_gen4m WIFI=MT8113T NICKEL_HOME=/mnt/onboard/.kobo LD_LIBRARY_PATH=/usr/local/Kobo QT_IM_MODULE= QT_QPA_GENERIC_PLUGINS= QTWEBENGINE_DISABLE_SANDBOX=1 QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --no-zygote --single-process --disable-gpu" QT_DEBUG_PLUGINS=1 NM_QEMU_WIFI_CONNECTED=1' \
    --change 'WORKDIR /usr/local/Kobo' \
    --change 'ENTRYPOINT ["/qemu-arm-static","/bin/sh","/emulator-start.sh"]' \
    - "$image"
echo "built $image; keep $work outside the repo because it contains Kobo firmware"
