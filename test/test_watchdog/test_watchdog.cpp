#include <unity.h>
#include "watchdog.h"

void setUp(void) {}
void tearDown(void) {}

// A connected, enabled station with sane thresholds.
static wifi_wd_input_t base_input(void) {
    wifi_wd_input_t in;
    in.sta_active = true;
    in.has_ssid = true;
    in.enabled = true;
    in.connected = true;
    in.reconnect_requested = false;
    in.now_ms = 10000;
    in.reconnect_interval_ms = 5000;
    in.reboot_after_ms = 120000;
    return in;
}

// ---------------- WiFi watchdog ----------------

void test_connected_does_nothing(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    TEST_ASSERT_EQUAL(WD_ACTION_NONE, wifi_wd_step(&st, &in));
    TEST_ASSERT_EQUAL_UINT32(0, st.down_since_ms);
}

void test_not_station_does_nothing(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.sta_active = false;
    in.connected = false;
    TEST_ASSERT_EQUAL(WD_ACTION_NONE, wifi_wd_step(&st, &in));
}

void test_disabled_does_not_heal(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.enabled = false;
    in.connected = false;
    TEST_ASSERT_EQUAL(WD_ACTION_NONE, wifi_wd_step(&st, &in));
}

void test_request_forces_reconnect_even_when_disabled(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.enabled = false;            // link watchdog off...
    in.reconnect_requested = true; // ...but an explicit request must still fire
    TEST_ASSERT_EQUAL(WD_ACTION_RECONNECT, wifi_wd_step(&st, &in));
    TEST_ASSERT_EQUAL_UINT32(in.now_ms, st.last_reconnect_ms);
}

void test_down_reconnects_then_backs_off(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.connected = false;
    in.now_ms = 10000;             // gap from last_reconnect(0) exceeds interval
    TEST_ASSERT_EQUAL(WD_ACTION_RECONNECT, wifi_wd_step(&st, &in));
    TEST_ASSERT_EQUAL_UINT32(10000, st.down_since_ms);
    TEST_ASSERT_EQUAL_UINT32(10000, st.last_reconnect_ms);

    in.now_ms = 12000;             // 2s later: inside the interval -> wait
    TEST_ASSERT_EQUAL(WD_ACTION_NONE, wifi_wd_step(&st, &in));

    in.now_ms = 15001;             // >5s after last attempt -> retry
    TEST_ASSERT_EQUAL(WD_ACTION_RECONNECT, wifi_wd_step(&st, &in));
    TEST_ASSERT_EQUAL_UINT32(15001, st.last_reconnect_ms);
}

void test_prolonged_outage_reboots(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.connected = false;
    in.now_ms = 10000;
    wifi_wd_step(&st, &in);                 // down_since = 10000
    in.now_ms = 10000 + 120000;             // exactly the reboot threshold
    TEST_ASSERT_EQUAL(WD_ACTION_REBOOT, wifi_wd_step(&st, &in));
}

void test_reboot_disabled_when_threshold_zero(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.connected = false;
    in.reboot_after_ms = 0;                 // never reboot
    in.now_ms = 10000;
    wifi_wd_step(&st, &in);
    in.now_ms = 10000 + 10000000;           // ages later
    TEST_ASSERT_NOT_EQUAL(WD_ACTION_REBOOT, wifi_wd_step(&st, &in));
}

void test_recovery_clears_outage_timer(void) {
    wifi_wd_state_t st = {0, 0};
    wifi_wd_input_t in = base_input();
    in.connected = false;
    in.now_ms = 10000;
    wifi_wd_step(&st, &in);
    TEST_ASSERT_NOT_EQUAL(0, st.down_since_ms);

    in.connected = true;
    in.now_ms = 11000;
    TEST_ASSERT_EQUAL(WD_ACTION_NONE, wifi_wd_step(&st, &in));
    TEST_ASSERT_EQUAL_UINT32(0, st.down_since_ms);
}

// ---------------- Moonraker reachability watchdog ----------------

void test_moonraker_reachable_resets(void) {
    moonraker_wd_state_t st = {3};
    TEST_ASSERT_FALSE(moonraker_wd_step(&st, true, true, 5));
    TEST_ASSERT_EQUAL_UINT16(0, st.fail_cycles);
}

void test_moonraker_disabled_resets(void) {
    moonraker_wd_state_t st = {3};
    TEST_ASSERT_FALSE(moonraker_wd_step(&st, false, false, 5));
    TEST_ASSERT_EQUAL_UINT16(0, st.fail_cycles);
}

void test_moonraker_bounces_twice_then_stops(void) {
    moonraker_wd_state_t st = {0};
    bool fired[12];
    for (int i = 1; i <= 11; i++)
        fired[i] = moonraker_wd_step(&st, false, true, 5);
    TEST_ASSERT_FALSE(fired[4]);
    TEST_ASSERT_TRUE(fired[5]);    // first bounce at the threshold
    TEST_ASSERT_FALSE(fired[6]);
    TEST_ASSERT_TRUE(fired[10]);   // second bounce at 2x
    TEST_ASSERT_FALSE(fired[11]);  // then stays quiet (no reboot loop)
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_connected_does_nothing);
    RUN_TEST(test_not_station_does_nothing);
    RUN_TEST(test_disabled_does_not_heal);
    RUN_TEST(test_request_forces_reconnect_even_when_disabled);
    RUN_TEST(test_down_reconnects_then_backs_off);
    RUN_TEST(test_prolonged_outage_reboots);
    RUN_TEST(test_reboot_disabled_when_threshold_zero);
    RUN_TEST(test_recovery_clears_outage_timer);
    RUN_TEST(test_moonraker_reachable_resets);
    RUN_TEST(test_moonraker_disabled_resets);
    RUN_TEST(test_moonraker_bounces_twice_then_stops);
    return UNITY_END();
}
