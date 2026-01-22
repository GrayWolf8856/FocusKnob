/*
 * Time Log Implementation
 *
 * Handles persistent storage of Pomodoro session data using LittleFS.
 * Uses ArduinoJson for JSON serialization/deserialization.
 *
 * Storage format (JSON):
 * {
 *   "streak": 5,
 *   "days": [
 *     {
 *       "date": "2024-01-15",
 *       "work": 150,
 *       "break": 25,
 *       "pomos": 6,
 *       "sessions": [
 *         {"t": "w", "s": "09:00", "e": "09:25", "d": 25},
 *         ...
 *       ]
 *     }
 *   ]
 * }
 */

#include "time_log.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

// Global time log instance
static time_log_t g_time_log;

// Forward declarations
static bool get_current_date(uint16_t* year, uint8_t* month, uint8_t* day);
static bool get_current_time(uint8_t* hour, uint8_t* minute);
static int find_today_index(void);
static void ensure_today_exists(void);
static void calculate_streak(void);
static bool is_consecutive_day(const daily_log_t* day1, const daily_log_t* day2);

// Initialize the time logging system
void time_log_init(void) {
    Serial.println("TimeLog: Initializing...");

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // true = format if mount fails
        Serial.println("TimeLog: LittleFS mount failed!");
        g_time_log.initialized = false;
        return;
    }
    Serial.println("TimeLog: LittleFS mounted");

    // Clear the log structure
    memset(&g_time_log, 0, sizeof(g_time_log));

    // Try to load existing log
    if (time_log_load()) {
        Serial.println("TimeLog: Loaded existing log");
    } else {
        Serial.println("TimeLog: Starting fresh log");
    }

    // Ensure today's entry exists
    ensure_today_exists();

    // Calculate current streak
    calculate_streak();

    g_time_log.initialized = true;
    Serial.printf("TimeLog: Ready. Streak: %d days\n", g_time_log.current_streak);
}

// Get current date from system time
static bool get_current_date(uint16_t* year, uint8_t* month, uint8_t* day) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    *year = timeinfo.tm_year + 1900;
    *month = timeinfo.tm_mon + 1;
    *day = timeinfo.tm_mday;

    return true;
}

// Get current time
static bool get_current_time(uint8_t* hour, uint8_t* minute) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    *hour = timeinfo.tm_hour;
    *minute = timeinfo.tm_min;

    return true;
}

// Find index of today's log entry, returns -1 if not found
static int find_today_index(void) {
    uint16_t year;
    uint8_t month, day;
    get_current_date(&year, &month, &day);

    for (int i = 0; i < g_time_log.day_count; i++) {
        if (g_time_log.days[i].year == year &&
            g_time_log.days[i].month == month &&
            g_time_log.days[i].day == day) {
            return i;
        }
    }
    return -1;
}

// Ensure today's entry exists in the log
static void ensure_today_exists(void) {
    int idx = find_today_index();
    if (idx >= 0) return;  // Already exists

    uint16_t year;
    uint8_t month, day;
    get_current_date(&year, &month, &day);

    // If log is full, shift out oldest day
    if (g_time_log.day_count >= MAX_DAYS_HISTORY) {
        memmove(&g_time_log.days[0], &g_time_log.days[1],
                sizeof(daily_log_t) * (MAX_DAYS_HISTORY - 1));
        g_time_log.day_count = MAX_DAYS_HISTORY - 1;
    }

    // Add new day at the end
    daily_log_t* today = &g_time_log.days[g_time_log.day_count];
    memset(today, 0, sizeof(daily_log_t));
    today->year = year;
    today->month = month;
    today->day = day;
    g_time_log.day_count++;
}

// Check if two days are consecutive
static bool is_consecutive_day(const daily_log_t* day1, const daily_log_t* day2) {
    // Simple check - this doesn't handle month/year boundaries perfectly
    // but is good enough for streak tracking
    if (day1->year == day2->year && day1->month == day2->month) {
        return (day2->day - day1->day == 1);
    }
    // Handle month boundary (simplified)
    if (day1->year == day2->year && day2->month - day1->month == 1 && day2->day == 1) {
        return true;  // First day of next month
    }
    return false;
}

