# NickelMenu actions on firmware 6.0.274403

Updated 2026-10-27 against Kobo Clara BW (N365), firmware 6.0.274403. This
report separates the offline simulator baseline, physical checks on the older
installed build, and repeat-open testing of the installed source candidate.
Generated logs, screenshots, settings, test EPUBs and firmware disassembly are
excluded from publication; the observations below are aggregate findings.

## Current physical checkpoint: delayed cloud route

The `09f8dc6` source candidate was built for Qt6 without the emulator-only
Wi-Fi patch. Package SHA256:
`5fe9a7fed54a7eb6247f3b10d8e406f888afcc1e5a541330fd96b00e953db86f`.
The package was copied to `.kobo/Kobo.tgz` on the physical N365, read back
with a matching hash, and safely unmounted. After the owner saw the update and
responsive Home, a second USB mount showed `Kobo.tgz`, `KoboRoot.tgz`, and
`upgrade` absent. The rootfs library bytes were not read back.

The owner then opened Google Drive from NickelMenu twice, returning to Home
between entries. Both file lists stayed responsive for about 30 seconds. From
the Google Drive list, NickelMenu opened Dropbox, then returned to Google
Drive; both lists stayed responsive. The previously uploaded Google Drive test
EPUB downloaded and opened in My Books with the expected `WALNUT` marker.
The fresh Dropbox test EPUB was uploaded to `Apps/Rakuten Kobo`, downloaded
through NickelMenu → Dropbox, and opened with its unique `ACORN` marker.
This verifies the previously failing repeat and provider-switch paths on this
device and firmware, plus one file transfer from each provider. Full library
sync and other Kobo models or firmware versions remain unverified.

## Earlier physical checkpoints

Physical actions were performed on the previously installed build, identified
by the `5955649c` marker. Dropbox initially opened, and its fixture downloaded
and opened successfully. Google Drive then caused a panic/restart after leaving
Dropbox; repeated Google Drive selections caused the same result, and later
Dropbox selections also caused panic/restarts.

The newer source candidate package SHA256
`a46f70b8a50aa36ca57a96e73854d569472f15f0ead5bbd0e5292d339fbaa657` (library
SHA256 `0abe16343f5f36239037b149eac8144706d60ceadf72d7b7074b2eaef5fc19a9`)
was copied to `.kobo/KoboRoot.tgz`. Firmware 6 expects `.kobo/Kobo.tgz`, so
that first staging remained unconsumed and was not installed; that attempt had
no physical test result. The c980d6c containment package was later staged with the
correct filename, `.kobo/Kobo.tgz`. After update, both the package and mistaken
`KoboRoot.tgz` were absent at the next USB mount; root confirmed normal unmount
and rollback consumption. The rootfs library was not read back.

The a46 package was then staged again under the correct `.kobo/Kobo.tgz` name
for a later physical test.
Its SHA256 was reported matched on USB before normal unmount. The owner saw the
update complete and a responsive Home screen. A second USB check found
`Kobo.tgz`, `KoboRoot.tgz`, and `upgrade` absent, confirming the update archive
was consumed. This is the first physical installation of a46. From Home, Google Drive opened
its list and the device stayed responsive. Selecting Google Drive a second time
caused a panic/restart. After that restart, selecting it again caused another
panic/restart. Root stopped further cloud tests and restored the c980d6c
containment build before testing the delayed `09f8dc6` candidate above.

The exact physical library bytes were not read back from the device rootfs.
The package hash identifies the archive staged over USB; it does not prove what
the device loaded after update.

The earlier physical package was SHA256
`1d5f593f7676350d01a1b692e85eea81df6546ec8851bcd5dc9b4ca72ac9b796`,
containing library SHA256
`5955649c5beec61e5b1e4bfc7c29ab96b8247439411406a179ef679b1309d855`.
The owner reported a responsive GUI and the `5955649c` Cloud test menu marker;
this identifies the old installed build involved in the earlier Dropbox and
provider-transition reports. The a46 candidate was unconsumed during those
earlier tests, was later installed and failed on Google Drive reentry, then was
rolled back to c980d6c. The old local `dist/` package is the offline simulator
baseline.

