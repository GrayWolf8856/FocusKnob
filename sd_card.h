#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize SD card
bool sd_card_init(void);

// Check if SD card is mounted
bool sd_card_is_mounted(void);

// Get SD card capacity in GB
float sd_card_get_capacity_gb(void);

// Write data to a file (overwrites existing)
esp_err_t sd_card_write_file(const char *path, const char *data);

// Append data to a file
esp_err_t sd_card_append_file(const char *path, const char *data);

// Read data from a file
esp_err_t sd_card_read_file(const char *path, char *buffer, uint32_t buffer_size, uint32_t *bytes_read);

// Check if file exists
bool sd_card_file_exists(const char *path);

// Delete a file
esp_err_t sd_card_delete_file(const char *path);

// Get file size (-1 if not found)
int32_t sd_card_get_file_size(const char *path);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H
