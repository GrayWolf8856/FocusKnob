#include "lcd_bsp.h"
#include "esp_lcd_sh8601.h"
#include "lcd_config.h"
#include "cst816.h"
#include "drv2605.h"
#include "time_log.h"
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
static lv_obj_t *home_time_label = NULL;
static lv_obj_t *home_date_label = NULL;
static lv_timer_t *clock_timer = NULL;

// UI elements - Time Log Screen
static lv_obj_t *timelog_screen = NULL;
static lv_obj_t *timelog_title_label = NULL;
static lv_obj_t *timelog_total_label = NULL;
static lv_obj_t *timelog_pomos_label = NULL;
static lv_obj_t *timelog_streak_label = NULL;
static lv_obj_t *timelog_sessions_container = NULL;
static lv_obj_t *timelog_back_btn = NULL;

// Screen state
typedef enum {
    SCREEN_HOME,
    SCREEN_TIMER,
    SCREEN_TIMELOG
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
    {LV_SYMBOL_WIFI, false},      // 2: WiFi
    {LV_SYMBOL_BLUETOOTH, false}, // 3: Bluetooth
    {LV_SYMBOL_AUDIO, false},     // 4: Music
    {LV_SYMBOL_IMAGE, false},     // 5: Gallery
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

    // Time label (large)
    home_time_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(home_time_label, COLOR_TEXT, 0);
    lv_obj_align(home_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_label_set_text(home_time_label, "12:00");

    // Date label
    home_date_label = lv_label_create(home_screen);
    lv_obj_set_style_text_font(home_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(home_date_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(home_date_label, LV_ALIGN_CENTER, 0, 35);
    lv_label_set_text(home_date_label, "Mon, Jan 1");

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

    // Update time (HH:MM format)
    static char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
    lv_label_set_text(home_time_label, time_buf);

    // Update date (Day, Mon DD format)
    static char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
    lv_label_set_text(home_date_label, date_buf);
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
    current_screen = SCREEN_HOME;
}

static void show_timer_screen(void)
{
    if (home_screen) lv_obj_add_flag(home_screen, LV_OBJ_FLAG_HIDDEN);
    // Hide time log screen
    if (timelog_screen) lv_obj_add_flag(timelog_screen, LV_OBJ_FLAG_HIDDEN);
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
    // Create Home screen UI (shown by default)
    create_home_ui();
    // Create Pomodoro timer UI (hidden initially)
    create_timer_ui();
    // Create Time Log UI (hidden initially)
    create_timelog_ui();
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
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    touch_start_y = -1;
    swipe_active = false;
  }
}
