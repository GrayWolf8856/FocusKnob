/*
 * Pomodoro Timer for ESP32-S3-Knob-Touch-LCD-1.8
 *
 * Features:
 * - Visual countdown timer with arc display
 * - Knob control: rotate to adjust time, press to start/pause
 * - WiFi AP mode for configuration
 * - USB serial sync with companion app
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lcd_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "bidi_switch_knob.h"
#include "wifi_config.h"
#include "usb_sync.h"
#include "jira_data.h"
#include "weather_data.h"
#include "calendar_data.h"
#include "jira_hours_data.h"

// Encoder pins
#define ENCODER_PIN_A    8
#define ENCODER_PIN_B    7

// Event bits for knob
#define KNOB_LEFT_BIT    (1 << 0)
#define KNOB_RIGHT_BIT   (1 << 1)

EventGroupHandle_t knob_event_group = NULL;
static knob_handle_t knob_handle = NULL;

// Knob callbacks
static void knob_left_callback(void *arg, void *data) {
    xEventGroupSetBits(knob_event_group, KNOB_LEFT_BIT);
}

static void knob_right_callback(void *arg, void *data) {
    xEventGroupSetBits(knob_event_group, KNOB_RIGHT_BIT);
}

// Input handling task
static void input_task(void *arg) {
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
            knob_event_group,
            KNOB_LEFT_BIT | KNOB_RIGHT_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(50)
        );

        if (bits & KNOB_LEFT_BIT) {
            if (is_jira_screen_active() || is_jira_picker_open() || is_jira_timer_screen_active()) {
                jira_knob_left();
            } else {
                timer_knob_left();
            }
        }
        if (bits & KNOB_RIGHT_BIT) {
            if (is_jira_screen_active() || is_jira_picker_open() || is_jira_timer_screen_active()) {
                jira_knob_right();
            } else {
                timer_knob_right();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.setRxBufferSize(2048);  // Need larger buffer for JIRA_PROJECTS JSON
    Serial.begin(115200);
    Serial.println("Pomodoro Timer Starting...");

    // Initialize LCD and LVGL
    lcd_lvgl_Init();

    // Set backlight
    lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

    // Initialize knob event group
    knob_event_group = xEventGroupCreate();

    // Initialize encoder
    knob_config_t knob_cfg = {
        .gpio_encoder_a = ENCODER_PIN_A,
        .gpio_encoder_b = ENCODER_PIN_B,
    };

    knob_handle = iot_knob_create(&knob_cfg);
    if (knob_handle != NULL) {
        iot_knob_register_cb(knob_handle, KNOB_LEFT, knob_left_callback, NULL);
        iot_knob_register_cb(knob_handle, KNOB_RIGHT, knob_right_callback, NULL);
        Serial.println("Knob initialized");
    } else {
        Serial.println("Knob init failed!");
    }

    // Create input handling task
    xTaskCreate(input_task, "input_task", 2048, NULL, 2, NULL);

    // Initialize WiFi config system
    wifi_config_init();

    // Initialize USB sync
    usb_sync_init();

    // Initialize Jira data cache
    jira_data_init();

    // Initialize weather data cache
    weather_data_init();

    // Initialize calendar data cache
    calendar_data_init();

    // Initialize Jira hours data cache
    jira_hours_data_init();

    Serial.println("Pomodoro Timer Ready!");
}

void loop() {
    // Check touch screen - only handle on timer screen
    static bool last_touch_state = false;
    uint16_t touch_x, touch_y;
    bool current_touch_state = getTouch(&touch_x, &touch_y);

    if (current_touch_state && !last_touch_state) {
        // Touch started - trigger action based on active screen
        // Skip if touch is in menu trigger zone (top 60 pixels after rotation)
        // Touch Y is inverted (360 - y), so top zone is when rotated_y < 60
        uint16_t rotated_y = 359 - touch_y;
        if (is_timer_screen_active() && rotated_y >= 60) {
            timer_knob_press();
            delay(200);  // Debounce to prevent multiple triggers
        } else if (is_jira_timer_screen_active() && rotated_y >= 60) {
            jira_knob_press();
            delay(200);
        }
    }
    last_touch_state = current_touch_state;

    // Process WiFi web server requests
    wifi_config_process();

    // Process USB serial commands
    usb_sync_process();

    vTaskDelay(pdMS_TO_TICKS(20));
}
