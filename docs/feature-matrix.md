# NickelMenu v0.6.0 compared with NickelMenuNG on Kobo 6

This inventory uses the upstream [v0.6.0 documentation](https://github.com/pgaskin/NickelMenu/tree/v0.6.0), its tagged `Makefile` and source, plus this branch's action registry and the [physical compatibility report](firmware-6.0.279225.md). “Implemented” means the action exists and checks its runtime dependencies; it does not mean every subaction was tested on the N365. The physical results below apply to Clara BW (N365) on 6.0.279225.

The same Qt 6 build was also run in a [5.18.270971 userspace simulator](emulator/README.md#firmware-518-settings-check-2026-10-27). The menu, Settings popup, saved switch, disabled-action message, and diagnostic message passed there. The standalone screenshot action did not finish in that QEMU guest, so the table's screenshot result applies only to 6.0.274403. The cloud model gate is deliberately limited to the two identified 6.x binaries; the 5.18 simulator did not test cloud linking or sync.

## Why an old installation sometimes remained after an update

Upstream v0.6.0 installed `libnm.so` at
`/usr/local/Kobo/imageformats/libnm.so` through `.kobo/KoboRoot.tgz` and kept
configuration under `/mnt/onboard/.adds/nm`. Its tagged source contains no
firmware-update watcher or self-reinstall service. If the menu remained after
an older update, that updater left the installed library in place. We have not
inspected the exact older update package that did so, so its preservation
mechanism is not established. Upstream's own installation instructions say to
reinstall if menu entries disappear after a reboot.

Firmware 6.0.279225 includes a full root filesystem image without that
`imageformats` directory. Applying it removed the library, while the onboard
configuration survived. Reinstalling the Qt 6 `Kobo.tgz` restored NickelMenu
on the physical N365. The package filename and extraction root differ from
the older `KoboRoot.tgz`; neither version has a guaranteed survival mechanism
for a full-rootfs replacement.

## Feature coverage

| Older v0.6.0 feature | NickelMenuNG on 6.0.279225 | How to enable or limit it |
|---|---|---|
| Main NickelMenu tab; reader, browser, library, selection, and selection-search menus | Implemented; main tab physically verified. Other menu locations retain their hooks but need a physical check on this firmware. | Add `menu_item` entries in `.adds/nm`; main tab is automatic. |
| Menu labels, ordering, separators, custom tab label/icons, and stock-tab visibility | Implemented from existing config and `experimental` options. | Configure in `.adds/nm`; some options need a reboot. |
| Action chains: success, failure, always, and `skip` | Implemented; disabled features fail normally so `chain_failure` can run. | Configure chains; `skip` stays available for recovery. |
| Selection substitution and transforms | Implemented, including the corrected `u` and `x` flags. | Use selection menu entries. |
| `cmd_spawn` and `cmd_output` | Implemented; command execution itself is not a Kobo 6 compatibility guarantee. | **Commands and output** switch; add config entries. |
| `dbg_syslog`, `dbg_error`, `dbg_msg`, `dbg_toast` | Implemented; always available for diagnostics and config errors. | Add config entries. |
| `kfmon`, `kfmon_id`, and KFMon menu generator | Implemented; requires KFMon installed separately. | **KFMon launchers** switch; add action or generator entries. |
| `_test` numbered menu generator | Implemented as a configuration test utility. | Add `generator:_test` with its location and count. |
| `nickel_setting`: invert, lockscreen, force Wi-Fi | Implemented with native ABI checks; hardware and preference effects need physical checks. | **Device settings** switch; add entries. |
| `nickel_setting:dark_mode` | Implemented; reader appearance passed the 6.0.274403 simulator test. | **Dark mode** switch; add entry. |
| Screenshot setting and `nickel_screenshot:capture` | Implemented; PNG capture passed the 6.0.274403 simulator test. The standalone capture stalled in the 5.18 simulator and needs diagnosis before claiming 5.x coverage. | **Screenshots** switch; add entry. |
| Automatic USB gadget setting; USB Connect and full rescan cycle | Implemented. Physical Connect mounted storage on 6.0.274403; disconnect once left a stale Connected screen. | **USB connection** switch; use normal Kobo USB Connect for installation. |
| `nickel_misc:rescan_books` | Implemented; physical effect unverified on 6.0.279225. | **Book rescan** switch; add entry. |
| Browser URL, modal browser, and CSS injection | Implemented with Qt 6 browser signature handling; full UI behavior unverified. | **Web browser** switch; add entry. |
| Kobo views: Home, Books, Authors, Series, Collections, Discover, Wishlist, Activity, Words, Store search, OverDrive and Instapaper | Implemented with symbol checks. Books and Activity opened in the 6.0.274403 simulator; other routes need checks. | **Kobo views** switch; add `nickel_open` or `nickel_misc:home` entries. |
| Dropbox and Google Drive shortcuts | Implemented for the exact N365 firmware guard. Both file lists, repeat entry, and provider switching passed physical checks on 6.0.279225. A fresh Google Drive EPUB downloaded and opened. | Independent **Dropbox** and **Google Drive** switches; account link, Wi-Fi and provider access still required. |
| `nickel_wifi` connect/enable/disable/toggle | Implemented; a QEMU network shim is not a physical Wi-Fi test. | **Wi-Fi** switch; add entry. |
| Orientation controls | Implemented with native ABI checks; physical behavior unverified on this firmware. | **Orientation** switch; add entry. |
| Sleep, reboot, shutdown | Implemented with native manager checks; physical behavior unverified on this firmware. | **Power controls** switch; add entry. |
| Uninstall marker and startup failsafe | Implemented; intentionally cannot be disabled in Settings. | Create `.adds/nm/uninstall` or use the existing failsafe. |
| Nickel Extras games and sketch pad | **Missing on Qt 6**; action returns an unsupported error. | Needs a new Qt 6 implementation; Settings cannot enable it. |
| Bluetooth actions | **Missing on Qt 6**; action returns an unsupported error. | Needs a new Qt 6 implementation and hardware checks. |
| Pocket library and Reading Life awards routes | **Missing on Qt 6**; routes return an unsupported error. | Firmware no longer exposes the old routes; Settings cannot enable them. |

The built-in **NickelMenu Settings** view has **Shortcuts**, **Menu items**, and
**Permissions** tabs. Shortcuts generate or remove ready-made menu items for
dark mode, screenshots, rotation, Dropbox, Google Drive, Wi-Fi, Sleep, Restart,
and Power off. Editing a generated item makes it a custom item; the shortcut
switch no longer owns it. Permissions control the supported action groups
above. All permissions default to on. Apply changes writes the hidden
`.adds/nm/.features.ini` file on onboard storage; Back or another bottom tab
asks before discarding unsaved changes.
Items whose actions are blocked by a permission appear dimmed and cannot be
activated in NickelMenu's menus. If any action in a chain is blocked, the whole
item is disabled. Re-enabling the permission restores the item when the menu is
next opened.
The Menu items editor appends ` *` to blocked rows and shows the explanation
below the list. This marker updates with unsaved permission switches and is
never written to the config file.
Long lists page in one step per swipe or scrollbar tap, regardless of Kobo model
or screen size. The native scrollbar appears only when a list has multiple pages.
The USB connection action is available in the custom item
editor, but is not a ready-made shortcut because it changes the device's
storage connection state.
The earlier popup opened on the physical N365 running 6.0.279225, and checkbox
choices persisted between openings. A disabled action has passed the 5.18
simulator check. On the physical N365, turning Dropbox off blocked its menu
action with the expected message and left the Kobo responsive; Dropbox was
then turned back on.
Turning a group off stops its NickelMenu action and displays a clear error,
including inside a chain. It does not change Kobo's own feature settings or
erase a provider link. Delete `.features.ini` to restore all defaults.
Debug actions, chaining and uninstall remain available so a bad choice cannot
lock the recovery path. This view does not turn missing Qt 6 actions into
working actions.

The **Menu items** tab reads the same `.adds/nm` files as NickelMenu. It shows
hand-written `menu_item` entries and writes changes back to their source file;
new entries go to `zz-nm-settings.conf`. It validates the entire config before
and after saving and refuses to overwrite a file changed while the view was
open. Duplicate prefills a new item and identical labels or actions are allowed.
Move up/down changes the order within a source file and carries its adjacent
`chain_*` lines with it. Editing a first action leaves its `chain_*` lines intact; removing the
item removes those chained actions. The 5.18 simulator verified adding and
running a new action, seeing a hand-edited label in both the menu and Settings,
and removing a chained item without a parse error. These UI edits have not yet
been tried on the physical N365. The 6.0.279225 simulator also opened the
two-tab editor and displayed it on a fresh, empty configuration. A later
native full-screen Settings view passed on the 6.0.279225 simulator: the
NickelMenu navigation tab was active, Kobo's Back button returned to the
previous tab, Kobo's keyboard entered text, and the native menu selector saved
`reader` to the config file. The first native view passed a physical touchscreen
check. Its revised layout and discard flow passed 5.18 and 6.0 simulator checks
and await a physical check. The menu and action choices now show readable names
while preserving their config keys. Apply changes forces a menu reload, so
edits appear without a reboot even when the onboard filesystem timestamp is
unchanged.

## Feasibility of the remaining old features

| Feature | Practical path on Qt 6 |
|---|---|
| Bluetooth controls | Plausible on Kobo models with Bluetooth hardware. The 6.0.279225 library exports `BluetoothManager::sharedInstance`, `up`, `scan`, and `stopScan`, but the old on/off entry points are absent. A new implementation needs the current power-control API and physical hardware checks. |
| Nickel Extras games and sketch pad | The old `ExtrasPluginLoader::loadPlugin` entry point is absent from 6.0.279225. Launching a separately maintained app is possible; restoring the old built-in games would require replacement UI and game code. |
| Pocket library | The old Pocket route is absent. The existing Instapaper route covers the supported article-library use case, but it is a different service. Restoring Pocket itself would require an independent integration. |
| Reading Life awards | The old route is absent. A new awards view could be built from available reading data, but it would be a new feature rather than re-enabling the old firmware screen. |
