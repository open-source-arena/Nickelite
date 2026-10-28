# Nickelite 1.1.0

Adds a ready-made **KOReader** shortcut to Nickelite Settings → Shortcuts. Install KOReader separately under `.adds/koreader`, then turn on the shortcut and apply. The generated menu item uses KOReader's documented Kobo launcher, `/mnt/onboard/.adds/koreader/koreader.sh`. Existing matching NickelMenu-style entries appear as enabled in Settings. The shortcut remains off until you turn it on.

The shortcut and menu update were checked in the 6.0.279225 QEMU userspace simulator. Its generated command and the launch script were checked there; QEMU userspace cannot verify Nickelite's detached process launch, and this release has not been tested with KOReader on a physical Kobo. The 5.18.270971 and 6.0.279225 ARM compatibility checks passed. The only physical model tested for Nickelite's earlier features remains Kobo Clara BW (N365).

Download the ZIP matching your exact firmware version, then follow the [installation guide](https://github.com/open-source-arena/Nickelite#download-and-install). KOReader itself is not included. Existing `.adds/nm` configuration is preserved.
