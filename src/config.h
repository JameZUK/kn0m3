#ifndef CONFIG_H
#define CONFIG_H

#define FW_VERSION "V1.0.4-kn0m3"

 // default 80 for http
#define SERVER_PORT 80

#define SCAN_SSIDS_NUM 10     //  The max number of SSIDs that we will scan for.

#define HOSTNAME "KNOMI"
#define AP_SSID "BTT-KNOMI" // Create a SSID for BTT KNOMI Access Point
#define AP_PWD "" // Default no password
#define AP_LOCAL_IP IPAddress(192, 168, 20, 1) // access point IP
#define AP_GATEWAY  IPAddress(192, 168, 20, 1) // gateway IP
#define AP_SUBNET   IPAddress(255, 255, 255, 0) // subnet mask

#define WIFI_STA_TIMEOUT 15000  // 15s

// --- Connectivity watchdog (self-healing) ---
#define WIFI_RECONNECT_INTERVAL  5000    // ms between forced reconnect attempts while the link is down
#define WIFI_DOWN_REBOOT_MS      120000  // reboot if the WiFi link stays down this long (last resort)
#define MOONRAKER_FAIL_RECONNECT 5       // failed poll cycles before bouncing WiFi to clear a wedged stack

// BTT red color for UI (RGB888)
#define LV_32BIT_BTT_RED    0xC02F30
#define LV_32BIT_BTT_BLUE   0x209ADE
#define LV_32BIT_BTT_PURPLE 0xA91DDA
#define LV_32BIT_BTT_GREEN  0x5DA910
#define LV_DEFAULT_COLOR    LV_32BIT_BTT_RED

#endif
