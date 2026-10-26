# NickelMenu actions on firmware 6.0.274403

Checked 2026-10-26 against Kobo Clara BW (N365). This report separates the
original offline simulator baseline from the later physical cloud candidate.
Generated logs, screenshots, settings, test EPUBs and firmware disassembly are
excluded from publication; the observations below are aggregate findings.

## Current physical checkpoint

The reviewed physical candidate was package SHA256
`1d5f593f7676350d01a1b692e85eea81df6546ec8851bcd5dc9b4ca72ac9b796`,
containing library SHA256
`5955649c5beec61e5b1e4bfc7c29ab96b8247439411406a179ef679b1309d855`.
The package/configuration were staged and verified before normal unmount. The
owner reported a responsive GUI and the `5955649c` Cloud test menu marker.
That marker confirms the configuration/boot checkpoint; there was no readback
of the installed rootfs library hash. These are owner-reported UI observations,
not a complete recording or an assertion that future rebuilds have those hashes.
The old local `dist/` package is the earlier baseline and is not this candidate.

| Feature | Physical outcome | Remaining limit |
|---|---|---|
| Dropbox | **Entry, authentication/linking and native listing PASS by owner report.** Get Started initially showed Loading for more than one minute; Back worked. The stall recovered and the account linked successfully with its file list visible. | Fixture download/open, byte preservation and actual file sync are unverified. |
| Google Drive | **Entry, authentication/linking and native listing PASS by owner report.** Get Started initially showed Loading for more than one minute; Back worked. The stall recovered and the account linked successfully with its file list visible. | Fixture download/open, byte preservation and actual file sync are unverified. |
| USB Connect | **PARTIAL.** Connect mounts the reader's storage. After safe eject and unplug, the stale Connected screen needs a short normal power press to restore the GUI. | Seamless disconnect recovery is not established; this is not a verified USB recovery fix. |

During the initial stall, neither service showed an error, link code, OAuth
screen or file list. The owner subsequently confirmed that **both services
linked successfully and displayed their file lists**. This supersedes a
continuing-blocker interpretation; the transient stall's cause remains unknown.
No credentials, actual authentication codes or private file identities were
supplied or retained. Linking and listing do not establish successful file
transfer, byte preservation or sync.

## Minimal reproduction and diagnostics

Use the exact firmware and a normal profile with working native Wi-Fi. Do not
change account/profile restrictions to make the test pass. Add a separate
`.adds/nm/cloud-check` text file with:

```text
menu_item:main:Dropbox check:nickel_open:library:dropbox
menu_item:main:Google Drive check:nickel_open:library:gdrive
```

1. Record firmware version and the SHA256 of the newly built package and payload.
   The README describes rebuilding and installation.
2. Open one entry and confirm the provider's native Get Started screen. Press
   Get Started, wait at least 60 seconds, and record whether Loading completes,
   an error appears, or a link code/OAuth screen appears. Test Back, then repeat
   for the other provider. Keep entry and authentication results separate.
3. If the earlier stall recurs, record elapsed time, Wi-Fi connection state and whether another
   ordinary native network action works. If a stock provider entry is available,
   compare its result. Do not infer provider reachability from Wi-Fi alone.
4. If SSH is already available, inspect `logread` locally around the reproduction.
   Retain only relevant errors; redact account identifiers, tokens, codes and
   network details before sharing. No device reset, sign-out or firmware rollback
   is needed for this diagnostic.

The [original reusable EPUB fixtures](test-fixtures/README.md) provide a bounded
future transfer test without private content. Record
download/open separately from byte-hash comparison and actual sync behavior.
Only successful linking and visible native file lists have been reported; no
private account contents are recorded here and folder-wide sync or Download all
was not tested.

## Qt6 cloud routing and guard

The original Qt6 port used `MoreController` cloud methods, which check native
service visibility before showing the provider workflow. Both entries silently
returned on the physical N365 before the restoration. The device's cloud service
flags were false; its sideloaded-mode key was absent. That differs from the
simulator's explicit `SideloadedMode=true` and does not establish an identical
cause on the physical device.

Qt6 now calls the native Dropbox/Google Drive workflow managers directly and
retains their native `QSharedPointer` handles for asynchronous callbacks. The
native Wi-Fi/authentication implementation remains responsible for the flow.
The shared guard checks effective demo, child-account, parental-control and
sideloaded-profile restrictions and fails closed when required symbols or native
metadata are missing. No account/token state or saved preferences are fabricated.
Qt5 routing is unchanged.

Local Go regressions, isolated Qt6 build and exact-firmware ARM checks passed for
the reviewed candidate. ARM checks cover profile denials, native User construction
and destruction, both workflow QObject classes, and shared-handle ownership.
These checks validate ABI/guard behavior, not service availability. An earlier
unguarded candidate reached native network handling in the offline guest; the
final guarded Dropbox action correctly rejected its sideloaded profile.

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
build; the physical checkpoint hash above identifies the earlier tested binary.

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
source changes and current physical observations supersede the baseline cloud
entry result, while the later owner report establishes linking/listing for both services.
File transfer and sync remain unverified.
