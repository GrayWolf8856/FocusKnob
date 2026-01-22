#include "wifi_config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Configuration
#define AP_SSID "FocusKnob-Setup"
#define AP_PASSWORD "Focus"
#define CONFIG_FILE "/wifi_config.json"
#define WIFI_CONNECT_TIMEOUT_MS 15000

// State
static wifi_state_t g_wifi_state = WIFI_STATE_DISCONNECTED;
static WebServer* g_server = nullptr;
static char g_ip_address[16] = "";
static char g_ssid[33] = "";
static char g_password[65] = "";
static char g_notion_key[64] = "";
static char g_notion_db[64] = "";
static bool g_ap_active = false;

// HTML page for configuration
static const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>FocusKnob Setup</title>
    <style>
        body { font-family: -apple-system, sans-serif; max-width: 400px; margin: 40px auto; padding: 20px; background: #1a1a2e; color: #eaeaea; }
        h1 { color: #4ecca3; text-align: center; }
        h2 { color: #888; font-size: 14px; margin-top: 30px; }
        input { width: 100%; padding: 12px; margin: 8px 0; box-sizing: border-box; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eaeaea; font-size: 16px; }
        input:focus { outline: none; border-color: #4ecca3; }
        button { width: 100%; padding: 14px; margin-top: 20px; background: #4ecca3; color: #1a1a2e; border: none; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; }
        button:hover { background: #3eb489; }
        .success { background: #2ecc71; padding: 15px; border-radius: 8px; text-align: center; margin: 20px 0; }
        .info { color: #888; font-size: 12px; margin-top: 5px; }
    </style>
</head>
<body>
    <h1>FocusKnob Setup</h1>
    <form action="/save" method="POST">
        <h2>WiFi Settings</h2>
        <input type="text" name="ssid" placeholder="WiFi Network Name" required>
        <input type="password" name="password" placeholder="WiFi Password">

        <h2>Notion Integration (Optional)</h2>
        <input type="text" name="notion_key" placeholder="Notion API Key">
        <p class="info">Get your key at notion.so/my-integrations</p>
        <input type="text" name="notion_db" placeholder="Notion Database ID">
        <p class="info">The ID from your database URL</p>

        <button type="submit">Save & Connect</button>
    </form>
</body>
</html>
)rawliteral";

static const char SUCCESS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>FocusKnob Setup</title>
    <style>
        body { font-family: -apple-system, sans-serif; max-width: 400px; margin: 40px auto; padding: 20px; background: #1a1a2e; color: #eaeaea; text-align: center; }
        h1 { color: #4ecca3; }
        .success { background: #2ecc71; padding: 20px; border-radius: 8px; margin: 30px 0; }
        p { color: #888; }
    </style>
</head>
<body>
    <h1>FocusKnob Setup</h1>
    <div class="success">Settings Saved!</div>
    <p>Device is connecting to WiFi.<br>You can close this page.</p>
</body>
</html>
)rawliteral";

// Forward declarations
static void handle_root(void);
static void handle_save(void);
static bool load_config(void);
static bool save_config(void);

void wifi_config_init(void) {
    Serial.println("WiFiConfig: Initializing...");

    // Load saved configuration
    if (load_config()) {
        Serial.println("WiFiConfig: Loaded saved config");
        Serial.printf("WiFiConfig: SSID: %s\n", g_ssid);
        Serial.printf("WiFiConfig: Notion configured: %s\n", strlen(g_notion_key) > 0 ? "Yes" : "No");
    } else {
        Serial.println("WiFiConfig: No saved config found");
    }

    g_wifi_state = WIFI_STATE_DISCONNECTED;
}

void wifi_config_start_ap(void) {
    Serial.println("WiFiConfig: Starting AP mode...");

    // Stop any existing connection
    WiFi.disconnect(true);
    delay(100);

    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    IPAddress ip = WiFi.softAPIP();
    snprintf(g_ip_address, sizeof(g_ip_address), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    Serial.printf("WiFiConfig: AP started - SSID: %s, IP: %s\n", AP_SSID, g_ip_address);

    // Start web server
    if (g_server == nullptr) {
        g_server = new WebServer(80);
        g_server->on("/", HTTP_GET, handle_root);
        g_server->on("/save", HTTP_POST, handle_save);
    }
    g_server->begin();

    g_ap_active = true;
    g_wifi_state = WIFI_STATE_AP_MODE;
}

void wifi_config_stop_ap(void) {
    if (g_ap_active) {
        Serial.println("WiFiConfig: Stopping AP mode...");
        if (g_server != nullptr) {
            g_server->stop();
        }
        WiFi.softAPdisconnect(true);
        g_ap_active = false;
    }
}

bool wifi_config_connect(void) {
    if (strlen(g_ssid) == 0) {
        Serial.println("WiFiConfig: No SSID configured");
        return false;
    }

    Serial.printf("WiFiConfig: Connecting to %s...\n", g_ssid);
    g_wifi_state = WIFI_STATE_CONNECTING;

    // Stop AP if running
    wifi_config_stop_ap();

    WiFi.mode(WIFI_STA);
    WiFi.begin(g_ssid, g_password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(g_ip_address, sizeof(g_ip_address), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        Serial.printf("WiFiConfig: Connected! IP: %s\n", g_ip_address);
        g_wifi_state = WIFI_STATE_CONNECTED;
        return true;
    } else {
        Serial.println("WiFiConfig: Connection failed");
        g_wifi_state = WIFI_STATE_DISCONNECTED;
        g_ip_address[0] = '\0';
        return false;
    }
}

void wifi_config_disconnect(void) {
    WiFi.disconnect(true);
    g_wifi_state = WIFI_STATE_DISCONNECTED;
    g_ip_address[0] = '\0';
}

bool wifi_config_has_credentials(void) {
    return strlen(g_ssid) > 0;
}

wifi_state_t wifi_config_get_state(void) {
    // Update state based on actual WiFi status
    if (g_ap_active) {
        return WIFI_STATE_AP_MODE;
    } else if (WiFi.status() == WL_CONNECTED) {
        return WIFI_STATE_CONNECTED;
    } else if (g_wifi_state == WIFI_STATE_CONNECTING) {
        return WIFI_STATE_CONNECTING;
    }
    return WIFI_STATE_DISCONNECTED;
}

const char* wifi_config_get_ip(void) {
    return g_ip_address;
}

const char* wifi_config_get_ssid(void) {
    return g_ssid;
}

const char* wifi_config_get_notion_key(void) {
    return g_notion_key;
}

const char* wifi_config_get_notion_db(void) {
    return g_notion_db;
}

bool wifi_config_has_notion(void) {
    return strlen(g_notion_key) > 0 && strlen(g_notion_db) > 0;
}

void wifi_config_clear_all(void) {
    g_ssid[0] = '\0';
    g_password[0] = '\0';
    g_notion_key[0] = '\0';
    g_notion_db[0] = '\0';
    g_ip_address[0] = '\0';

    // Delete config file
    if (SPIFFS.exists(CONFIG_FILE)) {
        SPIFFS.remove(CONFIG_FILE);
    }

    wifi_config_disconnect();
    Serial.println("WiFiConfig: All credentials cleared");
}

void wifi_config_process(void) {
    if (g_ap_active && g_server != nullptr) {
        g_server->handleClient();
    }
}

const char* wifi_config_get_ap_ssid(void) {
    return AP_SSID;
}

// Web server handlers
static void handle_root(void) {
    if (g_server != nullptr) {
        g_server->send(200, "text/html", CONFIG_PAGE);
    }
}

static void handle_save(void) {
    if (g_server == nullptr) return;

    // Get form data
    if (g_server->hasArg("ssid")) {
        strncpy(g_ssid, g_server->arg("ssid").c_str(), sizeof(g_ssid) - 1);
        g_ssid[sizeof(g_ssid) - 1] = '\0';
    }
    if (g_server->hasArg("password")) {
        strncpy(g_password, g_server->arg("password").c_str(), sizeof(g_password) - 1);
        g_password[sizeof(g_password) - 1] = '\0';
    }
    if (g_server->hasArg("notion_key")) {
        strncpy(g_notion_key, g_server->arg("notion_key").c_str(), sizeof(g_notion_key) - 1);
        g_notion_key[sizeof(g_notion_key) - 1] = '\0';
    }
    if (g_server->hasArg("notion_db")) {
        strncpy(g_notion_db, g_server->arg("notion_db").c_str(), sizeof(g_notion_db) - 1);
        g_notion_db[sizeof(g_notion_db) - 1] = '\0';
    }

    Serial.printf("WiFiConfig: Saving - SSID: %s\n", g_ssid);

    // Save to file
    save_config();

    // Send success page
    g_server->send(200, "text/html", SUCCESS_PAGE);

    // Schedule WiFi connection (after response is sent)
    delay(1000);
    wifi_config_stop_ap();
    wifi_config_connect();
}

// Config file operations
static bool load_config(void) {
    if (!SPIFFS.exists(CONFIG_FILE)) {
        return false;
    }

    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("WiFiConfig: JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Load values
    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["password"] | "";
    const char* nkey = doc["notion_key"] | "";
    const char* ndb = doc["notion_db"] | "";

    strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
    strncpy(g_password, pass, sizeof(g_password) - 1);
    strncpy(g_notion_key, nkey, sizeof(g_notion_key) - 1);
    strncpy(g_notion_db, ndb, sizeof(g_notion_db) - 1);

    return strlen(g_ssid) > 0;
}

static bool save_config(void) {
    StaticJsonDocument<512> doc;

    doc["ssid"] = g_ssid;
    doc["password"] = g_password;
    doc["notion_key"] = g_notion_key;
    doc["notion_db"] = g_notion_db;

    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("WiFiConfig: Failed to open config file for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("WiFiConfig: Config saved");
    return true;
}
