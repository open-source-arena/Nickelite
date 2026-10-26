# NickelMenu actions on firmware 6.0.274403

Checked 2026-10-26 against Kobo Clara BW (N365). This report separates the
original offline simulator baseline from the later physical cloud candidate
and subsequent panic report.
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
| Dropbox | Earlier linking and native listing were reported as successful. A later owner report says using the service caused the reader to panic. **Qt6 action disabled in source.** | Exact trigger, failure stage, recovery state and data impact are unknown. Do not retest. |
| Google Drive | Earlier linking and native listing were reported as successful. A later owner report says using the service caused the reader to panic. **Qt6 action disabled in source.** | Exact trigger, failure stage, recovery state and data impact are unknown. Do not retest. |
| USB Connect | **PARTIAL.** Connect mounts the reader's storage. After safe eject and unplug, the stale Connected screen needs a short normal power press to restore the GUI. | Seamless disconnect recovery is not established; this is not a verified USB recovery fix. |

During the initial stall, neither service showed an error, link code, OAuth
screen or file list. The earlier linking/listing report is historical and does
not establish service safety. The later panic report supersedes it for current
guidance. No panic log or precise failure point is available, and no credentials,
authentication codes or private file identities were supplied or retained.

## Safety and current implementation

**Do not reproduce or retest cloud access on the physical reader.** The source
now rejects both Qt6 cloud actions before native workflow construction. Once the
reader is back at a normal screen, any source update must use the stock USB
installation path in the README; this does not authorize another cloud attempt.
The [original EPUB fixtures](test-fixtures/README.md) are retained for a future
test only after the native route is independently reviewed and re-enabled.

## Qt6 cloud routing status

The original Qt6 port used `MoreController` cloud methods, which check native
service visibility before showing provider workflows. The later implementation
bypassed those gates and called native Dropbox/Google Drive workflow managers
directly. The physical N365's cloud service flags had been observed false before
that restoration. This is a source-supported risk, not a proven cause of the
reported panic.

Both Qt6 cloud actions now return an error before resolving or constructing
native cloud objects. This is containment; it does not establish the cause of
the panic or a functional cloud fix. Qt5 routing is unchanged. Do not test these
actions on the device until the native route has been reviewed and re-enabled.

The earlier candidate's Go, Qt6 build and exact-firmware ARM checks do not cover
the later-reported panic. Updated checks verify the disabled-action message; they
do not execute cloud workflows. No device-side verification has been performed
for the containment change.

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
source changes supersede the baseline cloud-entry result. The earlier owner
report of linking/listing is superseded for safety guidance by the later panic
report; neither report establishes successful file transfer or sync.
