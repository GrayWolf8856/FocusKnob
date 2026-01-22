#ifndef TIME_LOG_H
#define TIME_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum sessions to store per day (to limit memory usage)
#define MAX_SESSIONS_PER_DAY 20
// Maximum days to keep in history
#define MAX_DAYS_HISTORY 7
// File path for storage
#define TIME_LOG_FILE "/time_log.json"

// Session type enum
typedef enum {
    SESSION_WORK,
    SESSION_BREAK
} session_type_t;

// Individual session record
typedef struct {
    uint8_t start_hour;
    uint8_t start_minute;
    uint8_t end_hour;
    uint8_t end_minute;
    uint16_t duration_minutes;
    session_type_t type;
} session_record_t;

// Daily log structure
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    session_record_t sessions[MAX_SESSIONS_PER_DAY];
    uint8_t session_count;
    uint16_t total_work_minutes;
    uint16_t total_break_minutes;
    uint8_t pomodoros_completed;
} daily_log_t;

// Time log manager structure
typedef struct {
    daily_log_t days[MAX_DAYS_HISTORY];
    uint8_t day_count;
    uint8_t current_streak;
    bool initialized;
} time_log_t;

// Initialize the time logging system (call on boot)
void time_log_init(void);

// Log a completed session
// Returns true if successfully logged
bool time_log_add_session(session_type_t type, uint16_t duration_minutes);

// Get today's statistics
uint16_t time_log_get_today_work_minutes(void);
uint8_t time_log_get_today_pomodoros(void);
uint8_t time_log_get_current_streak(void);

// Get today's log (for UI display)
const daily_log_t* time_log_get_today(void);

// Save log to flash storage
bool time_log_save(void);

// Load log from flash storage
bool time_log_load(void);

// Format duration as string (e.g., "2h 15m")
void time_log_format_duration(uint16_t minutes, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // TIME_LOG_H
