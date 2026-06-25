#include "watchdog.h"

wd_action_t wifi_wd_step(wifi_wd_state_t *st, const wifi_wd_input_t *in) {
    // Nothing to police unless we're meant to be a station with credentials.
    // (AP/config-portal mode has no upstream link to heal.)
    if (!in->sta_active || !in->has_ssid) {
        st->down_since_ms = 0;
        return WD_ACTION_NONE;
    }

    // Honor an explicit reconnect request regardless of the link-watchdog toggle
    // (e.g. the moonraker watchdog asking us to bounce a wedged stack).
    if (in->reconnect_requested) {
        st->last_reconnect_ms = in->now_ms;
        st->down_since_ms = 0;
        return WD_ACTION_RECONNECT;
    }

    if (!in->enabled) {
        st->down_since_ms = 0;
        return WD_ACTION_NONE;
    }

    if (in->connected) {
        st->down_since_ms = 0;
        return WD_ACTION_NONE;
    }

    // Link is down -- start the outage timer on first detection.
    if (st->down_since_ms == 0)
        st->down_since_ms = in->now_ms;

    // Last resort: a prolonged outage we couldn't recover from -> reboot.
    if (in->reboot_after_ms != 0 &&
        (in->now_ms - st->down_since_ms) >= in->reboot_after_ms) {
        return WD_ACTION_REBOOT;
    }

    // Otherwise retry the connection on a fixed interval. (Unsigned subtraction
    // keeps this correct across the millis() rollover.)
    if ((in->now_ms - st->last_reconnect_ms) >= in->reconnect_interval_ms) {
        st->last_reconnect_ms = in->now_ms;
        return WD_ACTION_RECONNECT;
    }

    return WD_ACTION_NONE;
}

bool moonraker_wd_step(moonraker_wd_state_t *st, bool reachable,
                       bool enabled, uint16_t reconnect_threshold) {
    if (reachable || !enabled) {
        st->fail_cycles = 0;
        return false;
    }
    st->fail_cycles++;
    return (st->fail_cycles == reconnect_threshold ||
            st->fail_cycles == (uint16_t)(reconnect_threshold * 2));
}
