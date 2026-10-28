<h1 align="center">NickelMenuNG</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [website](https://pgaskin.net/NickelMenu) and [thread on MobileRead](https://mobileread.com/forums/showthread.php?t=329525) for screenshots and more details.

## NickelMenuNG: experimental Qt 6 build

This checkout targets Kobo's Qt 6 firmware, with **6.0.274403 and 6.0.279225**
as exact cloud-compatibility targets. It reuses [Nico Verbruggen's Qt 6 port](https://github.com/nicoverbruggen/NickelMenu/tree/1a15faa9d0de810cd96a1a53359e175be4c7c7d5)
on top of [upstream NickelMenu](https://github.com/pgaskin/NickelMenu/tree/bc799ebe95a413a4857aa97a88e2b16557e6427f),
and vendors its [NickelHook dependency](https://github.com/nicoverbruggen/NickelHook/tree/03abed0c8e6226c1fca75906b7906d5d11664055),
with a local fix to restore GOT page protection after hooking or rollback.
The original MIT license and authorship apply. This is an unofficial
development build. On a physical Clara BW (N365) running 6.0.274403, the
`09f8dc6` cloud navigation build was installed through `.kobo/Kobo.tgz`, the
archive was consumed, and Home remained responsive. Google Drive opened twice
from Home; Google Drive → Dropbox → Google Drive navigation also worked without
a panic. Fresh test EPUBs downloaded from both Google Drive and Dropbox and
opened with their expected content markers. The prior `a46` candidate panicked
on repeat Google Drive entry and was rolled back before this test. See [the
compatibility results](docs/action-compatibility-6.md) for that history and
remaining limits.
The 6.0.279225 full-rootfs update removed the NickelMenu plugin directory. A
new build passed ARM tests and linked-account cloud-list entry, repeat entry,
and provider switching in the QEMU userspace simulator; see the
[6.0.279225 report](docs/firmware-6.0.279225.md). The physical N365 loaded
NickelMenu after reinstall; repeat cloud entry and provider switching stayed
responsive. A successful build or symbol check does not establish
compatibility with other 6.x releases or devices.
The same build boots on the official Kobo 5.18.270971 firmware in an isolated
QEMU userspace simulator. The menu, Settings popup, saved feature switch, and
diagnostic action passed there; cloud access and screenshot capture have not
been established on 5.x. See the [5.18 simulator report](docs/emulator/README.md#firmware-518-settings-check-2026-10-27).

The repository contains source, the vendored NickelHook dependency, and local
build instructions. It excludes generated `dist/` packages, firmware payloads
and private test evidence. Rebuild before installing: the old local `dist/`
package was a baseline, not the later physical cloud-test candidate.

Use a Linux host with Docker, a POSIX shell and Git. The SDK build downloads
Ubuntu packages and the checksum-verified Qt 6.5.2 source, applies the included
Kobo patches, and takes tens of minutes. Allow substantial disk space for Qt
build intermediates. Run these commands from the repository root:

```sh
docker build -t nickelmenung-qt6-sdk -f toolchain/qt6/Dockerfile toolchain/qt6
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src nickelmenung-qt6-sdk \
  sh -c 'make clean && make NH_QT_MAJOR=6 -j8 all kobo'
```

The result is `Kobo.tgz`, containing only `imageformats/libnm.so`. Qt 6 firmware
extracts this archive under `/usr/local/Kobo`; use this filename when copying it
to `.kobo`. The Qt 5 `KoboRoot.tgz` package has a different extraction root.
To install on either listed firmware, back up `.adds/nm`, remove any existing
`.adds/nm/uninstall` marker, and copy the freshly built `Kobo.tgz` to the mounted
reader's `.kobo/` directory. Safely eject and disconnect so the reader can apply
it. Keep Qt5 and Qt6 packages separate.

Your existing `.adds/nm` configuration is retained, and the documentation file is
created there on first startup if missing. Create a separate text file such as
`.adds/nm/custom` for menu entries; do not edit `doc`. To uninstall, create
`.adds/nm/uninstall` and reboot. The plugin's startup failsafe remains enabled.
See [the configuration documentation](res/doc) to add menu entries. Errors are
shown in the menu when possible; startup errors appear in `logread` over SSH.
The built-in **NickelMenu Settings** view has **Shortcuts**, **Menu items**, and
**Permissions** tabs. Shortcut switches add or remove ready-made menu items for
dark mode, screenshots, rotation, cloud services, Wi-Fi, and separate power
actions. Permissions allow or block action groups, including custom items; they
default to on and are saved in `.adds/nm/.features.ini`. Deleting that hidden
file restores defaults. A menu item with any blocked action, including a
chained action, remains visible but appears dimmed and cannot be tapped. The
**Menu items** tab marks these entries with ` *` and explains the marker below
the list. The marker follows permission switches before Apply and is not saved
as part of the item label. The tab reads the
existing `.adds/nm` files. You can add, edit, duplicate, remove or move a menu item there;
changes made in a text editor appear when you reopen Settings. New items go
to `zz-nm-settings.conf`. Editing or moving an item keeps its chained actions,
while removing it removes the chain. Move up/down works within the item's source
config file. Duplicate prefills the editor and permits the same label and action.
**Apply changes** validates and writes the config,
then refreshes NickelMenu immediately; no reboot is needed. It rejects files
changed while the Settings view was open. The view fills the page above Kobo's
bottom navigation, shows readable menu and action names, and highlights the
NickelMenu tab while open. Back or another bottom tab asks before discarding
unsaved changes, including changes in the item editor. Its text fields use
Kobo's on-screen keyboard. Long lists turn one page per swipe or scrollbar tap;
the native thin scrollbar appears only when another page exists. Shortcuts and permissions use Kobo's toggle
switches. The selected menu item is shown as white text across a black row.
The NickelMenu tab uses separate [inactive](res/icon/NickelMenu-inactive.png)
and [active](res/icon/NickelMenu-active.png) 42 × 42 transparent PNGs unless
custom icons are configured.
System dialogs can appear above Settings. The USB
connection action remains available as a custom menu item; it is not a
ready-made shortcut because it changes Kobo's storage connection state. Back up `.adds/nm` before
substantial edits. See the
[v0.6.0 feature matrix](docs/feature-matrix.md) for coverage and limits.

The QEMU VNC simulator has a fixed portrait framebuffer. Its Rotate screen
action shows an explanatory message instead of cropping the display; rotation
on a physical Kobo still needs a separate check.

Extras, Bluetooth controls, Pocket and reading awards are unavailable in this
port. Actions check their required firmware symbols, native Qt allocation
metadata and Settings vtable slots before use. These checks do not prove private
object-layout compatibility on a new firmware release. Hardware actions and
menu behavior require testing on the device.
The port also corrects selection substitution to follow the documented `u`
(replace whitespace with underscores) and `x` (reject empty text) flags, and
escapes backslashes and control characters correctly in JSON substitutions.

Source tests require Go 1.25.4 or newer (see each module's `go.mod`); the first
run downloads its Go modules. Run `cd test/syms && go test ./...`. Check a locally
extracted firmware without downloading historical fixtures using:

```sh
cd test/syms
go run . -src ../../src -version 6.0.274403 -lib-dir /path/to/firmware-libraries
```

The library directory must contain `libnickel.so.1.0.0` and the firmware's Qt
libraries under their SONAMEs, including `libQt6Gui.so.6`. Extracting just the
Kobo directory does not include the Qt libraries from `/usr/lib`.

The loader regressions run with `cd NickelHook/tests && go test ./...`.
Build the ARM production-source regression and firmware metadata test with
`make NH_QT_MAJOR=6 test/qt_compat test/compat` inside the SDK container.
`test/qt_compat` can run under `qemu-arm` with the extracted firmware sysroot;
`test/compat` additionally needs the firmware's Kobo library directory on
`LD_LIBRARY_PATH`. Neither test starts Nickel or accesses device hardware.
The firmware is supplied locally; see [firmware preparation](docs/firmware-6-analysis.md).

[Simulator notes](docs/emulator/README.md) describe the observed genuine-Nickel
userspace test and the included VNC/startup scripts. A simulator image builder
is not included; a fresh clone alone cannot launch that prepared local guest.

## Legacy Qt 5 build

The default Makefile still targets firmware 4.x using [NickelTC](https://github.com/pgaskin/NickelTC).
Keep its build separate from Qt 6 objects:

```sh
make clean
docker run --rm --entrypoint make --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src ghcr.io/pgaskin/nickeltc:1.0 all koboroot
```

Use `KoboRoot.tgz` only for the legacy Qt 5 firmware.
