#!/usr/bin/env bash
# ESP-KVM RAM meter host script.
# Streams "R<used%>" lines to the StickC over USB from macOS memory pressure.
# Usage: ./tools/ram-meter.sh [/dev/cu.usbserial-XXXX]
set -u
PORT="${1:-$(ls -t /dev/cu.usbserial-* 2>/dev/null | head -1)}"
if [ -z "${PORT:-}" ] || [ ! -e "$PORT" ]; then
  echo "No serial port. Plug in the StickC and pass its port:"
  echo "  ./tools/ram-meter.sh /dev/cu.usbserial-XXXX"
  exit 1
fi
# Open the port and hold it open BEFORE setting the baud — macOS resets termios
# to the default on open, so configuring after the open is what makes 115200 stick.
exec 3<>"$PORT"
stty -f "$PORT" 115200 cs8 -parenb -cstopb -echo raw -hupcl 2>/dev/null
echo "ESP-KVM RAM meter -> $PORT @115200  (Ctrl-C to stop)"
while true; do
  free=$(memory_pressure 2>/dev/null | awk -F: '/free percentage/ {gsub(/[ %]/,"",$2); print $2}')
  [ -z "$free" ] && free=100
  used=$(( 100 - free ))
  printf 'R%d\n' "$used" >&3
  sleep 2
done
