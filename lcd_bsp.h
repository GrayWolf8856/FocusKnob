#ifndef LCD_BSP_H
#define LCD_BSP_H
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "lvgl.h"
#include "esp_check.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

void lcd_lvgl_Init(void);

// Timer control functions (call from main sketch)
void timer_knob_left(void);
void timer_knob_right(void);
void timer_knob_press(void);

// Screen state check
bool is_timer_screen_active(void);

// Jira knob control (called from main sketch)
void jira_knob_left(void);
void jira_knob_right(void);
void jira_knob_press(void);

// Jira screen state checks
bool is_jira_screen_active(void);
bool is_jira_timer_screen_active(void);
bool is_jira_picker_open(void);

// Called by USB sync when Jira project data arrives
void jira_update_projects_ui(void);

// Called by USB sync when Jira log response arrives
void jira_update_log_status(bool success, const char* message);

// Called by USB sync when weather data arrives
void weather_update_ui(void);

// Weather screen state check
bool is_weather_screen_active(void);

// Called by USB sync when calendar data arrives
void calendar_update_ui(void);

// Calendar screen state check
bool is_calendar_screen_active(void);

// Called by USB sync when Jira hours data arrives
void jira_hours_update_ui(void);

// Touch function (from cst816)
uint8_t getTouch(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif

#endif
