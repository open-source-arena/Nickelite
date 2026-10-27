# Recreate the real-network Kobo 6 simulator

This procedure reproduces the 2026-10-27 QEMU ARM userspace test of Kobo
6.0.274403 on an N365 profile. It uses Kobo's unmodified DNS, certificate
store, HTTPS endpoints and cloud device-link pages. QEMU has no Wi-Fi radio;
the **emulator-only** source patch reports connected state only after a
peer-verified TLS handshake with `www.kobo.com` succeeds. The Docker bridge has
ordinary outbound networking. It does not simulate a booted Kobo board, power,
USB hardware or the firmware updater.

The cloud plugin on this branch is experimental. The `09f8dc6` source was
verified on one physical N365 running 6.0.274403 after earlier builds panicked
on repeat cloud entry. Other devices and firmware versions are unverified.

## Prerequisites

Use a Linux host with Docker, Git, `curl`, `unzip`, GNU `tar`, `zstd`,
`e2fsprogs` (`debugfs`), `qemu-user-static`, `sha256sum`, `patch`, Python
`websockify`, and a noVNC checkout or distribution. Docker must be able to
reach the official Kobo firmware CDN and package repositories while building.
Allocate several gigabytes for the firmware, Qt SDK and image. Store the work
directory **outside this Git checkout**: its `onboard` subdirectory holds the
Kobo account and cloud tokens after sign-in. Never commit, copy or publish it.

Clone the private `Sojaner/NickelMenuNg` repository using your normal GitHub
access and check out `experiment/cloud-reentry-qt6`. All commands below run
from that checkout's root. Set your own work and noVNC paths; use a local,
dedicated directory for each fresh guest:

```sh
git switch experiment/cloud-reentry-qt6
work="$HOME/nickelmenung-qemu-6"
novnc="$HOME/noVNC"
mkdir -p "$work"
```

## Build the display backend and account-capable plugin

The VNC backend patch is required for repaint and touch input:

```sh
docker build --target build -t nickelmenung-qt6-sdk-build \
  -f toolchain/qt6/Dockerfile toolchain/qt6
docker run --rm --network none -v "$PWD/docs/emulator:/out" \
  nickelmenung-qt6-sdk-build sh /out/rebuild-vnc.sh
```

Build the experimental plugin in a disposable copy of this branch. The patch
is restricted to the exact N365 firmware hash and only activates when
`NM_QEMU_WIFI_CONNECTED=1`. It is intentionally absent from production source:

```sh
mkdir -p "$work/plugin-src"
git archive HEAD | tar -x -C "$work/plugin-src"
patch -d "$work/plugin-src" -p1 < docs/emulator/real-network-wifi.patch
docker build -t nickelmenung-qt6-sdk \
  -f toolchain/qt6/Dockerfile toolchain/qt6
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$work/plugin-src:/src" -w /src nickelmenung-qt6-sdk \
  sh -c 'make clean && make NH_QT_MAJOR=6 -j8 all kobo'
tar -tzf "$work/plugin-src/Kobo.tgz"
```

The last command must list only `imageformats/libnm.so`. Keep that archive in
the emulator work directory. This is an experimental test artifact, not a
release package.

## Assemble the image and run Nickel

The image builder downloads the pinned official Kobo ZIP, verifies both the
ZIP and compressed rootfs SHA-256 hashes, extracts the full ext4 filesystem
with `debugfs`, verifies `libnickel`, adds static QEMU and the rebuilt VNC
backend, then imports a local Docker image. It refuses to overwrite an
existing extracted rootfs. If your distribution puts `qemu-arm-static`
elsewhere, supply its absolute path as the third argument.

```sh
sh docs/emulator/build-image.sh "$work" "$work/plugin-src/Kobo.tgz"
sh docs/emulator/run-real-network.sh "$work" "$novnc" 6093
```

Open <http://127.0.0.1:6093/vnc.html?autoconnect=1&resize=scale> on the same
computer. Keep the second command running in its terminal. The browser bridge
listens only on `127.0.0.1`; it is not a public VNC endpoint. The Docker guest
has outbound access through `nm-real-cloud-egress`, with no host devices,
privileged mode, custom DNS, host overrides, CA replacement, or mock service.
The synthetic read-only battery files and a separate `onboard` mount are made
by the launcher. If your host filesystem gives ARM32 inode overflow errors,
put `work` on a tmpfs-backed filesystem with enough room; account data there
will disappear when that filesystem is cleared.

