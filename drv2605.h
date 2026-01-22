#ifndef DRV2605_H
#define DRV2605_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// DRV2605 I2C address
#define DRV2605_ADDR 0x5A

// Registers
#define DRV2605_REG_STATUS      0x00
#define DRV2605_REG_MODE        0x01
#define DRV2605_REG_WAVESEQ1    0x04
#define DRV2605_REG_WAVESEQ2    0x05
#define DRV2605_REG_GO          0x0C
#define DRV2605_REG_OVERDRIVE   0x0D
#define DRV2605_REG_SUSTAINPOS  0x0E
#define DRV2605_REG_SUSTAINNEG  0x0F
#define DRV2605_REG_BREAK       0x10
#define DRV2605_REG_FEEDBACK    0x1A
#define DRV2605_REG_CONTROL3    0x1D

// Useful effects (1-123 available)
#define EFFECT_STRONG_CLICK     1
#define EFFECT_STRONG_CLICK_60  2
#define EFFECT_STRONG_CLICK_30  3
#define EFFECT_SHARP_CLICK      4
#define EFFECT_SHARP_CLICK_60   5
#define EFFECT_SOFT_BUMP        7
#define EFFECT_DOUBLE_CLICK     10
#define EFFECT_TRIPLE_CLICK     12
#define EFFECT_SOFT_FUZZ        13
#define EFFECT_STRONG_BUZZ      14
#define EFFECT_ALERT_750MS      15
#define EFFECT_ALERT_1000MS     16
#define EFFECT_STRONG_1         17
#define EFFECT_STRONG_2         18
#define EFFECT_STRONG_3         19
#define EFFECT_SHARP_TICK       27
#define EFFECT_SHORT_DOUBLE_SHARP_TICK 28
#define EFFECT_LIGHT_CLICK      49

// Initialize DRV2605 haptic driver
void haptic_init(void);

// Play a haptic effect (1-123)
void haptic_play(uint8_t effect);

// Quick haptic for button press
void haptic_click(void);

#ifdef __cplusplus
}
#endif

#endif