// Calculate current streak
static void calculate_streak(void) {
    g_time_log.current_streak = 0;

    if (g_time_log.day_count == 0) return;

    // Start from most recent day and count backwards
    for (int i = g_time_log.day_count - 1; i >= 0; i--) {
        if (g_time_log.days[i].pomodoros_completed > 0) {
            g_time_log.current_streak++;

            // Check if previous day is consecutive
            if (i > 0 && !is_consecutive_day(&g_time_log.days[i-1], &g_time_log.days[i])) {
                break;
            }
        } else if (i == g_time_log.day_count - 1) {
            // Today has no pomodoros yet, check if yesterday continues streak
            continue;
        } else {
            break;
        }
    }
}

// Log a completed session
bool time_log_add_session(session_type_t type, uint16_t duration_minutes) {
    if (!g_time_log.initialized) {
        Serial.println("TimeLog: Not initialized!");
        return false;
    }

    ensure_today_exists();
    int idx = find_today_index();
    if (idx < 0) {
        Serial.println("TimeLog: Failed to find today's entry!");
        return false;
    }

    daily_log_t* today = &g_time_log.days[idx];

    // Check if we have room for more sessions
    if (today->session_count >= MAX_SESSIONS_PER_DAY) {
        Serial.println("TimeLog: Max sessions reached for today");
        // Still update totals even if we can't store the session detail
    } else {
        // Get current time for end time
        uint8_t end_hour, end_minute;
        get_current_time(&end_hour, &end_minute);

        // Calculate start time (current time - duration)
        int start_total_minutes = (end_hour * 60 + end_minute) - duration_minutes;
        if (start_total_minutes < 0) start_total_minutes += 24 * 60;

        uint8_t start_hour = start_total_minutes / 60;
        uint8_t start_minute = start_total_minutes % 60;

        // Add session record
        session_record_t* session = &today->sessions[today->session_count];
        session->start_hour = start_hour;
        session->start_minute = start_minute;
        session->end_hour = end_hour;
        session->end_minute = end_minute;
        session->duration_minutes = duration_minutes;
        session->type = type;
        today->session_count++;
    }

    // Update totals
    if (type == SESSION_WORK) {
        today->total_work_minutes += duration_minutes;
        today->pomodoros_completed++;
        Serial.printf("TimeLog: Work session logged. Total: %d min, Pomos: %d\n",
                     today->total_work_minutes, today->pomodoros_completed);
    } else {
        today->total_break_minutes += duration_minutes;
        Serial.printf("TimeLog: Break session logged. Total breaks: %d min\n",
                     today->total_break_minutes);
    }

    // Recalculate streak
    calculate_streak();

    // Save to flash (non-blocking would be better, but keeping simple)
    return time_log_save();
}

// Get today's work minutes
uint16_t time_log_get_today_work_minutes(void) {
    int idx = find_today_index();
    if (idx < 0) return 0;
    return g_time_log.days[idx].total_work_minutes;
}

// Get today's pomodoro count
uint8_t time_log_get_today_pomodoros(void) {
    int idx = find_today_index();
    if (idx < 0) return 0;
    return g_time_log.days[idx].pomodoros_completed;
}

// Get current streak
uint8_t time_log_get_current_streak(void) {
    return g_time_log.current_streak;
}

// Get today's log
const daily_log_t* time_log_get_today(void) {
    int idx = find_today_index();
    if (idx < 0) return NULL;
    return &g_time_log.days[idx];
}

