# Nickelite 1.3.0

Adds opt-in KFMon install and uninstall assets for Kobo firmware 5.18.270971 and
6.0.279225, plus explicit installer modes and manual instructions. The default
Nickelite installer remains unchanged and does not install KFMon. The updater
scripts refuse to stage KFMon while any Nickel updater archive is pending.

## Verification

- Qt 6 packages for the pinned firmware builds and ARM compatibility checks pass.
- KFMon OTA install and uninstall stage1 extraction pass against both pinned
  firmware OTA scripts in QEMU harnesses.
- On a physical Clara BW (N365) with firmware 6.0.279225, KFMon install
  completed, Nickel Home stayed responsive, Nickelite detected KFMon, and its
  launcher-list generator displayed a disposable IPC-only watch. Tapping that
  watch's `/bin/true` action returned success. The temporary watch and menu
  generator were removed.
- Physical firmware 6 uninstall and rollback were not exercised. Firmware 5
  physical install was not exercised.
- KOReader v2026.07.1 launched and stayed responsive on the physical device.
  `Exit > Exit` froze at the boot animation and required a non-erasing power
  cycle; `Exit > Reboot device` returned to Nickel. Nickelite does not bundle
  KOReader or claim to resolve this upstream behavior.

KFMon release archives come from the `open-source-arena/kfmon` fork at commit
`372432b15933887422aed1975d684126802fd9bc`. Hashes are in `SHA256SUMS`; source
and updater behavior are described in `dist/kfmon/PROVENANCE.md`.
