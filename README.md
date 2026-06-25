# kn0m3 — a self-healing KNOMI firmware

A modern fork of the [BigTreeTech KNOMI](https://github.com/bigtreetech/KNOMI) firmware
(`firmware` branch) focused on **WiFi reliability**. Stock KNOMI hardware works well but
has a well-known habit of **dropping off WiFi and silently losing its connection to
Klipper/Moonraker** until it's power-cycled. This fork makes the firmware **fix itself**.

Supports both **KNOMI V1** (ESP32-WROOM + GC9A01) and **KNOMI V2** (ESP32-S3). Both
targets build from this tree.

---

## Why this fork exists

The stock firmware has two gaps that together cause the "it just stops updating" symptom:

1. **WiFi modem-sleep is left on.** The ESP32 periodically powers its radio down to save
   energy. On a mains-powered KNOMI that saving is pointless, and on some routers it
   causes intermittent disconnects and dropped TCP packets.
2. **There is no recovery.** When the link drops, the stock firmware only *flags* the
   disconnect — it never re-establishes the connection. It sits on the "WiFi
   disconnected" screen until you reboot it.

kn0m3 closes both gaps and adds a proper **watchdog** so the device detects trouble and
recovers on its own — and exposes the behaviour as **persistent toggles in the settings
GUI** so you stay in control.

---

## What's new

### 🔁 Self-healing connectivity watchdog
- **Modem-sleep disabled** (`WiFi.setSleep(false)`) — removes the single most common cause
  of drops on mains-powered ESP32s. Plus `setAutoReconnect(true)` for transient blips.
- **WiFi link watchdog** — if the link goes down, the firmware retries the connection
  every few seconds, and as a **last resort reboots** the device after a prolonged outage
  (default 2 minutes) so it always comes back.
- **Klipper reachability watchdog** — catches the nasty case where WiFi *looks* connected
  but HTTP requests to Moonraker keep failing (a wedged TCP/WiFi stack, the router moving
  you to a new IP, etc.). It **bounces the WiFi link a couple of times** to clear the stall.
  It deliberately **does not reboot-loop** when Klipper is simply powered off — it shows
  the warning and waits.
- **User is always warned** — the watchdog reuses the existing on-screen UI: the *WiFi
  disconnected* screen and the *"Moonraker Connect failed — check your printer or KNOMI IP"*
  popup. All WiFi manipulation stays on the WiFi task; the display task only renders.

### ⚙️ Persistent watchdog toggles in the settings GUI
The web config page (the one you use to set your SSID / Moonraker IP) gains a
**Connection Watchdog** card with three switches:

| Toggle | Default | Effect |
| --- | --- | --- |
| **Disable WiFi power-saving** | On | Keeps the radio awake (`WiFi.setSleep`). Turn off to restore stock power-saving. |
| **Auto-reconnect & reboot on WiFi loss** | On | Enables the WiFi link watchdog (reconnect + last-resort reboot). |
| **Recover when Klipper unreachable** | On | Enables the Moonraker reachability watchdog (link bounce). |

Settings are **saved to EEPROM** and survive reboots. They live in their **own EEPROM
region**, separate from your WiFi/Moonraker config, so a future firmware update that
changes the main config layout won't wipe your watchdog preferences (and vice-versa).

### 🩹 Automated lvgl black-screen patch
The stock project requires a **manual edit to lvgl's `lv_disp.c`** (lvgl PR #4487 + a
`prev_scr`/`scr_to_load` reset) to avoid a black screen when screens switch quickly — but
nothing applied it. kn0m3 ships [`apply_patches.py`](apply_patches.py) as a PlatformIO
**pre-build hook**, so every build is flash-ready with **no manual step**. It's idempotent.

### 🧪 Host-side unit tests
The watchdog decision logic is extracted into a pure, hardware-independent core
([`lib/Watchdog`](lib/Watchdog)) and covered by **unit tests that run on your computer** —
no device required:

```bash
pio test -e native
```

```
11 test cases: 11 succeeded
```

---

## Tunables

All watchdog timings live in [`src/config.h`](src/config.h):

```c
#define WIFI_RECONNECT_INTERVAL  5000    // ms between forced reconnect attempts while down
#define WIFI_DOWN_REBOOT_MS      120000  // reboot if WiFi can't be restored within this window
#define MOONRAKER_FAIL_RECONNECT 5       // failed poll cycles before bouncing WiFi
```

---

## Building & flashing

