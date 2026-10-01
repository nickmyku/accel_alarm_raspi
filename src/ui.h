#pragma once

#include <stdint.h>

#include "trace.h"

#define UI_W 240
#define UI_H 240

static inline uint16_t ui_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    /* GC9A01 is written little-endian from this RP2040, so swap bytes. */
    return (uint16_t)((c << 8) | (c >> 8));
}

#define UI_COLOR_X ui_rgb565(255, 72, 72)
#define UI_COLOR_Y ui_rgb565(48, 220, 112)
#define UI_COLOR_Z ui_rgb565(72, 156, 255)
#define UI_COLOR_ALARM ui_rgb565(255, 196, 40)

typedef struct {
    int32_t limit_mg;
    int32_t mag_mg;
    int32_t peak_mg;
    int32_t dyn_mg[3];
    int alarm_on;
    int imu_ok;
    int settled;
    int alarm_gpio;
} ui_status_t;

void ui_draw(uint16_t *fb, const trace_t *trace, const ui_status_t *status);
int32_t ui_scale_mg(const trace_t *trace, int32_t limit_mg);
