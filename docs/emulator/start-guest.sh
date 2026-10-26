#!/bin/sh
set -eu
/qemu-arm-static /bin/mkdir -p /run/kobo/udev /mnt/onboard/.kobo/Kobo
if [ ! -f '/mnt/onboard/.kobo/Kobo/Kobo eReader.conf' ]; then
    /qemu-arm-static /bin/cp /emulator-offline.conf '/mnt/onboard/.kobo/Kobo/Kobo eReader.conf'
fi
: > /tmp/emulator-syslog.log
/qemu-arm-static /sbin/syslogd -O /tmp/emulator-syslog.log
exec /qemu-arm-static /usr/local/Kobo/nickel -platform vnc:size=1072x1448:port=5900