The startup wrapper creates `.kobo/Kobo/Kobo eReader.conf` only when absent.
It sets `SideloadedMode=false` for the normal Kobo account setup flow. Enter
your own account in the **simulated Kobo UI**. No password, link code, account
database, token, or browser profile belongs in this repository. The Wi-Fi icon
is a compatibility state, not an emulated radio. If the official TLS probe
fails, the shim stays disabled and Kobo's normal Wi-Fi scan cannot complete.

## Link and exercise the clouds

In the guest, complete Kobo account setup, open **More → My Dropbox** or
**More → My Google Drive**, and tap **Get Started**. Use the code shown on the
guest at the official `kobo.com/dropbox` or `kobo.com/googledrive` page in your
browser. Review each provider's access scope before approving it. The guest
should advance to a registration success screen and then the cloud file list.
Do not copy device codes or screenshots containing account details into Git.

For the repeat-entry check, leave each file list using its back arrow, open
the same service again, then switch to the other service. Check that each file
list appears and Nickel stays responsive. A linked list is a narrower result
than a successful file download, open, or sync; record those separately.

On 2026-10-27, this setup linked both services and displayed both file lists.
Google Drive also reopened after leaving its list, and Dropbox opened after
Google Drive. Google Drive downloaded the repository's
`NickelMenuNG-GoogleDrive-20261026T141758Z.epub` fixture, showed it in My Books,
and opened its page ending in `WALNUT`. This verifies a complete EPUB transfer
and reader path in this guest.

Downloading each provider's sample PDF produced its native "could not be
synced" error dialog, although both PDF files reached onboard storage with
valid PDF headers and end markers. Nickel logged a Foxit metadata warning and
`dropboxParseFailed`/`googleDriveParseFailed` with a `file path` error. The
cause of PDF import failure is not established. The guest stayed responsive,
and Google Drive still opened after the Dropbox error. These observations
apply only to the QEMU userspace guest. At that point the physical repeat-open
panic remained unresolved; the later delayed-route test below addressed it.
Dropbox EPUB import and a full library sync were not verified in this run.

### NickelMenu action route check (2026-10-27)

The earlier linked-account checks used Kobo's stock More menu. To exercise
NickelMenu's actual route, add this **emulator-only** file to
`$work/onboard/.adds/nm/cloud-reentry-test` and restart the guest:

```text
menu_item:main:QEMU Dropbox route:nickel_open:library:dropbox
menu_item:main:QEMU Google Drive route:nickel_open:library:gdrive
```

The original Qt6 cloud action invoked `MoreController` directly while the
NickelMenu touch-menu event was still unwinding. The QEMU guest logged a
`GDriveView` push but remained visibly on Home and ignored a More tab tap
after the same wait used for the candidate. This was confirmed by restoring
the original plugin in the same guest after the successful candidate run.
The candidate in `src/action_cc.cc` schedules that call 250 ms later. With
linked accounts and real network access, this candidate opened the Google
Drive file list twice from Home, switched Google Drive → Dropbox → Google
Drive through the NickelMenu actions, and remained responsive. The syslog
showed each native method return and each corresponding cloud view push, with
no fatal signal. The Kobo EPUB fixture had already downloaded and opened in
this guest through the stock cloud UI. A later physical N365 test also passed
repeat entry, provider switching, and fresh EPUB download/open from both
cloud providers; see
[the compatibility report](../action-compatibility-6.md).

## Reuse or remove the guest

The account-bearing data lives under `$work/onboard`, outside the image. Stop
the VNC bridge with Ctrl-C. On the next run, start the existing container and
bridge it to the current Docker IP:

```sh
docker start nm-real-cloud-wifi
ip=$(docker inspect --format '{{with index .NetworkSettings.Networks "nm-real-cloud-egress"}}{{.IPAddress}}{{end}}' nm-real-cloud-wifi)
websockify --web "$novnc" 127.0.0.1:6093 "$ip:5900"
```

`docker stop -t 2 nm-real-cloud-wifi` stops Nickel. Removing the container does
not remove `$work/onboard`; that directory contains the account session until
you explicitly remove it. Never place it in a Git worktree or shared backup.
