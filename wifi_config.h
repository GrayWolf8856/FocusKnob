#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi connection states
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
} wifi_state_t;

// Initialize WiFi configuration system
void wifi_config_init(void);

// Start AP mode for configuration (creates hotspot)
void wifi_config_start_ap(void);

// Stop AP mode
void wifi_config_stop_ap(void);

// Connect to saved WiFi network
bool wifi_config_connect(void);

// Disconnect from WiFi
void wifi_config_disconnect(void);

// Check if WiFi credentials are saved
bool wifi_config_has_credentials(void);

// Get current WiFi state
wifi_state_t wifi_config_get_state(void);

// Get IP address (returns empty string if not connected)
const char* wifi_config_get_ip(void);

// Get connected SSID (returns empty string if not connected)
const char* wifi_config_get_ssid(void);

// Get Notion API key (returns empty string if not set)
const char* wifi_config_get_notion_key(void);

// Get Notion Database ID (returns empty string if not set)
const char* wifi_config_get_notion_db(void);

// Check if Notion is configured
bool wifi_config_has_notion(void);

// Clear all saved credentials
void wifi_config_clear_all(void);

// Process web server (call from loop)
void wifi_config_process(void);

// Get AP mode SSID
const char* wifi_config_get_ap_ssid(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONFIG_H
