<h1 align="center">NickelMenuNG</h1>

The easiest way to launch custom scripts, change hidden settings, and run actions on Kobo eReaders.

See the [website](https://pgaskin.net/NickelMenu) and [thread on MobileRead](https://mobileread.com/forums/showthread.php?t=329525) for screenshots and more details.

## NickelMenuNG: experimental Kobo 6.x build

This checkout targets Kobo's Qt 6 firmware, with **6.0.274403** as the validation
target. It reuses [Nico Verbruggen's Qt 6 port](https://github.com/nicoverbruggen/NickelMenu/tree/1a15faa9d0de810cd96a1a53359e175be4c7c7d5)
on top of [upstream NickelMenu](https://github.com/pgaskin/NickelMenu/tree/bc799ebe95a413a4857aa97a88e2b16557e6427f),
and vendors its [NickelHook dependency](https://github.com/nicoverbruggen/NickelHook/tree/03abed0c8e6226c1fca75906b7906d5d11664055),
with a local fix to restore GOT page protection after hooking or rollback.
The original MIT license and authorship apply. This is an unofficial development
build, tested locally against that exact firmware and on one Clara BW (N365).
The owner first reported successful Dropbox and Google Drive linking and file
listing, then reported that using both cloud services caused the physical reader
to panic. The cause and exact failure stage are unknown. **Qt6 Dropbox and Google
Drive actions are disabled in this source. Do not invoke cloud actions from an
older installed build.** Once the reader is back at a normal screen, update only
through the stock USB installation path described below; do not sign out, reset,
or use firmware recovery. See [the compatibility results](docs/action-compatibility-6.md).
A successful build or symbol check does not establish compatibility with other
6.x releases or devices.

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
To install on the tested firmware, back up `.adds/nm`, remove any existing
`.adds/nm/uninstall` marker, and copy the freshly built `Kobo.tgz` to the mounted
reader's `.kobo/` directory. Safely eject and disconnect so the reader can apply
it. Keep Qt5 and Qt6 packages separate.

Your existing `.adds/nm` configuration is retained, and the documentation file is
created there on first startup if missing. Create a separate text file such as
`.adds/nm/custom` for menu entries; do not edit `doc`. To uninstall, create
`.adds/nm/uninstall` and reboot. The plugin's startup failsafe remains enabled.
See [the configuration documentation](res/doc) to add menu entries. Errors are
shown in the menu when possible; startup errors appear in `logread` over SSH.

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
