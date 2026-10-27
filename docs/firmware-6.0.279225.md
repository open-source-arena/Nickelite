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

The later menu-editor candidate (`Kobo.tgz` SHA-256
`f1b0c9821528d3244fbd1304f97ac2783936da4d816ae83e6255000b9b5140ff`)
was installed on the same N365 after a private backup of `.adds/nm`. Its
Features and Menu items tabs opened, and the Kobo remained responsive. Its
generic Qt fields did not invoke Kobo's soft keyboard on physical hardware.
A revised full-screen native view passed both simulator checks and awaits
physical installation and touchscreen verification.
The revised package (`Kobo.tgz` SHA-256
`13bbde21709c1de2c6f33ab670d35573e1224670f9d1e0c602c719ae3fde9de9`)
was verified on N365 storage and safely unmounted for the physical check.
The user confirmed its two tabs and menu editor work on the N365, then reported
that the content did not fill the page, button layout and raw action names were
hard to use, and saved menu changes were not visible immediately.

The follow-up Settings UX package (`Kobo.tgz` SHA-256
`b3210fb0066dd095bbdc395884c62de44bd7ad8f799e5681192dfd0576e513fc`)
fills the area above Kobo's bottom tabs, aligns the editor buttons, labels menu
locations and actions in plain language, and forces the menu to reload after
Apply changes. Back or another tab asks before discarding unsaved changes,
including in the item editor. These behaviors passed in 5.18.270971 and
6.0.279225 simulator guests. The package was copied to the N365 after backing
up `.adds/nm` to `/tmp/nm-n365-before-settings-ux-20261027.tar.gz`; the copy's
checksum matched and Kobo storage was safely unmounted. The user confirmed on
the physical N365 that the view filled the page, readable choices appeared,
and the Kobo remained responsive. They also spotted focus outlines, a brown
selection color in the simulator, and a small width mismatch in the editor.

The visual follow-up (`Kobo.tgz` SHA-256
`f638c428fbd5e90bdad311b56f15984383a3c72f47ace8213d65a3ed305c3903`)
removes focus from touch-only controls, uses a gray selected row with a black
underline, and aligns the editing controls with Apply changes. The revision
passed visual checks in both simulator guests and awaits a physical touchscreen
check.

To recreate the guest, follow [real-cloud.md](emulator/real-cloud.md) with
`NM_FIRMWARE_VERSION=6.0.279225`. Keep the work directory outside Git because
its `onboard` subdirectory contains account tokens. The rebuilt VNC platform
plugin must be installed in the image at `/usr/lib/plugins/platforms/libqvnc.so`;
the firmware's Qt plugin search path does not load it from
`/usr/local/Kobo/platforms`.