This is a standard [PlatformIO](https://platformio.org/) project.

```bash
# KNOMI V1 (ESP32-WROOM)
pio run -e knomiv1

# KNOMI V2 (ESP32-S3)
pio run -e knomiv2

# Host unit tests for the watchdog
pio test -e native
```

The resulting firmware is at `.pio/build/<env>/firmware.bin`.

> The build runs fine on a Raspberry Pi (PlatformIO supports ARM); the produced `.bin` can
> then be flashed from any machine.

### Flashing over the network (OTA) ✅

KNOMI ships a **dual-OTA partition layout** and an OTA web endpoint (AsyncElegantOTA), so
you can update **without removing it from the toolhead**:

1. **Web UI** — browse to `http://<knomi-ip>/update` (or `http://KNOMI.local/update` via
   mDNS, using your configured hostname), upload `firmware.bin`, and let it reboot. The
   on-screen **Update Firmware** button on the settings page links here too.
2. **PlatformIO over the network** — uncomment the OTA lines in `platformio.ini`
   (`upload_protocol = custom`, `extra_scripts = platformio_upload_ota.py`,
   `upload_url = http://<knomi-ip>/update`) and run `pio run -e knomiv1 -t upload`.

Because the stock BTT firmware uses the **same partition table and OTA endpoint**, you can
even flash kn0m3 onto a stock KNOMI straight over the network — no USB needed. (First boot
still resets the stored config, so you'll re-enter WiFi via the `BTT-KNOMI` AP.)

### Flashing over USB

Removed from the toolhead and connected to a computer, flash with `esptool.py` (or
`pio run -e knomiv1 -t upload` with a serial port). This is the fallback if a device is
ever bricked or unreachable on the network.

### First-time / upgrade notes
- Flashing this firmware for the first time **resets the stored config** (you'll re-enter
  WiFi via the `BTT-KNOMI` access point, same as stock). This is unavoidable when the
  config format changes.
- After that, the watchdog toggles persist independently across kn0m3 updates.

---

## How it compares to the other KNOMI forks

None of the existing community forks address WiFi stability — they're hardware ports or
cosmetic re-skins. For reference:

| Fork | What it changes | WiFi-stability fix? |
| --- | --- | --- |
| [DrKlipper/KNOMI_C3](https://github.com/DrKlipper/KNOMI_C3) | Real port to the cheaper **ESP32-C3** (RISC-V, less RAM): swaps the display lib **TFT_eSPI → LovyanGFX**, new partition layout, drops the splash screen to save RAM, moves to a **webserver-less** config flow, and tweaks HTTP/screen-timing logic. | ❌ |
| [azertui/KNOMI_waveshare…](https://github.com/azertui/KNOMI_waveshare_esp32_s3_touch_lcd_128) | Ports **KNOMI V2** to a Waveshare ESP32-S3 board (and a C3 SuperMini): new pinouts, extra **multi-colour filament animations**, a CI workflow, and a variant with OTA removed from the captive portal. | ❌ |
| [Rami-Pastrami/Knomi_custom_firmware](https://github.com/Rami-Pastrami/Knomi_custom_firmware) | **KNOMI V2**, purely cosmetic — custom face/animation GIFs and a tiny `platformio.ini` tweak. No logic changes. | ❌ |
| **kn0m3 (this fork)** | **WiFi self-healing watchdog**, persistent settings-GUI toggles, automated lvgl patch, host unit tests. | ✅ |

---

## Project layout (what changed vs. upstream)

```
apply_patches.py              # NEW: auto-applies the lvgl black-screen fix at build time
lib/Watchdog/watchdog.{h,cpp} # NEW: pure, testable watchdog decision logic
test/test_watchdog/           # NEW: host unit tests (pio test -e native)
src/config.h                  # watchdog tunables + fork version tag
src/wifi_setup.cpp            # modem-sleep off, WiFi watchdog, EEPROM watchdog config
src/moonraker.cpp             # Klipper-reachability watchdog
src/webserver.cpp             # handles the watchdog toggle form
src/index_html.h              # "Connection Watchdog" settings card
src/knomi.h                   # knomi_watchdog_t + declarations
platformio.ini               # patch hook + native test env
```

---

## Credits & licence

A fork of [bigtreetech/KNOMI](https://github.com/bigtreetech/KNOMI). All original code and
assets remain the property of BigTreeTech under their original licence; the changes in this
fork are offered in the same spirit for the Voron/Klipper community.

Hardware recap: KNOMI mounts on the Voron Stealthburner and talks to **Moonraker over
WiFi** — no Klipper config changes required.
