#include "weather_data.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>

static weather_state_t g_weather_state;

void weather_data_init(void) {
    memset(&g_weather_state, 0, sizeof(g_weather_state));
    g_weather_state.synced = false;
    Serial.println("WeatherData: Initialized");
}

void weather_data_set(const char* json) {
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.printf("WeatherData: JSON parse error: %s\n", err.c_str());
        return;
    }

    // Parse current weather
    JsonObject current = doc["current"];
    g_weather_state.current.temp = (int16_t)current["temp"].as<float>();
    g_weather_state.current.temp_min = (int16_t)current["temp_min"].as<float>();
    g_weather_state.current.temp_max = (int16_t)current["temp_max"].as<float>();
    g_weather_state.current.humidity = current["humidity"] | 0;
    g_weather_state.current.wind_speed = current["wind_speed"] | 0;
    g_weather_state.current.condition_id = current["condition_id"] | 800;

    const char* condition = current["condition"] | "Unknown";
    strncpy(g_weather_state.current.condition, condition, WEATHER_CONDITION_LEN - 1);
    g_weather_state.current.condition[WEATHER_CONDITION_LEN - 1] = '\0';

    const char* desc = current["description"] | "";
    strncpy(g_weather_state.current.description, desc, WEATHER_DESC_LEN - 1);
    g_weather_state.current.description[WEATHER_DESC_LEN - 1] = '\0';

    // Parse forecast array
    JsonArray forecast_arr = doc["forecast"];
    g_weather_state.forecast_count = 0;

    if (!forecast_arr.isNull()) {
        for (JsonObject entry : forecast_arr) {
            if (g_weather_state.forecast_count >= WEATHER_MAX_FORECAST) break;

            weather_forecast_t *f = &g_weather_state.forecast[g_weather_state.forecast_count];
            f->temp = (int16_t)entry["temp"].as<float>();
            f->condition_id = entry["condition_id"] | 800;

            const char* hour = entry["hour_str"] | "";
            strncpy(f->hour_str, hour, WEATHER_HOUR_LEN - 1);
            f->hour_str[WEATHER_HOUR_LEN - 1] = '\0';

            const char* fdesc = entry["description"] | "";
            strncpy(f->description, fdesc, WEATHER_DESC_LEN - 1);
            f->description[WEATHER_DESC_LEN - 1] = '\0';

            g_weather_state.forecast_count++;
        }
    }

    g_weather_state.synced = true;

    Serial.printf("WeatherData: Updated, %d deg, %d forecast entries\n",
                  g_weather_state.current.temp, g_weather_state.forecast_count);
}

const weather_current_t* weather_data_get_current(void) {
    return &g_weather_state.current;
}

const weather_forecast_t* weather_data_get_forecast(uint8_t index) {
    if (index >= g_weather_state.forecast_count) return NULL;
    return &g_weather_state.forecast[index];
}

uint8_t weather_data_get_forecast_count(void) {
    return g_weather_state.forecast_count;
}

bool weather_data_is_synced(void) {
    return g_weather_state.synced;
}
