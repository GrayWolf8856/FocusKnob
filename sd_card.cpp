#include "sd_card.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"

// SD Card pins for ESP32-S3-Knob-Touch-LCD-1.8 (from Waveshare demo)
#define SDMMC_CMD_PIN   (gpio_num_t)3
#define SDMMC_D0_PIN    (gpio_num_t)5
#define SDMMC_D1_PIN    (gpio_num_t)6
#define SDMMC_D2_PIN    (gpio_num_t)42
#define SDMMC_D3_PIN    (gpio_num_t)2
#define SDMMC_CLK_PIN   (gpio_num_t)4

#define SD_MOUNT_POINT "/sdcard"

static sdmmc_card_t *sd_card = NULL;
static bool sd_mounted = false;

bool sd_card_init(void) {
    Serial.println("SD Card: Initializing SDMMC...");

    // Exact settings from Waveshare Arduino demo
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 512,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = SDMMC_CLK_PIN;
    slot_config.cmd = SDMMC_CMD_PIN;
    slot_config.d0 = SDMMC_D0_PIN;
    slot_config.d1 = SDMMC_D1_PIN;
    slot_config.d2 = SDMMC_D2_PIN;
    slot_config.d3 = SDMMC_D3_PIN;

    Serial.println("SD Card: Attempting mount...");

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);

    if (sd_card != NULL) {
        sdmmc_card_print_info(stdout, sd_card);
        Serial.printf("SD Card: Capacity: %.2f GB\n", sd_card_get_capacity_gb());

        if (ret == ESP_OK) {
            sd_mounted = true;
            Serial.println("SD Card: Mounted successfully!");
            return true;
        } else {
            Serial.printf("SD Card: Card detected but filesystem mount failed (0x%x: %s)\n", ret, esp_err_to_name(ret));
        }
    } else {
        Serial.printf("SD Card: No card detected (0x%x: %s)\n", ret, esp_err_to_name(ret));
    }

    sd_mounted = false;
    return false;
}

bool sd_card_is_mounted(void) {
    return sd_mounted && (sd_card != NULL);
}

float sd_card_get_capacity_gb(void) {
    if (sd_card != NULL) {
        return (float)(sd_card->csd.capacity) / 2048.0f / 1024.0f;
    }
    return 0.0f;
}

esp_err_t sd_card_write_file(const char *path, const char *data) {
    if (!sd_card_is_mounted()) {
        Serial.println("SD Card: Not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, "w");
    if (f == NULL) {
        Serial.printf("SD Card: Failed to open %s for writing\n", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    fprintf(f, "%s", data);
    fclose(f);
    return ESP_OK;
}

esp_err_t sd_card_append_file(const char *path, const char *data) {
    if (!sd_card_is_mounted()) {
        Serial.println("SD Card: Not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, "a");
    if (f == NULL) {
        Serial.printf("SD Card: Failed to open %s for appending\n", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    fprintf(f, "%s", data);
    fclose(f);
    return ESP_OK;
}

esp_err_t sd_card_read_file(const char *path, char *buffer, uint32_t buffer_size, uint32_t *bytes_read) {
    if (!sd_card_is_mounted()) {
        Serial.println("SD Card: Not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        Serial.printf("SD Card: Failed to open %s for reading\n", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint32_t to_read = (file_size < (long)(buffer_size - 1)) ? file_size : (buffer_size - 1);
    uint32_t read_bytes = fread(buffer, 1, to_read, f);
    buffer[read_bytes] = '\0';

    if (bytes_read != NULL) {
        *bytes_read = read_bytes;
    }

    fclose(f);
    return ESP_OK;
}

bool sd_card_file_exists(const char *path) {
    if (!sd_card_is_mounted()) {
        return false;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t sd_card_delete_file(const char *path) {
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    if (unlink(full_path) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

int32_t sd_card_get_file_size(const char *path) {
    if (!sd_card_is_mounted()) {
        return -1;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        return -1;
    }
    return (int32_t)st.st_size;
}
