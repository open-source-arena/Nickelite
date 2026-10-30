# Nickelite 1.2.0

Adds installed-only KOReader controls and a KOReader launch preset, and makes KFMon permissions and actions appear only when KFMon is installed. The Menu items editor can add an optional KFMon launcher list generator.

The 1.2.0 Qt 6 cross-build completed. ARM compatibility checks passed in QEMU userspace for firmware 5.18.270971 and 6.0.279225, including the expected unsupported-firmware guard on 5.18. A pre-release 1.2.0 candidate kept Home and all three Settings views responsive on a physical Clara BW (N365), with KOReader and KFMon rows hidden while both launchers were absent. The reviewed release package was staged on the device, but its post-update response is unconfirmed. KFMon-installed UI state could not be verified: firmware 6’s `/usr/local/Kobo` overlay did not install its daemon at `/usr/local/kfmon`, the expected path. Nickelite does not bundle KFMon.

The README’s Menu items and Permissions screenshots are 1.1.1 simulator captures. Its Shortcuts image shows a pre-release Nickelite 1.2.0 candidate on a physical Clara BW with KOReader absent. QEMU Settings renders blank, so refreshed Menu items and Permissions captures remain pending.

Download the ZIP matching your exact firmware version and follow the [installation guide](https://github.com/open-source-arena/Nickelite#download-and-install). Existing `.adds/nm` configuration is preserved.
