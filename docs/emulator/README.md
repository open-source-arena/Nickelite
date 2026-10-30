# Local Nickel userspace simulator

The original local test ran genuine Kobo **6.0.274403** Nickel under QEMU ARM
user-mode emulation with the project now called Nickelite installed. It exercised the native UI,
plugin loading and selected actions. This is a userspace approximation, not a
booted Kobo board. See [action results](../action-compatibility-6.md) and
[full-system feasibility](../emulator-board-feasibility.md).

## Nickelite 1.2.0 build checks (2026-10-30)

The Qt 6 cross-build completed with `make -j8 all kobo test/qt_compat test/compat`.
`test/compat` and `test/qt_compat` passed in the 6.0.279225 guest; both also
passed in the 5.18.270971 guest with
`NM_TEST_UNSUPPORTED_FIRMWARE=1` for the firmware guard. The UI smoke check could not be completed because the available QEMU guests
rendered a blank screen. A physical N365 check confirmed Home and all three
Settings views remained responsive, KOReader and KFMon rows were hidden when
both launchers were absent, and Menu items spacing was even. The Menu items and Permissions screenshots remain 1.1.1 simulator captures; the
Shortcuts screenshot now documents the physical 1.2.0 absent-KOReader state. A
physical KFMon-installed check remains unavailable:
KFMon’s upstream package ships a rootfs `KoboRoot.tgz`, while firmware 6’s
`Kobo.tgz` overlay targets `/usr/local/Kobo`, and did not install the daemon
at its expected `/usr/local/kfmon` path. No KFMon payload is included in Nickelite.

## Nickelite 1.1.1 screenshot check (2026-10-29)

The Shortcuts, Menu items, and Permissions screenshots were recaptured on the
6.0.279225 guest with the 1.1.1 UI. The Menu items button grid now explicitly
uses 6 px vertical spacing. A temporary simulator-only startup hook opened
Settings because the existing guest was asleep; that hook was removed before
building the release package. The main menu and native More screenshots still
match the current default menu and cloud entries.

## Nickelite 1.1.0 KOReader shortcut check (2026-10-29)

The 1.1.0 binary loaded on the 6.0.279225 guest. Shortcuts displayed the new
KOReader switch; enabling it and applying wrote
`menu_item:main:KOReader:cmd_spawn:quiet:exec /mnt/onboard/.adds/koreader/koreader.sh`
to `.adds/nm/zz-nm-settings.conf`, and the menu item appeared immediately.
The launcher script command ran under an explicit QEMU ARM interpreter. This
userspace guest cannot test `QProcess::startDetached` because the container
cannot directly execute its ARM `/bin/sh`; tapping the item displayed a process
start error. KOReader itself was not installed or launched in this guest.
The 1.1.0 ARM compatibility tests passed in the 5.18.270971 guest with its
unsupported-firmware expectation, and in the 6.0.279225 guest.

The image builder also pins official Clara BW **5.18.270971** and **6.0.279225**
firmware. Set `NM_FIRMWARE_VERSION=5.18.270971` for the 5.x image. Its SHA-256
checks are in [build-image.sh](build-image.sh); firmware stays outside Git.

## What a fresh clone provides

This repository includes the SDK builder, emulator-only VNC patch, guest
startup wrapper and an image builder that downloads and verifies the pinned
official firmware. It does not include Kobo firmware, a prepared image or
account data. For a fresh-clone, outbound-network guest with cloud linking,
follow [real-cloud.md](real-cloud.md). The commands below also document the
older offline guest and manual plugin installation.

For the older offline guest, an integrator must supply:

- Linux with Docker and an extracted, locally obtained 6.0.274403 rootfs, including
  Nickel, Qt libraries/plugins, its ARM loader/libc, fonts and runtime resources.
  [Firmware notes](../firmware-6-analysis.md) identify the archive and show partial
  extraction for static checks; that partial tree is not a complete simulator.
- A host-native static QEMU ARM interpreter at `/qemu-arm-static` inside the
  image, with every ARM program launched through it. No binfmt registration was
  assumed by the original startup wrapper.
- The VNC backend rebuilt below, installed as the guest's Qt platform VNC plugin;
  the shipped Kobo backend did not repaint correctly in this setup.
