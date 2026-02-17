#include "jira_hours_data.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static jira_hours_state_t g_jira_hours_state;

void jira_hours_data_init(void) {
    memset(&g_jira_hours_state, 0, sizeof(g_jira_hours_state));
    g_jira_hours_state.synced = false;
    Serial.println("JiraHoursData: Initialized");
}

void jira_hours_data_set(const char* json) {
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.printf("JiraHoursData: JSON parse error: %s\n", err.c_str());
        return;
    }

    g_jira_hours_state.logged_min = doc["logged_min"] | 0;
    g_jira_hours_state.target_min = doc["target_min"] | 0;
    g_jira_hours_state.synced = true;

    Serial.printf("JiraHoursData: %d min logged, target %d min\n",
                  g_jira_hours_state.logged_min, g_jira_hours_state.target_min);
}

const jira_hours_state_t* jira_hours_data_get(void) {
    return &g_jira_hours_state;
}

bool jira_hours_data_is_synced(void) {
    return g_jira_hours_state.synced;
}
