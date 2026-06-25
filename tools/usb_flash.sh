#!/usr/bin/env bash
#
# One-time USB flash of kn0m3 onto a KNOMI.
#
# This is needed exactly once: the stock BTT firmware's OTA is broken for *every*
# image (verified — even BTT's own official firmware won't OTA onto a stock unit),
# so the first kn0m3 install has to go over USB. After this, every future update
# works over the network with tools/ota_flash.sh.
#
# It flashes only the application slot and clears the OTA selector so the device
# boots kn0m3 — it reuses the bootloader + partition table already on the device
# (which match kn0m3's layout), so there's nothing else to download.
#
# Requirements: esptool (pip install esptool), the KNOMI connected over USB.
#
# Usage:
#   tools/usb_flash.sh <v1|v2> [serial-port]
# Examples:
#   tools/usb_flash.sh v1                 # auto-detect port
#   tools/usb_flash.sh v1 /dev/ttyUSB0
#
set -euo pipefail

BOARD="${1:?usage: usb_flash.sh <v1|v2> [serial-port]}"
PORT="${2:-}"

case "$BOARD" in
  v1) CHIP=esp32;   APP=knomi-v1-firmware.bin ;;
  v2) CHIP=esp32s3; APP=knomi-v2-firmware.bin ;;
  *)  echo "board must be 'v1' or 'v2'" >&2; exit 1 ;;
esac

if [ ! -f "$APP" ]; then
  echo "Put $APP in the current directory first (download it from the kn0m3 Releases page):" >&2
  echo "  https://github.com/JameZUK/kn0m3/releases/latest" >&2
  exit 1
fi

ESPTOOL=(python -m esptool)
command -v esptool.py >/dev/null 2>&1 && ESPTOOL=(esptool.py)
PORTARG=(); [ -n "$PORT" ] && PORTARG=(--port "$PORT")

echo ">> Flashing $APP to $CHIP over USB..."
# Clear the OTA selector (otadata) so the bootloader boots the slot we write below.
"${ESPTOOL[@]}" --chip "$CHIP" "${PORTARG[@]}" erase_region 0xe000 0x2000
# Write kn0m3 into the first app slot.
"${ESPTOOL[@]}" --chip "$CHIP" "${PORTARG[@]}" write_flash 0x10000 "$APP"

echo
echo ">> Done. The KNOMI will boot kn0m3."
echo ">> Reconnect it to WiFi via the 'BTT-KNOMI' access point, then use"
echo ">> tools/ota_flash.sh for every future update — no USB ever again."
