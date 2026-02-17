#ifndef JIRA_DATA_H
#define JIRA_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Limits
#define JIRA_MAX_PROJECTS 20
#define JIRA_KEY_LEN 16
#define JIRA_NAME_LEN 48
#define JIRA_PROJ_LEN 24
#define JIRA_STATUS_LEN 16
#define JIRA_DESC_LEN 128

// Single issue entry
typedef struct {
    char key[JIRA_KEY_LEN];       // e.g. "DEMOCAI-44"
    char name[JIRA_NAME_LEN];     // Issue summary
    char proj[JIRA_PROJ_LEN];     // Project name e.g. "Democratized AI"
    char status[JIRA_STATUS_LEN]; // Status e.g. "In Progress"
    char desc[JIRA_DESC_LEN];     // Description (first ~3 lines)
} jira_project_t;

// Full Jira state (RAM-only, refreshed each USB connection)
typedef struct {
    jira_project_t projects[JIRA_MAX_PROJECTS];
    uint8_t project_count;
    int8_t selected_index;   // -1 = none selected
    bool synced;             // true if projects received from Mac
} jira_state_t;

// Initialize Jira data module
void jira_data_init(void);

// Parse JSON project list from Mac companion
// Expected format: [{"key":"PROJ","name":"Project Name"},...]
void jira_data_set_projects(const char* json);

// Getters
uint8_t jira_data_get_count(void);
const jira_project_t* jira_data_get_project(uint8_t index);
const jira_project_t* jira_data_get_selected(void);
int8_t jira_data_get_selected_index(void);
bool jira_data_is_synced(void);

// Select a project by index
void jira_data_select(int8_t index);

#ifdef __cplusplus
}
#endif

#endif // JIRA_DATA_H
