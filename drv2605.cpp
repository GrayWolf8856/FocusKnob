#include "drv2605.h"
#include "lcd_config.h"
#include "driver/i2c.h"
#include <Arduino.h>

#define I2C_PORT I2C_NUM_0

// Registers
#define DRV2605_REG_STATUS      0x00
#define DRV2605_REG_MODE        0x01
#define DRV2605_REG_RTPIN       0x02
#define DRV2605_REG_LIBRARY     0x03
#define DRV2605_REG_WAVESEQ1    0x04
#define DRV2605_REG_WAVESEQ2    0x05
#define DRV2605_REG_GO          0x0C
#define DRV2605_REG_OVERDRIVE   0x0D
#define DRV2605_REG_SUSTAINPOS  0x0E
#define DRV2605_REG_SUSTAINNEG  0x0F
#define DRV2605_REG_BREAK       0x10
#define DRV2605_REG_AUDIOMAX    0x13
#define DRV2605_REG_FEEDBACK    0x1A
#define DRV2605_REG_CONTROL3    0x1D

static esp_err_t drv2605_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    esp_err_t ret = i2c_master_write_to_device(I2C_PORT, DRV2605_ADDR, buf, 2, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        Serial.printf("DRV2605 write reg 0x%02X failed: %d\n", reg, ret);
    }
    return ret;
}

static uint8_t drv2605_read_reg(uint8_t reg) {
    uint8_t val = 0;
    esp_err_t ret = i2c_master_write_read_device(I2C_PORT, DRV2605_ADDR, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        Serial.printf("DRV2605 read reg 0x%02X failed: %d\n", reg, ret);
    }
    return val;
}

// Scan I2C bus for devices
static void i2c_scan(void) {
    Serial.println("Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t dummy;
        esp_err_t ret = i2c_master_write_read_device(I2C_PORT, addr, &dummy, 0, &dummy, 0, pdMS_TO_TICKS(50));
        if (ret == ESP_OK) {
            Serial.printf("  Found device at 0x%02X\n", addr);
        }
    }
    Serial.println("I2C scan complete");
}

void haptic_init(void) {
    // Scan I2C bus first
    i2c_scan();

    // Read status to verify communication and get chip ID
    uint8_t status = drv2605_read_reg(DRV2605_REG_STATUS);
    uint8_t chipId = status >> 5;

    Serial.printf("DRV2605 status: 0x%02X, chip ID: 0x%02X\n", status, chipId);

    // Exit standby mode
    drv2605_write_reg(DRV2605_REG_MODE, 0x00);

    // No real-time playback
    drv2605_write_reg(DRV2605_REG_RTPIN, 0x00);

    // Set default waveform
    drv2605_write_reg(DRV2605_REG_WAVESEQ1, 1);  // Strong click
    drv2605_write_reg(DRV2605_REG_WAVESEQ2, 0);  // End sequence

    // No overdrive
    drv2605_write_reg(DRV2605_REG_OVERDRIVE, 0);
    drv2605_write_reg(DRV2605_REG_SUSTAINPOS, 0);
    drv2605_write_reg(DRV2605_REG_SUSTAINNEG, 0);
    drv2605_write_reg(DRV2605_REG_BREAK, 0);
    drv2605_write_reg(DRV2605_REG_AUDIOMAX, 0x64);

    // Select ERM library 1
    drv2605_write_reg(DRV2605_REG_LIBRARY, 1);

    // Configure for ERM motor (read-modify-write)
    // Turn off N_ERM_LRA bit (bit 7) for ERM mode
    uint8_t feedback = drv2605_read_reg(DRV2605_REG_FEEDBACK);
    drv2605_write_reg(DRV2605_REG_FEEDBACK, feedback & 0x7F);

    // Turn on ERM_OPEN_LOOP bit (bit 5)
    uint8_t control3 = drv2605_read_reg(DRV2605_REG_CONTROL3);
    drv2605_write_reg(DRV2605_REG_CONTROL3, control3 | 0x20);

    // Set internal trigger mode
    drv2605_write_reg(DRV2605_REG_MODE, 0x00);

    Serial.println("DRV2605 haptic driver initialized");

    // Test vibration on startup
    delay(100);
    haptic_play(EFFECT_STRONG_CLICK);
    Serial.println("DRV2605 test vibration triggered");
}

void haptic_play(uint8_t effect) {
    // Set the effect to play
    drv2605_write_reg(DRV2605_REG_WAVESEQ1, effect);
    drv2605_write_reg(DRV2605_REG_WAVESEQ2, 0);  // End sequence
    // Go!
    drv2605_write_reg(DRV2605_REG_GO, 1);
}

void haptic_click(void) {
    // Use Strong 1 for maximum feedback
    haptic_play(EFFECT_STRONG_1);
}