// Save log to flash storage
bool time_log_save(void) {
    Serial.println("TimeLog: Saving to flash...");

    // Create JSON document
    // Size calculation: base + (days * (date + sessions + stats))
    StaticJsonDocument<4096> doc;

    doc["streak"] = g_time_log.current_streak;

    JsonArray days = doc.createNestedArray("days");

    for (int i = 0; i < g_time_log.day_count; i++) {
        const daily_log_t* day = &g_time_log.days[i];

        JsonObject dayObj = days.createNestedObject();

        // Format date as YYYY-MM-DD
        char date_str[12];
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                 day->year, day->month, day->day);
        dayObj["date"] = date_str;

        dayObj["work"] = day->total_work_minutes;
        dayObj["break"] = day->total_break_minutes;
        dayObj["pomos"] = day->pomodoros_completed;

        // Add sessions (limit to save space)
        JsonArray sessions = dayObj.createNestedArray("sessions");
        int max_sessions = min((int)day->session_count, 10);  // Limit stored sessions

        for (int j = 0; j < max_sessions; j++) {
            const session_record_t* sess = &day->sessions[j];
            JsonObject sessObj = sessions.createNestedObject();

            sessObj["t"] = (sess->type == SESSION_WORK) ? "w" : "b";

            char time_str[6];
            snprintf(time_str, sizeof(time_str), "%02d:%02d",
                     sess->start_hour, sess->start_minute);
            sessObj["s"] = time_str;

            snprintf(time_str, sizeof(time_str), "%02d:%02d",
                     sess->end_hour, sess->end_minute);
            sessObj["e"] = time_str;

            sessObj["d"] = sess->duration_minutes;
        }
    }

    // Write to file
    File file = LittleFS.open(TIME_LOG_FILE, "w");
    if (!file) {
        Serial.println("TimeLog: Failed to open file for writing!");
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();

    Serial.printf("TimeLog: Saved %d bytes\n", written);
    return written > 0;
}

// Load log from flash storage
bool time_log_load(void) {
    Serial.println("TimeLog: Loading from flash...");

    if (!LittleFS.exists(TIME_LOG_FILE)) {
        Serial.println("TimeLog: No existing log file");
        return false;
    }

    File file = LittleFS.open(TIME_LOG_FILE, "r");
    if (!file) {
        Serial.println("TimeLog: Failed to open file for reading!");
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("TimeLog: JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Parse streak
    g_time_log.current_streak = doc["streak"] | 0;

    // Parse days
    JsonArray days = doc["days"];
    g_time_log.day_count = 0;

    for (JsonObject dayObj : days) {
        if (g_time_log.day_count >= MAX_DAYS_HISTORY) break;

        daily_log_t* day = &g_time_log.days[g_time_log.day_count];
        memset(day, 0, sizeof(daily_log_t));

        // Parse date (YYYY-MM-DD)
        const char* date_str = dayObj["date"];
        if (date_str && strlen(date_str) >= 10) {
            sscanf(date_str, "%hu-%hhu-%hhu", &day->year, &day->month, &day->day);
        }

        day->total_work_minutes = dayObj["work"] | 0;
        day->total_break_minutes = dayObj["break"] | 0;
        day->pomodoros_completed = dayObj["pomos"] | 0;

        // Parse sessions
        JsonArray sessions = dayObj["sessions"];
        day->session_count = 0;

        for (JsonObject sessObj : sessions) {
            if (day->session_count >= MAX_SESSIONS_PER_DAY) break;

            session_record_t* sess = &day->sessions[day->session_count];

            const char* type_str = sessObj["t"];
            sess->type = (type_str && type_str[0] == 'w') ? SESSION_WORK : SESSION_BREAK;

            const char* start_str = sessObj["s"];
            if (start_str) {
                sscanf(start_str, "%hhu:%hhu", &sess->start_hour, &sess->start_minute);
            }

            const char* end_str = sessObj["e"];
            if (end_str) {
                sscanf(end_str, "%hhu:%hhu", &sess->end_hour, &sess->end_minute);
            }

            sess->duration_minutes = sessObj["d"] | 0;
            day->session_count++;
        }

        g_time_log.day_count++;
    }

    Serial.printf("TimeLog: Loaded %d days of history\n", g_time_log.day_count);
    return true;
}

// Format duration as human-readable string
void time_log_format_duration(uint16_t minutes, char* buffer, size_t buffer_size) {
    if (minutes < 60) {
        snprintf(buffer, buffer_size, "%dm", minutes);
    } else {
        uint16_t hours = minutes / 60;
        uint16_t mins = minutes % 60;
        if (mins == 0) {
            snprintf(buffer, buffer_size, "%dh", hours);
        } else {
            snprintf(buffer, buffer_size, "%dh %dm", hours, mins);
        }
    }
}
