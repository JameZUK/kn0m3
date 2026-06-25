#ifndef WATCHDOG_H
#define WATCHDOG_H

//
// Pure, hardware-independent decision logic for the connectivity watchdog.
//
// Keeping the state machines free of Arduino/WiFi calls lets them be unit-tested
// on the host (see test/test_watchdog) instead of only on a flashed device.
// wifi_setup.cpp / moonraker.cpp are thin wrappers that gather inputs, call
// these, and perform the resulting action.
//

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WD_ACTION_NONE = 0,
    WD_ACTION_RECONNECT,  // caller should drop & re-establish the WiFi link
    WD_ACTION_REBOOT,     // caller should reboot the device (last resort)
} wd_action_t;

// ---- WiFi link watchdog ----
typedef struct {
    uint32_t down_since_ms;      // millis() when the link first went down (0 == up)
    uint32_t last_reconnect_ms;  // millis() of the last forced reconnect
} wifi_wd_state_t;

typedef struct {
    bool     sta_active;            // station mode active (STA or AP+STA)
    bool     has_ssid;              // a station SSID is configured
    bool     enabled;               // wifi link watchdog toggle
    bool     connected;             // WiFi.status() == WL_CONNECTED
    bool     reconnect_requested;   // an external force-reconnect is pending
    uint32_t now_ms;                // current millis()
    uint32_t reconnect_interval_ms; // min gap between forced reconnects
    uint32_t reboot_after_ms;       // outage length that triggers reboot (0 = never)
} wifi_wd_input_t;

// Advance the WiFi watchdog one cycle and return the action to perform.
// State (timers) is updated in place so the caller is purely reactive.
wd_action_t wifi_wd_step(wifi_wd_state_t *st, const wifi_wd_input_t *in);

// ---- Moonraker reachability watchdog ----
typedef struct {
    uint16_t fail_cycles;  // consecutive failed poll cycles
} moonraker_wd_state_t;

// Call once per poll cycle while WiFi is connected. Returns true when the caller
// should request a WiFi reconnect to clear a wedged TCP/WiFi stack. Bounded: it
// only fires at the threshold and at 2x the threshold, then stays quiet (a
// persistently unreachable host is most likely just powered off) and never reboots.
bool moonraker_wd_step(moonraker_wd_state_t *st, bool reachable,
                       bool enabled, uint16_t reconnect_threshold);

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_H
