#include "lcd_bsp.h"
#include "esp_lcd_sh8601.h"
#include "lcd_config.h"
#include "cst816.h"
#include "drv2605.h"
#include "time_log.h"
#include "wifi_config.h"
#include "jira_data.h"
#include "weather_data.h"
#include "calendar_data.h"
#include "jira_hours_data.h"
#include "usb_sync.h"
#include "home_bg.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>

static SemaphoreHandle_t lvgl_mux = NULL;
#define LCD_HOST    SPI2_HOST

static esp_lcd_panel_io_handle_t amoled_panel_io_handle = NULL;

// Forward declarations
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area);
static void example_increase_lvgl_tick(void *arg);
static void example_lvgl_port_task(void *arg);
static void example_lvgl_unlock(void);
static bool example_lvgl_lock(int timeout_ms);
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] =
{
  {0xF0, (uint8_t[]){0x28}, 1, 0},
  {0xF2, (uint8_t[]){0x28}, 1, 0},
  {0x73, (uint8_t[]){0xF0}, 1, 0},
  {0x7C, (uint8_t[]){0xD1}, 1, 0},
  {0x83, (uint8_t[]){0xE0}, 1, 0},
  {0x84, (uint8_t[]){0x61}, 1, 0},
  {0xF2, (uint8_t[]){0x82}, 1, 0},
  {0xF0, (uint8_t[]){0x00}, 1, 0},
  {0xF0, (uint8_t[]){0x01}, 1, 0},
  {0xF1, (uint8_t[]){0x01}, 1, 0},
  {0xB0, (uint8_t[]){0x56}, 1, 0},
  {0xB1, (uint8_t[]){0x4D}, 1, 0},
  {0xB2, (uint8_t[]){0x24}, 1, 0},
  {0xB4, (uint8_t[]){0x87}, 1, 0},
  {0xB5, (uint8_t[]){0x44}, 1, 0},
  {0xB6, (uint8_t[]){0x8B}, 1, 0},
  {0xB7, (uint8_t[]){0x40}, 1, 0},
  {0xB8, (uint8_t[]){0x86}, 1, 0},
  {0xBA, (uint8_t[]){0x00}, 1, 0},
  {0xBB, (uint8_t[]){0x08}, 1, 0},
  {0xBC, (uint8_t[]){0x08}, 1, 0},
  {0xBD, (uint8_t[]){0x00}, 1, 0},
  {0xC0, (uint8_t[]){0x80}, 1, 0},
  {0xC1, (uint8_t[]){0x10}, 1, 0},
  {0xC2, (uint8_t[]){0x37}, 1, 0},
  {0xC3, (uint8_t[]){0x80}, 1, 0},
  {0xC4, (uint8_t[]){0x10}, 1, 0},
  {0xC5, (uint8_t[]){0x37}, 1, 0},
  {0xC6, (uint8_t[]){0xA9}, 1, 0},
  {0xC7, (uint8_t[]){0x41}, 1, 0},
  {0xC8, (uint8_t[]){0x01}, 1, 0},
  {0xC9, (uint8_t[]){0xA9}, 1, 0},
  {0xCA, (uint8_t[]){0x41}, 1, 0},
  {0xCB, (uint8_t[]){0x01}, 1, 0},
  {0xD0, (uint8_t[]){0x91}, 1, 0},
  {0xD1, (uint8_t[]){0x68}, 1, 0},
  {0xD2, (uint8_t[]){0x68}, 1, 0},
  {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
  {0xDD, (uint8_t[]){0x4F}, 1, 0},
  {0xDE, (uint8_t[]){0x4F}, 1, 0},
  {0xF1, (uint8_t[]){0x10}, 1, 0},
  {0xF0, (uint8_t[]){0x00}, 1, 0},
  {0xF0, (uint8_t[]){0x02}, 1, 0},
  {0xE0, (uint8_t[]){0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34}, 14, 0},
  {0xE1, (uint8_t[]){0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33}, 14, 0},
  {0xF0, (uint8_t[]){0x10}, 1, 0},
  {0xF3, (uint8_t[]){0x10}, 1, 0},
  {0xE0, (uint8_t[]){0x07}, 1, 0},
  {0xE1, (uint8_t[]){0x00}, 1, 0},
  {0xE2, (uint8_t[]){0x00}, 1, 0},
  {0xE3, (uint8_t[]){0x00}, 1, 0},
  {0xE4, (uint8_t[]){0xE0}, 1, 0},
  {0xE5, (uint8_t[]){0x06}, 1, 0},
  {0xE6, (uint8_t[]){0x21}, 1, 0},
  {0xE7, (uint8_t[]){0x01}, 1, 0},
  {0xE8, (uint8_t[]){0x05}, 1, 0},
  {0xE9, (uint8_t[]){0x02}, 1, 0},
  {0xEA, (uint8_t[]){0xDA}, 1, 0},
  {0xEB, (uint8_t[]){0x00}, 1, 0},
  {0xEC, (uint8_t[]){0x00}, 1, 0},
  {0xED, (uint8_t[]){0x0F}, 1, 0},
  {0xEE, (uint8_t[]){0x00}, 1, 0},
  {0xEF, (uint8_t[]){0x00}, 1, 0},
  {0xF8, (uint8_t[]){0x00}, 1, 0},
  {0xF9, (uint8_t[]){0x00}, 1, 0},
  {0xFA, (uint8_t[]){0x00}, 1, 0},
  {0xFB, (uint8_t[]){0x00}, 1, 0},
  {0xFC, (uint8_t[]){0x00}, 1, 0},
  {0xFD, (uint8_t[]){0x00}, 1, 0},
  {0xFE, (uint8_t[]){0x00}, 1, 0},
  {0xFF, (uint8_t[]){0x00}, 1, 0},
  {0x60, (uint8_t[]){0x40}, 1, 0},
  {0x61, (uint8_t[]){0x04}, 1, 0},
  {0x62, (uint8_t[]){0x00}, 1, 0},
  {0x63, (uint8_t[]){0x42}, 1, 0},
  {0x64, (uint8_t[]){0xD9}, 1, 0},
  {0x65, (uint8_t[]){0x00}, 1, 0},
  {0x66, (uint8_t[]){0x00}, 1, 0},
  {0x67, (uint8_t[]){0x00}, 1, 0},
  {0x68, (uint8_t[]){0x00}, 1, 0},
  {0x69, (uint8_t[]){0x00}, 1, 0},
  {0x6A, (uint8_t[]){0x00}, 1, 0},
  {0x6B, (uint8_t[]){0x00}, 1, 0},
  {0x70, (uint8_t[]){0x40}, 1, 0},
  {0x71, (uint8_t[]){0x03}, 1, 0},
  {0x72, (uint8_t[]){0x00}, 1, 0},
  {0x73, (uint8_t[]){0x42}, 1, 0},
  {0x74, (uint8_t[]){0xD8}, 1, 0},
  {0x75, (uint8_t[]){0x00}, 1, 0},
  {0x76, (uint8_t[]){0x00}, 1, 0},
  {0x77, (uint8_t[]){0x00}, 1, 0},
  {0x78, (uint8_t[]){0x00}, 1, 0},
  {0x79, (uint8_t[]){0x00}, 1, 0},
  {0x7A, (uint8_t[]){0x00}, 1, 0},
  {0x7B, (uint8_t[]){0x00}, 1, 0},
  {0x80, (uint8_t[]){0x48}, 1, 0},
  {0x81, (uint8_t[]){0x00}, 1, 0},
  {0x82, (uint8_t[]){0x06}, 1, 0},
  {0x83, (uint8_t[]){0x02}, 1, 0},
  {0x84, (uint8_t[]){0xD6}, 1, 0},
  {0x85, (uint8_t[]){0x04}, 1, 0},
  {0x86, (uint8_t[]){0x00}, 1, 0},
  {0x87, (uint8_t[]){0x00}, 1, 0},
  {0x88, (uint8_t[]){0x48}, 1, 0},
  {0x89, (uint8_t[]){0x00}, 1, 0},
  {0x8A, (uint8_t[]){0x08}, 1, 0},
  {0x8B, (uint8_t[]){0x02}, 1, 0},
  {0x8C, (uint8_t[]){0xD8}, 1, 0},
  {0x8D, (uint8_t[]){0x04}, 1, 0},
  {0x8E, (uint8_t[]){0x00}, 1, 0},
  {0x8F, (uint8_t[]){0x00}, 1, 0},
  {0x90, (uint8_t[]){0x48}, 1, 0},
  {0x91, (uint8_t[]){0x00}, 1, 0},
  {0x92, (uint8_t[]){0x0A}, 1, 0},
  {0x93, (uint8_t[]){0x02}, 1, 0},
  {0x94, (uint8_t[]){0xDA}, 1, 0},
  {0x95, (uint8_t[]){0x04}, 1, 0},
  {0x96, (uint8_t[]){0x00}, 1, 0},
  {0x97, (uint8_t[]){0x00}, 1, 0},
  {0x98, (uint8_t[]){0x48}, 1, 0},
  {0x99, (uint8_t[]){0x00}, 1, 0},
  {0x9A, (uint8_t[]){0x0C}, 1, 0},
  {0x9B, (uint8_t[]){0x02}, 1, 0},
  {0x9C, (uint8_t[]){0xDC}, 1, 0},
  {0x9D, (uint8_t[]){0x04}, 1, 0},
  {0x9E, (uint8_t[]){0x00}, 1, 0},
  {0x9F, (uint8_t[]){0x00}, 1, 0},
  {0xA0, (uint8_t[]){0x48}, 1, 0},
  {0xA1, (uint8_t[]){0x00}, 1, 0},
  {0xA2, (uint8_t[]){0x05}, 1, 0},
  {0xA3, (uint8_t[]){0x02}, 1, 0},
  {0xA4, (uint8_t[]){0xD5}, 1, 0},
  {0xA5, (uint8_t[]){0x04}, 1, 0},
  {0xA6, (uint8_t[]){0x00}, 1, 0},
  {0xA7, (uint8_t[]){0x00}, 1, 0},
  {0xA8, (uint8_t[]){0x48}, 1, 0},
  {0xA9, (uint8_t[]){0x00}, 1, 0},
  {0xAA, (uint8_t[]){0x07}, 1, 0},
  {0xAB, (uint8_t[]){0x02}, 1, 0},
  {0xAC, (uint8_t[]){0xD7}, 1, 0},
  {0xAD, (uint8_t[]){0x04}, 1, 0},
  {0xAE, (uint8_t[]){0x00}, 1, 0},
  {0xAF, (uint8_t[]){0x00}, 1, 0},
  {0xB0, (uint8_t[]){0x48}, 1, 0},
  {0xB1, (uint8_t[]){0x00}, 1, 0},
  {0xB2, (uint8_t[]){0x09}, 1, 0},
  {0xB3, (uint8_t[]){0x02}, 1, 0},
  {0xB4, (uint8_t[]){0xD9}, 1, 0},
  {0xB5, (uint8_t[]){0x04}, 1, 0},
  {0xB6, (uint8_t[]){0x00}, 1, 0},
  {0xB7, (uint8_t[]){0x00}, 1, 0},
  {0xB8, (uint8_t[]){0x48}, 1, 0},
  {0xB9, (uint8_t[]){0x00}, 1, 0},
  {0xBA, (uint8_t[]){0x0B}, 1, 0},
  {0xBB, (uint8_t[]){0x02}, 1, 0},
  {0xBC, (uint8_t[]){0xDB}, 1, 0},
  {0xBD, (uint8_t[]){0x04}, 1, 0},
  {0xBE, (uint8_t[]){0x00}, 1, 0},
  {0xBF, (uint8_t[]){0x00}, 1, 0},
  {0xC0, (uint8_t[]){0x10}, 1, 0},
  {0xC1, (uint8_t[]){0x47}, 1, 0},
  {0xC2, (uint8_t[]){0x56}, 1, 0},
  {0xC3, (uint8_t[]){0x65}, 1, 0},
  {0xC4, (uint8_t[]){0x74}, 1, 0},
  {0xC5, (uint8_t[]){0x88}, 1, 0},
  {0xC6, (uint8_t[]){0x99}, 1, 0},
  {0xC7, (uint8_t[]){0x01}, 1, 0},
  {0xC8, (uint8_t[]){0xBB}, 1, 0},
  {0xC9, (uint8_t[]){0xAA}, 1, 0},
  {0xD0, (uint8_t[]){0x10}, 1, 0},
  {0xD1, (uint8_t[]){0x47}, 1, 0},
  {0xD2, (uint8_t[]){0x56}, 1, 0},
  {0xD3, (uint8_t[]){0x65}, 1, 0},
  {0xD4, (uint8_t[]){0x74}, 1, 0},
  {0xD5, (uint8_t[]){0x88}, 1, 0},
  {0xD6, (uint8_t[]){0x99}, 1, 0},
  {0xD7, (uint8_t[]){0x01}, 1, 0},
  {0xD8, (uint8_t[]){0xBB}, 1, 0},
  {0xD9, (uint8_t[]){0xAA}, 1, 0},
  {0xF3, (uint8_t[]){0x01}, 1, 0},
  {0xF0, (uint8_t[]){0x00}, 1, 0},
  {0x21, (uint8_t[]){0x00}, 1, 0},
  {0x11, (uint8_t[]){0x00}, 1, 120},
  {0x29, (uint8_t[]){0x00}, 1, 0},
  {0x36, (uint8_t[]){0xC0}, 1, 0},  // 180 degree rotation
};

// Timer state
typedef enum {
    TIMER_STATE_READY,
    TIMER_STATE_RUNNING,
    TIMER_STATE_PAUSED,
    TIMER_STATE_DONE
} timer_state_t;

// Timer configuration
#define DEFAULT_MINUTES     25
#define MIN_MINUTES         1
#define MAX_MINUTES         60

// Base colors (constant)
#define COLOR_BG            lv_color_hex(0x1a1a2e)
#define COLOR_ARC_BG        lv_color_hex(0x16213e)
#define COLOR_TEXT          lv_color_hex(0xeaeaea)
#define COLOR_TEXT_DIM      lv_color_hex(0x888888)

// Theme colors (accent based)
typedef struct {
    uint32_t accent;      // Main accent color
    uint32_t accent_dim;  // Dimmed accent for paused state
    const char* name;
} theme_t;

static const theme_t themes[] = {
    {0x4ecca3, 0x3a9a7a, "Teal"},     // Default teal/mint
    {0x3498db, 0x2980b9, "Blue"},     // Blue
    {0xe74c3c, 0xc0392b, "Red"},      // Red
    {0x9b59b6, 0x8e44ad, "Purple"},   // Purple
    {0xe67e22, 0xd35400, "Orange"},   // Orange
    {0x1abc9c, 0x16a085, "Cyan"},     // Cyan
};
#define NUM_THEMES (sizeof(themes) / sizeof(themes[0]))

static int current_theme = 0;

// Dynamic color getters
static lv_color_t get_accent_color(void) {
    return lv_color_hex(themes[current_theme].accent);
}

static lv_color_t get_accent_dim_color(void) {
    return lv_color_hex(themes[current_theme].accent_dim);
}

// UI elements - Timer
static lv_obj_t *timer_screen = NULL;
static lv_obj_t *arc = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *hint_label = NULL;
static lv_obj_t *btn_continue = NULL;
static lv_obj_t *btn_reset = NULL;
static lv_timer_t *countdown_timer = NULL;

// UI elements - Menu
static lv_obj_t *menu_overlay = NULL;
static lv_obj_t *menu_container = NULL;
static bool menu_open = false;

// UI elements - Settings
static lv_obj_t *settings_overlay = NULL;
static lv_obj_t *settings_container = NULL;
static bool settings_open = false;

// UI elements - Home Screen
static lv_obj_t *home_screen = NULL;
static lv_obj_t *home_bg_canvas = NULL;
static lv_obj_t *home_time_label = NULL;
static lv_obj_t *home_time_shadow = NULL;
static lv_obj_t *home_date_label = NULL;
static lv_obj_t *home_date_shadow = NULL;
static lv_obj_t *home_day_label = NULL;
static lv_obj_t *home_day_shadow = NULL;
static lv_timer_t *clock_timer = NULL;
static lv_color_t *home_canvas_buf = NULL;

// UI elements - Time Log Screen
static lv_obj_t *timelog_screen = NULL;
static lv_obj_t *timelog_title_label = NULL;
static lv_obj_t *timelog_total_label = NULL;
static lv_obj_t *timelog_pomos_label = NULL;
static lv_obj_t *timelog_streak_label = NULL;
static lv_obj_t *timelog_sessions_container = NULL;
static lv_obj_t *timelog_back_btn = NULL;

// WiFi screen UI elements
static lv_obj_t *wifi_screen = NULL;
static lv_obj_t *wifi_title_label = NULL;
static lv_obj_t *wifi_status_label = NULL;
static lv_obj_t *wifi_ssid_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *wifi_ap_btn = NULL;
static lv_obj_t *wifi_connect_btn = NULL;
static lv_obj_t *wifi_back_btn = NULL;

// Jira main screen UI elements
static lv_obj_t *jira_screen = NULL;
static lv_obj_t *jira_title_label = NULL;
static lv_obj_t *jira_dash_count_label = NULL;  // Dashboard: large issue count
static lv_obj_t *jira_dash_hint_label = NULL;   // Dashboard: "Turn knob →"
static lv_obj_t *jira_selected_label = NULL;     // Issue detail: key
static lv_obj_t *jira_summary_label = NULL;      // Issue detail: summary title
static lv_obj_t *jira_desc_label = NULL;         // Issue detail: description (3 lines)
static lv_obj_t *jira_hint_label = NULL;         // Issue detail: "← Turn knob →"
static lv_obj_t *jira_start_btn = NULL;
static lv_obj_t *jira_log_btn = NULL;
static lv_obj_t *jira_back_btn = NULL;
static lv_obj_t *jira_loading_arc = NULL;
static lv_obj_t *jira_loading_label = NULL;
static lv_timer_t *jira_loading_timer = NULL;

// Jira issue detail overlay (scrollable full-info view)
static lv_obj_t *jira_detail_overlay = NULL;
static lv_obj_t *jira_detail_content = NULL;   // Scrollable container
static lv_obj_t *jira_detail_key_label = NULL;
static lv_obj_t *jira_detail_summary_label = NULL;
static lv_obj_t *jira_detail_proj_label = NULL;
static lv_obj_t *jira_detail_status_label = NULL;
static lv_obj_t *jira_detail_desc_label = NULL;
static lv_obj_t *jira_detail_open_btn = NULL;
static lv_obj_t *jira_detail_close_btn = NULL;
static bool jira_detail_open = false;

// Jira issue picker overlay
static lv_obj_t *jira_picker_overlay = NULL;
static lv_obj_t *jira_picker_key_label = NULL;
static lv_obj_t *jira_picker_proj_label = NULL;
static lv_obj_t *jira_picker_name_label = NULL;
static lv_obj_t *jira_picker_status_label = NULL;
static lv_obj_t *jira_picker_pos_label = NULL;
static lv_obj_t *jira_picker_hint_label = NULL;
static bool jira_picker_open = false;

// Jira timer screen UI elements
static lv_obj_t *jira_timer_screen = NULL;
static lv_obj_t *jira_timer_arc = NULL;
static lv_obj_t *jira_timer_time_label = NULL;
static lv_obj_t *jira_timer_status_label = NULL;
static lv_obj_t *jira_timer_hint_label = NULL;
static lv_obj_t *jira_timer_project_label = NULL;
static lv_obj_t *jira_timer_btn_continue = NULL;
static lv_obj_t *jira_timer_btn_reset = NULL;
static lv_timer_t *jira_countdown_timer = NULL;

// Jira timer state (independent from main pomodoro)
static timer_state_t jira_timer_state = TIMER_STATE_READY;
static int jira_set_minutes = DEFAULT_MINUTES;
static int jira_remaining_seconds = DEFAULT_MINUTES * 60;
static uint32_t jira_paused_at_ms = 0;

// Jira done screen
static lv_obj_t *jira_done_screen = NULL;
static lv_obj_t *jira_done_title_label = NULL;
static lv_obj_t *jira_done_status_label = NULL;
static lv_obj_t *jira_done_back_btn = NULL;
static lv_timer_t *jira_done_timeout_timer = NULL;

// Startup splash screen
static lv_obj_t *splash_screen = NULL;
static lv_obj_t *splash_arc = NULL;
static lv_obj_t *splash_label = NULL;
static lv_timer_t *splash_timer = NULL;
static int splash_progress = 0;

// Weather screen UI elements
static lv_obj_t *weather_screen = NULL;
static lv_obj_t *weather_title_label = NULL;
static lv_obj_t *weather_temp_label = NULL;
static lv_obj_t *weather_icon_label = NULL;
static lv_obj_t *weather_condition_label = NULL;
static lv_obj_t *weather_hilo_label = NULL;
static lv_obj_t *weather_humidity_label = NULL;
static lv_obj_t *weather_wind_label = NULL;
static lv_obj_t *weather_forecast_labels[4] = {NULL};
static lv_obj_t *weather_loading_label = NULL;
static lv_obj_t *weather_back_btn = NULL;

// Home screen weather elements
static lv_obj_t *home_weather_icon = NULL;
static lv_obj_t *home_weather_temp = NULL;

// Calendar screen elements
static lv_obj_t *calendar_screen = NULL;
static lv_obj_t *calendar_title_label = NULL;
static lv_obj_t *calendar_loading_label = NULL;
static lv_obj_t *calendar_event_labels[7];
static lv_obj_t *calendar_back_btn = NULL;

// Home screen calendar element
static lv_obj_t *home_calendar_label = NULL;

// Home screen Jira hours element
static lv_obj_t *home_jira_hours_label = NULL;

// Calendar screen LOG button
static lv_obj_t *calendar_log_btn = NULL;

// Screen state
typedef enum {
    SCREEN_HOME,
    SCREEN_TIMER,
    SCREEN_TIMELOG,
    SCREEN_WIFI,
    SCREEN_JIRA,
    SCREEN_JIRA_TIMER,
    SCREEN_WEATHER,
    SCREEN_CALENDAR
} screen_state_t;
static screen_state_t current_screen = SCREEN_HOME;

// Swipe detection
static int16_t touch_start_y = -1;
static bool swipe_active = false;
#define SWIPE_THRESHOLD 50
#define MENU_TRIGGER_ZONE 60

// Timer state variables
static timer_state_t timer_state = TIMER_STATE_READY;
static int set_minutes = DEFAULT_MINUTES;
static int remaining_seconds = DEFAULT_MINUTES * 60;
static uint32_t paused_at_ms = 0;  // Debounce: track when we entered PAUSED
#define BUTTON_DEBOUNCE_MS 300

static SemaphoreHandle_t timer_mux = NULL;

// Forward declarations for timer
static void update_timer_display(void);
static void countdown_timer_cb(lv_timer_t *timer);
static void btn_continue_cb(lv_event_t *e);
static void btn_reset_cb(lv_event_t *e);

// Forward declarations for menu
static void create_menu_ui(void);
static void show_menu(void);
static void hide_menu(void);
static void menu_app_cb(lv_event_t *e);
static void menu_overlay_cb(lv_event_t *e);

// Forward declarations for settings
static void create_settings_ui(void);
static void show_settings(void);
static void hide_settings(void);
static void settings_theme_cb(lv_event_t *e);
static void settings_overlay_cb(lv_event_t *e);
static void apply_theme(void);

// Forward declarations for startup splash
static void create_splash_ui(void);
static void splash_anim_cb(lv_timer_t *timer);

// Forward declarations for home screen
static void create_home_ui(void);
static void update_clock(lv_timer_t *timer);
static void show_home_screen(void);
static void show_timer_screen(void);

// Forward declarations for time log screen
static void create_timelog_ui(void);
static void show_timelog_screen(void);
static void hide_timelog_screen(void);
static void update_timelog_display(void);
static void timelog_back_cb(lv_event_t *e);

// Forward declarations for WiFi screen
static void create_wifi_ui(void);
static void show_wifi_screen(void);
static void hide_wifi_screen(void);
static void update_wifi_display(void);
static void wifi_back_cb(lv_event_t *e);
static void wifi_ap_btn_cb(lv_event_t *e);
static void wifi_connect_btn_cb(lv_event_t *e);

// Forward declarations for Jira main screen
static void create_jira_ui(void);
static void show_jira_screen(void);
static void hide_jira_screen(void);
static void update_jira_display(void);
static void jira_back_cb(lv_event_t *e);
static void jira_start_btn_cb(lv_event_t *e);
static void jira_log_btn_cb(lv_event_t *e);
static void jira_open_issue_cb(lv_event_t *e);
static void jira_loading_anim_cb(lv_timer_t *timer);

// Forward declarations for Jira issue detail overlay
static void create_jira_detail_ui(void);
static void show_jira_detail(void);
static void hide_jira_detail(void);
static void update_jira_detail_display(void);
static void jira_detail_close_cb(lv_event_t *e);
static void jira_detail_open_browser_cb(lv_event_t *e);

// Forward declarations for Jira project picker
static void create_jira_picker_ui(void);
static void show_jira_picker(void);
static void hide_jira_picker(void);
static void update_jira_picker_display(void);
static void jira_picker_overlay_cb(lv_event_t *e);

// Forward declarations for Jira timer
static void create_jira_timer_ui(void);
static void show_jira_timer_screen(void);
static void hide_jira_timer_screen(void);
static void update_jira_timer_display(void);
static void jira_timer_countdown_cb(lv_timer_t *timer);
static void jira_timer_btn_continue_cb(lv_event_t *e);
static void jira_timer_btn_reset_cb(lv_event_t *e);

// Forward declarations for Jira done screen
static void create_jira_done_ui(void);
static void show_jira_done_screen(void);
static void jira_done_back_cb(lv_event_t *e);
static void jira_done_timeout_cb(lv_timer_t *timer);
static void jira_done_auto_return_cb(lv_timer_t *timer);

// Forward declarations for Weather screen
static void create_weather_ui(void);
static void show_weather_screen(void);
static void hide_weather_screen(void);
static void update_weather_display(void);
static void weather_back_cb(lv_event_t *e);
static const char* weather_icon_for_condition(uint16_t condition_id);

// Forward declarations for Calendar screen
static void create_calendar_ui(void);
static void show_calendar_screen(void);
static void update_calendar_display(void);
static void calendar_back_cb(lv_event_t *e);
static void calendar_log_cb(lv_event_t *e);

// Public functions for knob control (called from main sketch)
void timer_knob_left(void) {
    if (timer_state == TIMER_STATE_READY) {
        if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
            set_minutes--;
            if (set_minutes < MIN_MINUTES) set_minutes = MIN_MINUTES;
            remaining_seconds = set_minutes * 60;
            update_timer_display();
            xSemaphoreGive(lvgl_mux);
        }
    }
}

