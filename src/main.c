#include "config.h"
#include "cst816s.h"
#include "gc9a01.h"
#include "qmi8658.h"
#include "settings.h"
#include "trace.h"
#include "ui.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <string.h>

static trace_t g_trace;
static alarm_t g_alarm;
static mutex_t g_i2c_lock;
static uint16_t g_fb[UI_W * UI_H];

static volatile int32_t g_limit_mg;
static volatile int32_t g_mag_mg;
static volatile int32_t g_peak_mg;
static volatile int32_t g_dyn_mg[3];
static volatile int g_alarm_on;
static volatile uint32_t g_samples;

static int32_t g_peak;
static uint32_t g_peak_at;
static uint32_t g_boot_ms;
static int g_core1_up;
static int g_dirty;
static uint32_t g_dirty_at;

static void i2c_bus_init(void) {
    gpio_init(PIN_I2C_SDA);
    gpio_init(PIN_I2C_SCL);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
    gpio_set_dir(PIN_I2C_SCL, GPIO_OUT);
    gpio_set_dir(PIN_I2C_SDA, GPIO_IN);
    gpio_put(PIN_I2C_SCL, 1);
    for (int i = 0; i < 9 && gpio_get(PIN_I2C_SDA) == 0; i++) {
        gpio_put(PIN_I2C_SCL, 0);
        sleep_us(5);
        gpio_put(PIN_I2C_SCL, 1);
        sleep_us(5);
    }
    gpio_set_dir(PIN_I2C_SDA, GPIO_OUT);
    gpio_put(PIN_I2C_SDA, 0);
    sleep_us(5);
    gpio_put(PIN_I2C_SCL, 1);
    sleep_us(5);
    gpio_put(PIN_I2C_SDA, 1);
    sleep_us(5);

    i2c_init(I2C_INST == 1 ? i2c1 : i2c0, 400 * 1000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
}

static void publish_status(const ui_status_t *blank_imu) {
    ui_status_t st;
    if (blank_imu) {
        st = *blank_imu;
    } else {
        st.limit_mg = g_limit_mg;
        st.mag_mg = g_mag_mg;
        st.peak_mg = g_peak_mg;
        st.dyn_mg[0] = g_dyn_mg[0];
        st.dyn_mg[1] = g_dyn_mg[1];
        st.dyn_mg[2] = g_dyn_mg[2];
        st.alarm_on = g_alarm_on;
        st.imu_ok = 1;
        st.settled = (to_ms_since_boot(get_absolute_time()) - g_boot_ms) >= ALARM_SETTLE_MS;
        st.alarm_gpio = ALARM_GPIO;
    }
    ui_draw(g_fb, &g_trace, &st);
    gc9a01_flush(g_fb, UI_W, UI_H);
}

static void set_limit(int32_t mg) {
    mg = limit_clamp(mg);
    if (mg == g_limit_mg) {
        return;
    }
    g_limit_mg = mg;
    g_dirty = 1;
    g_dirty_at = to_ms_since_boot(get_absolute_time());
    printf("limit %ld mg\n", (long)mg);
}

static void sample_core(void) {
    multicore_lockout_victim_init();
    absolute_time_t next = get_absolute_time();
    const uint32_t period_us = 1000000u / ACCEL_SAMPLE_HZ;
    const uint32_t t0 = to_ms_since_boot(next);

    while (1) {
        next = delayed_by_us(next, period_us);
        absolute_time_t nowt = get_absolute_time();
        if (absolute_time_diff_us(next, nowt) > 20000) {
            next = delayed_by_us(nowt, period_us);
        }

        int32_t raw[3];
        mutex_enter_blocking(&g_i2c_lock);
        bool ok = qmi8658_read_mg(raw);
        mutex_exit(&g_i2c_lock);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (ok) {
            int32_t dyn[3];
            trace_push_sample(&g_trace, now, raw, dyn);
            int32_t mag = vec_mag_i32(dyn);
            int on = 0;
            if ((uint32_t)(now - t0) >= ALARM_SETTLE_MS) {
                on = alarm_update(&g_alarm, now, mag, g_limit_mg, ALARM_HYSTERESIS_MG, ALARM_PULSE_MS);
            } else {
                alarm_reset(&g_alarm);
            }
            gpio_put(ALARM_GPIO, on);
            if (mag >= g_peak || (uint32_t)(now - g_peak_at) > PEAK_HOLD_MS) {
                g_peak = mag;
                g_peak_at = now;
            }
            g_dyn_mg[0] = dyn[0];
            g_dyn_mg[1] = dyn[1];
            g_dyn_mg[2] = dyn[2];
            g_mag_mg = mag;
            g_peak_mg = g_peak;
            g_alarm_on = on;
            g_samples++;
        }
        sleep_until(next);
    }
}

static void service_serial(void) {
    static char line[48];
    static int len = 0;
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            line[len] = 0;
            if (len > 0) {
                int value = 0;
                if (sscanf(line, "limit %d", &value) == 1) {
                    set_limit(value);
                } else if (strcmp(line, "status") == 0 || strcmp(line, "limit") == 0) {
                    printf("mag %ld mg  peak %ld mg  limit %ld mg  pin %d  X %ld Y %ld Z %ld  samples %lu\n",
                           (long)g_mag_mg, (long)g_peak_mg, (long)g_limit_mg, g_alarm_on ? 1 : 0,
                           (long)g_dyn_mg[0], (long)g_dyn_mg[1], (long)g_dyn_mg[2],
                           (unsigned long)g_samples);
                } else {
                    printf("commands: limit <mg>    status\n");
                }
            }
            len = 0;
        } else if (len < (int)sizeof line - 1) {
            line[len++] = (char)c;
        }
    }
}

