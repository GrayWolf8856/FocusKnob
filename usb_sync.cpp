/*
 * USB Sync Implementation
 *
 * Handles serial communication with companion computer app for:
 * - Time synchronization
 * - Sending time logs
 * - Sending notes to Notion
 */

#include "usb_sync.h"
#include "time_log.h"
#include "jira_data.h"
#include "jira_hours_data.h"
#include "weather_data.h"
#include "calendar_data.h"
#include "lcd_bsp.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

// Serial command buffer
static char g_serial_buffer[USB_SYNC_BUFFER_SIZE];
static uint16_t g_buffer_index = 0;

// Pending notes queue
static usb_sync_note_t g_pending_notes[USB_SYNC_MAX_PENDING_NOTES];
static uint8_t g_note_count = 0;

// Connection state
static bool g_connected = false;
static unsigned long g_last_ping_time = 0;
#define CONNECTION_TIMEOUT_MS 15000

// Forward declarations
static void handle_command(const char* command);
static void handle_time_command(const char* payload);
static void handle_ping(void);
static void handle_get_logs(void);
static void send_ready(void);
static void send_pending_notes(void);
static void handle_jira_projects(const char* json_payload);
static void handle_jira_log_ok(void);
static void handle_jira_log_error(const char* message);

// jira_update_log_status and jira_update_projects_ui declared in lcd_bsp.h

// Initialize USB sync module
void usb_sync_init(void) {
    Serial.println("USBSync: Initialized");
    g_buffer_index = 0;
    g_note_count = 0;
    g_connected = false;
    memset(g_serial_buffer, 0, sizeof(g_serial_buffer));
    memset(g_pending_notes, 0, sizeof(g_pending_notes));
}

// Process incoming serial commands
void usb_sync_process(void) {
    // Check for connection timeout
    if (g_connected && (millis() - g_last_ping_time > CONNECTION_TIMEOUT_MS)) {
        g_connected = false;
        Serial.println("USBSync: Connection timed out");
    }

    // Read available serial data
    while (Serial.available()) {
        char c = Serial.read();

        // End of command (newline)
        if (c == '\n') {
            g_serial_buffer[g_buffer_index] = '\0';
            if (g_buffer_index > 0) {
                handle_command(g_serial_buffer);
            }
            g_buffer_index = 0;
            memset(g_serial_buffer, 0, sizeof(g_serial_buffer));
        }
        // Ignore carriage return
        else if (c == '\r') {
            continue;
        }
        // Add character to buffer
        else if (g_buffer_index < USB_SYNC_BUFFER_SIZE - 1) {
            g_serial_buffer[g_buffer_index++] = c;
        }
    }
}

// Handle a complete command
static void handle_command(const char* command) {
    Serial.printf("USBSync: Received command: %s\n", command);

    // PING command
    if (strcmp(command, "PING") == 0) {
        handle_ping();
    }
    // TIME command: TIME:YYYY-MM-DDTHH:MM:SS
    else if (strncmp(command, "TIME:", 5) == 0) {
        handle_time_command(command + 5);
    }
    // GET_LOGS command
    else if (strcmp(command, "GET_LOGS") == 0) {
        handle_get_logs();
    }
    // OK acknowledgment from computer
    else if (strcmp(command, "OK") == 0) {
        // Acknowledgment received - can remove sent items from queue
        if (g_note_count > 0) {
            // Remove first note from queue
            memmove(&g_pending_notes[0], &g_pending_notes[1],
                    sizeof(usb_sync_note_t) * (USB_SYNC_MAX_PENDING_NOTES - 1));
            g_note_count--;
            Serial.printf("USBSync: Note acknowledged, %d remaining\n", g_note_count);
        }
    }
    // JIRA_PROJECTS:<json> - project list from Mac
    else if (strncmp(command, "JIRA_PROJECTS:", 14) == 0) {
        handle_jira_projects(command + 14);
    }
    // JIRA_LOG_OK - worklog posted successfully
    else if (strcmp(command, "JIRA_LOG_OK") == 0) {
        handle_jira_log_ok();
    }
    // JIRA_LOG_ERROR:<message>
    else if (strncmp(command, "JIRA_LOG_ERROR:", 15) == 0) {
        handle_jira_log_error(command + 15);
    }
    // WEATHER:<json> - weather data from Mac
    else if (strncmp(command, "WEATHER:", 8) == 0) {
        weather_data_set(command + 8);
        Serial.println("WEATHER_OK");
        weather_update_ui();
    }
    // CALENDAR:<json> - calendar data from Mac
    else if (strncmp(command, "CALENDAR:", 9) == 0) {
        calendar_data_set(command + 9);
        Serial.println("CALENDAR_OK");
        calendar_update_ui();
    }
    // JIRA_HOURS:<json> - daily hours from Mac
    else if (strncmp(command, "JIRA_HOURS:", 11) == 0) {
        jira_hours_data_set(command + 11);
        Serial.println("JIRA_HOURS_OK");
        jira_hours_update_ui();
    }
    // Unknown command
    else {
        Serial.printf("USBSync: Unknown command: %s\n", command);
        Serial.println("ERROR:Unknown command");
    }
}

