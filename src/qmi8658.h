#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ±2 g scale: 16384 LSB per g. */
static inline int32_t qmi_raw_to_mg(int16_t raw) {
    int32_t q = (int32_t)raw * 1000;
    if (q >= 0) {
        q += 8192;
    } else {
        q -= 8192;
    }
    return q / 16384;
}

/* Pick the byte order whose vector length is nearer to 1 g (16384 counts). */
static inline int qmi_prefer_big_endian(int32_t mag_le, int32_t mag_be) {
    const int32_t target = 16384;
    int32_t dle = mag_le - target;
    int32_t dbe = mag_be - target;
    if (dle < 0) {
        dle = -dle;
    }
    if (dbe < 0) {
        dbe = -dbe;
    }
    return dbe + 2000 < dle;
}

/* I2C must already be configured. Probes 0x6B then 0x6A. */
bool qmi8658_init(void);
bool qmi8658_read_mg(int32_t mg[3]);
int qmi8658_address(void);
int qmi8658_big_endian(void);
