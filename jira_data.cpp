#include "jira_data.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static jira_state_t g_jira_state;

void jira_data_init(void) {
    memset(&g_jira_state, 0, sizeof(g_jira_state));
    g_jira_state.selected_index = -1;
    g_jira_state.synced = false;
    Serial.println("JiraData: Initialized");
}

void jira_data_set_projects(const char* json) {
    StaticJsonDocument<6144> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.printf("JiraData: JSON parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    g_jira_state.project_count = 0;

    for (JsonObject obj : arr) {
        if (g_jira_state.project_count >= JIRA_MAX_PROJECTS) break;

        const char* key = obj["key"] | "";
        const char* name = obj["name"] | "";
        const char* proj = obj["proj"] | "";
        const char* status = obj["status"] | "";
        const char* desc = obj["desc"] | "";

        jira_project_t *entry = &g_jira_state.projects[g_jira_state.project_count];

        strncpy(entry->key, key, JIRA_KEY_LEN - 1);
        entry->key[JIRA_KEY_LEN - 1] = '\0';

        strncpy(entry->name, name, JIRA_NAME_LEN - 1);
        entry->name[JIRA_NAME_LEN - 1] = '\0';

        strncpy(entry->proj, proj, JIRA_PROJ_LEN - 1);
        entry->proj[JIRA_PROJ_LEN - 1] = '\0';

        strncpy(entry->status, status, JIRA_STATUS_LEN - 1);
        entry->status[JIRA_STATUS_LEN - 1] = '\0';

        strncpy(entry->desc, desc, JIRA_DESC_LEN - 1);
        entry->desc[JIRA_DESC_LEN - 1] = '\0';

        g_jira_state.project_count++;
    }

    g_jira_state.synced = true;

    // Start on dashboard (index -1) — user turns knob to browse issues

    Serial.printf("JiraData: Loaded %d projects\n", g_jira_state.project_count);
}

uint8_t jira_data_get_count(void) {
    return g_jira_state.project_count;
}

const jira_project_t* jira_data_get_project(uint8_t index) {
    if (index >= g_jira_state.project_count) return NULL;
    return &g_jira_state.projects[index];
}

const jira_project_t* jira_data_get_selected(void) {
    if (g_jira_state.selected_index < 0 ||
        g_jira_state.selected_index >= g_jira_state.project_count) {
        return NULL;
    }
    return &g_jira_state.projects[g_jira_state.selected_index];
}

int8_t jira_data_get_selected_index(void) {
    return g_jira_state.selected_index;
}

bool jira_data_is_synced(void) {
    return g_jira_state.synced;
}

void jira_data_select(int8_t index) {
    if (index == -1 || (index >= 0 && index < g_jira_state.project_count)) {
        g_jira_state.selected_index = index;
    }
}