// Handle PING command
static void handle_ping(void) {
    g_connected = true;
    g_last_ping_time = millis();
    Serial.println("PONG");

    // Send any pending notes
    send_pending_notes();
}

// Handle TIME command
static void handle_time_command(const char* payload) {
    // Parse ISO8601 format: YYYY-MM-DDTHH:MM:SS
    int year, month, day, hour, minute, second;

    if (sscanf(payload, "%d-%d-%dT%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        Serial.println("ERROR:Invalid time format");
        return;
    }

    // Set system time
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = -1;

    time_t t = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };

    if (settimeofday(&tv, NULL) == 0) {
        Serial.println("TIME_OK");
        Serial.printf("USBSync: Time set to %04d-%02d-%02d %02d:%02d:%02d\n",
                     year, month, day, hour, minute, second);
    } else {
        Serial.println("ERROR:Failed to set time");
    }
}

// Handle GET_LOGS command
static void handle_get_logs(void) {
    usb_sync_send_pending_logs();
}

// Send READY identification
static void send_ready(void) {
    Serial.println("READY:FocusKnob");
}

// Send pending notes to computer
static void send_pending_notes(void) {
    for (int i = 0; i < g_note_count; i++) {
        if (g_pending_notes[i].pending) {
            StaticJsonDocument<512> doc;
            doc["text"] = g_pending_notes[i].text;

            char date_str[20];
            snprintf(date_str, sizeof(date_str), "%04d-%02d-%02dT%02d:%02d:00",
                    g_pending_notes[i].year, g_pending_notes[i].month,
                    g_pending_notes[i].day, g_pending_notes[i].hour,
                    g_pending_notes[i].minute);
            doc["timestamp"] = date_str;

            Serial.print("NOTE:");
            serializeJson(doc, Serial);
            Serial.println();

            // Mark as sent (will be removed on OK acknowledgment)
            g_pending_notes[i].pending = false;

            // Only send one at a time, wait for OK
            break;
        }
    }
}

// Queue a note for Notion sync
bool usb_sync_queue_note(const char* text) {
    if (g_note_count >= USB_SYNC_MAX_PENDING_NOTES) {
        Serial.println("USBSync: Note queue full!");
        return false;
    }

    usb_sync_note_t* note = &g_pending_notes[g_note_count];

    // Copy text
    strncpy(note->text, text, USB_SYNC_MAX_NOTE_LEN - 1);
    note->text[USB_SYNC_MAX_NOTE_LEN - 1] = '\0';

    // Get current timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    note->year = timeinfo.tm_year + 1900;
    note->month = timeinfo.tm_mon + 1;
    note->day = timeinfo.tm_mday;
    note->hour = timeinfo.tm_hour;
    note->minute = timeinfo.tm_min;
    note->pending = true;

    g_note_count++;
    Serial.printf("USBSync: Note queued (%d pending)\n", g_note_count);

    // If connected, send immediately
    if (g_connected) {
        send_pending_notes();
    }

    return true;
}

// Check connection state
bool usb_sync_is_connected(void) {
    return g_connected;
}