- Writable runtime directories, a normal firmware environment, and synthetic
  read-only battery files reporting 100% / Full at the paths Nickel reads.
  Use a dedicated tmpfs-backed user-storage mount at `/mnt/onboard`: Docker
  overlay inode numbers exceeded ARM32 `dirent` limits in the original test.
- A minimal `/emulator-offline.conf` with `[ApplicationPreferences]`,
  `CurrentLocale=en_US` and `SideloadedMode=true`. The real-network builder
  instead uses `SideloadedMode=false` for account setup. Mount writable scratch
  user storage separately from the image, so it survives container restarts.
- Product environment `PRODUCT=spaKoboBW`, `PLATFORM=MT8113T-ntx`,
  `NICKEL_HOME=/mnt/onboard/.kobo`, `LD_LIBRARY_PATH=/usr/local/Kobo`, and
  `QTWEBENGINE_DISABLE_SANDBOX=1`. The original environment also used
  `INTERFACE=wlan0`, `WIFI=MT8113T`, `WIFI_MODULE=wlan_drv_gen4m`, empty
  `QT_IM_MODULE`/`QT_QPA_GENERIC_PLUGINS`, and
  `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --no-zygote --single-process --disable-gpu`.
  Those Chromium flags were confined to the isolated offline guest.

The older battery mount layout, rootfs assembly and image/container creation
steps were manual; [the real-network scripts](real-cloud.md) now automate their
equivalents. Set the working directory to `/usr/local/Kobo` and install
[start-guest.sh](start-guest.sh) as `/emulator-start.sh`; the original entrypoint
was `/qemu-arm-static /bin/sh /emulator-start.sh`. That wrapper expects the
firmware's `/bin` and `/sbin` tools and creates the offline preferences only when
absent. It starts firmware syslog, then Nickel with VNC on port 5900.

Keep the guest on a Docker **internal** network, with no host device mappings,
capabilities or privileged mode. Bind any browser bridge only to loopback. The
original `nickelmenung-emulation` network and `nickelmenung-simulator` container
were manually prepared; these names do not imply they exist on another host.

## Rebuild the emulator-only VNC backend

From the repository root, build the SDK **build** stage, which retains Qt source
and intermediates. This downloads dependencies and builds Qt on a clean host;
a cached SDK is not required. The final production SDK omits those intermediates.

```sh
docker build --target build -t nickelmenung-qt6-sdk-build \
  -f toolchain/qt6/Dockerfile toolchain/qt6
docker run --rm --network none -v "$PWD/docs/emulator:/out" \
  nickelmenung-qt6-sdk-build sh /out/rebuild-vnc.sh
```

[rebuild-vnc.sh](rebuild-vnc.sh) writes `docs/emulator/libqvnc-repaint.so` for
local guest installation. It forwards Kobo's framebuffer repaint overload to
Qt's ordinary implementation and translates VNC left-button gestures into Qt
touch events, preserving one contact through right-button chord changes.
Run it against a fresh container from the build-stage image: its asserted patch
expects unmodified source. The root-run container may leave a root-owned output.
This emulator-only plugin does not belong in the production `Kobo.tgz` archive.

## Firmware 5.18 settings check (2026-10-27)

The later menu-editor candidate was also exercised on this guest. An action
added in Settings appeared in NickelMenu and ran. A hand edit to its config
line appeared in both the menu and Settings after reopening. Removing that
item in Settings removed its config line. A separate item with a `chain_success`
line was removed in Settings; its chain line was removed while nearby comments
remained, and the menu kept working. The same candidate's two-tab Settings
view opened on the 6.0.279225 sideloaded-mode guest. A later full-screen view
used Kobo's Back button, native keyboard and menu selectors; the 6.0 guest
saved a `reader` menu item with keyboard-entered text to its config file.
The same native view and selector opened on the 5.18 simulator. These editor
checks are simulator results, not physical touchscreen checks.

The revised view filled the area above the bottom navigation in both firmware
guests. In the 6.0 guest, an edited menu item appeared immediately after Apply
changes. A clean tab switch closed both the item editor and Settings. An
unsaved edit in the child editor or a changed feature switch in Settings showed
the native Discard changes confirmation on a tab switch. The 5.18 guest also
showed that confirmation. Human-readable menu and action names appeared in the
native selectors, with the original keys retained in config files.

