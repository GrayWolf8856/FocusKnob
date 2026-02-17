#include "calendar_data.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static calendar_state_t g_calendar_state;

void calendar_data_init(void) {
    memset(&g_calendar_state, 0, sizeof(g_calendar_state));
    g_calendar_state.next_meeting_min = -2;
    g_calendar_state.synced = false;
    Serial.println("CalendarData: Initialized");
}

void calendar_data_set(const char* json) {
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.printf("CalendarData: JSON parse error: %s\n", err.c_str());
        return;
    }

    // Parse events array
    JsonArray events = doc["events"];
    g_calendar_state.event_count = 0;

    if (!events.isNull()) {
        for (JsonObject ev : events) {
            if (g_calendar_state.event_count >= CALENDAR_MAX_EVENTS) break;

            calendar_event_t *e = &g_calendar_state.events[g_calendar_state.event_count];

            const char* title = ev["title"] | "No Title";
            strncpy(e->title, title, CALENDAR_TITLE_LEN - 1);
            e->title[CALENDAR_TITLE_LEN - 1] = '\0';

            const char* start_str = ev["start_str"] | "";
            strncpy(e->start_str, start_str, CALENDAR_TIME_LEN - 1);
            e->start_str[CALENDAR_TIME_LEN - 1] = '\0';

            const char* start_time = ev["start_time"] | "";
            strncpy(e->start_time, start_time, CALENDAR_TIME_LEN - 1);
            e->start_time[CALENDAR_TIME_LEN - 1] = '\0';

            const char* end_time = ev["end_time"] | "";
            strncpy(e->end_time, end_time, CALENDAR_TIME_LEN - 1);
            e->end_time[CALENDAR_TIME_LEN - 1] = '\0';

            e->duration_min = ev["duration_min"] | 0;
            e->is_all_day = ev["is_all_day"] | false;

            const char* location = ev["location"] | "";
            strncpy(e->location, location, CALENDAR_LOCATION_LEN - 1);
            e->location[CALENDAR_LOCATION_LEN - 1] = '\0';

            g_calendar_state.event_count++;
        }
    }

    // Next meeting minutes (pre-computed by Mac)
    g_calendar_state.next_meeting_min = doc["next_meeting_min"] | -2;

    g_calendar_state.synced = true;

    Serial.printf("CalendarData: %d events, next in %d min\n",
                  g_calendar_state.event_count, g_calendar_state.next_meeting_min);
}

uint8_t calendar_data_get_count(void) {
    return g_calendar_state.event_count;
}

const calendar_event_t* calendar_data_get_event(uint8_t index) {
    if (index >= g_calendar_state.event_count) return NULL;
    return &g_calendar_state.events[index];
}

int16_t calendar_data_get_next_meeting_min(void) {
    return g_calendar_state.next_meeting_min;
}

bool calendar_data_is_synced(void) {
    return g_calendar_state.synced;
}
