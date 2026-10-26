# Firmware 6 compatibility evidence

Investigation date: 2026-10-26. Target supplied by the user's device-information image: Kobo Clara BW, model N365, software 6.0.274403. No device serial or MAC address is retained here. The user subsequently requested 6.x support instead of 5.x.

## Authentic artifact and identity

The exact software image is downloadable from Kobo's HTTPS CDN:

https://ereaderfiles.kobo.com/firmwares/kobo12/Sep2026/qt6-update-6.0.274403.zip

The downloaded ZIP contains `update.tar`; that tar contains a Zstandard-compressed ext4 root filesystem. The filesystem was decompressed and read with `debugfs`, without mounting it, running Kobo programs, or installing/flashing anything. Its rootfs digest matches the vendor's Zstandard-compressed `sha2-256sums` manifest in the update tar.

| Artifact | SHA-256 |
| --- | --- |
| Original ZIP | `31d1462b3c7ae9f850c04b752b654404407cc4605c50885318a876ddb3b9ce47` |
| Inner update.tar | `0e328b2d7aa11b6f8102968648c7a9bf6540c600c15214ea2e84a34c732d1ee9` |
| Compressed rootfs.img | `6d401357e39891c48c99011f76fe5c8f6b852c497707a8765566674521db864c` |
| /usr/local/Kobo/libnickel.so.1.0.0 | `b22059eb126d355312f69abb9d1cafc72f0cf54ed4ffe5607eadda2d65670df7` |
| /usr/lib/libQt6Core.so.6.5.2 | `1dd0f3b5b0962b89116e102cccf835f7ae4197f23d0bd46b27e2340c2a854c55` |

`libnickel` GNU Build ID: `eaa30496411474df4019643cf7ba21efbdc25b52`.

Kobo's official support article independently identifies 6.0.274403 as optional beta software for Clara BW, Clara Colour and Libra Colour. It describes missing-content issues and explicitly warns against rollback, reinstalling older firmware, factory reset or account sign-out as attempted recovery. This analysis requires none of those operations:

https://help.kobo.com/hc/en-us/articles/43753873356055-Missing-books-reading-stats-or-notes-after-Kobo-software-update

This report is pinned to 6.0.274403, not a claim about the latest available firmware.

## Native shell evidence: ABI and hooking

These observations come from `file`, `readelf`, `strings`, `nm` and `objdump`, not REA decompilation.

* `libnickel` is ELF32 little-endian ARM EABI5, ARMv7-A, Thumb-2, VFPv3, NEONv1, with `Tag_ABI_VFP_args: VFP registers` and 8-byte required alignment.
* Its dynamic dependencies include `libQt6Core.so.6`, `libQt6Gui.so.6`, `libQt6Widgets.so.6` and other Qt6 modules. Its embedded QtCore build string is `Qt 6.5.2 (arm-little_endian-ilp32-eabi-hardfloat shared (dynamic) release build; by GCC 11.5.0)`.
* The shipped libc reports GNU libc 2.35; the shipped C++ runtime is `libstdc++.so.6.0.29`. `libnickel` has a GCC 11.5.0 compiler comment, alongside comments from constituent linked objects built using other compilers. These comments do not identify every translation unit's compiler.
* `DT_BIND_NOW` and `DT_FLAGS_1 NOW` are set. The `PT_GNU_RELRO` range begins at virtual address `0x026c8698`, length `0x00107968`; section mapping includes `.got`. A GOT-writing hook must temporarily make the affected page writable and restore the original protection. ELF virtual addresses must be combined with the runtime load base.
* The MainNavView constructor relocation is `R_ARM_JUMP_SLOT`, GOT address `0x027a523c`, mangled symbol `_ZN11MainNavViewC1EP7QWidget`. Dynamic symbol value `0x015f9835` has the Thumb bit set; code begins at `0x015f9834`. Its ARM PLT entry is `0x009f1aec` (after the Thumb interworking stub).

The inspected MainNavView constructor returns `this` under the ARM constructor
ABI. Its inspected caller does not consume that return value, so that caller
did not establish a crash from a void-return wrapper. A return-preserving hook
remains the correct ABI. The caller's allocation size only validates this
specific class on this firmware, not unrelated private objects.

## Analysis limits

A bounded REA/Hopper overview of the shipped `hindenburg` helper succeeded. It
did not establish `libnickel` behavior. Larger comparator analyses timed out;
an offline ELF-layout provider lacked its Python dependencies. ABI conclusions
here came from ordinary ELF tools and bounded inspection, not a successful
whole-application decompilation. Generated analysis output and proprietary
firmware disassembly are excluded from publication.

## Implications for implementation and verification

The existing Qt5 build cannot supply Qt6's C++ ABI. Build a separate ARM hard-float Qt6 plugin using the firmware's Qt 6.5.2 headers and a glibc 2.35-compatible sysroot. Debian bookworm's default glibc 2.36 toolchain can introduce imports unavailable on this device; a successful cross link alone does not prove compatibility.

The inspected `nicoverbruggen/NickelMenu` `qt6-support` branch supplies a Ubuntu 22.04/GCC11/glibc2.35 toolchain using Kobo-published Qt6.5.2 source. It is a plausible source reuse candidate; static matching and independent review remain required:

https://github.com/nicoverbruggen/NickelMenu/tree/qt6-support/toolchain/qt6

