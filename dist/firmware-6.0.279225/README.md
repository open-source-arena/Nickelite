# Nickelite 1.0.0 — N365 / Kobo 6.0.279225 package

`Kobo.tgz` installs the Qt 6 plugin binary. It contains only
`imageformats/libnm.so`. `cloud-shortcuts.conf` adds Dropbox and Google Drive
to the Nickelite tab; the binary enables Kobo's stock cloud views only when
the N365 model and reviewed firmware library both match. The two services
still need a linked Kobo account and working Wi-Fi.

Check `sha256sum -c SHA256SUMS`, then copy `Kobo.tgz` to the mounted reader's
`.kobo/Kobo.tgz`. If the two cloud shortcuts are absent from all existing
`.adds/nm` config files, copy `cloud-shortcuts.conf` to
`.adds/nm/cloud-shortcuts.conf`. Keep existing menu items and permissions.
Safely eject and unplug the reader to run the update. The config file can
later be edited in Nickelite Settings or by hand.

This package has not been validated on other Kobo models or firmware versions.