static void nudge_limit(int direction) {
    set_limit(g_limit_mg + direction * ALARM_LIMIT_STEP_MG);
}

static void service_touch(void) {
    static int was_down = 0;
    static uint32_t repeat_at = 0;
    int x = 0;
    int y = 0;
    int down = 0;
    int gesture = 0;
    (void)y;

    mutex_enter_blocking(&g_i2c_lock);
    bool ok = cst816s_read(&x, &y, &down, &gesture);
    mutex_exit(&g_i2c_lock);
    if (!ok) {
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (down && !was_down) {
        if (gesture == 3) {
            nudge_limit(-1);
        } else if (gesture == 4) {
            nudge_limit(1);
        } else if (x < 100) {
            nudge_limit(-1);
        } else if (x > 140) {
            nudge_limit(1);
        }
        repeat_at = now + 500;
    } else if (down && (int32_t)(now - repeat_at) >= 0) {
        if (x < 100) {
            nudge_limit(-1);
        } else if (x > 140) {
            nudge_limit(1);
        }
        repeat_at = now + 180;
    }
    was_down = down;
}

static void service_save(void) {
    if (!g_dirty) {
        return;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((uint32_t)(now - g_dirty_at) < 2000) {
        return;
    }
    settings_save(g_limit_mg, g_core1_up);
    g_dirty = 0;
    printf("saved limit %ld mg\n", (long)g_limit_mg);
}

int main(void) {
    stdio_init_all();

    gpio_init(ALARM_GPIO);
    gpio_set_dir(ALARM_GPIO, GPIO_OUT);
    gpio_put(ALARM_GPIO, 0);

    gc9a01_init();
    i2c_bus_init();
    mutex_init(&g_i2c_lock);
    trace_init(&g_trace);
    alarm_reset(&g_alarm);
    g_boot_ms = to_ms_since_boot(get_absolute_time());

    ui_status_t waiting = {
        .limit_mg = ALARM_LIMIT_MG_DEFAULT,
        .alarm_gpio = ALARM_GPIO,
        .imu_ok = 0,
        .settled = 0,
    };
    publish_status(&waiting);

    while (!qmi8658_init()) {
        publish_status(&waiting);
        sleep_ms(400);
    }

    bool touch_ok = cst816s_init();
    g_limit_mg = settings_load();
    waiting.limit_mg = g_limit_mg;
    waiting.imu_ok = 1;

    printf("RP2040-Touch-LCD-1.28-B accelerometer\n");
    printf("QMI8658 addr 0x%02X endian %s  touch %s\n", qmi8658_address(),
           qmi8658_big_endian() ? "be" : "le", touch_ok ? "ok" : "none");
    printf("alarm GP%d idle low, pulse %d ms, limit %ld mg\n", ALARM_GPIO, ALARM_PULSE_MS,
           (long)g_limit_mg);
    printf("commands: limit <mg>    status\n");

    g_core1_up = 1;
    multicore_launch_core1(sample_core);

    while (1) {
        service_serial();
        if (touch_ok) {
            service_touch();
        }
        service_save();
        publish_status(NULL);
    }
}