| Feature | Physical outcome | Remaining limit |
|---|---|---|
| Dropbox | **09f8dc6: PASS list/navigation and fresh EPUB download/open.** Opened from Google Drive and remained responsive; the new fixture showed its `ACORN` marker. | Full library sync and other firmware/device combinations remain unverified. |
| Google Drive | **09f8dc6: PASS repeat entry, provider switching, and test EPUB download/open.** The prior a46 build panicked on repeat entry. | Longer use and other firmware/device combinations remain unverified. |
| USB Connect | **PARTIAL.** Connect mounts the reader's storage. After safe eject and unplug, the stale Connected screen needs a short normal power press to restore the GUI. | Seamless disconnect recovery is not established; this is not a verified USB recovery fix. |

The c980d6c containment rollback package SHA256 is
`e01f0389df1c76f842b3bead50178a5ca51a9cb9bc7240a30386c12a71bdf03e`. It was
staged as `.kobo/Kobo.tgz` and its consumption was confirmed after the normal
stock-USB update. The owner selected Dropbox, received the expected disabled
message, and confirmed Kobo remained responsive with no panic. Google Drive's
disabled-message behavior has not been checked. The a46 package was then
installed from the correct `.kobo/Kobo.tgz` path and consumption was confirmed
on a second USB mount. The first Google Drive entry opened its list; its second
selection and a further selection after restart caused panic/restarts. Root stopped testing and restored containment; Kobo is responsive. The earlier four and five stack
traces came from the old 595 build. In the candidate-era log set, records 00–04
are unchanged old records; only records 05 and 06 are new. Those two contain
SIGSEGV traces in Qt's shared-pointer/event-dispatch path. Raw logs are private
and excluded from this repository. No credentials, authentication codes or
private file identities were retained in this report.

## Safety and current implementation

The source candidate calls Kobo's `MoreController` methods and retains one
controller as an application child for asynchronous callbacks. It overrides
the model capability checks only when `libnickel` matches firmware 6.0.274403
(stock or the exact two-method model patch described below) and the device
reports `SpaKoboBW`. It does not change user-owned `FeatureSettings` or
`OneStoreServices` values. The immediate-call a46 candidate panicked on Google
Drive reentry and was rolled back. The current `09f8dc6` build delays the call
until after the NickelMenu touch popup closes; it passed the physical checks
listed above.

## Crash evidence

The four earlier records are Nickel user-space `SIGSEGV` reports, not kernel
panic traces. Three stop in Qt 6.5.2's gesture-event path at
`QGesture::gestureType()`; the firmware-matched implementation dereferences a
null private-data pointer. The fourth stops in Qt's shared-object reference
path while processing an event. The top frames are Qt and GLib event dispatch;
they do not identify a NickelMenu or `libnickel` caller. This localizes the
failure to invalid Qt gesture/object state during UI dispatch, but does not
identify the object that supplied it or prove a specific source-level cause.

The earlier four records and a later set of five pre-candidate records from
the old installed build show SIGSEGV/SIGBUS in Qt event dispatch. In the candidate-era
log set, records 00–04 are unchanged old records. New records 05 and 06,
timestamped 11:25 and 11:31, show
SIGSEGV in `QtSharedPointer::ExternalRefCountData::getAndRef(QObject const*)`,
followed by `QApplication::notify` and Qt event dispatch. Both also contain the startup message “target firmware recognized; N365 model
check remains active” and Google Drive native-method “invoking” and “method
returned” markers. This confirms the firmware guard was recognized; the log does
not say whether the model predicate returned true. The methods returned before
these crashes were recorded; logs do not show which QObject is invalid or identify
a source-level cause. The signature is consistent with invalid Qt shared-pointer
state during event handling, but does not prove controller reentry caused it.
The physical sequence ties the crashes to repeated Google Drive selection; the
logs themselves do not encode taps. Raw logs remain private.

On 2026-10-27, a linked-account, networked QEMU guest exercised the exact
NickelMenu cloud actions for the first time. A source candidate defers the
native `MoreController` call by 250 ms so the NickelMenu touch popup can
finish closing. In QEMU, two Google Drive entries from Home and a Google
Drive → Dropbox → Google Drive switch opened the expected file lists without
a fatal signal. A same-guest control run with the original immediate-call
plugin logged a view push but remained on Home and ignored the next tab tap.
This supports a menu-event timing cause; the exact QObject responsible for
the earlier physical SIGSEGV remains unidentified. The delayed build also
passed the physical repeat and cross-provider checks above.

## Qt6 cloud routing status