// Handle JIRA_PROJECTS command
static void handle_jira_projects(const char* json_payload) {
    jira_data_set_projects(json_payload);
    Serial.println("JIRA_PROJECTS_OK");
    // Refresh Jira UI if visible
    jira_update_projects_ui();
}

// Handle JIRA_LOG_OK command
static void handle_jira_log_ok(void) {
    jira_update_log_status(true, "Logged to Jira!");
    Serial.println("USBSync: Jira worklog confirmed");
}

// Handle JIRA_LOG_ERROR command
static void handle_jira_log_error(const char* message) {
    jira_update_log_status(false, message);
    Serial.printf("USBSync: Jira worklog failed: %s\n", message);
}

// Send Jira timer completion notification
// Uses simple pipe-delimited format to avoid USB CDC byte-drop on long JSON:
//   JIRA_TIMER_DONE:ISSUE_KEY|MINUTES
// Kept short (<64 bytes) to fit in single USB packet
void usb_sync_send_jira_timer_done(const char* project_key, uint16_t duration_minutes) {
    char buf[64];
    snprintf(buf, sizeof(buf), "JIRA_TIMER_DONE:%s|%u", project_key, duration_minutes);
    Serial.println(buf);
}

// Send manual Jira time log request
// Mac companion will prompt for duration and description
void usb_sync_send_jira_log_time(const char* issue_key) {
    char buf[64];
    snprintf(buf, sizeof(buf), "JIRA_LOG_TIME:%s", issue_key);
    Serial.println(buf);
}

// Send request to open Jira issue in browser on Mac
void usb_sync_send_jira_open(const char* issue_key) {
    char buf[64];
    snprintf(buf, sizeof(buf), "JIRA_OPEN:%s", issue_key);
    Serial.println(buf);
}

// Send pending time logs
void usb_sync_send_pending_logs(void) {
    const daily_log_t* today = time_log_get_today();
    if (!today) {
        Serial.println("LOG:{}");
        return;
    }

    // Create JSON with today's log data
    StaticJsonDocument<2048> doc;

    char date_str[12];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
             today->year, today->month, today->day);
    doc["date"] = date_str;
    doc["total_work_minutes"] = today->total_work_minutes;
    doc["total_break_minutes"] = today->total_break_minutes;
    doc["pomodoros"] = today->pomodoros_completed;

    JsonArray sessions = doc.createNestedArray("sessions");
    for (int i = 0; i < today->session_count; i++) {
        const session_record_t* sess = &today->sessions[i];
        JsonObject sessObj = sessions.createNestedObject();

        sessObj["type"] = (sess->type == SESSION_WORK) ? "work" : "break";

        // Format times in 12-hour format with AM/PM
        char time_str[12];
        uint8_t start_hr_12 = sess->start_hour % 12;
        if (start_hr_12 == 0) start_hr_12 = 12;
        snprintf(time_str, sizeof(time_str), "%d:%02d %s",
                 start_hr_12, sess->start_minute,
                 sess->start_hour < 12 ? "AM" : "PM");
        sessObj["start"] = time_str;

        uint8_t end_hr_12 = sess->end_hour % 12;
        if (end_hr_12 == 0) end_hr_12 = 12;
        snprintf(time_str, sizeof(time_str), "%d:%02d %s",
                 end_hr_12, sess->end_minute,
                 sess->end_hour < 12 ? "AM" : "PM");
        sessObj["end"] = time_str;

        sessObj["duration"] = sess->duration_minutes;
    }

    Serial.print("LOG:");
    serializeJson(doc, Serial);
    Serial.println();
}

// Send calendar meeting log request to Mac
// Uses pipe-delimited format: JIRA_LOG_MEETING:Title|duration_min
void usb_sync_send_jira_log_meeting(const char* title, uint16_t duration_min) {
    char buf[128];
    char short_title[64];
    strncpy(short_title, title, 63);
    short_title[63] = '\0';
    snprintf(buf, sizeof(buf), "JIRA_LOG_MEETING:%s|%u", short_title, duration_min);
    Serial.println(buf);
}