The three-tab Settings build was then run on both the 5.18.270971 and
6.0.279225 guests. Shortcuts added Screenshot and Rotate screen to the Menu
items list; Apply made them appear in NickelMenu without restarting Nickel.
An earlier scrolling build showed an external, draggable vertical scrollbar on
each list. On 5.18, a 30-item `reader` fixture in a temporary simulator config
was scrolled to its final item. On both guests, the Permissions scrollbar
reached Book rescan. In 6.0, a diagnostic dialog opened while Settings was
visible; Settings yielded to the dialog and returned after OK. A physical USB
prompt and touch flick still require hardware verification.

On 2026-10-28, the 6.0.279225 simulator was updated to use the firmware's
`ArrowScrollBar` (including its native `TouchScrollBar`) and `ToggleSwitch`
controls in NickelMenu Settings. A VNC
mouse drag scrolled Permissions by pixels; the selected Menu items row reversed
the whole row to white on black. The NickelMenu tab first showed four bars;
the supplied 42 × 42 PNG then replaced that icon as the default. The tab now
uses the supplied inactive and active PNGs for their respective states. Because
the VNC platform has a fixed portrait framebuffer, Rotate screen now reports
that rotation is unavailable there and leaves the view uncropped. The revised
controls still need a physical touch check before installation on the N365.

The same two-icon build booted on the 5.18 guest and rendered the inactive tab
icon. That guest did not respond to pointer clicks, so its active state was not
visually confirmed there. The 6.0.279225 guest displayed both states.
The next 6.0.279225 Settings build copied the stock Energy saving and privacy
page's gray section headings, fixed heading above the list, page count and
native thin scrollbar. A 30-item disposable menu fixture showed five full
pages; a VNC swipe advanced one page, and gutter taps reached page five.
Permissions advanced from page one to two without elastic movement. Duplicate
prefilled the native editor, accepted an identical Dropbox label and action,
and a Move down tap changed the order in the saved guest config. These touch
gestures and controls still need a physical e-ink latency check.
The final paginated build also booted on the 5.18 guest and showed the
NickelMenu tab. Pointer input remained unresponsive there, so its Settings
controls could not be exercised on that guest.
On the 6.0.279225 guest, turning Dropbox's action permission off dimmed its
existing menu entry. Tapping the dimmed entry left the menu open and did not
run the action or show an error. Turning the permission back on restored the
normal label when the menu reopened.
The subsequent 6.0.279225 Settings build showed `Dropbox · NickelMenu tab *`
immediately after its permission was switched off, with a legend below the
list. The marker persisted after Apply and reopening Settings; the underlying
`menu_item` line did not gain an asterisk.
The next build replaced each tab's separate hint with descriptive text in its
title bar, reduced and dimmed the marker legend, and grouped menu items by
target. Duplicating Dark mode into While reading completed without a freeze;
Apply wrote `menu_item:reader:Dark mode:nickel_setting:toggle:dark_mode`.
The list paged with the While reading title and its item together on page 2.
The updated binary also booted in the 5.18.270971 guest, whose VNC pointer
input did not respond, so this UI flow was not verified there.
In an earlier build, the arrow scrollbar's native layout was adjusted to give the track more of the
list height. A VNC drag reached Book rescan without paging, and the down and up
arrows moved the list to its last items and back to the start. A fresh 5.18
guest booted and displayed the supplied icon, but pointer input stopped
responding even after the plugin was temporarily removed; this prevents a
reliable interaction check of this final build on that guest.

The 5.18.270971 Kobo firmware image booted with the same Qt 6 `Kobo.tgz`
build. On a fresh, sideloaded-mode onboard directory, the NickelMenu tab and
Settings popup appeared. Saving **Dark mode** off wrote
`.adds/nm/.features.ini` with `dark_mode=false`; the Dark mode menu action
then displayed the disabled-feature message. The diagnostic message action
worked, and Nickel remained responsive through those checks. The standalone
`nickel_screenshot:capture` test stopped before returning an action result in
this QEMU guest, so screenshot support on 5.18 is **unverified**. Cloud login,
sync, and USB hardware were not tested on 5.18. The ARM compatibility test
passed with `NM_TEST_UNSUPPORTED_FIRMWARE=1`, confirming the 5.18 binary does
not accidentally receive the N365 cloud capability override reserved for the
two reviewed 6.x binaries.