The prior Qt6 candidate called the native Dropbox/Google Drive workflow
managers directly, bypassing `MoreController`'s provider checks. Exact-firmware
disassembly shows those checks call `DropboxItemManager::isDropboxEnabled()` or
`GDriveItemManager::isGoogleDriveEnabled()` before showing a workflow. Those
checks also consult `Device::supportsDropbox()` or
`Device::supportsGoogleDrive()`. The candidate overrides only those model
predicates for N365 on the recognized target firmware, mirroring the approach
in the historical [Kobo patch PR #139](https://github.com/pgaskin/kobopatch-patches/pull/139).
That PR targets firmware 4.38.21908 and also requires owner-managed
`FeatureSettings` and `OneStoreServices` values; it does not establish behavior
on Qt6 or explain this crash. NickelMenu leaves those settings untouched. The
immediate-call MoreController route plus scoped capability override was installed
on the N365, then rolled back after repeat entry panicked. The native call had
returned before those crashes. The controller is parented to the application,
and source lookup reuses the same named child if it exists; logs do not prove
which object was invalid. The delayed `09f8dc6` route passed repeat entry,
provider switching, and one fixture transfer/open from each cloud provider on
the physical N365.

Upstream [NickelMenu issue #143](https://github.com/pgaskin/NickelMenu/issues/143)
contains separate reports of Google Drive remaining in a loading loop on Clara
BW; one commenter reported rebooting cleared that loading loop. The maintainer
also states NickelMenu intentionally does not modify `Kobo eReader.conf`. That
issue does not describe the repeated panic/restart reported here and does not
establish that rebooting fixes it.

Qt5 routing is unchanged. The Qt6 candidate logs whether its exact firmware
guard is active, which provider's native method is invoked, and whether the call
returned; it does not log account, folder, or file data.

The earlier Go, Qt6 build, symbol, and offline QEMU checks did not cover the
reported panic. The later linked-account QEMU test exercised NickelMenu's
actual cloud actions, and the physical `09f8dc6` test covered the failing
repeat and provider-switch sequence. Existing fail-closed tests still cover
missing native MoreController symbols. The original offline QEMU guest had no
linked accounts, so its blank-view result remains only a baseline.

## Publication verification (2026-10-26)

The publication lead reran `go test ./...` in `test/syms` and
`NickelHook/tests`; both passed. The cached SDK also completed:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src nickelmenung-qt6-sdk \
  make NH_QT_MAJOR=6 -j8 all kobo test/qt_compat test/compat
```

The ARM binaries were built, not executed in this publication pass: the required
QEMU ARM interpreter/firmware sysroot were not supplied for this check. A full
Qt5 build was not repeated. Earlier runtime results below retain their original
scope. Updating embedded `res/doc` changes the library hash and requires a fresh
build; the a46 hash above identifies the archive that was installed on the
physical reader. The earlier Dropbox fixture pass and later old-build
panic/restarts were on the 595 build. The c980d6c containment period included a
passing Dropbox disabled-message check. On a46, Google Drive's first list entry
opened, but repeated entry panicked/restarted; no candidate fixture download or
Dropbox test was performed on a46. Root restored containment before installing
and testing `09f8dc6`, whose results are recorded at the top of this report.

## Original offline simulator baseline

The [simulator](emulator/README.md) ran genuine Nickel under QEMU userspace
emulation with no linked accounts and no external network. Its baseline library
SHA256 was `8915f2cb08e738a25e4c5ebf3871c04969bbc68419f08038edc74179da5d56e6`;
package SHA256 was `9f4861e5e6dfe150dd65ed43539011797d10ad02166bd0d9310359d2c7f36cee`.
Those identities predate the cloud restoration.

| Feature | Baseline observation |
|---|---|
| Screenshot | PASS in simulator: native capture produced an inspected 1072 × 1448 PNG of Books; library and reader captures restored `Screenshots=false`. |
| Reader dark mode | PASS in simulator: an original local EPUB visibly changed to white text on black, then back to black text on white. The native preference changed true, then false. Restart persistence was not tested. |
| Activity / Books | PASS in simulator: visible native Activity and Books screens opened, beyond handler return success. |
| Dropbox / Google Drive | Dispatch returned success but no cloud page appeared. Exact-firmware inspection identified a sideloaded-profile gate, consistent with this guest's explicit setting. No account or network behavior was tested. |
| USB, Wi-Fi hardware, suspend, power, e-ink | Not modeled or validated by the simulator. |

Private firmware inspection supported the baseline gating explanation; it did not
trace the exact executed branch or demonstrate an empty landing-page URL. The
source changes supersede the baseline cloud-entry result. The earlier owner
report of linking/listing is superseded for safety guidance by the later panic
report; neither report establishes successful file transfer or sync.