void timer_knob_right(void) {
    if (timer_state == TIMER_STATE_READY) {
        if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
            set_minutes++;
            if (set_minutes > MAX_MINUTES) set_minutes = MAX_MINUTES;
            remaining_seconds = set_minutes * 60;
            update_timer_display();
            xSemaphoreGive(lvgl_mux);
        }
    }
}

void timer_knob_press(void) {
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        switch (timer_state) {
            case TIMER_STATE_READY:
                haptic_click();
                timer_state = TIMER_STATE_RUNNING;
                if (countdown_timer == NULL) {
                    countdown_timer = lv_timer_create(countdown_timer_cb, 1000, NULL);
                } else {
                    lv_timer_resume(countdown_timer);
                }
                break;
            case TIMER_STATE_RUNNING:
                haptic_click();
                timer_state = TIMER_STATE_PAUSED;
                paused_at_ms = lv_tick_get();  // Record time for button debounce
                if (countdown_timer) lv_timer_pause(countdown_timer);
                break;
            case TIMER_STATE_PAUSED:
                // When paused, no haptic - use Continue/Reset buttons instead
                break;
            case TIMER_STATE_DONE:
                haptic_click();
                timer_state = TIMER_STATE_READY;
                remaining_seconds = set_minutes * 60;
                if (countdown_timer) lv_timer_pause(countdown_timer);
                break;
        }
        update_timer_display();
        xSemaphoreGive(lvgl_mux);
    }
}

// Button callbacks
static void btn_continue_cb(lv_event_t *e) {
    if (timer_state == TIMER_STATE_PAUSED) {
        // Ignore if buttons just appeared (prevents accidental tap)
        if (lv_tick_elaps(paused_at_ms) < BUTTON_DEBOUNCE_MS) return;
        haptic_click();
        timer_state = TIMER_STATE_RUNNING;
        if (countdown_timer) lv_timer_resume(countdown_timer);
        update_timer_display();
    }
}

static void btn_reset_cb(lv_event_t *e) {
    if (timer_state == TIMER_STATE_PAUSED) {
        // Ignore if buttons just appeared (prevents accidental tap)
        if (lv_tick_elaps(paused_at_ms) < BUTTON_DEBOUNCE_MS) return;
        haptic_click();
        timer_state = TIMER_STATE_READY;
        remaining_seconds = set_minutes * 60;
        if (countdown_timer) lv_timer_pause(countdown_timer);
        update_timer_display();
    }
}

static void countdown_timer_cb(lv_timer_t *timer) {
    if (timer_state != TIMER_STATE_RUNNING) return;

    if (remaining_seconds > 0) {
        remaining_seconds--;
        update_timer_display();
    }

    if (remaining_seconds == 0) {
        timer_state = TIMER_STATE_DONE;
        // Log the completed work session
        time_log_add_session(SESSION_WORK, set_minutes);
        update_timer_display();
    }
}