Build and run the isolated 5.18 image from a fresh clone after building the
Qt 6 plugin and VNC backend as above:

```sh
work="$HOME/nickelmenung-qemu-5.18"
NM_FIRMWARE_VERSION=5.18.270971 sh docs/emulator/build-image.sh \
  "$work" /absolute/path/to/Kobo.tgz /absolute/path/to/qemu-arm-static
NM_FIRMWARE_VERSION=5.18.270971 sh docs/emulator/run-real-network.sh \
  "$work" /absolute/path/to/noVNC-1.6.0 6096
```

For an account-free check, create
`$work/onboard/.kobo/Kobo/Kobo eReader.conf` with
`[ApplicationPreferences]`, `CurrentLocale=en_US`, and
`SideloadedMode=true` before launching. Add a disposable
`.adds/nm/simulator` menu item such as
`menu_item:main:Simulator marker:dbg_msg:Kobo 5.18`. The launcher binds both
battery names read by the 5.x and 6.x userspace. Its VNC patch translates
clicks to touchscreen events; rebuilding the older repaint-only VNC module
leaves the 5.18 interface visible but untappable.

## Reuse an already prepared guest

These commands require an existing `nickelmenung-simulator` container:

```sh
docker start nickelmenung-simulator
docker restart -t 2 nickelmenung-simulator
docker stop -t 2 nickelmenung-simulator
```

A noVNC browser client and Python `websockify` are separate host prerequisites.
The original host used noVNC 1.6.0. With those tools installed, find the current
internal-network address and start a loopback bridge in a terminal:

```sh
guest_ip=$(docker inspect --format '{{with index .NetworkSettings.Networks "nickelmenung-emulation"}}{{.IPAddress}}{{end}}' nickelmenung-simulator)
websockify --web /absolute/path/to/noVNC-1.6.0 \
  127.0.0.1:6087 "$guest_ip:5900"
```

Check that `guest_ip` is nonempty and belongs to the intended isolated network.
Open <http://127.0.0.1:6087/vnc.html?autoconnect=1&resize=scale>. Ctrl-C stops
the bridge. The address is read dynamically because Docker can reassign it.

## Install a newly built plugin in the prepared guest

Build `Kobo.tgz` using the root README; no `dist/` binary is supplied. With the
prepared guest running, from the repository root:

```sh
docker cp Kobo.tgz nickelmenung-simulator:/tmp/Kobo.tgz
docker exec nickelmenung-simulator /qemu-arm-static \
  /bin/tar -xzvf /tmp/Kobo.tgz -C /usr/local/Kobo
docker exec nickelmenung-simulator /qemu-arm-static \
  /bin/mkdir -p /mnt/onboard/.adds/nm
```

Create the following text file as `.adds/nm/simulator` in the guest's mounted
user-storage directory, then restart the container:

```text
menu_item:main:Simulator check:dbg_msg:Nickelite is running in the isolated QEMU guest.
```

Activate **NickelMenu → Simulator check** and confirm the native dialog. This
manual extraction tests plugin loading after a process restart; it does not
exercise Kobo's firmware updater, physical reboot or USB installation.

The historical baseline visibly showed Nickel's Books page, the injected menu,
a native message dialog, screenshots, reader dark-mode reversal and native
navigation. Generated screenshots/logs/preferences and test EPUBs are excluded
from publication. Their summary and the later cloud panic report are in
[action compatibility](../action-compatibility-6.md). A freshly rebuilt
plugin has not automatically inherited those historical runtime results.

Wi-Fi radio, RTC, framebuffer ioctls, e-ink updates, suspend, power and USB hardware are
not modeled. In an isolated offline guest, the Qt6 source candidate opened Google Drive
to a blank content area, Escape returned to Books, and a second entry opened the same
blank area. The Nickel process remained running after the second entry, and the VNC test
command completed successfully. This does not model the network or service
state that may trigger the physical candidate
repeat-open panic/restart. The emulator did not test service listing, login,
downloads or sync. The later real-network guest results are in
[real-cloud.md](real-cloud.md). See
[the compatibility report](../action-compatibility-6.md) for the current status.
