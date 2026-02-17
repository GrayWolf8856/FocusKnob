#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Buffer limits
#define WEATHER_CONDITION_LEN 32
#define WEATHER_DESC_LEN 48
#define WEATHER_HOUR_LEN 8
#define WEATHER_MAX_FORECAST 8

// Current weather data
typedef struct {
    int16_t temp;           // Current temperature (F)
    int16_t temp_min;       // Daily low
    int16_t temp_max;       // Daily high
    uint8_t humidity;       // Humidity percentage
    uint8_t wind_speed;     // Wind speed (mph)
    uint16_t condition_id;  // OWM condition code (800=clear, etc.)
    char condition[WEATHER_CONDITION_LEN];   // e.g. "Clear", "Rain"
    char description[WEATHER_DESC_LEN];      // e.g. "scattered clouds"
} weather_current_t;

// Forecast entry (3-hour interval)
typedef struct {
    int16_t temp;
    uint16_t condition_id;
    char hour_str[WEATHER_HOUR_LEN];    // "3pm", "12am" — pre-computed
    char description[WEATHER_DESC_LEN];
} weather_forecast_t;

// Full weather state
typedef struct {
    weather_current_t current;
    weather_forecast_t forecast[WEATHER_MAX_FORECAST];
    uint8_t forecast_count;
    bool synced;            // true if data received from Mac
} weather_state_t;

// Initialize weather data module
void weather_data_init(void);

// Parse weather JSON from Mac companion
void weather_data_set(const char* json);

// Getters
const weather_current_t* weather_data_get_current(void);
const weather_forecast_t* weather_data_get_forecast(uint8_t index);
uint8_t weather_data_get_forecast_count(void);
bool weather_data_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif // WEATHER_DATA_H
