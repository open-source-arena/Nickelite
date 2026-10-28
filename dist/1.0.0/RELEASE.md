# Nickelite 1.0.0

A configurable Nickel menu and native Settings editor for Kobo firmware 5+.

Download the ZIP matching the exact firmware version in Kobo Settings → Device information. A preceding editor candidate passed UI checks in the 5.18.270971 QEMU userspace simulator; the 1.0.0 build loads its tab and passes ARM compatibility tests. The 6.0.279225 package includes cloud shortcuts; cloud functionality was tested on a physical Kobo Clara BW (N365) using preceding candidates. The 1.0.0 build's Settings UI was checked in the 6.0 simulator. Other devices and firmware builds have not been physically verified.

The packages preserve `.adds/nm` configuration. Follow the [installation guide](https://github.com/Sojaner/NickelMenuNg#download-and-install), then safely eject and unplug the reader to apply `Kobo.tgz`.
