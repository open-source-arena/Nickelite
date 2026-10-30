## Nickelite 1.5.0

Nickelite 1.5.0 adds an optional bottom-tab action override. Add `.adds/nickelite/tab.ini` with one supported Nickelite action, its argument, and an optional label to run that action on a short tap. Press and hold the tab to open the regular Nickelite menu. Removing or renaming the file restores the standard menu behavior. This direct tab action bypasses Settings permission switches; the other action entry points keep their normal permission checks.

The release includes firmware-specific Nickelite packages for Kobo firmware 5.18.270971 and 6.0.279225, along with the optional KFMon install, uninstall, and defaults assets. The default Nickelite installation does not install KFMon.

### Verification

- Qt 6 firmware 5 and 6 builds and ARM compatibility checks passed. QEMU tested the firmware 5 and 6 OTA extraction paths; the firmware 5 unsupported-firmware guard also passed.
- Genuine Nickel 6.0.279225 QEMU UI checks verified the configured tab label, exactly-once short tap, and press-and-hold menu behavior. QEMU does not emulate Kobo hardware; KOReader was not launched in the guest and KFMon's framebuffer daemon path was unavailable there.
- On a physical Kobo Clara BW (N365), firmware 6.0.279225, the short tap ran once, press-and-hold opened the regular menu without launching the action on release, and a direct KOReader v2026.07.1 launch worked. KOReader `Exit > Reboot device` returned to responsive Nickel. `Exit > Exit` froze on the boot animation during an earlier test and required a non-erasing power cycle; use `Exit > Reboot device` on this tested device. The physical test does not establish behavior on other Kobo models or firmware versions.
- KFMon launcher picker IPC and UI were tested on the same physical device in the 1.4.0/1.4.1 series; this release retains those features. Physical firmware 5 testing, KFMon firmware 6 uninstall/rollback, and other Kobo models have not been verified.

See the [compatibility matrix](../../docs/feature-matrix.md), [tab override guide](../../docs/tab-override.md), and [README](../../README.md) for setup and recovery instructions.
