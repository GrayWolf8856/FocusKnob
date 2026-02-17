#ifndef USB_SYNC_H
#define USB_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum size for note text
#define USB_SYNC_MAX_NOTE_LEN 256
// Maximum pending notes in queue
#define USB_SYNC_MAX_PENDING_NOTES 10
// Serial buffer size (large enough for JIRA_PROJECTS JSON with descriptions)
#define USB_SYNC_BUFFER_SIZE 8192

// Note entry for Notion sync
typedef struct {
    char text[USB_SYNC_MAX_NOTE_LEN];
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    bool pending;
} usb_sync_note_t;

// Initialize USB sync module
void usb_sync_init(void);

// Process incoming serial commands - call from loop()
void usb_sync_process(void);

// Queue a note to be sent to Notion when USB is connected
// Returns true if note was queued successfully
bool usb_sync_queue_note(const char* text);

// Check if USB sync is currently connected/active
bool usb_sync_is_connected(void);

// Send pending time logs via USB
void usb_sync_send_pending_logs(void);

// Send Jira timer completion notification
void usb_sync_send_jira_timer_done(const char* project_key, uint16_t duration_minutes);

// Send manual Jira time log request (Mac prompts for duration)
void usb_sync_send_jira_log_time(const char* issue_key);

// Send request to open Jira issue in browser
void usb_sync_send_jira_open(const char* issue_key);

// Send calendar meeting log request to Mac
void usb_sync_send_jira_log_meeting(const char* title, uint16_t duration_min);

#ifdef __cplusplus
}
#endif

#endif // USB_SYNC_H