The separate upstream draft PR240 reports basic on-device work on N367/5.18.270971, not this user's N365/6.0.274403. Its private object sizes and action ABIs must not be accepted solely from that report:

https://github.com/pgaskin/NickelMenu/pull/240

Exported symbol presence establishes resolvability, not class layout compatibility, action correctness, menu survival through USB transitions, or runtime installation safety. Private Settings/vtable assumptions, allocations of Nickel objects, event dispatch and GUI-thread rules need implementation checks; hardware behavior is outside this static analysis; see the separate
[physical and simulator action results](action-compatibility-6.md), including
the later report of panics after cloud use and the current source containment.

Independent bounded inspection supported the retained Settings and navigation
storage sizes on 6.0.274403: observed Settings writes fit within 128 bytes, and
the inspected navigation constructors wrote only the vptr and initial fields.
These observations do not establish future layouts, every method's behavior or
on-device execution. No proprietary instruction listings are included here.

## Historical baseline acceptance results

The baseline implementation reused the credited Qt6 fork identified in the README and vendors its NickelHook dependency. The local loader correction restores each GOT page's original protection, preserves originally writable lazy-binding pages, and rolls the pointer back if restoring protection fails. Independent review accepted this correction and the scoped 6.0.274403 port.

Baseline lead checks passed on 2026-10-26:

* `go test -count=1 ./...` in `test/syms` and `NickelHook/tests`: source/configuration regressions and 96 loader combinations, initialization rollback and injected protection-restoration failure.
* The source symbol checker against the exact firmware: all 53 private function fingerprints matched, alongside the required exported symbols and allocation metadata.
* The SDK's `check.sh`: ARM hard-float/NEON, firmware configuration headers, pkg-config, qmake6 and CMake/moc/private-header builds.
* A forced Qt6 cross-build using `make -B NH_QT_MAJOR=6 VERSION=NickelMenuNG-6.0.274403-dev -j8 all kobo test/qt_compat test/compat`, plus a clean Qt5 regression cross-build in a separate directory.
* Both ARM regression executables under QEMU with the exact firmware's libraries: constructor return forwarding and selection/JSON transformations; symbol resolution, native allocation sizes and deliberate Settings-vtable incompatibility rejection. These tests do not start Nickel or access Kobo hardware.
* Archive inspection and SHA-256 verification: the delivered package contains exactly `imageformats/libnm.so`, owned by root with mode 0755, and its payload matches the rebuilt ARM library. Imports require Qt6/Qt6.5 and GLIBC no newer than 2.34, within the inspected firmware's 2.35 runtime.

Those acceptance checks describe the earlier baseline, whose package SHA256 was
`9f4861e5e6dfe150dd65ed43539011797d10ad02166bd0d9310359d2c7f36cee`.
The later Qt6 cloud bypass, panic report, and current containment are recorded in
[action compatibility](action-compatibility-6.md). The stale local `dist/`
baseline is excluded from publication. Rebuild from source using the README;
embedded documentation, version strings and tar metadata can change hashes.

## Preparing local firmware libraries

Firmware payloads are not included. On Linux, install `curl`, `unzip`, GNU tar,
`zstd`, `sha256sum`, and `debugfs` (usually packaged as `e2fsprogs`). Work in a
separate empty directory with enough space for the archive and a 1 GiB raw
filesystem. These commands read the image without mounting or flashing it:

```sh
curl -fL -o firmware.zip \
  https://ereaderfiles.kobo.com/firmwares/kobo12/Sep2026/qt6-update-6.0.274403.zip
sha256sum -c <<'SUM'
31d1462b3c7ae9f850c04b752b654404407cc4605c50885318a876ddb3b9ce47  firmware.zip
SUM
unzip -p firmware.zip update.tar > update.tar
tar -xf update.tar rootfs.img sha2-256sums
zstd -d -c rootfs.img > rootfs.raw
mkdir -p extracted symbol-libs
debugfs -R 'rdump /usr/local/Kobo extracted' rootfs.raw
debugfs -R 'rdump /usr/lib extracted' rootfs.raw
cp extracted/Kobo/libnickel.so.1.0.0 symbol-libs/
cp extracted/lib/libQt6*.so.6.5.2 symbol-libs/
for library in symbol-libs/libQt6*.so.6.5.2; do
  ln -s "$(basename "$library")" "${library%.5.2}"
done
```

Unprivileged `debugfs rdump` may warn that it cannot restore ownership; verify
the required files and hashes rather than relying on its exit status alone.
Pass the absolute `symbol-libs` path to the README's `-lib-dir` command. The
checker needs firmware Qt libraries under SONAMEs such as `libQt6Gui.so.6`;
Kobo's application directory alone is insufficient. Confirm the extracted
`libnickel.so.1.0.0` hash against the table above. These libraries support static
checks; running ARM tests additionally requires a QEMU ARM interpreter and the
firmware's loader, libc and other runtime dependencies in a prepared sysroot.
That full runtime/sysroot assembly is not provided by the commands above.

Private downloaded images and analysis caches are local prerequisites, not
repository dependencies. See [simulator notes](emulator/README.md) for the
additional userspace-GUI setup gaps. Neither a static symbol match nor an ARM
unit test proves physical USB, e-ink, network or power behavior.
