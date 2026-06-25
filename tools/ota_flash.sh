#!/usr/bin/env bash
#
# Flash a KNOMI over the network, bypassing the buggy web upload page.
#
# Works against BOTH the stock BTT firmware and kn0m3: it posts the firmware to
# the AsyncElegantOTA /update endpoint with the MD5 field FIRST (the device reads
# it on the first upload chunk) and prints the device's real response — so if it
# fails you see the actual reason instead of the web UI's opaque
# "[HTTP ERROR] Bad Request".
#
# Usage:
#   tools/ota_flash.sh <knomi-ip-or-host> <firmware.bin>
#
# Example:
#   tools/ota_flash.sh 192.168.1.50 knomi-v1-firmware.bin
#
set -euo pipefail

HOST="${1:?usage: ota_flash.sh <knomi-ip-or-host> <firmware.bin>}"
BIN="${2:?usage: ota_flash.sh <knomi-ip-or-host> <firmware.bin>}"
[ -f "$BIN" ] || { echo "no such file: $BIN" >&2; exit 1; }

# MD5 of exactly the bytes we upload — this is what the device verifies.
if command -v md5sum >/dev/null 2>&1; then
    MD5=$(md5sum "$BIN" | cut -d' ' -f1)
else
    MD5=$(md5 -q "$BIN")   # macOS
fi

size=$(wc -c < "$BIN" | tr -d ' ')
echo ">> Flashing $BIN ($size bytes, MD5 $MD5) -> http://$HOST/update"

body=$(mktemp)
# -H "Expect:"  : some embedded servers stall on the 100-continue handshake
# -F order      : MD5 part is sent before the firmware part (required)
# filename!="filesystem" selects an app (U_FLASH) update, not a SPIFFS one
code=$(curl -sS --connect-timeout 10 -m 300 \
       -H "Expect:" \
       -F "MD5=$MD5" \
       -F "firmware=@${BIN};filename=firmware" \
       -o "$body" -w '%{http_code}' \
       "http://$HOST/update" || echo "000")

echo ">> HTTP $code: $(cat "$body")"
rm -f "$body"

case "$code" in
  200) echo ">> Success — the KNOMI will reboot into the new firmware shortly." ;;
  000) echo ">> No HTTP response (wrong IP, device offline, or it rebooted mid-upload). Retry." ; exit 1 ;;
  *)   echo ">> Upload rejected. The message above is the device's real reason"
       echo "   (the web UI hides this behind '[HTTP ERROR] Bad Request')."
       exit 1 ;;
esac
