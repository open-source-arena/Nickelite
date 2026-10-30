# Nickelite 1.4.0

Adds an installed-only KFMon launcher picker to Shortcuts and the Menu items
Action choices. Selecting the one persistent menu row opens a picker for
GUI-visible watches and shows an empty-state message when there are none. The
**KFMon actions** permission gates the picker, `kfmon`, and `kfmon_id`. README
documents watch setup and stable filename-basename action syntax. Existing
per-watch `generator:...:kfmon:gui` config behavior remains supported; the
canonical row created by the earlier 1.4.0 Settings editor is recognized as
the new picker during Settings edits. In custom chains, **List KFMon launchers**
is terminal; remaining actions are skipped after it opens. KOReader remains
independently installed-only.

## Verification

- Clean Qt 6 cross-build completed with `make -j8 all kobo test/qt_compat test/compat`.
- `test/qt_compat` passed in firmware 5.18.270971 QEMU with
  `NM_TEST_UNSUPPORTED_FIRMWARE=1` and in firmware 6.0.279225 QEMU.
- Shell and PowerShell installer tests for both firmware versions passed for
  checksum verification, no-config-loss behavior, and pending-updater refusal.
- Qt 6 cross-build and compatibility checks passed for both pinned firmware
  userlands. Release ZIP hashes and embedded manifests verified.
- Physical Clara BW (N365) firmware 6.0.279225 check passed: Home and Settings
  remained responsive; the native KFMon picker opened and closed through Back
  and the My Books bottom tab without a crash; the **Save device status** watch
  showed KFMon's success notification and wrote its expected report. Permissions
  showed **KFMon actions** and Add Action offered List/File/Entry. The Shortcuts
  toggle added and removed one menu row. The sample watch and script remain
  documented in README; Nickelite does not create them automatically.
- The physical check does not cover firmware 5, other Kobo models, or KFMon
  uninstall/rollback. QEMU renders Settings blank and could not verify touch UI.
- KOReader v2026.07.1 launched and responded on that Clara BW, but `Exit > Exit`
  froze on the boot animation and required a non-erasing power cycle. `Exit > Reboot device`
  returned to Nickel; use that option on this tested device.
  This does not establish behavior on other firmware or Kobo models.

The optional KFMon install, uninstall, and defaults ZIPs are unchanged from
1.3.0. The upstream KFMon compatibility variant and physical install limits are
listed in `dist/kfmon/PROVENANCE.md`.
