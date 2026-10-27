#!/bin/sh
# Run the account-capable simulator and a loopback-only noVNC bridge.
set -eu
umask 077

work=${1:?usage: run-real-network.sh WORK_DIRECTORY NOVNC_WEB_DIRECTORY [PORT]}
novnc=${2:?usage: run-real-network.sh WORK_DIRECTORY NOVNC_WEB_DIRECTORY [PORT]}
port=${3:-6093}
image="nickelmenung-simulator-${NM_FIRMWARE_VERSION:-6.0.274403}:latest"
work=$(CDPATH= cd -- "$work" && pwd)
novnc=$(CDPATH= cd -- "$novnc" && pwd)
command -v websockify >/dev/null || { echo 'missing websockify' >&2; exit 1; }
test -f "$novnc/vnc.html" || { echo "missing noVNC in $novnc" >&2; exit 1; }
docker image inspect "$image" >/dev/null
if docker container inspect nm-real-cloud-wifi >/dev/null 2>&1; then
    echo 'nm-real-cloud-wifi already exists; start it explicitly or choose a fresh work directory' >&2
    exit 1
fi
mkdir -p "$work/onboard" "$work/sys-devices/platform/pmic_battery.1/power_supply/battery"
battery="$work/sys-devices/platform/pmic_battery.1/power_supply/battery"
printf '100\n' > "$battery/capacity"
printf 'Full\n' > "$battery/status"
printf '0\n' > "$battery/time_to_full_now"
printf '4200000\n' > "$battery/voltage_now"
printf '1\n' > "$battery/present"
if ! docker network inspect nm-real-cloud-egress >/dev/null 2>&1; then
    docker network create nm-real-cloud-egress >/dev/null
fi
docker run -d --name nm-real-cloud-wifi --network nm-real-cloud-egress \
    -v "$work/onboard:/mnt/onboard" -v "$work/sys-devices:/sys/devices:ro" \
    "$image" >/dev/null
ip=$(docker inspect --format '{{with index .NetworkSettings.Networks "nm-real-cloud-egress"}}{{.IPAddress}}{{end}}' nm-real-cloud-wifi)
test -n "$ip" || { echo 'guest has no network address' >&2; exit 1; }
echo "Open http://127.0.0.1:$port/vnc.html?autoconnect=1&resize=scale"
echo 'The bridge stays in this terminal; Ctrl-C stops only the viewer.'
websockify --web "$novnc" "127.0.0.1:$port" "$ip:5900"
