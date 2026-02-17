#ifndef JIRA_HOURS_DATA_H
#define JIRA_HOURS_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t logged_min;   // Minutes logged to Jira today
    uint16_t target_min;   // 480 weekday, 0 weekend
    bool synced;
} jira_hours_state_t;

void jira_hours_data_init(void);
void jira_hours_data_set(const char* json);
const jira_hours_state_t* jira_hours_data_get(void);
bool jira_hours_data_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif // JIRA_HOURS_DATA_H
