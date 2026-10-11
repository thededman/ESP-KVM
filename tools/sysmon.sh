#!/usr/bin/env bash
# Nexus system monitor host script.
# Streams "S <mem> <gpu> <cpu> <disk>" (used %) to the DONGLE's USB-CDC serial;
# the CoreS3 rebroadcasts them over ESP-NOW to the (battery-powered) StickC.
# Usage: ./tools/sysmon.sh /dev/cu.usbmodemXXXX   (the CoreS3's serial port)
set -u
PORT="${1:-$(ls -t /dev/cu.usbmodem* 2>/dev/null | head -1)}"
if [ -z "${PORT:-}" ] || [ ! -e "$PORT" ]; then
  echo "No serial port. Pass the dongle port:"
  echo "  ./tools/sysmon.sh /dev/cu.usbmodemXXXX"; exit 1
fi
exec 3<>"$PORT"
stty -f "$PORT" 115200 cs8 -parenb -cstopb -echo raw -hupcl 2>/dev/null
echo "Nexus sysmon -> $PORT @115200  (Ctrl-C to stop)"
while true; do
  free=$(memory_pressure 2>/dev/null | awk -F: '/free percentage/ {gsub(/[ %]/,"",$2); print $2}'); [ -z "$free" ] && free=100
  mem=$(( 100 - free ))
  cpuline=$(top -l 2 -n 0 2>/dev/null | grep -E 'CPU usage' | tail -1)
  idle=$(echo "$cpuline" | sed -E 's/.* ([0-9.]+)% idle.*/\1/')
  cpu=$(echo "${idle:-100}" | awk '{v=100-$1; if(v<0)v=0; printf "%d", v+0.5}')
  gpu=$(ioreg -r -d 1 -c IOAccelerator -w0 2>/dev/null | grep -oE '"Device Utilization %"=[0-9]+' | head -1 | grep -oE '[0-9]+$')
  [ -z "$gpu" ] && gpu=$(ioreg -r -d 1 -w0 2>/dev/null | grep -oiE '"GPU.?Core.?Utilization[^0-9]*=[0-9]+' | head -1 | grep -oE '[0-9]+$')
  [ -z "$gpu" ] && gpu=0
  disk=$(df / 2>/dev/null | awk 'NR==2{gsub(/%/,"",$5); print $5}'); [ -z "$disk" ] && disk=0
  printf 'S %d %d %d %d\n' "$mem" "$gpu" "$cpu" "$disk" >&3
  sleep 1
done
