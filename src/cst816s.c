#include "cst816s.h"

#include "config.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#define CST816_ADDR 0x15

static i2c_inst_t *bus(void) {
    return I2C_INST == 1 ? i2c1 : i2c0;
}

static bool write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_write_blocking(bus(), CST816_ADDR, buf, 2, false) == 2;
}

static bool read_reg(uint8_t reg, uint8_t *dst, size_t n) {
    if (i2c_write_blocking(bus(), CST816_ADDR, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(bus(), CST816_ADDR, dst, n, false) == (int)n;
}

bool cst816s_init(void) {
    gpio_init(PIN_TOUCH_RST);
    gpio_set_dir(PIN_TOUCH_RST, GPIO_OUT);
    gpio_init(PIN_TOUCH_INT);
    gpio_set_dir(PIN_TOUCH_INT, GPIO_IN);
    gpio_pull_up(PIN_TOUCH_INT);

    gpio_put(PIN_TOUCH_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_TOUCH_RST, 1);
    sleep_ms(50);

    uint8_t id = 0;
    if (!read_reg(0xA7, &id, 1)) {
        return false;
    }
    /* 0xFE disables auto-sleep. 0xFA = IrqCtl, 0x41 is point mode. */
    write_reg(0xFE, 0x01);
    write_reg(0xFA, 0x41);
    return id == 0xB4 || id == 0xB5 || id == 0xB6;
}

bool cst816s_read(int *x, int *y, int *down, int *gesture) {
    uint8_t buf[6];
    if (!read_reg(0x01, buf, 6)) {
        return false;
    }
    *gesture = buf[0];
    *down = buf[1] > 0;
    *x = ((buf[2] & 0x0F) << 8) | buf[3];
    *y = ((buf[4] & 0x0F) << 8) | buf[5];
    if (TOUCH_MIRROR_X) {
        *x = 239 - *x;
    }
    return true;
}
