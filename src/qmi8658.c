#include "qmi8658.h"

#include "config.h"
#include "trace.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"

static int g_addr = QMI8658_ADDR_PRIMARY;
static int g_big_endian = 0;

static i2c_inst_t *bus(void) {
    return I2C_INST == 1 ? i2c1 : i2c0;
}

static bool write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_write_blocking(bus(), (uint8_t)g_addr, buf, 2, false) == 2;
}

static bool read_reg(uint8_t reg, uint8_t *dst, size_t n) {
    if (i2c_write_blocking(bus(), (uint8_t)g_addr, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(bus(), (uint8_t)g_addr, dst, n, false) == (int)n;
}

static int16_t decode_sample(uint8_t b0, uint8_t b1) {
    if (g_big_endian) {
        return (int16_t)((b0 << 8) | b1);
    }
    return (int16_t)((b1 << 8) | b0);
}

bool qmi8658_init(void) {
    const int candidates[2] = {QMI8658_ADDR_PRIMARY, QMI8658_ADDR_SECONDARY};
    bool found = false;
    for (int i = 0; i < 2 && !found; i++) {
        g_addr = candidates[i];
        for (int retry = 0; retry < 6; retry++) {
            uint8_t id = 0;
            if (read_reg(0x00, &id, 1) && id == 0x05) {
                found = true;
                break;
            }
            sleep_ms(8);
        }
    }
    if (!found) {
        return false;
    }

    /* CTRL7 off, then CTRL1 auto-increment, CTRL2 ±2g at ACCEL_ODR_CODE,
     * CTRL5 low-pass off, CTRL7 accelerometer only. */
    write_reg(0x08, 0x00);
    sleep_ms(8);
    write_reg(0x02, 0x60);
    write_reg(0x03, (uint8_t)(ACCEL_ODR_CODE & 0x0F));
    write_reg(0x06, 0x00);
    write_reg(0x08, 0x01);
    sleep_ms(20);

    uint8_t raw[6] = {0};
    if (!read_reg(0x35, raw, 6)) {
        return false;
    }
    int32_t le[3];
    int32_t be[3];
    for (int a = 0; a < 3; a++) {
        le[a] = (int16_t)((raw[a * 2 + 1] << 8) | raw[a * 2]);
        be[a] = (int16_t)((raw[a * 2] << 8) | raw[a * 2 + 1]);
    }
    g_big_endian = qmi_prefer_big_endian(vec_mag_i32(le), vec_mag_i32(be));
    return true;
}

bool qmi8658_read_mg(int32_t mg[3]) {
    uint8_t raw[6];
    if (!read_reg(0x35, raw, 6)) {
        return false;
    }
    for (int a = 0; a < 3; a++) {
        int16_t counts = decode_sample(raw[a * 2], raw[a * 2 + 1]);
        mg[a] = qmi_raw_to_mg(counts);
    }
    return true;
}

int qmi8658_address(void) {
    return g_addr;
}

int qmi8658_big_endian(void) {
    return g_big_endian;
}
