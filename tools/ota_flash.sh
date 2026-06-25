#!/usr/bin/env bash
#
# Flash a kn0m3 KNOMI over the network.
#
# Targets the kn0m3 OTA endpoint (v1.0.4+), which validates the image's SHA-256
# and reports real errors as readable HTTP 500. We also send the firmware's
# SHA-256 as an end-to-end transport check. No MD5, no field-ordering games.
#
# (Stock BTT firmware can't be flashed this way — its OTA is the buggy thing
#  kn0m3 replaces; the very first kn0m3 flash must be done over USB.)
#
# Usage:
#   tools/ota_flash.sh <knomi-ip-or-host> <firmware.bin>
#
set -euo pipefail

HOST="${1:?usage: ota_flash.sh <knomi-ip-or-host> <firmware.bin>}"
BIN="${2:?usage: ota_flash.sh <knomi-ip-or-host> <firmware.bin>}"
[ -f "$BIN" ] || { echo "no such file: $BIN" >&2; exit 1; }

if command -v sha256sum >/dev/null 2>&1; then
    SHA=$(sha256sum "$BIN" | cut -d' ' -f1)
else
    SHA=$(shasum -a 256 "$BIN" | cut -d' ' -f1)   # macOS
fi

size=$(wc -c < "$BIN" | tr -d ' ')
echo ">> Flashing $BIN ($size bytes, sha256 $SHA) -> http://$HOST/update"

body=$(mktemp)
code=$(curl -sS --connect-timeout 10 -m 300 \
       -H "Expect:" \
       -F "sha256=$SHA" \
       -F "firmware=@${BIN};filename=firmware" \
       -o "$body" -w '%{http_code}' \
       "http://$HOST/update" || echo "000")

echo ">> HTTP $code: $(cat "$body")"
rm -f "$body"

case "$code" in
  200) echo ">> Success — the KNOMI will reboot into the new firmware shortly." ;;
  000) echo ">> No HTTP response (wrong host, device offline, or it rebooted mid-upload). Retry." ; exit 1 ;;
  *)   echo ">> Upload rejected — the message above is the device's real reason." ; exit 1 ;;
esac
