#include "trace.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>

static uint32_t g_rnd = 1;

static int noise(void) {
    g_rnd = g_rnd * 1664525u + 1013904223u;
    return (int)((g_rnd >> 24) & 7) - 3;
}

static void write_ppm(const char *path, const uint16_t *fb) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", UI_W, UI_H);
    for (int i = 0; i < UI_W * UI_H; i++) {
        uint16_t c = (uint16_t)((fb[i] << 8) | (fb[i] >> 8));
        uint8_t rgb[3];
        rgb[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        rgb[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        rgb[2] = (uint8_t)((c & 31) * 255 / 31);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "build/preview.ppm";
    trace_t trace;
    alarm_t alarm;
    trace_init(&trace);
    alarm_reset(&alarm);

    int32_t dyn[3] = {0, 0, 0};
    int32_t peak = 0;
    uint32_t peak_at = 0;
    int alarm_on = 0;
    const int32_t limit = 50;

    for (uint32_t ms = 0; ms <= 300000; ms += 4) {
        int32_t x = noise();
        int32_t y = noise();
        int32_t z = 1000 + noise();
        uint32_t into = ms % 12000;
        int late = (ms + 150 >= 300000);
        if ((into < 48 && ms > 8000) || late) {
            int phase = (int)((ms / 12000) % 3);
            if (late) {
                phase = 2;
            }
            if (phase == 0) {
                x += 80;
                y += 25;
                z += 30;
            } else if (phase == 1) {
                x -= 20;
                y += 85;
                z += 25;
            } else {
                x += 30;
                y -= 35;
                z += 80;
            }
        }
        int32_t raw[3] = {x, y, z};
        trace_push_sample(&trace, ms, raw, dyn);
        int32_t mag = vec_mag_i32(dyn);
        if (ms >= 1500) {
            alarm_on = alarm_update(&alarm, ms, mag, limit, ALARM_HYSTERESIS_MG, ALARM_PULSE_MS);
        }
        if (mag >= peak || ms - peak_at > PEAK_HOLD_MS) {
            peak = mag;
            peak_at = ms;
        }
    }

    int hot = 0;
    for (int col = 0; col < 180; col++) {
        for (int axis = 0; axis < 3; axis++) {
            int16_t mn, mx;
            if (trace_column_range(&trace, axis, col, 180, &mn, &mx) && (mx >= 55 || -mn >= 55)) {
                hot++;
                break;
            }
        }
    }
    if (hot < 15) {
        fprintf(stderr, "expected many footstep columns, saw %d\n", hot);
        return 1;
    }
    if (!alarm_on) {
        fprintf(stderr, "preview should end inside an alarm pulse\n");
        return 1;
    }

    ui_status_t st = {
        .limit_mg = limit,
        .mag_mg = vec_mag_i32(dyn),
        .peak_mg = peak,
        .dyn_mg = {dyn[0], dyn[1], dyn[2]},
        .alarm_on = alarm_on,
        .imu_ok = 1,
        .settled = 1,
        .alarm_gpio = ALARM_GPIO,
    };
    uint16_t *fb = calloc((size_t)UI_W * UI_H, sizeof(uint16_t));
    if (!fb) {
        return 1;
    }
    ui_draw(fb, &trace, &st);

    int z_pixels = 0;
    int x_pixels = 0;
    int y_pixels = 0;
    int alarm_pixels = 0;
    int leak = 0;
    uint16_t zc = UI_COLOR_Z;
    uint16_t xc = UI_COLOR_X;
    uint16_t yc = UI_COLOR_Y;
    uint16_t alarm_c = UI_COLOR_ALARM;
    uint16_t red = ui_rgb565(255, 64, 64);
    for (int y = 0; y < UI_H; y++) {
        for (int x = 0; x < UI_W; x++) {
            uint16_t c = fb[y * UI_W + x];
            int dx = x - 120;
            int dy = y - 120;
            if (dx * dx + dy * dy > 116 * 116) {
                if (c != 0) {
                    leak++;
                }
                continue;
            }
            if (c == zc) {
                z_pixels++;
            } else if (c == xc) {
                x_pixels++;
            } else if (c == yc) {
                y_pixels++;
            } else if (c == alarm_c) {
                alarm_pixels++;
            }
        }
    }
    if (leak) {
        fprintf(stderr, "drew %d pixels outside the round glass\n", leak);
        return 1;
    }
    if (z_pixels < 400 || x_pixels < 200 || y_pixels < 200) {
        fprintf(stderr, "trace is missing from the frame (X %d Y %d Z %d)\n", x_pixels, y_pixels,
                z_pixels);
        return 1;
    }
    if (alarm_pixels < 40) {
        fprintf(stderr, "alarm limit is missing from the frame (%d pixels)\n", alarm_pixels);
        return 1;
    }
    if (fb[6 * UI_W + 120] != red && fb[7 * UI_W + 120] != red) {
        fprintf(stderr, "alarm bezel is not drawn on the top of the glass\n");
        return 1;
    }

    write_ppm(path, fb);
    free(fb);
    printf("wrote %s  footstep columns %d  peak %ld mg\n", path, hot, (long)peak);
    return 0;
}
