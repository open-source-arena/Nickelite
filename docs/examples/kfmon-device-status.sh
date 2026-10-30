#!/bin/sh

output=/mnt/onboard/.adds/kfmon/device-status.txt
temporary="${output}.tmp"

{
    printf 'Nickelite device status\n'
    printf 'Timestamp: '
    if command -v date >/dev/null 2>&1; then
        date '+%Y-%m-%d %H:%M:%S %z' 2>/dev/null || printf 'unavailable\n'
    else
        printf 'unavailable\n'
    fi

    if [ -r /proc/uptime ]; then
        IFS=' ' read -r uptime _ < /proc/uptime
        printf 'Uptime seconds: %s\n' "${uptime%%.*}"
    else
        printf 'Uptime seconds: unavailable\n'
    fi

    if [ -r /sys/class/power_supply/battery/capacity ]; then
        IFS= read -r battery < /sys/class/power_supply/battery/capacity
        printf 'Battery percent: %s\n' "$battery"
    else
        printf 'Battery percent: unavailable\n'
    fi

    printf 'Onboard storage (KiB):\n'
    if command -v df >/dev/null 2>&1; then
        df -k /mnt/onboard 2>/dev/null
    else
        printf 'unavailable\n'
    fi
} > "$temporary" || exit 1

mv "$temporary" "$output"
