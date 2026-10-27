# Local Nickel userspace simulator

The original local test ran genuine Kobo **6.0.274403** Nickel under QEMU ARM
user-mode emulation with NickelMenuNG installed. It exercised the native UI,
plugin loading and selected actions. This is a userspace approximation, not a
booted Kobo board. See [action results](../action-compatibility-6.md) and
[full-system feasibility](../emulator-board-feasibility.md).

## What a fresh clone provides

This repository includes the production SDK builder, an emulator-only VNC
backend patch script, and a guest startup wrapper. **It does not include a
simulator Dockerfile/image builder or a prepared firmware rootfs.** The original
container, mounted storage and browser tools were local session artifacts under
`/tmp/nickelmenu-emulator`; a host reboot may remove those files. There is no
fresh-clone simulator launch command yet. The commands below reuse an already
prepared guest and describe the missing prerequisites without assuming that the
original host cache is available.

To recreate the guest, an integrator must supply:

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
  `CurrentLocale=en_US` and `SideloadedMode=true`. Do not copy real credentials or
  an account database. Mount writable scratch user storage separately from the
  image, so it survives container restarts.
- Product environment `PRODUCT=spaKoboBW`, `PLATFORM=MT8113T-ntx`,
  `NICKEL_HOME=/mnt/onboard/.kobo`, `LD_LIBRARY_PATH=/usr/local/Kobo`, and
  `QTWEBENGINE_DISABLE_SANDBOX=1`. The original environment also used
  `INTERFACE=wlan0`, `WIFI=MT8113T`, `WIFI_MODULE=wlan_drv_gen4m`, empty
  `QT_IM_MODULE`/`QT_QPA_GENERIC_PLUGINS`, and
  `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --no-zygote --single-process --disable-gpu`.
  Those Chromium flags were confined to the isolated offline guest.

The battery mount layout, rootfs assembly and image/container creation steps are
not automated here. Set the working directory to `/usr/local/Kobo` and install
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
menu_item:main:Simulator check:dbg_msg:NickelMenuNG is running in the isolated QEMU guest.
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

Wi-Fi, RTC, framebuffer ioctls, e-ink updates, suspend, power and USB hardware are
not modeled. In an isolated offline guest, the Qt6 source candidate opened Google Drive
to a blank content area, Escape returned to Books, and a second entry opened the same
blank area. The Nickel process remained running after the second entry, and the VNC test
command completed successfully. This does not model the network or service
state that may trigger the physical candidate
repeat-open panic/restart. The emulator did not test service listing, login,
downloads or sync. See
[the compatibility report](../action-compatibility-6.md) for the current status.
