# Kobo N365 / 6.0.274403: full-system QEMU feasibility

Checked 2026-10-26. Conclusion: stock QEMU has no matching Kobo/MediaTek MT8110/MT8512 board model in the inspected ARM board inventory. The original device kernel and bootloader cannot be treated as a bootable `virt` image. A genuine Nickel userspace GUI remains a separate, potentially workable experiment; this report makes no conclusion that Qt userspace cannot run.

## Observed firmware facts

Source: exact Kobo firmware `https://ereaderfiles.kobo.com/firmwares/kobo12/Sep2026/qt6-update-6.0.274403.zip`, ZIP SHA-256 `31d1462b3c7ae9f850c04b752b654404407cc4605c50885318a876ddb3b9ce47`. Archive and root filesystem identities are recorded in [the firmware analysis](firmware-6-analysis.md).

The inner update tar includes `spa-bw/kernel.img`, `spa-tpv-bw/kernel.img`, and MediaTek `aud8113tp1/{bl2,uboot,tee}.img`. Each is Zstandard compressed. After decompression, the kernel and U-Boot are FIT containers; the kernel FIT embeds its device trees. These are actual available boot assets, not missing download dependencies.

A bounded, read-only FDT token parser with header assertions extracted the embedded device trees and their declared properties. Default FIT configuration `conf@1` selects `kernel@1` and `fdt@1`:

- FIT description: `U-Boot fitImage for Poky (Yocto Project Reference Distro)/4.9/aud8113tp1-kobo`.
- Embedded DTB description: `aud8113tp1-E60T00-D0x00-BW.dtb`.
- DTB root model: `MediaTek MT8110 board E60T00`.
- DTB root compatible strings: `mediatek,mt8110`, `mediatek,mt8512`.
- CPU compatible: `arm,cortex-a53` (default BW device tree has cpu@0; second FIT device tree additionally has cpu@1).
- Kernel string: Linux `4.9.77`, GCC `4.9.1`, build `f2bdeab-20260707T105829-B0727145819`.
- Device tree binds MediaTek clock/pinctrl/MMC/sysirq/UART/USB devices, `mediatek,MTKFB`, `mediatek,hwtcon` at `/hwtcon@14000000`, and `cy,cyttsp5_i2c_adapter` touch support.
- Boot arguments include UART MMIO `0x11002000`, `androidboot.hardware=mt8512`, and `/vendor/firmware`.
- U-Boot FIT identifies aud8113tp1-kobo/2019.04 and carries the console prompt `MT8113TP1>`.

The actual update driver's `spaKoboBW`/`spaBW` case selects `spa-bw`, with a PCB-dependent `spa-tpv-bw` alternative. Its MediaTek update path writes BL2 to the eMMC boot partition, U-Boot to UBOOT, TEE to tee_a, kernel to boot_a, and rootfs to system_a. The update is not a complete raw eMMC disk image with the user's board hwcfg, userdata and existing boot state.

The bounded inspection retained aggregate FIT/DTB declarations above. Firmware
payloads, extracted device trees and generated analysis files are excluded from
publication. Recreate local inputs from the exact archive; the original scratch
directory is not a dependency.

## QEMU support boundary

QEMU's official ARM documentation explains that a board model is required, that a kernel for a different ARM board generally will not boot, and that extracting the filesystem for a different kernel is a distinct alternative:

https://www.qemu.org/docs/master/system/target-arm.html

The official `virt` platform is deliberately unrelated to real hardware. It supplies PL011/virtio/PCI devices and requires a suitably configured guest kernel; its display option is typically virtio-gpu, not Kobo's HWTCON:

https://www.qemu.org/docs/master/system/arm/virt.html

For a bounded source check, QEMU master commit `d73149283205645625bfa9fe029cfb68e4c4a9ae` reports VERSION `11.1.50`. Its ARM `Kconfig`, ARM `meson.build`, and ARM target documentation contain no matches for MediaTek, MT8110, MT8113, MT8512, Kobo, Netronix or HWTCON. This is the standard upstream ARM board inventory, not a claim about every third-party QEMU fork:

https://github.com/qemu/qemu/tree/d73149283205645625bfa9fe029cfb68e4c4a9ae/hw/arm

The original host check found no system-mode ARM QEMU on PATH. Its ARM interpreter was userspace emulation. QEMU documents that userspace mode translates system calls to the host kernel; it does not boot the Kobo kernel or implement its board:

https://www.qemu.org/docs/master/user/main.html

## What a faithful full boot still requires

A matching MediaTek/Kobo machine implementation, including the device interfaces the kernel actually needs for boot and storage, is absent from stock QEMU's inspected inventory. Faithful visible Kobo operation additionally requires the HWTCON display and touch/peripheral behavior. Alternatively, a QEMU-compatible Linux kernel plus the extracted firmware filesystem can host userspace, but replacing the kernel/device layer is a port, and hardware-specific Nickel dependencies still need handling. Merely adding a DTB or selecting Cortex-A53 does not create those MMIO devices.

Commands used: `tar tf/xf`, `zstd -d -c`, `file`, `strings`, read-only `debugfs`, asserted FIT/FDT parsing, and primary QEMU source/document retrieval. No bootloader/kernel was executed, no physical device was flashed, no global emulator installation/configuration changed, and no REA deep-analysis rerun was needed for these container/DTB declarations.

Not checked: a third-party MediaTek QEMU fork, physical hwcfg, a substituted-kernel port or an actual full-system boot attempt. For the separate observed userspace GUI and its setup limitations, see [simulator notes](emulator/README.md).