static void update_timer_display(void) {
    // Update time display
    int mins = remaining_seconds / 60;
    int secs = remaining_seconds % 60;
    static char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", mins, secs);
    lv_label_set_text(time_label, time_buf);

    // Update arc progress
    int progress;
    if (timer_state == TIMER_STATE_READY) {
        // In READY state, show set time as percentage of max (60 min)
        progress = (set_minutes * 100) / MAX_MINUTES;
    } else {
        // During countdown, show remaining time as percentage of set time
        int total_seconds = set_minutes * 60;
        progress = (remaining_seconds * 100) / total_seconds;
    }
    lv_arc_set_value(arc, progress);

    // Update status and colors (using theme accent color)
    switch (timer_state) {
        case TIMER_STATE_READY:
            lv_label_set_text(status_label, "READY");
            lv_label_set_text(hint_label, "Knob: adjust | Tap: start");
            lv_obj_set_style_arc_color(arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_RUNNING:
            lv_label_set_text(status_label, "FOCUS");
            lv_label_set_text(hint_label, "Tap: pause");
            lv_obj_set_style_arc_color(arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_PAUSED:
            lv_label_set_text(status_label, "PAUSED");
            lv_obj_set_style_arc_color(arc, get_accent_dim_color(), LV_PART_INDICATOR);
            lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_DONE:
            lv_label_set_text(status_label, "DONE!");
            lv_label_set_text(hint_label, "Tap: reset");
            lv_obj_set_style_arc_color(arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

// ── Startup Splash Screen ──────────────────────────────────────────
static void splash_anim_cb(lv_timer_t *timer)
{
    splash_progress += 3;  // ~2 seconds to fill (360 degrees / 3 per tick @ 15ms)
    if (splash_progress >= 360) {
        splash_progress = 360;
    }
    lv_arc_set_angles(splash_arc, 0, splash_progress);

    // Update label opacity fade-in during first half
    if (splash_progress < 180) {
        lv_opa_t opa = (lv_opa_t)((splash_progress * 255) / 180);
        lv_obj_set_style_text_opa(splash_label, opa, 0);
    }

    if (splash_progress >= 360) {
        // Hold for a moment then transition
        lv_timer_del(splash_timer);
        splash_timer = NULL;

        // Hide splash, show home
        lv_obj_add_flag(splash_screen, LV_OBJ_FLAG_HIDDEN);
        show_home_screen();
    }
}

static void create_splash_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    splash_screen = lv_obj_create(screen);
    lv_obj_set_size(splash_screen, 360, 360);
    lv_obj_center(splash_screen);
    lv_obj_set_style_bg_color(splash_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(splash_screen, 0, 0);
    lv_obj_set_style_radius(splash_screen, 180, 0);
    lv_obj_set_style_pad_all(splash_screen, 0, 0);
    lv_obj_clear_flag(splash_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Progress arc — ring that fills up
    splash_arc = lv_arc_create(splash_screen);
    lv_obj_set_size(splash_arc, 280, 280);
    lv_obj_center(splash_arc);
    lv_arc_set_rotation(splash_arc, 270);        // Start from top
    lv_arc_set_bg_angles(splash_arc, 0, 360);    // Full background ring
    lv_arc_set_angles(splash_arc, 0, 0);          // Start empty
    lv_arc_set_range(splash_arc, 0, 360);
    lv_obj_remove_style(splash_arc, NULL, LV_PART_KNOB);     // No knob
    lv_obj_clear_flag(splash_arc, LV_OBJ_FLAG_CLICKABLE);    // Not interactive

    // Arc styling
    lv_obj_set_style_arc_width(splash_arc, 6, LV_PART_MAIN);        // Thin bg ring
    lv_obj_set_style_arc_color(splash_arc, COLOR_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(splash_arc, 6, LV_PART_INDICATOR);   // Thin fill
    lv_obj_set_style_arc_color(splash_arc, lv_color_hex(0x4ecca3), LV_PART_INDICATOR);  // Teal accent
    lv_obj_set_style_arc_rounded(splash_arc, true, LV_PART_INDICATOR);

    // App name label
    splash_label = lv_label_create(splash_screen);
    lv_label_set_text(splash_label, "FocusKnob");
    lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(splash_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_opa(splash_label, LV_OPA_TRANSP, 0);  // Start invisible
    lv_obj_center(splash_label);

    // Start animation timer (15ms interval for smooth ~2s fill)
    splash_progress = 0;
    splash_timer = lv_timer_create(splash_anim_cb, 15, NULL);
}

// Create timer UI
static void create_timer_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Set background
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Create arc
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, 320, 320);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_value(arc, 100);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc, COLOR_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, get_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    // Time label
    time_label = lv_label_create(screen);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, COLOR_TEXT, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -15);

    // Status label
    status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 35);

    // Hint label
    hint_label = lv_label_create(screen);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    // Continue button (hidden by default) - icon only, no background
    btn_continue = lv_btn_create(screen);
    lv_obj_set_size(btn_continue, 50, 50);
    lv_obj_align(btn_continue, LV_ALIGN_CENTER, -45, 80);
    lv_obj_set_style_bg_opa(btn_continue, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_continue, 0, 0);
    lv_obj_set_style_border_width(btn_continue, 0, 0);
    lv_obj_add_event_cb(btn_continue, btn_continue_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_continue = lv_label_create(btn_continue);
    lv_label_set_text(lbl_continue, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(lbl_continue, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_continue, COLOR_TEXT, 0);
    lv_obj_center(lbl_continue);

    // Reset button (hidden by default) - icon only, no background
    btn_reset = lv_btn_create(screen);
    lv_obj_set_size(btn_reset, 50, 50);
    lv_obj_align(btn_reset, LV_ALIGN_CENTER, 45, 80);
    lv_obj_set_style_bg_opa(btn_reset, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(btn_reset, 0, 0);
    lv_obj_set_style_border_width(btn_reset, 0, 0);
    lv_obj_add_event_cb(btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_reset, COLOR_TEXT, 0);
    lv_obj_center(lbl_reset);

    update_timer_display();
}

// Menu UI creation
// App definitions for menu
typedef struct {
    const char* icon;
    bool active;  // true = implemented, false = placeholder
} app_def_t;

// 8 outer ring apps (arranged in circle at 45° intervals)
static const app_def_t apps[] = {
    {LV_SYMBOL_BELL, true},       // 0: Pomodoro Timer (top)
    {LV_SYMBOL_LIST, true},       // 1: Time Log
    {LV_SYMBOL_WIFI, true},       // 2: WiFi
    {LV_SYMBOL_EDIT, true},       // 3: Jira TimeLog
    {LV_SYMBOL_EYE_OPEN, true},   // 4: Weather
    {LV_SYMBOL_ENVELOPE, true},    // 5: Calendar
    {LV_SYMBOL_CHARGE, false},    // 6: Battery
    {LV_SYMBOL_HOME, false},      // 7: Home
};
#define NUM_APPS 8

static void create_menu_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Semi-transparent overlay (covers whole screen)
    menu_overlay = lv_obj_create(screen);
    lv_obj_set_size(menu_overlay, 360, 360);
    lv_obj_center(menu_overlay);
    lv_obj_set_style_bg_color(menu_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(menu_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(menu_overlay, 0, 0);
    lv_obj_set_style_radius(menu_overlay, 180, 0);
    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(menu_overlay, menu_overlay_cb, LV_EVENT_CLICKED, NULL);

    // Menu container
    menu_container = lv_obj_create(menu_overlay);
    lv_obj_set_size(menu_container, 340, 340);
    lv_obj_center(menu_container);
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_clear_flag(menu_container, LV_OBJ_FLAG_SCROLLABLE);

    // Center settings button
    lv_obj_t *center_btn = lv_btn_create(menu_container);
    lv_obj_set_size(center_btn, 60, 60);
    lv_obj_center(center_btn);
    lv_obj_set_style_bg_color(center_btn, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(center_btn, LV_OPA_20, 0);
    lv_obj_set_style_radius(center_btn, 30, 0);
    lv_obj_set_style_border_width(center_btn, 0, 0);
    lv_obj_set_style_shadow_width(center_btn, 0, 0);
    lv_obj_add_event_cb(center_btn, menu_app_cb, LV_EVENT_CLICKED, (void*)(intptr_t)99);  // 99 = settings

    lv_obj_t *center_lbl = lv_label_create(center_btn);
    lv_label_set_text(center_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(center_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_lbl, COLOR_TEXT, 0);
    lv_obj_center(center_lbl);

    // 8 outer ring buttons at 45° intervals
    int btn_size = 50;
    int radius = 110;

    for (int i = 0; i < NUM_APPS; i++) {
        lv_obj_t *btn = lv_btn_create(menu_container);
        lv_obj_set_size(btn, btn_size, btn_size);

        // Position at 45° intervals, starting from top (- PI/2)
        float angle = (3.14159f * 2.0f * i / 8.0f) - 3.14159f / 2.0f;
        int x = (int)(radius * cosf(angle));
        int y = (int)(radius * sinf(angle));

        lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_20, 0);
        lv_obj_set_style_radius(btn, btn_size / 2, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, menu_app_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, apps[i].icon);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl, apps[i].active ? COLOR_TEXT : COLOR_TEXT_DIM, 0);
        lv_obj_center(lbl);
    }
}

static void show_menu(void)
{
    if (!menu_open && menu_overlay != NULL) {
        lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
        menu_open = true;
    }
}

static void hide_menu(void)
{
    if (menu_open && menu_overlay != NULL) {
        lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
        menu_open = false;
    }
}

static void menu_app_cb(lv_event_t *e)
{
    int app_id = (int)(intptr_t)lv_event_get_user_data(e);

    switch (app_id) {
        case 99: // Settings (center button)
            hide_menu();
            show_settings();
            break;
        case 0: // Pomodoro Timer
            hide_menu();
            show_timer_screen();
            break;
        case 1: // Time Log (Notes icon)
            hide_menu();
            show_timelog_screen();
            break;
        case 2: // WiFi
            hide_menu();
            show_wifi_screen();
            break;
        case 3: // Jira TimeLog
            hide_menu();
            show_jira_screen();
            break;
        case 4: // Weather
            hide_menu();
            show_weather_screen();
            break;
        case 5: // Calendar
            hide_menu();
            show_calendar_screen();
            break;
        case 7: // Home
            hide_menu();
            show_home_screen();
            break;
        default: // Other apps - placeholders
            hide_menu();
            break;
    }
}

static void menu_overlay_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    // Only close if clicked on overlay itself, not on menu container or buttons
    if (target == menu_overlay) {
        hide_menu();
    }
}

// Settings UI creation
static void create_settings_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Semi-transparent overlay
    settings_overlay = lv_obj_create(screen);
    lv_obj_set_size(settings_overlay, 360, 360);
    lv_obj_center(settings_overlay);
    lv_obj_set_style_bg_color(settings_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(settings_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(settings_overlay, 0, 0);
    lv_obj_set_style_radius(settings_overlay, 180, 0);
    lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(settings_overlay, settings_overlay_cb, LV_EVENT_CLICKED, NULL);

    // Settings container
    settings_container = lv_obj_create(settings_overlay);
    lv_obj_set_size(settings_container, 280, 280);
    lv_obj_center(settings_container);
    lv_obj_set_style_bg_color(settings_container, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(settings_container, LV_OPA_90, 0);
    lv_obj_set_style_border_color(settings_container, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(settings_container, 1, 0);
    lv_obj_set_style_border_opa(settings_container, LV_OPA_20, 0);
    lv_obj_set_style_radius(settings_container, 140, 0);
    lv_obj_set_style_pad_all(settings_container, 0, 0);
    lv_obj_clear_flag(settings_container, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(settings_container);
    lv_label_set_text(title, "Theme");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // Color buttons arranged in 2 rows of 3
    int btn_size = 50;
    int spacing = 60;
    int start_x = -spacing;
    int row1_y = -25;
    int row2_y = 45;

    for (int i = 0; i < NUM_THEMES; i++) {
        lv_obj_t *btn = lv_btn_create(settings_container);
        lv_obj_set_size(btn, btn_size, btn_size);

        int row = i / 3;
        int col = i % 3;
        int x = start_x + (col * spacing);
        int y = (row == 0) ? row1_y : row2_y;

        lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(themes[i].accent), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, btn_size / 2, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_width(btn, (i == current_theme) ? 3 : 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, settings_theme_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

static void show_settings(void)
{
    if (!settings_open && settings_overlay != NULL) {
        lv_obj_clear_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
        settings_open = true;
    }
}

static void hide_settings(void)
{
    if (settings_open && settings_overlay != NULL) {
        lv_obj_add_flag(settings_overlay, LV_OBJ_FLAG_HIDDEN);
        settings_open = false;
    }
}

static void apply_theme(void)
{
    // Update timer arc colors
    if (arc != NULL) {
        // Re-run display update to apply new colors
        lv_obj_set_style_arc_color(arc, get_accent_color(), LV_PART_INDICATOR);
    }
}

static void settings_theme_cb(lv_event_t *e)
{
    int theme_id = (int)(intptr_t)lv_event_get_user_data(e);

    if (theme_id >= 0 && theme_id < NUM_THEMES) {
        current_theme = theme_id;
        apply_theme();
        // Recreate settings UI to update border highlights
        lv_obj_del(settings_overlay);
        settings_open = false;  // Reset flag after deleting
        create_settings_ui();
        show_settings();
    }
}

static void settings_overlay_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (target == settings_overlay) {
        hide_settings();
    }
}

// Home Screen UI
static void create_home_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Home screen container
    home_screen = lv_obj_create(screen);
    lv_obj_set_size(home_screen, 360, 360);
    lv_obj_center(home_screen);
    lv_obj_set_style_bg_color(home_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(home_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_screen, 0, 0);
    lv_obj_set_style_radius(home_screen, 180, 0);
    lv_obj_set_style_pad_all(home_screen, 0, 0);
    lv_obj_clear_flag(home_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Day of week label ──
    home_day_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_day_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_day_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(home_day_label, 4, 0);
    lv_obj_align(home_day_label, LV_ALIGN_CENTER, 0, -62);
    lv_label_set_text(home_day_label, "SUNDAY");

    // ── Time label (large) ──
    home_time_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(home_time_label, COLOR_TEXT, 0);
    lv_obj_align(home_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(home_time_label, "12:00");

    // ── Date label ──
    home_date_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(home_date_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(home_date_label, LV_ALIGN_CENTER, 0, 35);
    lv_label_set_text(home_date_label, "Jan 1, 2025");

    // ── Weather temp + condition (below date) ──
    home_weather_icon = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_weather_icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(home_weather_icon, COLOR_TEXT_DIM, 0);
    lv_obj_align(home_weather_icon, LV_ALIGN_CENTER, 0, 65);
    lv_obj_set_style_text_align(home_weather_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_weather_icon, "");

    // (hidden — combined into icon label above)
    home_weather_temp = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_weather_temp, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(home_weather_temp, COLOR_TEXT, 0);
    lv_obj_align(home_weather_temp, LV_ALIGN_CENTER, 15, 65);
    lv_obj_add_flag(home_weather_temp, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(home_weather_temp, "");

    // ── Next meeting (below weather) ──
    home_calendar_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_calendar_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_calendar_label, lv_color_hex(0x3498db), 0);
    lv_obj_align(home_calendar_label, LV_ALIGN_CENTER, 0, 95);
    lv_obj_set_width(home_calendar_label, 240);
    lv_obj_set_style_text_align(home_calendar_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(home_calendar_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(home_calendar_label, "");

    // ── Jira daily hours (below calendar) ──
    home_jira_hours_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_jira_hours_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_jira_hours_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(home_jira_hours_label, LV_ALIGN_CENTER, 0, 120);
    lv_obj_set_width(home_jira_hours_label, 200);
    lv_obj_set_style_text_align(home_jira_hours_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_jira_hours_label, "");

    // Create clock update timer (updates every second)
    clock_timer = lv_timer_create(update_clock, 1000, NULL);
    update_clock(NULL);  // Initial update
}

static void update_clock(lv_timer_t *timer)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Update time (12-hour format with AM/PM)
    static char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &timeinfo);
    lv_label_set_text(home_time_label, time_buf);

    // Update date (Mon DD, YYYY format)
    static char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%b %d, %Y", &timeinfo);
    lv_label_set_text(home_date_label, date_buf);

    // Update day of week (uppercase)
    static char day_buf[16];
    strftime(day_buf, sizeof(day_buf), "%A", &timeinfo);
    // Convert to uppercase
    for (int i = 0; day_buf[i]; i++) {
        if (day_buf[i] >= 'a' && day_buf[i] <= 'z') {
            day_buf[i] -= 32;
        }
    }
    if (home_day_label) lv_label_set_text(home_day_label, day_buf);

    // Update home screen weather display (combined temp + condition)
    if (weather_data_is_synced() && home_weather_icon) {
        const weather_current_t* w = weather_data_get_current();
        if (w) {
            static char wbuf[32];
            snprintf(wbuf, sizeof(wbuf), "%d\xC2\xB0  %s", w->temp, w->condition);
            lv_label_set_text(home_weather_icon, wbuf);
        }
    }

    // Update home screen calendar display
    if (calendar_data_is_synced() && home_calendar_label) {
        int16_t mins = calendar_data_get_next_meeting_min();
        const calendar_event_t* next = calendar_data_get_event(0);
        if (mins == -1 && next) {
            // Meeting in progress
            static char cal_buf[48];
            snprintf(cal_buf, sizeof(cal_buf), LV_SYMBOL_BELL " %s (now)", next->title);
            lv_label_set_text(home_calendar_label, cal_buf);
        } else if (mins >= 0 && mins <= 60 && next) {
            static char cal_buf[48];
            snprintf(cal_buf, sizeof(cal_buf), LV_SYMBOL_BELL " %s in %dm", next->title, mins);
            lv_label_set_text(home_calendar_label, cal_buf);
        } else if (next) {
            static char cal_buf[48];
            snprintf(cal_buf, sizeof(cal_buf), LV_SYMBOL_BELL " %s %s", next->title, next->start_str);
            lv_label_set_text(home_calendar_label, cal_buf);
        } else {
            lv_label_set_text(home_calendar_label, "");
        }
    }

    // Update Jira daily hours display
    if (jira_hours_data_is_synced() && home_jira_hours_label) {
        const jira_hours_state_t* h = jira_hours_data_get();
        if (h->target_min > 0) {
            // Weekday — show hours logged vs target
            float logged = h->logged_min / 60.0f;
            float target = h->target_min / 60.0f;
            static char jhbuf[24];
            snprintf(jhbuf, sizeof(jhbuf), "%.1f / %.1fh", logged, target);
            lv_label_set_text(home_jira_hours_label, jhbuf);

            // Color coding: green >= target, amber >= 75%, dim otherwise
            if (h->logged_min >= h->target_min) {
                lv_obj_set_style_text_color(home_jira_hours_label, lv_color_hex(0x2ecc71), 0);
            } else if (h->logged_min >= (h->target_min * 3 / 4)) {
                lv_obj_set_style_text_color(home_jira_hours_label, lv_color_hex(0xf39c12), 0);
            } else {
                lv_obj_set_style_text_color(home_jira_hours_label, COLOR_TEXT_DIM, 0);
            }
        } else {
            // Weekend — hide hours
            lv_label_set_text(home_jira_hours_label, "");
        }
    }
}

static void show_home_screen(void)
{
    if (home_screen) lv_obj_clear_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    // Hide timer elements
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    // Hide time log screen
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    // Hide Jira screens
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    // Hide weather screen
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    // Hide calendar screen
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_HOME;
}

static void show_timer_screen(void)
{
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
    // Show timer elements
    if (arc) lv_obj_clear_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    // Buttons visibility handled by update_timer_display
    current_screen = SCREEN_TIMER;
    update_timer_display();
}

// Public function to check if timer screen is active
bool is_timer_screen_active(void)
{
    return current_screen == SCREEN_TIMER;
}

// Time Log UI
static void create_timelog_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Time log screen container
    timelog_screen = lv_obj_create(screen);
    lv_obj_set_size(timelog_screen, 360, 360);
    lv_obj_center(timelog_screen);
    lv_obj_set_style_bg_color(timelog_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(timelog_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(timelog_screen, 0, 0);
    lv_obj_set_style_radius(timelog_screen, 180, 0);
    lv_obj_set_style_pad_all(timelog_screen, 0, 0);
    lv_obj_clear_flag(timelog_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    timelog_title_label = lv_label_create(timelog_screen);
    lv_obj_set_style_text_font(timelog_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(timelog_title_label, COLOR_TEXT, 0);
    lv_obj_align(timelog_title_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(timelog_title_label, "Time Log");

    // Total work time today
    timelog_total_label = lv_label_create(timelog_screen);
    lv_obj_set_style_text_font(timelog_total_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(timelog_total_label, get_accent_color(), 0);
    lv_obj_align(timelog_total_label, LV_ALIGN_CENTER, 0, -40);
    lv_label_set_text(timelog_total_label, "0m");

    // Pomodoros completed
    timelog_pomos_label = lv_label_create(timelog_screen);
    lv_obj_set_style_text_font(timelog_pomos_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timelog_pomos_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(timelog_pomos_label, LV_ALIGN_CENTER, 0, 10);
    lv_label_set_text(timelog_pomos_label, "0 pomodoros today");

    // Current streak
    timelog_streak_label = lv_label_create(timelog_screen);
    lv_obj_set_style_text_font(timelog_streak_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timelog_streak_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(timelog_streak_label, LV_ALIGN_CENTER, 0, 40);
    lv_label_set_text(timelog_streak_label, "0 day streak");

    // Back button (icon only)
    timelog_back_btn = lv_btn_create(timelog_screen);
    lv_obj_set_size(timelog_back_btn, 50, 50);
    lv_obj_align(timelog_back_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_opa(timelog_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(timelog_back_btn, 0, 0);
    lv_obj_set_style_border_width(timelog_back_btn, 0, 0);
    lv_obj_add_event_cb(timelog_back_btn, timelog_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(timelog_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void update_timelog_display(void)
{
    if (!timelog_screen) return;

    // Get today's stats
    uint16_t work_mins = time_log_get_today_work_minutes();
    uint8_t pomos = time_log_get_today_pomodoros();
    uint8_t streak = time_log_get_current_streak();

    // Format and display total time
    char time_buf[16];
    time_log_format_duration(work_mins, time_buf, sizeof(time_buf));
    lv_label_set_text(timelog_total_label, time_buf);

    // Update accent color
    lv_obj_set_style_text_color(timelog_total_label, get_accent_color(), 0);

    // Display pomodoros
    static char pomo_buf[32];
    snprintf(pomo_buf, sizeof(pomo_buf), "%d pomodoro%s today", pomos, pomos == 1 ? "" : "s");
    lv_label_set_text(timelog_pomos_label, pomo_buf);

    // Display streak
    static char streak_buf[32];
    snprintf(streak_buf, sizeof(streak_buf), "%d day streak", streak);
    lv_label_set_text(timelog_streak_label, streak_buf);
}

static void show_timelog_screen(void)
{
    // Hide other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);

    // Show time log screen
    if (timelog_screen) lv_obj_clear_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);

    // Update display
    update_timelog_display();

    current_screen = SCREEN_TIMELOG;
}

static void hide_timelog_screen(void)
{
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
}

static void timelog_back_cb(lv_event_t *e)
{
    show_home_screen();
}

// WiFi Screen UI
static void create_wifi_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // WiFi screen container
    wifi_screen = lv_obj_create(screen);
    lv_obj_set_size(wifi_screen, 360, 360);
    lv_obj_center(wifi_screen);
    lv_obj_set_style_bg_color(wifi_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(wifi_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_screen, 0, 0);
    lv_obj_set_style_radius(wifi_screen, 180, 0);
    lv_obj_set_style_pad_all(wifi_screen, 0, 0);
    lv_obj_clear_flag(wifi_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    wifi_title_label = lv_label_create(wifi_screen);
    lv_obj_set_style_text_font(wifi_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(wifi_title_label, COLOR_TEXT, 0);
    lv_obj_align(wifi_title_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(wifi_title_label, LV_SYMBOL_WIFI " WiFi");

    // Status label
    wifi_status_label = lv_label_create(wifi_screen);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_CENTER, 0, -50);
    lv_label_set_text(wifi_status_label, "Disconnected");

    // SSID label
    wifi_ssid_label = lv_label_create(wifi_screen);
    lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, get_accent_color(), 0);
    lv_obj_align(wifi_ssid_label, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(wifi_ssid_label, "");

    // IP label
    wifi_ip_label = lv_label_create(wifi_screen);
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_ip_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_CENTER, 0, 10);
    lv_label_set_text(wifi_ip_label, "");

    // AP Mode button
    wifi_ap_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(wifi_ap_btn, 120, 40);
    lv_obj_align(wifi_ap_btn, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_bg_color(wifi_ap_btn, get_accent_color(), 0);
    lv_obj_set_style_radius(wifi_ap_btn, 20, 0);
    lv_obj_set_style_shadow_width(wifi_ap_btn, 0, 0);
    lv_obj_add_event_cb(wifi_ap_btn, wifi_ap_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ap_lbl = lv_label_create(wifi_ap_btn);
    lv_label_set_text(ap_lbl, "Setup");
    lv_obj_set_style_text_font(ap_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ap_lbl, COLOR_BG, 0);
    lv_obj_center(ap_lbl);

    // Back button
    wifi_back_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(wifi_back_btn, 50, 50);
    lv_obj_align(wifi_back_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_opa(wifi_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(wifi_back_btn, 0, 0);
    lv_obj_set_style_border_width(wifi_back_btn, 0, 0);
    lv_obj_add_event_cb(wifi_back_btn, wifi_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(wifi_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void update_wifi_display(void)
{
    if (!wifi_screen) return;

    wifi_state_t state = wifi_config_get_state();

    switch (state) {
        case WIFI_STATE_CONNECTED:
            lv_label_set_text(wifi_status_label, "Connected");
            lv_obj_set_style_text_color(wifi_status_label, get_accent_color(), 0);
            lv_label_set_text(wifi_ssid_label, wifi_config_get_ssid());
            lv_label_set_text(wifi_ip_label, wifi_config_get_ip());
            lv_label_set_text(lv_obj_get_child(wifi_ap_btn, 0), "Disconnect");
            break;
        case WIFI_STATE_CONNECTING:
            lv_label_set_text(wifi_status_label, "Connecting...");
            lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_DIM, 0);
            lv_label_set_text(wifi_ssid_label, wifi_config_get_ssid());
            lv_label_set_text(wifi_ip_label, "");
            break;
        case WIFI_STATE_AP_MODE:
            lv_label_set_text(wifi_status_label, "Setup Mode");
            lv_obj_set_style_text_color(wifi_status_label, get_accent_color(), 0);
            lv_label_set_text(wifi_ssid_label, wifi_config_get_ap_ssid());
            lv_label_set_text(wifi_ip_label, "Password: Focus");
            lv_label_set_text(lv_obj_get_child(wifi_ap_btn, 0), "Stop");
            break;
        default: // WIFI_STATE_DISCONNECTED
            lv_label_set_text(wifi_status_label, "Disconnected");
            lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_DIM, 0);
            lv_label_set_text(wifi_ssid_label, "");
            lv_label_set_text(wifi_ip_label, "");
            if (wifi_config_has_credentials()) {
                lv_label_set_text(lv_obj_get_child(wifi_ap_btn, 0), "Connect");
            } else {
                lv_label_set_text(lv_obj_get_child(wifi_ap_btn, 0), "Setup");
            }
            break;
    }

    // Update accent color
    lv_obj_set_style_text_color(wifi_ssid_label, get_accent_color(), 0);
    lv_obj_set_style_bg_color(wifi_ap_btn, get_accent_color(), 0);
}

static void show_wifi_screen(void)
{
    // Hide other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);

    // Show WiFi screen
    if (wifi_screen) lv_obj_clear_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);

    // Update display
    update_wifi_display();

    current_screen = SCREEN_WIFI;
}

static void hide_wifi_screen(void)
{
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_back_cb(lv_event_t *e)
{
    show_home_screen();
}

static void wifi_ap_btn_cb(lv_event_t *e)
{
    wifi_state_t state = wifi_config_get_state();

    switch (state) {
        case WIFI_STATE_CONNECTED:
            wifi_config_disconnect();
            break;
        case WIFI_STATE_AP_MODE:
            wifi_config_stop_ap();
            break;
        default:
            if (wifi_config_has_credentials()) {
                wifi_config_connect();
            } else {
                wifi_config_start_ap();
            }
            break;
    }

    update_wifi_display();
}

static void wifi_connect_btn_cb(lv_event_t *e)
{
    // Reserved for future use
}

// ============================================================
// Jira Main Screen
// ============================================================

static void create_jira_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    jira_screen = lv_obj_create(screen);
    lv_obj_set_size(jira_screen, 360, 360);
    lv_obj_center(jira_screen);
    lv_obj_set_style_bg_color(jira_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(jira_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(jira_screen, 0, 0);
    lv_obj_set_style_radius(jira_screen, 180, 0);
    lv_obj_set_style_pad_all(jira_screen, 0, 0);
    lv_obj_clear_flag(jira_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);

    // Title — Jira brand blue (shared by dashboard + issue pages)
    jira_title_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(jira_title_label, lv_color_hex(0x2684FF), 0);
    lv_obj_align(jira_title_label, LV_ALIGN_TOP_MID, 0, 42);
    lv_label_set_text(jira_title_label, LV_SYMBOL_EDIT " Jira");

    // Loading spinner arc (shown while waiting for data)
    jira_loading_arc = lv_arc_create(jira_screen);
    lv_obj_set_size(jira_loading_arc, 80, 80);
    lv_obj_align(jira_loading_arc, LV_ALIGN_CENTER, 0, -15);
    lv_arc_set_rotation(jira_loading_arc, 0);
    lv_arc_set_bg_angles(jira_loading_arc, 0, 360);
    lv_arc_set_angles(jira_loading_arc, 0, 90);
    lv_obj_remove_style(jira_loading_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(jira_loading_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(jira_loading_arc, lv_color_hex(0x2684FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(jira_loading_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(jira_loading_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_clear_flag(jira_loading_arc, LV_OBJ_FLAG_CLICKABLE);

    // Loading text
    jira_loading_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_loading_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_loading_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_loading_label, LV_ALIGN_CENTER, 0, 35);
    lv_label_set_text(jira_loading_label, "Loading issues...");

    // === DASHBOARD elements (shown when selected_index == -1) ===

    // Large issue count — centered, big font
    jira_dash_count_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_dash_count_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(jira_dash_count_label, COLOR_TEXT, 0);
    lv_obj_align(jira_dash_count_label, LV_ALIGN_CENTER, 0, -15);
    lv_label_set_text(jira_dash_count_label, "0");
    lv_obj_add_flag(jira_dash_count_label, LV_OBJ_FLAG_HIDDEN);

    // Dashboard hint — "Turn knob →"
    jira_dash_hint_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_dash_hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_dash_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_dash_hint_label, LV_ALIGN_CENTER, 0, 30);
    lv_label_set_text(jira_dash_hint_label, "Turn knob " LV_SYMBOL_RIGHT);
    lv_obj_add_flag(jira_dash_hint_label, LV_OBJ_FLAG_HIDDEN);

    // === ISSUE DETAIL elements (shown when selected_index >= 0) ===

    // Issue key — large tap target (button-style container, opens detail overlay)
    lv_obj_t *key_btn = lv_btn_create(jira_screen);
    lv_obj_set_size(key_btn, 260, 44);
    lv_obj_align(key_btn, LV_ALIGN_CENTER, 0, -45);
    lv_obj_set_style_bg_opa(key_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(key_btn, 0, 0);
    lv_obj_set_style_border_width(key_btn, 0, 0);
    lv_obj_add_event_cb(key_btn, jira_open_issue_cb, LV_EVENT_CLICKED, NULL);

    jira_selected_label = lv_label_create(key_btn);
    lv_obj_set_style_text_font(jira_selected_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(jira_selected_label, lv_color_hex(0x2684FF), 0);
    lv_obj_set_style_text_align(jira_selected_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_selected_label, 250);
    lv_label_set_long_mode(jira_selected_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(jira_selected_label, "");
    lv_obj_center(jira_selected_label);

    // Issue summary title (single line)
    jira_summary_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_summary_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_summary_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(jira_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_summary_label, 260);
    lv_obj_align(jira_summary_label, LV_ALIGN_CENTER, 0, -22);
    lv_label_set_long_mode(jira_summary_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(jira_summary_label, 20, 0);
    lv_label_set_text(jira_summary_label, "");

    // Issue description (up to 3 lines, dimmer text)
    jira_desc_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_desc_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_desc_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(jira_desc_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_desc_label, 240);
    lv_obj_align(jira_desc_label, LV_ALIGN_CENTER, 0, 8);
    lv_label_set_long_mode(jira_desc_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_height(jira_desc_label, 48, 0);
    lv_label_set_text(jira_desc_label, "");

    // Hint — navigation arrows
    jira_hint_label = lv_label_create(jira_screen);
    lv_obj_set_style_text_font(jira_hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_hint_label, LV_ALIGN_CENTER, 0, 42);
    lv_label_set_text(jira_hint_label, LV_SYMBOL_LEFT " Turn knob " LV_SYMBOL_RIGHT);
    lv_obj_add_flag(jira_hint_label, LV_OBJ_FLAG_HIDDEN);

    // Start Timer button (left side)
    jira_start_btn = lv_btn_create(jira_screen);
    lv_obj_set_size(jira_start_btn, 120, 36);
    lv_obj_align(jira_start_btn, LV_ALIGN_CENTER, -65, 72);
    lv_obj_set_style_bg_color(jira_start_btn, get_accent_color(), 0);
    lv_obj_set_style_radius(jira_start_btn, 18, 0);
    lv_obj_set_style_shadow_width(jira_start_btn, 0, 0);
    lv_obj_add_event_cb(jira_start_btn, jira_start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(jira_start_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *start_lbl = lv_label_create(jira_start_btn);
    lv_label_set_text(start_lbl, LV_SYMBOL_PLAY " Timer");
    lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(start_lbl, COLOR_BG, 0);
    lv_obj_center(start_lbl);

    // Log Time button (right side)
    jira_log_btn = lv_btn_create(jira_screen);
    lv_obj_set_size(jira_log_btn, 120, 36);
    lv_obj_align(jira_log_btn, LV_ALIGN_CENTER, 65, 72);
    lv_obj_set_style_bg_color(jira_log_btn, lv_color_hex(0x3498db), 0);
    lv_obj_set_style_radius(jira_log_btn, 18, 0);
    lv_obj_set_style_shadow_width(jira_log_btn, 0, 0);
    lv_obj_add_event_cb(jira_log_btn, jira_log_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(jira_log_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *log_lbl = lv_label_create(jira_log_btn);
    lv_label_set_text(log_lbl, LV_SYMBOL_EDIT " Log");
    lv_obj_set_style_text_font(log_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(log_lbl, lv_color_white(), 0);
    lv_obj_center(log_lbl);

    // Back button
    jira_back_btn = lv_btn_create(jira_screen);
    lv_obj_set_size(jira_back_btn, 50, 50);
    lv_obj_align(jira_back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(jira_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(jira_back_btn, 0, 0);
    lv_obj_set_style_border_width(jira_back_btn, 0, 0);
    lv_obj_add_event_cb(jira_back_btn, jira_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(jira_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void update_jira_display(void)
{
    if (!jira_screen) return;

    bool synced = jira_data_is_synced() && jira_data_get_count() > 0;

    if (!synced) {
        // === LOADING STATE === show spinner, hide everything else
        lv_obj_clear_flag(jira_loading_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(jira_loading_label, LV_OBJ_FLAG_HIDDEN);
        // Hide dashboard elements
        lv_obj_add_flag(jira_dash_count_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_dash_hint_label, LV_OBJ_FLAG_HIDDEN);
        // Hide issue detail elements
        lv_label_set_text(jira_selected_label, "");
        lv_label_set_text(jira_summary_label, "");
        lv_label_set_text(jira_desc_label, "");
        lv_obj_add_flag(jira_start_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_log_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_hint_label, LV_OBJ_FLAG_HIDDEN);

        // Start loading animation
        if (!jira_loading_timer) {
            jira_loading_timer = lv_timer_create(jira_loading_anim_cb, 30, NULL);
        }

        // Update loading text based on connection
        if (usb_sync_is_connected()) {
            lv_label_set_text(jira_loading_label, "Loading issues...");
        } else {
            lv_label_set_text(jira_loading_label, "Waiting for USB...");
        }
        return;
    }

    // === DATA LOADED === hide spinner
    lv_obj_add_flag(jira_loading_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(jira_loading_label, LV_OBJ_FLAG_HIDDEN);

    // Stop loading animation
    if (jira_loading_timer) {
        lv_timer_del(jira_loading_timer);
        jira_loading_timer = NULL;
    }

    int8_t sel_idx = jira_data_get_selected_index();
    uint8_t count = jira_data_get_count();

    if (sel_idx < 0) {
        // === DASHBOARD MODE === show issue count + hint to turn right
        // Show dashboard elements
        static char count_buf[8];
        snprintf(count_buf, sizeof(count_buf), "%d", count);
        lv_label_set_text(jira_dash_count_label, count_buf);
        lv_obj_clear_flag(jira_dash_count_label, LV_OBJ_FLAG_HIDDEN);

        // Update title to show "issues" context
        static char dash_title[32];
        snprintf(dash_title, sizeof(dash_title), "%d open issue%s",
                 count, count == 1 ? "" : "s");
        lv_label_set_text(jira_title_label, dash_title);

        lv_obj_clear_flag(jira_dash_hint_label, LV_OBJ_FLAG_HIDDEN);

        // Hide issue detail elements
        lv_label_set_text(jira_selected_label, "");
        lv_label_set_text(jira_summary_label, "");
        lv_label_set_text(jira_desc_label, "");
        lv_obj_add_flag(jira_start_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_log_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        // === ISSUE DETAIL MODE === show selected issue info
        // Hide dashboard elements
        lv_obj_add_flag(jira_dash_count_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(jira_dash_hint_label, LV_OBJ_FLAG_HIDDEN);

        // Restore title
        lv_label_set_text(jira_title_label, LV_SYMBOL_EDIT " Jira");

        const jira_project_t *sel = jira_data_get_selected();
        if (sel) {
            // Issue key (tappable to open in browser)
            lv_label_set_text(jira_selected_label, sel->key);

            // Summary title
            if (sel->name[0]) {
                lv_label_set_text(jira_summary_label, sel->name);
            } else {
                lv_label_set_text(jira_summary_label, "");
            }

            // Description (up to 3 lines)
            if (sel->desc[0]) {
                lv_label_set_text(jira_desc_label, sel->desc);
            } else {
                lv_label_set_text(jira_desc_label, "");
            }

            // Show action buttons
            lv_obj_clear_flag(jira_start_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(jira_log_btn, LV_OBJ_FLAG_HIDDEN);

            // Show navigation hint
            lv_obj_clear_flag(jira_hint_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update accent colors
    lv_obj_set_style_bg_color(jira_start_btn, get_accent_color(), 0);
}

static void show_jira_screen(void)
{
    // Hide all other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);

    if (jira_screen) lv_obj_clear_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    update_jira_display();
    current_screen = SCREEN_JIRA;
}

static void hide_jira_screen(void)
{
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
}

static void jira_back_cb(lv_event_t *e)
{
    show_home_screen();
}

static void jira_start_btn_cb(lv_event_t *e)
{
    if (jira_data_get_selected()) {
        // Reset Jira timer for new session
        jira_timer_state = TIMER_STATE_READY;
        jira_remaining_seconds = jira_set_minutes * 60;
        show_jira_timer_screen();
    }
}

static void jira_log_btn_cb(lv_event_t *e)
{
    const jira_project_t *sel = jira_data_get_selected();
    if (sel) {
        // Send manual log request to Mac companion (always send, even if connection state stale)
        usb_sync_send_jira_log_time(sel->key);
        // Show done screen with "Logging..." status
        if (jira_done_title_label) {
            lv_label_set_text(jira_done_title_label, "Logging...");
        }
        if (jira_done_status_label) {
            lv_label_set_text(jira_done_status_label, "Check your Mac for prompts");
        }
        show_jira_done_screen();
    }
}

static void jira_open_issue_cb(lv_event_t *e)
{
    const jira_project_t *sel = jira_data_get_selected();
    if (sel) {
        haptic_click();
        show_jira_detail();
    }
}

static void jira_loading_anim_cb(lv_timer_t *timer)
{
    static uint16_t angle = 0;
    angle = (angle + 10) % 360;
    if (jira_loading_arc) {
        lv_arc_set_angles(jira_loading_arc, angle, angle + 90);
    }
}

// ============================================================
// Jira Issue Detail Overlay (scrollable full-info view)
// ============================================================

static void create_jira_detail_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Full-screen overlay background — tap anywhere on it to close
    jira_detail_overlay = lv_obj_create(screen);
    lv_obj_set_size(jira_detail_overlay, 360, 360);
    lv_obj_center(jira_detail_overlay);
    lv_obj_set_style_bg_color(jira_detail_overlay, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(jira_detail_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(jira_detail_overlay, 0, 0);
    lv_obj_set_style_radius(jira_detail_overlay, 180, 0);
    lv_obj_set_style_pad_all(jira_detail_overlay, 0, 0);
    lv_obj_clear_flag(jira_detail_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(jira_detail_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(jira_detail_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(jira_detail_overlay, jira_detail_close_cb, LV_EVENT_CLICKED, NULL);

    // Close button — wide bar at bottom, easy to tap on round display
    jira_detail_close_btn = lv_btn_create(jira_detail_overlay);
    lv_obj_set_size(jira_detail_close_btn, 140, 38);
    lv_obj_align(jira_detail_close_btn, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_set_style_bg_color(jira_detail_close_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(jira_detail_close_btn, 19, 0);
    lv_obj_set_style_shadow_width(jira_detail_close_btn, 0, 0);
    lv_obj_set_style_border_width(jira_detail_close_btn, 0, 0);
    lv_obj_add_event_cb(jira_detail_close_btn, jira_detail_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_lbl = lv_label_create(jira_detail_close_btn);
    lv_label_set_text(close_lbl, LV_SYMBOL_LEFT " Close");
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(close_lbl, COLOR_TEXT, 0);
    lv_obj_center(close_lbl);

    // Scrollable content area — inset for round display safe zone
    jira_detail_content = lv_obj_create(jira_detail_overlay);
    lv_obj_set_size(jira_detail_content, 250, 200);
    lv_obj_align(jira_detail_content, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(jira_detail_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(jira_detail_content, 0, 0);
    lv_obj_set_style_pad_all(jira_detail_content, 0, 0);
    lv_obj_set_style_pad_row(jira_detail_content, 6, 0);
    lv_obj_add_flag(jira_detail_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(jira_detail_content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(jira_detail_content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(jira_detail_content, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(jira_detail_content, lv_color_hex(0x2684FF), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(jira_detail_content, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(jira_detail_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(jira_detail_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Issue key — large Jira blue
    jira_detail_key_label = lv_label_create(jira_detail_content);
    lv_obj_set_style_text_font(jira_detail_key_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(jira_detail_key_label, lv_color_hex(0x2684FF), 0);
    lv_obj_set_style_text_align(jira_detail_key_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_detail_key_label, 240);
    lv_label_set_text(jira_detail_key_label, "");

    // Summary — white, wrapping
    jira_detail_summary_label = lv_label_create(jira_detail_content);
    lv_obj_set_style_text_font(jira_detail_summary_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_detail_summary_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(jira_detail_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_detail_summary_label, 240);
    lv_label_set_long_mode(jira_detail_summary_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(jira_detail_summary_label, "");

    // Project + Status combined
    jira_detail_proj_label = lv_label_create(jira_detail_content);
    lv_obj_set_style_text_font(jira_detail_proj_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_detail_proj_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(jira_detail_proj_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_detail_proj_label, 240);
    lv_label_set_text(jira_detail_proj_label, "");

    // Status badge
    jira_detail_status_label = lv_label_create(jira_detail_content);
    lv_obj_set_style_text_font(jira_detail_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_detail_status_label, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_style_text_align(jira_detail_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_detail_status_label, 240);
    lv_label_set_text(jira_detail_status_label, "");

    // Divider line
    lv_obj_t *divider = lv_obj_create(jira_detail_content);
    lv_obj_set_size(divider, 180, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    // Description — full text, wrapping, no truncation
    jira_detail_desc_label = lv_label_create(jira_detail_content);
    lv_obj_set_style_text_font(jira_detail_desc_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_detail_desc_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(jira_detail_desc_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(jira_detail_desc_label, 230);
    lv_label_set_long_mode(jira_detail_desc_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(jira_detail_desc_label, "");

    // "Open in Browser" button at the bottom of scrollable content
    jira_detail_open_btn = lv_btn_create(jira_detail_content);
    lv_obj_set_size(jira_detail_open_btn, 170, 34);
    lv_obj_set_style_bg_color(jira_detail_open_btn, lv_color_hex(0x2684FF), 0);
    lv_obj_set_style_radius(jira_detail_open_btn, 17, 0);
    lv_obj_set_style_shadow_width(jira_detail_open_btn, 0, 0);
    lv_obj_add_event_cb(jira_detail_open_btn, jira_detail_open_browser_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *open_lbl = lv_label_create(jira_detail_open_btn);
    lv_label_set_text(open_lbl, LV_SYMBOL_NEW_LINE " Open in Browser");
    lv_obj_set_style_text_font(open_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(open_lbl, lv_color_white(), 0);
    lv_obj_center(open_lbl);
}

static void update_jira_detail_display(void)
{
    const jira_project_t *sel = jira_data_get_selected();
    if (!sel) return;

    lv_label_set_text(jira_detail_key_label, sel->key);
    lv_label_set_text(jira_detail_summary_label, sel->name);
    lv_label_set_text(jira_detail_proj_label, sel->proj);

    // Status with color
    lv_label_set_text(jira_detail_status_label, sel->status);
    if (strstr(sel->status, "Progress") || strstr(sel->status, "progress")) {
        lv_obj_set_style_text_color(jira_detail_status_label, lv_color_hex(0x4FC3F7), 0);
    } else if (strstr(sel->status, "To Do") || strstr(sel->status, "Hold")) {
        lv_obj_set_style_text_color(jira_detail_status_label, lv_color_hex(0xFFB74D), 0);
    } else {
        lv_obj_set_style_text_color(jira_detail_status_label, COLOR_TEXT_DIM, 0);
    }

    // Description
    if (sel->desc[0]) {
        lv_label_set_text(jira_detail_desc_label, sel->desc);
    } else {
        lv_label_set_text(jira_detail_desc_label, "No description");
        lv_obj_set_style_text_color(jira_detail_desc_label, COLOR_TEXT_DIM, 0);
    }

    // Scroll back to top
    lv_obj_scroll_to_y(jira_detail_content, 0, LV_ANIM_OFF);
}

static void show_jira_detail(void)
{
    if (!jira_detail_open && jira_detail_overlay != NULL) {
        update_jira_detail_display();
        lv_obj_clear_flag(jira_detail_overlay, LV_OBJ_FLAG_HIDDEN);
        jira_detail_open = true;
    }
}

static void hide_jira_detail(void)
{
    if (jira_detail_open && jira_detail_overlay != NULL) {
        lv_obj_add_flag(jira_detail_overlay, LV_OBJ_FLAG_HIDDEN);
        jira_detail_open = false;
    }
}

static void jira_detail_close_cb(lv_event_t *e)
{
    haptic_click();
    hide_jira_detail();
}

static void jira_detail_open_browser_cb(lv_event_t *e)
{
    const jira_project_t *sel = jira_data_get_selected();
    if (sel) {
        haptic_click();
        usb_sync_send_jira_open(sel->key);
    }
}

// ============================================================
// Jira Project Picker Overlay
// ============================================================

static void create_jira_picker_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    // Full-screen overlay
    jira_picker_overlay = lv_obj_create(screen);
    lv_obj_set_size(jira_picker_overlay, 360, 360);
    lv_obj_center(jira_picker_overlay);
    lv_obj_set_style_bg_color(jira_picker_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(jira_picker_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(jira_picker_overlay, 0, 0);
    lv_obj_set_style_radius(jira_picker_overlay, 180, 0);
    lv_obj_set_style_pad_all(jira_picker_overlay, 0, 0);
    lv_obj_clear_flag(jira_picker_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(jira_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(jira_picker_overlay, jira_picker_overlay_cb, LV_EVENT_CLICKED, NULL);

    // Issue key (prominent, accent color)
    jira_picker_key_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_key_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(jira_picker_key_label, get_accent_color(), 0);
    lv_obj_align(jira_picker_key_label, LV_ALIGN_CENTER, 0, -55);
    lv_label_set_long_mode(jira_picker_key_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(jira_picker_key_label, 300);
    lv_obj_set_style_text_align(jira_picker_key_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(jira_picker_key_label, "");

    // Project name (dim, smaller)
    jira_picker_proj_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_proj_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_picker_proj_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(jira_picker_proj_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_picker_proj_label, 280);
    lv_obj_align(jira_picker_proj_label, LV_ALIGN_CENTER, 0, -28);
    lv_label_set_long_mode(jira_picker_proj_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(jira_picker_proj_label, "");

    // Issue summary (white, wrapping for longer text)
    jira_picker_name_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_picker_name_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(jira_picker_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_picker_name_label, 260);
    lv_obj_align(jira_picker_name_label, LV_ALIGN_CENTER, 0, 5);
    lv_label_set_text(jira_picker_name_label, "");
    lv_label_set_long_mode(jira_picker_name_label, LV_LABEL_LONG_WRAP);

    // Status badge
    jira_picker_status_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_picker_status_label, lv_color_hex(0x4FC3F7), 0);
    lv_obj_set_style_text_align(jira_picker_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(jira_picker_status_label, LV_ALIGN_CENTER, 0, 42);
    lv_label_set_text(jira_picker_status_label, "");

    // Position indicator
    jira_picker_pos_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_pos_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_picker_pos_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_picker_pos_label, LV_ALIGN_CENTER, 0, 65);
    lv_label_set_text(jira_picker_pos_label, "");

    // Hint
    jira_picker_hint_label = lv_label_create(jira_picker_overlay);
    lv_obj_set_style_text_font(jira_picker_hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_picker_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_picker_hint_label, LV_ALIGN_CENTER, 0, 85);
    lv_label_set_text(jira_picker_hint_label, "Turn knob | Tap to select");
}

static void update_jira_picker_display(void)
{
    int8_t idx = jira_data_get_selected_index();
    const jira_project_t *issue = jira_data_get_selected();

    if (issue) {
        lv_label_set_text(jira_picker_key_label, issue->key);
        lv_label_set_text(jira_picker_proj_label, issue->proj);
        lv_label_set_text(jira_picker_name_label, issue->name);
        lv_obj_set_style_text_color(jira_picker_key_label, get_accent_color(), 0);

        // Status with color coding
        lv_label_set_text(jira_picker_status_label, issue->status);
        if (strstr(issue->status, "Progress") || strstr(issue->status, "progress")) {
            lv_obj_set_style_text_color(jira_picker_status_label, lv_color_hex(0x4FC3F7), 0); // blue
        } else if (strstr(issue->status, "To Do") || strstr(issue->status, "Hold")) {
            lv_obj_set_style_text_color(jira_picker_status_label, lv_color_hex(0xFFB74D), 0); // orange
        } else {
            lv_obj_set_style_text_color(jira_picker_status_label, COLOR_TEXT_DIM, 0);
        }

        static char pos_buf[16];
        snprintf(pos_buf, sizeof(pos_buf), "%d / %d", idx + 1, jira_data_get_count());
        lv_label_set_text(jira_picker_pos_label, pos_buf);
    } else {
        lv_label_set_text(jira_picker_key_label, "---");
        lv_label_set_text(jira_picker_proj_label, "");
        lv_label_set_text(jira_picker_name_label, "No issues");
        lv_label_set_text(jira_picker_status_label, "");
        lv_label_set_text(jira_picker_pos_label, "");
    }
}

static void show_jira_picker(void)
{
    if (!jira_picker_open && jira_picker_overlay != NULL) {
        lv_obj_clear_flag(jira_picker_overlay, LV_OBJ_FLAG_HIDDEN);
        jira_picker_open = true;
        update_jira_picker_display();
    }
}

static void hide_jira_picker(void)
{
    if (jira_picker_open && jira_picker_overlay != NULL) {
        lv_obj_add_flag(jira_picker_overlay, LV_OBJ_FLAG_HIDDEN);
        jira_picker_open = false;
    }
}

static void jira_picker_overlay_cb(lv_event_t *e)
{
    // Tap anywhere on picker = confirm selection and close
    haptic_click();
    hide_jira_picker();
    update_jira_display();
}

// ============================================================
// Jira Timer Screen
// ============================================================

static void create_jira_timer_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    jira_timer_screen = lv_obj_create(screen);
    lv_obj_set_size(jira_timer_screen, 360, 360);
    lv_obj_center(jira_timer_screen);
    lv_obj_set_style_bg_color(jira_timer_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(jira_timer_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(jira_timer_screen, 0, 0);
    lv_obj_set_style_radius(jira_timer_screen, 180, 0);
    lv_obj_set_style_pad_all(jira_timer_screen, 0, 0);
    lv_obj_clear_flag(jira_timer_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);

    // Project key label at top
    jira_timer_project_label = lv_label_create(jira_timer_screen);
    lv_obj_set_style_text_font(jira_timer_project_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(jira_timer_project_label, get_accent_color(), 0);
    lv_obj_align(jira_timer_project_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_label_set_text(jira_timer_project_label, "");

    // Arc
    jira_timer_arc = lv_arc_create(jira_timer_screen);
    lv_obj_set_size(jira_timer_arc, 300, 300);
    lv_obj_center(jira_timer_arc);
    lv_arc_set_rotation(jira_timer_arc, 270);
    lv_arc_set_bg_angles(jira_timer_arc, 0, 360);
    lv_arc_set_value(jira_timer_arc, 100);
    lv_obj_remove_style(jira_timer_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(jira_timer_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(jira_timer_arc, COLOR_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(jira_timer_arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_color(jira_timer_arc, get_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(jira_timer_arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(jira_timer_arc, true, LV_PART_INDICATOR);

    // Time label
    jira_timer_time_label = lv_label_create(jira_timer_screen);
    lv_obj_set_style_text_font(jira_timer_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(jira_timer_time_label, COLOR_TEXT, 0);
    lv_obj_align(jira_timer_time_label, LV_ALIGN_CENTER, 0, -10);

    // Status label
    jira_timer_status_label = lv_label_create(jira_timer_screen);
    lv_obj_set_style_text_font(jira_timer_status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(jira_timer_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_timer_status_label, LV_ALIGN_CENTER, 0, 35);

    // Hint label
    jira_timer_hint_label = lv_label_create(jira_timer_screen);
    lv_obj_set_style_text_font(jira_timer_hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(jira_timer_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(jira_timer_hint_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    // Continue button (hidden by default)
    jira_timer_btn_continue = lv_btn_create(jira_timer_screen);
    lv_obj_set_size(jira_timer_btn_continue, 50, 50);
    lv_obj_align(jira_timer_btn_continue, LV_ALIGN_CENTER, -45, 80);
    lv_obj_set_style_bg_opa(jira_timer_btn_continue, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(jira_timer_btn_continue, 0, 0);
    lv_obj_set_style_border_width(jira_timer_btn_continue, 0, 0);
    lv_obj_add_event_cb(jira_timer_btn_continue, jira_timer_btn_continue_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(jira_timer_btn_continue, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *cont_lbl = lv_label_create(jira_timer_btn_continue);
    lv_label_set_text(cont_lbl, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(cont_lbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(cont_lbl, COLOR_TEXT, 0);
    lv_obj_center(cont_lbl);

    // Reset button (hidden by default)
    jira_timer_btn_reset = lv_btn_create(jira_timer_screen);
    lv_obj_set_size(jira_timer_btn_reset, 50, 50);
    lv_obj_align(jira_timer_btn_reset, LV_ALIGN_CENTER, 45, 80);
    lv_obj_set_style_bg_opa(jira_timer_btn_reset, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(jira_timer_btn_reset, 0, 0);
    lv_obj_set_style_border_width(jira_timer_btn_reset, 0, 0);
    lv_obj_add_event_cb(jira_timer_btn_reset, jira_timer_btn_reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(jira_timer_btn_reset, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *rst_lbl = lv_label_create(jira_timer_btn_reset);
    lv_label_set_text(rst_lbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(rst_lbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(rst_lbl, COLOR_TEXT, 0);
    lv_obj_center(rst_lbl);
}

static void update_jira_timer_display(void)
{
    // Project key
    const jira_project_t *sel = jira_data_get_selected();
    if (sel) {
        lv_label_set_text(jira_timer_project_label, sel->key);
    }
    lv_obj_set_style_text_color(jira_timer_project_label, get_accent_color(), 0);

    // Time
    int mins = jira_remaining_seconds / 60;
    int secs = jira_remaining_seconds % 60;
    static char jt_buf[16];
    snprintf(jt_buf, sizeof(jt_buf), "%02d:%02d", mins, secs);
    lv_label_set_text(jira_timer_time_label, jt_buf);

    // Arc progress
    int progress;
    if (jira_timer_state == TIMER_STATE_READY) {
        progress = (jira_set_minutes * 100) / MAX_MINUTES;
    } else {
        int total_seconds = jira_set_minutes * 60;
        progress = (jira_remaining_seconds * 100) / total_seconds;
    }
    lv_arc_set_value(jira_timer_arc, progress);

    // Status and buttons
    switch (jira_timer_state) {
        case TIMER_STATE_READY:
            lv_label_set_text(jira_timer_status_label, "READY");
            lv_label_set_text(jira_timer_hint_label, "Knob: adjust | Tap: start");
            lv_obj_set_style_arc_color(jira_timer_arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(jira_timer_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_RUNNING:
            lv_label_set_text(jira_timer_status_label, "FOCUS");
            lv_label_set_text(jira_timer_hint_label, "Tap: pause");
            lv_obj_set_style_arc_color(jira_timer_arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(jira_timer_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_PAUSED:
            lv_label_set_text(jira_timer_status_label, "PAUSED");
            lv_obj_set_style_arc_color(jira_timer_arc, get_accent_dim_color(), LV_PART_INDICATOR);
            lv_obj_add_flag(jira_timer_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(jira_timer_btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(jira_timer_btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
        case TIMER_STATE_DONE:
            lv_label_set_text(jira_timer_status_label, "DONE!");
            lv_label_set_text(jira_timer_hint_label, "Logging...");
            lv_obj_set_style_arc_color(jira_timer_arc, get_accent_color(), LV_PART_INDICATOR);
            lv_obj_clear_flag(jira_timer_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_continue, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jira_timer_btn_reset, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

static void show_jira_timer_screen(void)
{
    // Hide all other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);

    if (jira_timer_screen) lv_obj_clear_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    update_jira_timer_display();
    current_screen = SCREEN_JIRA_TIMER;
}

static void hide_jira_timer_screen(void)
{
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
}

static void jira_timer_countdown_cb(lv_timer_t *timer)
{
    if (jira_timer_state != TIMER_STATE_RUNNING) return;

    if (jira_remaining_seconds > 0) {
        jira_remaining_seconds--;
        update_jira_timer_display();
    }

    if (jira_remaining_seconds == 0) {
        jira_timer_state = TIMER_STATE_DONE;
        // Log to the general time log
        time_log_add_session(SESSION_WORK, jira_set_minutes);
        // Send completion to Mac companion
        const jira_project_t *sel = jira_data_get_selected();
        if (sel) {
            usb_sync_send_jira_timer_done(sel->key, jira_set_minutes);
        }
        update_jira_timer_display();
        // Transition to done screen after a moment
        show_jira_done_screen();
    }
}

static void jira_timer_btn_continue_cb(lv_event_t *e)
{
    if (jira_timer_state == TIMER_STATE_PAUSED) {
        if (lv_tick_elaps(jira_paused_at_ms) < BUTTON_DEBOUNCE_MS) return;
        haptic_click();
        jira_timer_state = TIMER_STATE_RUNNING;
        if (jira_countdown_timer) lv_timer_resume(jira_countdown_timer);
        update_jira_timer_display();
    }
}

static void jira_timer_btn_reset_cb(lv_event_t *e)
{
    if (jira_timer_state == TIMER_STATE_PAUSED) {
        if (lv_tick_elaps(jira_paused_at_ms) < BUTTON_DEBOUNCE_MS) return;
        haptic_click();
        jira_timer_state = TIMER_STATE_READY;
        jira_remaining_seconds = jira_set_minutes * 60;
        if (jira_countdown_timer) lv_timer_pause(jira_countdown_timer);
        update_jira_timer_display();
    }
}

// ============================================================
// Jira Done Screen
// ============================================================

static void create_jira_done_ui(void)
{
    lv_obj_t *screen = lv_scr_act();

    jira_done_screen = lv_obj_create(screen);
    lv_obj_set_size(jira_done_screen, 360, 360);
    lv_obj_center(jira_done_screen);
    lv_obj_set_style_bg_color(jira_done_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(jira_done_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(jira_done_screen, 0, 0);
    lv_obj_set_style_radius(jira_done_screen, 180, 0);
    lv_obj_set_style_pad_all(jira_done_screen, 0, 0);
    lv_obj_clear_flag(jira_done_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);

    // Done title
    jira_done_title_label = lv_label_create(jira_done_screen);
    lv_obj_set_style_text_font(jira_done_title_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(jira_done_title_label, get_accent_color(), 0);
    lv_obj_align(jira_done_title_label, LV_ALIGN_CENTER, 0, -30);
    lv_label_set_text(jira_done_title_label, "Done!");

    // Status
    jira_done_status_label = lv_label_create(jira_done_screen);
    lv_obj_set_style_text_font(jira_done_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jira_done_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(jira_done_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(jira_done_status_label, 280);
    lv_obj_align(jira_done_status_label, LV_ALIGN_CENTER, 0, 20);
    lv_label_set_text(jira_done_status_label, "Sending to Mac...");
    lv_label_set_long_mode(jira_done_status_label, LV_LABEL_LONG_DOT);

    // Back button
    jira_done_back_btn = lv_btn_create(jira_done_screen);
    lv_obj_set_size(jira_done_back_btn, 50, 50);
    lv_obj_align(jira_done_back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_opa(jira_done_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(jira_done_back_btn, 0, 0);
    lv_obj_set_style_border_width(jira_done_back_btn, 0, 0);
    lv_obj_add_event_cb(jira_done_back_btn, jira_done_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(jira_done_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void show_jira_done_screen(void)
{
    // Hide Jira timer screen
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);

    // Reset status text
    lv_label_set_text(jira_done_status_label, "Sending to Mac...");
    lv_obj_set_style_text_color(jira_done_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_color(jira_done_title_label, get_accent_color(), 0);

    if (jira_done_screen) lv_obj_clear_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);

    // Start 30-second timeout
    if (jira_done_timeout_timer) {
        lv_timer_del(jira_done_timeout_timer);
    }
    jira_done_timeout_timer = lv_timer_create(jira_done_timeout_cb, 30000, NULL);
    lv_timer_set_repeat_count(jira_done_timeout_timer, 1);
}

static void jira_done_back_cb(lv_event_t *e)
{
    if (jira_done_timeout_timer) {
        lv_timer_del(jira_done_timeout_timer);
        jira_done_timeout_timer = NULL;
    }
    show_jira_screen();
}

static void jira_done_timeout_cb(lv_timer_t *timer)
{
    jira_done_timeout_timer = NULL;
    lv_label_set_text(jira_done_status_label, "No response from Mac");
    lv_obj_set_style_text_color(jira_done_status_label, COLOR_TEXT_DIM, 0);
}

static void jira_done_auto_return_cb(lv_timer_t *timer)
{
    // Auto-return to Jira main screen after log result
    show_jira_screen();
}

// ============================================================
// Jira Public Functions (called from outside lcd_bsp.c)
// ============================================================

void jira_knob_left(void)
{
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (jira_detail_open) {
            // Scroll up in detail overlay
            lv_obj_scroll_by(jira_detail_content, 0, 30, LV_ANIM_ON);
        } else if (jira_picker_open) {
            // Cycle through projects in picker overlay
            int8_t idx = jira_data_get_selected_index();
            idx--;
            if (idx < 0) idx = jira_data_get_count() - 1;
            jira_data_select(idx);
            haptic_click();
            update_jira_picker_display();
        } else if (current_screen == SCREEN_JIRA && jira_data_get_count() > 0) {
            // Navigate issues: left from first issue → back to dashboard
            int8_t idx = jira_data_get_selected_index();
            if (idx <= 0) {
                // Go back to dashboard
                jira_data_select(-1);
            } else {
                idx--;
                jira_data_select(idx);
            }
            haptic_click();
            update_jira_display();
        } else if (current_screen == SCREEN_JIRA_TIMER && jira_timer_state == TIMER_STATE_READY) {
            jira_set_minutes--;
            if (jira_set_minutes < MIN_MINUTES) jira_set_minutes = MIN_MINUTES;
            jira_remaining_seconds = jira_set_minutes * 60;
            update_jira_timer_display();
        }
        xSemaphoreGive(lvgl_mux);
    }
}

void jira_knob_right(void)
{
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (jira_detail_open) {
            // Scroll down in detail overlay
            lv_obj_scroll_by(jira_detail_content, 0, -30, LV_ANIM_ON);
        } else if (jira_picker_open) {
            int8_t idx = jira_data_get_selected_index();
            idx++;
            if (idx >= jira_data_get_count()) idx = 0;
            jira_data_select(idx);
            haptic_click();
            update_jira_picker_display();
        } else if (current_screen == SCREEN_JIRA && jira_data_get_count() > 0) {
            // Navigate issues: right from dashboard → first issue, right from last → wrap to first
            int8_t idx = jira_data_get_selected_index();
            if (idx < 0) {
                // Dashboard → first issue
                jira_data_select(0);
            } else {
                idx++;
                if (idx >= jira_data_get_count()) idx = 0;
                jira_data_select(idx);
            }
            haptic_click();
            update_jira_display();
        } else if (current_screen == SCREEN_JIRA_TIMER && jira_timer_state == TIMER_STATE_READY) {
            jira_set_minutes++;
            if (jira_set_minutes > MAX_MINUTES) jira_set_minutes = MAX_MINUTES;
            jira_remaining_seconds = jira_set_minutes * 60;
            update_jira_timer_display();
        }
        xSemaphoreGive(lvgl_mux);
    }
}

void jira_knob_press(void)
{
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (jira_detail_open) {
            // Close detail overlay
            haptic_click();
            hide_jira_detail();
        } else if (jira_picker_open) {
            // Confirm selection
            haptic_click();
            hide_jira_picker();
            update_jira_display();
        } else if (current_screen == SCREEN_JIRA_TIMER) {
            switch (jira_timer_state) {
                case TIMER_STATE_READY:
                    haptic_click();
                    jira_timer_state = TIMER_STATE_RUNNING;
                    if (jira_countdown_timer == NULL) {
                        jira_countdown_timer = lv_timer_create(jira_timer_countdown_cb, 1000, NULL);
                    } else {
                        lv_timer_resume(jira_countdown_timer);
                    }
                    break;
                case TIMER_STATE_RUNNING:
                    haptic_click();
                    jira_timer_state = TIMER_STATE_PAUSED;
                    jira_paused_at_ms = lv_tick_get();
                    if (jira_countdown_timer) lv_timer_pause(jira_countdown_timer);
                    break;
                case TIMER_STATE_PAUSED:
                    break;
                case TIMER_STATE_DONE:
                    haptic_click();
                    jira_timer_state = TIMER_STATE_READY;
                    jira_remaining_seconds = jira_set_minutes * 60;
                    if (jira_countdown_timer) lv_timer_pause(jira_countdown_timer);
                    break;
            }
            update_jira_timer_display();
        }
        xSemaphoreGive(lvgl_mux);
    }
}

bool is_jira_screen_active(void)
{
    return current_screen == SCREEN_JIRA;
}

bool is_jira_timer_screen_active(void)
{
    return current_screen == SCREEN_JIRA_TIMER;
}

bool is_jira_picker_open(void)
{
    return jira_picker_open;
}

void jira_update_projects_ui(void)
{
    // Called from usb_sync when projects arrive — always refresh Jira screen
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        update_jira_display();
        xSemaphoreGive(lvgl_mux);
    }
}

void jira_update_log_status(bool success, const char* message)
{
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (jira_done_status_label) {
            lv_label_set_text(jira_done_status_label, message);
            if (success) {
                lv_obj_set_style_text_color(jira_done_status_label, get_accent_color(), 0);
                if (jira_done_title_label) {
                    lv_label_set_text(jira_done_title_label, LV_SYMBOL_OK " Done!");
                }
            } else {
                lv_obj_set_style_text_color(jira_done_status_label, lv_color_hex(0xe74c3c), 0);
            }
        }
        // Cancel timeout timer
        if (jira_done_timeout_timer) {
            lv_timer_del(jira_done_timeout_timer);
            jira_done_timeout_timer = NULL;
        }
        // Auto-return to Jira screen after 2 seconds
        lv_timer_t *ret = lv_timer_create(jira_done_auto_return_cb, 2000, NULL);
        lv_timer_set_repeat_count(ret, 1);
        xSemaphoreGive(lvgl_mux);
    }
}

// ═══════════════════════════════════════════════════════════════════
// Weather screen
// ═══════════════════════════════════════════════════════════════════

static const char* weather_icon_for_condition(uint16_t condition_id) {
    // LVGL built-in FontAwesome symbols + clear text labels
    if (condition_id == 800)                          return LV_SYMBOL_CHARGE " ";  // clear sky (bolt = bright)
    if (condition_id == 801)                          return LV_SYMBOL_IMAGE " ";   // few clouds
    if (condition_id >= 802 && condition_id <= 804)   return LV_SYMBOL_IMAGE " ";   // cloudy
    if (condition_id >= 500 && condition_id < 600)    return LV_SYMBOL_TINT " ";    // rain (droplet)
    if (condition_id >= 300 && condition_id < 400)    return LV_SYMBOL_TINT " ";    // drizzle
    if (condition_id >= 600 && condition_id < 700)    return "* ";                  // snow
    if (condition_id >= 200 && condition_id < 300)    return LV_SYMBOL_WARNING " "; // thunderstorm
    if (condition_id >= 700 && condition_id < 800)    return "~ ";                  // fog/haze
    return "";
}

static void create_weather_ui(void) {
    lv_obj_t *screen = lv_scr_act();

    weather_screen = lv_obj_create(screen);
    lv_obj_set_size(weather_screen, 360, 360);
    lv_obj_center(weather_screen);
    lv_obj_set_style_bg_color(weather_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(weather_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(weather_screen, 0, 0);
    lv_obj_set_style_radius(weather_screen, 180, 0);
    lv_obj_set_style_pad_all(weather_screen, 0, 0);
    lv_obj_clear_flag(weather_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    weather_title_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0x3498db), 0);
    lv_obj_set_style_text_letter_space(weather_title_label, 3, 0);
    lv_obj_align(weather_title_label, LV_ALIGN_TOP_MID, 0, 45);
    lv_label_set_text(weather_title_label, "WEATHER");

    // Weather icon (large text)
    weather_icon_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_icon_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(weather_icon_label, lv_color_hex(0xf1c40f), 0);
    lv_obj_align(weather_icon_label, LV_ALIGN_CENTER, -75, -40);
    lv_label_set_text(weather_icon_label, "");

    // Current temperature (large)
    weather_temp_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(weather_temp_label, COLOR_TEXT, 0);
    lv_obj_align(weather_temp_label, LV_ALIGN_CENTER, 10, -40);
    lv_label_set_text(weather_temp_label, "--\xC2\xB0");

    // Condition text
    weather_condition_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_condition_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_condition_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(weather_condition_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(weather_condition_label, 240);
    lv_obj_align(weather_condition_label, LV_ALIGN_CENTER, 0, -5);
    lv_label_set_text(weather_condition_label, "");

    // High/Low
    weather_hilo_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_hilo_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_hilo_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(weather_hilo_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(weather_hilo_label, LV_ALIGN_CENTER, 0, 15);
    lv_label_set_text(weather_hilo_label, "");

    // Humidity
    weather_humidity_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(weather_humidity_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(weather_humidity_label, LV_ALIGN_CENTER, -55, 38);
    lv_label_set_text(weather_humidity_label, "");

    // Wind
    weather_wind_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(weather_wind_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(weather_wind_label, LV_ALIGN_CENTER, 55, 38);
    lv_label_set_text(weather_wind_label, "");

    // Forecast row (4 entries across bottom area)
    for (int i = 0; i < 4; i++) {
        weather_forecast_labels[i] = lv_label_create(weather_screen);
        lv_obj_set_style_text_font(weather_forecast_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(weather_forecast_labels[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_align(weather_forecast_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        int x_offset = -90 + (i * 60);
        lv_obj_align(weather_forecast_labels[i], LV_ALIGN_CENTER, x_offset, 65);
        lv_label_set_text(weather_forecast_labels[i], "");
    }

    // Loading label (shown when no data)
    weather_loading_label = lv_label_create(weather_screen);
    lv_obj_set_style_text_font(weather_loading_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_loading_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(weather_loading_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(weather_loading_label, "Waiting for data...");

    // Back button
    weather_back_btn = lv_btn_create(weather_screen);
    lv_obj_set_size(weather_back_btn, 50, 50);
    lv_obj_align(weather_back_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(weather_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(weather_back_btn, 0, 0);
    lv_obj_set_style_border_width(weather_back_btn, 0, 0);
    lv_obj_add_event_cb(weather_back_btn, weather_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(weather_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void update_weather_display(void) {
    if (!weather_screen) return;

    if (!weather_data_is_synced()) {
        // Show loading, hide data
        if (weather_loading_label) lv_obj_clear_flag(weather_loading_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Hide loading
    if (weather_loading_label) lv_obj_add_flag(weather_loading_label, LV_OBJ_FLAG_HIDDEN);

    const weather_current_t* w = weather_data_get_current();
    if (!w) return;

    // Temperature
    static char temp_buf[16];
    snprintf(temp_buf, sizeof(temp_buf), "%d\xC2\xB0", w->temp);
    lv_label_set_text(weather_temp_label, temp_buf);

    // Icon
    lv_label_set_text(weather_icon_label, weather_icon_for_condition(w->condition_id));

    // Condition
    lv_label_set_text(weather_condition_label, w->description);

    // High/Low
    static char hilo_buf[32];
    snprintf(hilo_buf, sizeof(hilo_buf), "H: %d\xC2\xB0   L: %d\xC2\xB0", w->temp_max, w->temp_min);
    lv_label_set_text(weather_hilo_label, hilo_buf);

    // Humidity & Wind
    static char hum_buf[24], wind_buf[24];
    snprintf(hum_buf, sizeof(hum_buf), "Humidity %d%%", w->humidity);
    snprintf(wind_buf, sizeof(wind_buf), "Wind %d mph", w->wind_speed);
    lv_label_set_text(weather_humidity_label, hum_buf);
    lv_label_set_text(weather_wind_label, wind_buf);

    // Forecast entries (show up to 4)
    uint8_t count = weather_data_get_forecast_count();
    for (int i = 0; i < 4; i++) {
        if (i < count && weather_forecast_labels[i]) {
            const weather_forecast_t* f = weather_data_get_forecast(i);
            if (f) {
                static char fbuf[4][24];
                snprintf(fbuf[i], sizeof(fbuf[i]), "%s\n%d\xC2\xB0", f->hour_str, f->temp);
                lv_label_set_text(weather_forecast_labels[i], fbuf[i]);
            }
        } else if (weather_forecast_labels[i]) {
            lv_label_set_text(weather_forecast_labels[i], "");
        }
    }
}

static void show_weather_screen(void) {
    // Hide all other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);

    if (weather_screen) lv_obj_clear_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    update_weather_display();
    current_screen = SCREEN_WEATHER;
}

static void hide_weather_screen(void) {
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    if (calendar_screen) lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
}

static void weather_back_cb(lv_event_t *e) {
    show_home_screen();
}

bool is_weather_screen_active(void) {
    return current_screen == SCREEN_WEATHER;
}

void weather_update_ui(void) {
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Update weather app screen if visible
        if (current_screen == SCREEN_WEATHER) {
            update_weather_display();
        }
        // Update home screen weather (combined temp + condition)
        if (weather_data_is_synced() && home_weather_icon) {
            const weather_current_t* w = weather_data_get_current();
            if (w) {
                static char wt_buf2[32];
                snprintf(wt_buf2, sizeof(wt_buf2), "%d\xC2\xB0  %s", w->temp, w->condition);
                lv_label_set_text(home_weather_icon, wt_buf2);
            }
        }
        xSemaphoreGive(lvgl_mux);
    }
}

// ── Calendar App Screen ──

static void calendar_back_cb(lv_event_t *e) {
    show_home_screen();
}

static void calendar_log_cb(lv_event_t *e) {
    // Find first non-all-day event to log
    uint8_t count = calendar_data_get_count();
    for (uint8_t i = 0; i < count; i++) {
        const calendar_event_t* ev = calendar_data_get_event(i);
        if (ev && !ev->is_all_day) {
            usb_sync_send_jira_log_meeting(ev->title, ev->duration_min);
            break;
        }
    }
}

static void create_calendar_ui(void) {
    lv_obj_t *scr = lv_scr_act();

    calendar_screen = lv_obj_create(scr);
    lv_obj_set_size(calendar_screen, 360, 360);
    lv_obj_center(calendar_screen);
    lv_obj_set_style_bg_color(calendar_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(calendar_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(calendar_screen, 0, 0);
    lv_obj_set_style_radius(calendar_screen, 180, 0);
    lv_obj_set_style_pad_all(calendar_screen, 0, 0);
    lv_obj_clear_flag(calendar_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);

    // Title
    calendar_title_label = lv_label_create(calendar_screen);
    lv_obj_set_style_text_font(calendar_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(calendar_title_label, lv_color_hex(0x3498db), 0);
    lv_obj_set_style_text_letter_space(calendar_title_label, 3, 0);
    lv_obj_align(calendar_title_label, LV_ALIGN_TOP_MID, 0, 45);
    lv_label_set_text(calendar_title_label, "CALENDAR");

    // Event list labels (7 visible slots)
    for (int i = 0; i < 7; i++) {
        calendar_event_labels[i] = lv_label_create(calendar_screen);
        lv_obj_set_style_text_font(calendar_event_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(calendar_event_labels[i], COLOR_TEXT, 0);
        lv_obj_set_width(calendar_event_labels[i], 260);
        lv_obj_set_style_text_align(calendar_event_labels[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(calendar_event_labels[i], LV_LABEL_LONG_DOT);
        int y_pos = 80 + (i * 28);
        lv_obj_align(calendar_event_labels[i], LV_ALIGN_TOP_MID, 0, y_pos);
        lv_label_set_text(calendar_event_labels[i], "");
    }

    // Loading label
    calendar_loading_label = lv_label_create(calendar_screen);
    lv_obj_set_style_text_font(calendar_loading_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(calendar_loading_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(calendar_loading_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(calendar_loading_label, "Waiting for data...");

    // Back button
    calendar_back_btn = lv_btn_create(calendar_screen);
    lv_obj_set_size(calendar_back_btn, 50, 50);
    lv_obj_align(calendar_back_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_opa(calendar_back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(calendar_back_btn, 0, 0);
    lv_obj_set_style_border_width(calendar_back_btn, 0, 0);
    lv_obj_add_event_cb(calendar_back_btn, calendar_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(calendar_back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT, 0);
    lv_obj_center(back_lbl);

    // LOG meeting button
    calendar_log_btn = lv_btn_create(calendar_screen);
    lv_obj_set_size(calendar_log_btn, 70, 36);
    lv_obj_align(calendar_log_btn, LV_ALIGN_BOTTOM_MID, 60, -20);
    lv_obj_set_style_bg_color(calendar_log_btn, lv_color_hex(0x3498db), 0);
    lv_obj_set_style_radius(calendar_log_btn, 18, 0);
    lv_obj_set_style_shadow_width(calendar_log_btn, 0, 0);
    lv_obj_set_style_border_width(calendar_log_btn, 0, 0);
    lv_obj_add_event_cb(calendar_log_btn, calendar_log_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *log_lbl = lv_label_create(calendar_log_btn);
    lv_label_set_text(log_lbl, "LOG");
    lv_obj_set_style_text_font(log_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(log_lbl, COLOR_TEXT, 0);
    lv_obj_center(log_lbl);
}

static void update_calendar_display(void) {
    if (!calendar_screen) return;

    if (!calendar_data_is_synced()) {
        if (calendar_loading_label) lv_obj_clear_flag(calendar_loading_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (calendar_loading_label) lv_obj_add_flag(calendar_loading_label, LV_OBJ_FLAG_HIDDEN);

    uint8_t count = calendar_data_get_count();
    static char evbuf[7][48];

    for (int i = 0; i < 7; i++) {
        if (i < count && calendar_event_labels[i]) {
            const calendar_event_t* ev = calendar_data_get_event(i);
            if (ev) {
                if (ev->is_all_day) {
                    snprintf(evbuf[i], sizeof(evbuf[i]), "All day  %s", ev->title);
                } else {
                    snprintf(evbuf[i], sizeof(evbuf[i]), "%s  %s (%dm)",
                             ev->start_str, ev->title, ev->duration_min);
                }
                lv_label_set_text(calendar_event_labels[i], evbuf[i]);
            }
        } else if (calendar_event_labels[i]) {
            lv_label_set_text(calendar_event_labels[i], "");
        }
    }

    if (count == 0) {
        // Show "No meetings" message
        if (calendar_event_labels[0]) {
            lv_label_set_text(calendar_event_labels[0], "No more meetings today");
            lv_obj_set_style_text_color(calendar_event_labels[0], COLOR_TEXT_DIM, 0);
        }
    }
}

static void show_calendar_screen(void) {
    // Hide all other screens
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
    if (wifi_screen) lv_obj_add_flag(wifi_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_screen) lv_obj_add_flag(jira_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_timer_screen) lv_obj_add_flag(jira_timer_screen, LV_OBJ_FLAG_HIDDEN);
    if (jira_done_screen) lv_obj_add_flag(jira_done_screen, LV_OBJ_FLAG_HIDDEN);
    if (weather_screen) lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);

    // Show calendar screen
    if (calendar_screen) lv_obj_clear_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
    update_calendar_display();
    current_screen = SCREEN_CALENDAR;
}

bool is_calendar_screen_active(void) {
    return current_screen == SCREEN_CALENDAR;
}

void calendar_update_ui(void) {
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Update calendar app screen if visible
        if (current_screen == SCREEN_CALENDAR) {
            update_calendar_display();
        }
        // Update home screen calendar label
        if (calendar_data_is_synced() && home_calendar_label) {
            int16_t mins = calendar_data_get_next_meeting_min();
            const calendar_event_t* next = calendar_data_get_event(0);
            if (mins == -1 && next) {
                static char hcal_buf[48];
                snprintf(hcal_buf, sizeof(hcal_buf), LV_SYMBOL_BELL " %s (now)", next->title);
                lv_label_set_text(home_calendar_label, hcal_buf);
            } else if (mins >= 0 && mins <= 60 && next) {
                static char hcal_buf[48];
                snprintf(hcal_buf, sizeof(hcal_buf), LV_SYMBOL_BELL " %s in %dm", next->title, mins);
                lv_label_set_text(home_calendar_label, hcal_buf);
            } else if (next) {
                static char hcal_buf[48];
                snprintf(hcal_buf, sizeof(hcal_buf), LV_SYMBOL_BELL " %s %s", next->title, next->start_str);
                lv_label_set_text(home_calendar_label, hcal_buf);
            } else {
                lv_label_set_text(home_calendar_label, "");
            }
        }
        xSemaphoreGive(lvgl_mux);
    }
}

// Public function: update Jira hours display when new data arrives
void jira_hours_update_ui(void) {
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (jira_hours_data_is_synced() && home_jira_hours_label) {
            const jira_hours_state_t* h = jira_hours_data_get();
            if (h->target_min > 0) {
                float logged = h->logged_min / 60.0f;
                float target = h->target_min / 60.0f;
                static char jhbuf2[24];
                snprintf(jhbuf2, sizeof(jhbuf2), "%.1f / %.1fh", logged, target);
                lv_label_set_text(home_jira_hours_label, jhbuf2);

                if (h->logged_min >= h->target_min) {
                    lv_obj_set_style_text_color(home_jira_hours_label, lv_color_hex(0x2ecc71), 0);
                } else if (h->logged_min >= (h->target_min * 3 / 4)) {
                    lv_obj_set_style_text_color(home_jira_hours_label, lv_color_hex(0xf39c12), 0);
                } else {
                    lv_obj_set_style_text_color(home_jira_hours_label, COLOR_TEXT_DIM, 0);
                }
            } else {
                lv_label_set_text(home_jira_hours_label, "");
            }
        }
        xSemaphoreGive(lvgl_mux);
    }
}

void lcd_lvgl_Init(void)
{
  static lv_disp_draw_buf_t disp_buf;
  static lv_disp_drv_t disp_drv;

  const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                               EXAMPLE_PIN_NUM_LCD_DATA0,
                                                               EXAMPLE_PIN_NUM_LCD_DATA1,
                                                               EXAMPLE_PIN_NUM_LCD_DATA2,
                                                               EXAMPLE_PIN_NUM_LCD_DATA3,
                                                               EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
  ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_handle_t io_handle = NULL;

  const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                              example_notify_lvgl_flush_ready,
                                                                              &disp_drv);

  sh8601_vendor_config_t vendor_config =
  {
    .init_cmds = lcd_init_cmds,
    .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    .flags =
    {
      .use_qspi_interface = 1,
    },
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
  amoled_panel_io_handle = io_handle;
  esp_lcd_panel_handle_t panel_handle = NULL;
  const esp_lcd_panel_dev_config_t panel_config =
  {
    .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = LCD_BIT_PER_PIXEL,
    .vendor_config = &vendor_config,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_init(panel_handle));

  lv_init();
  lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = example_lvgl_flush_cb;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  lv_disp_drv_register(&disp_drv);

  // Initialize touch
  Touch_Init();

  // Initialize haptic feedback (DRV2605 on same I2C bus)
  haptic_init();

  // Register LVGL touch input device
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = example_lvgl_touch_cb;
  lv_indev_drv_register(&indev_drv);

  const esp_timer_create_args_t lvgl_tick_timer_args =
  {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
  xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

  // Initialize time logging system (before LVGL UI so data is ready)
  time_log_init();

  if (example_lvgl_lock(-1))
  {
    // Create Home screen UI (hidden until splash completes)
    create_home_ui();
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    // Create Pomodoro timer UI (hidden initially)
    create_timer_ui();
    // Create Time Log UI (hidden initially)
    create_timelog_ui();
    // Create WiFi UI (hidden initially)
    create_wifi_ui();
    // Create Jira UI (hidden initially)
    create_jira_ui();
    create_jira_detail_ui();
    create_jira_picker_ui();
    create_jira_timer_ui();
    create_jira_done_ui();
    // Create Weather UI (hidden initially)
    create_weather_ui();
    // Create Calendar UI (hidden initially)
    create_calendar_ui();
    // Hide timer UI initially
    if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (time_label) lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    if (status_label) lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    if (btn_continue) lv_obj_add_flag(btn_continue, LV_OBJ_FLAG_HIDDEN);
    if (btn_reset) lv_obj_add_flag(btn_reset, LV_OBJ_FLAG_HIDDEN);
    // Create menu overlay (on top)
    create_menu_ui();
    // Create settings overlay (on top of menu)
    create_settings_ui();
    // Create splash screen on top of everything (shown first)
    create_splash_ui();
    current_screen = SCREEN_HOME;
    example_lvgl_unlock();
  }
}

static bool example_lvgl_lock(int timeout_ms)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    if (example_lvgl_lock(-1))
    {
      task_delay_ms = lv_timer_handler();
      example_lvgl_unlock();
    }
    if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    }
    else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

static void example_increase_lvgl_tick(void *arg)
{
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
  lv_disp_flush_ready(disp_driver);
  return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
  uint16_t x1 = area->x1;
  uint16_t x2 = area->x2;
  uint16_t y1 = area->y1;
  uint16_t y2 = area->y2;
  area->x1 = (x1 >> 1) << 1;
  area->y1 = (y1 >> 1) << 1;
  area->x2 = ((x2 >> 1) << 1) + 1;
  area->y2 = ((y2 >> 1) << 1) + 1;
}

static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  uint16_t x, y;
  if (getTouch(&x, &y)) {
    // Rotate touch coordinates 180 degrees to match display rotation
    data->point.x = EXAMPLE_LCD_H_RES - 1 - x;
    data->point.y = EXAMPLE_LCD_V_RES - 1 - y;
    data->state = LV_INDEV_STATE_PRESSED;

    // Swipe detection for menu (use rotated coordinates)
    int16_t rotated_y = data->point.y;
    if (touch_start_y < 0) {
      // Touch just started
      touch_start_y = rotated_y;
      swipe_active = (rotated_y < MENU_TRIGGER_ZONE); // Started from top
    } else if (swipe_active && !menu_open) {
      // Check for swipe down
      if ((rotated_y - touch_start_y) > SWIPE_THRESHOLD) {
        show_menu();
        swipe_active = false;
      }
    }

    // Swipe-up detection for Jira project picker
    if (current_screen == SCREEN_JIRA && !jira_picker_open) {
      static int16_t jira_swipe_start_y = -1;
      if (jira_swipe_start_y < 0) {
        if (rotated_y > (EXAMPLE_LCD_V_RES - MENU_TRIGGER_ZONE)) {
          jira_swipe_start_y = rotated_y;  // Started from bottom
        }
      } else {
        if ((jira_swipe_start_y - rotated_y) > SWIPE_THRESHOLD) {
          show_jira_picker();
          jira_swipe_start_y = -1;
        }
      }
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    touch_start_y = -1;
    swipe_active = false;
  }
}
