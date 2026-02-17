#ifndef CALENDAR_DATA_H
#define CALENDAR_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Buffer limits
#define CALENDAR_MAX_EVENTS 10
#define CALENDAR_TITLE_LEN 32
#define CALENDAR_TIME_LEN 8
#define CALENDAR_LOCATION_LEN 32

// Single calendar event
typedef struct {
    char title[CALENDAR_TITLE_LEN];          // e.g. "Team Standup"
    char start_str[CALENDAR_TIME_LEN];       // "10:00a" pre-formatted 12h
    char start_time[CALENDAR_TIME_LEN];      // "10:00" 24h format
    char end_time[CALENDAR_TIME_LEN];        // "10:30" 24h format
    uint16_t duration_min;                   // Duration in minutes
    bool is_all_day;                         // All-day event flag
    char location[CALENDAR_LOCATION_LEN];    // e.g. "Zoom", "Room A"
} calendar_event_t;

// Full calendar state
typedef struct {
    calendar_event_t events[CALENDAR_MAX_EVENTS];
    uint8_t event_count;
    int16_t next_meeting_min;    // Minutes until next: -1 = in progress, -2 = none
    bool synced;                 // true if data received from Mac
} calendar_state_t;

// Initialize calendar data module
void calendar_data_init(void);

// Parse calendar JSON from Mac companion
void calendar_data_set(const char* json);

// Getters
uint8_t calendar_data_get_count(void);
const calendar_event_t* calendar_data_get_event(uint8_t index);
int16_t calendar_data_get_next_meeting_min(void);
bool calendar_data_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif // CALENDAR_DATA_H
