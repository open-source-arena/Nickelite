# Firmware 6.0.279225 (Clara BW N365)

The 2026-10-23 Kobo firmware update replaced the full root filesystem. Its
`rootfs.img` has no `/usr/local/Kobo/imageformats` directory, so a NickelMenu
plugin installed there on 6.0.274403 is removed by the update. The user
configuration under `.adds/nm` stays on onboard storage. Reinstall the Qt 6
`Kobo.tgz` after this update; the Qt 5 `KoboRoot.tgz` has a different extraction
root. A future full-rootfs update may require another reinstall.

The official firmware ZIP is
`https://ereaderfiles.kobo.com/firmwares/kobo12/Oct2026/qt6-update-6.0.279225.zip`
(SHA-256 `48b777179a79f68a5d1216e2a8ae20936f1bfacb6c5595351c9a6104b058f2ac`).
Its compressed `rootfs.img` SHA-256 is
`0b77560f6d8c4eda0a446e087281a9ec05d8b9374f0654f0864682ef17650865`;
stock `libnickel.so.1.0.0` SHA-256 is
`82c877261652bec2a7d601b1ac3007f48bd6f3b39dd1950b19d0869cb29a7a44`.
The cloud capability guard in `src/nickelmenu.cc` accepts this exact binary or
the previous 6.0.274403 binary and still requires an N365 model check. Other
firmware binaries keep their stock cloud gates.

The production `Kobo.tgz` built for this check contained only
`imageformats/libnm.so` (SHA-256
`8b537a099fb12a6029dee824a777bb830334ac8dd888940be9cfe2f2227d025e`).
`test/qt_compat` passed under QEMU ARM against both firmware libraries,
including the stock and already-patched method bytes. The 6.0.279225 QEMU
userspace guest showed NickelMenu on Home, opened linked Google Drive and
Dropbox file lists, reopened Google Drive, and switched between providers
without a panic. The guest reused account data stored outside Git. It does
not emulate the physical touchscreen, USB hardware, updater, or power state.
On the physical N365, the staged archive was applied after safe unmount and
NickelMenu returned on Home. Google Drive opened twice from Home; Google Drive
→ Dropbox → Google Drive switching also opened each linked file list and
remained responsive for 30 seconds. A Google Drive swipe still opened the
touched item instead of scrolling, matching the earlier firmware's list
behavior. The narrow up/down arrows at the list's right edge page through
items and work on this firmware. A fresh Google Drive EPUB named
`000-NickelMenuNG-6.0.279225-20261027.epub` downloaded and opened in My Books
with its `PECAN` marker. Dropbox transfer on this firmware remains pending.

A later Settings build was installed on the same N365. The built-in
NickelMenu Settings popup opened, and its checkbox choices persisted between
openings.
Turning Dropbox off and saving showed the expected disabled message when its
menu action was tapped; the Kobo stayed responsive. Dropbox was then turned
back on.

To recreate the guest, follow [real-cloud.md](emulator/real-cloud.md) with
`NM_FIRMWARE_VERSION=6.0.279225`. Keep the work directory outside Git because
its `onboard` subdirectory contains account tokens. The rebuilt VNC platform
plugin must be installed in the image at `/usr/lib/plugins/platforms/libqvnc.so`;
the firmware's Qt plugin search path does not load it from
`/usr/local/Kobo/platforms`.
