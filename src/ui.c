#include "ui.h"

#include "font5x7.h"

#include <stdio.h>
#include <string.h>

enum {
    GRAPH_X = 42,
    GRAPH_Y = 46,
    GRAPH_W = 156,
    GRAPH_H = 100
};

static void px(uint16_t *fb, int x, int y, uint16_t color) {
    if ((unsigned)x >= UI_W || (unsigned)y >= UI_H) {
        return;
    }
    int dx = x - 120;
    int dy = y - 120;
    if (dx * dx + dy * dy > 116 * 116) {
        return;
    }
    fb[y * UI_W + x] = color;
}

static int draw_text(uint16_t *fb, int x, int y, const char *s, uint16_t color, int scale) {
    int advance = 6 * scale;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        const uint8_t *g = font5x7 + (unsigned)c * 5;
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & 1u) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            px(fb, x + col * scale + sx, y + row * scale + sy, color);
                        }
                    }
                }
                bits >>= 1;
            }
        }
        x += advance;
    }
    return x;
}

static int text_px(const char *s, int scale) {
    return (int)strlen(s) * 6 * scale;
}

static void draw_centered(uint16_t *fb, int cx, int y, const char *s, uint16_t color, int scale) {
    draw_text(fb, cx - text_px(s, scale) / 2, y, s, color, scale);
}

static void hline(uint16_t *fb, int x0, int x1, int y, uint16_t color) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++) {
        px(fb, x, y, color);
    }
}

static void vline(uint16_t *fb, int x, int y0, int y1, uint16_t color) {
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++) {
        px(fb, x, y, color);
    }
}

static void fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            px(fb, xx, yy, color);
        }
    }
}

int32_t ui_scale_mg(const trace_t *trace, int32_t limit_mg) {
    int32_t need = GRAPH_MIN_SCALE_MG;
    int32_t peak = trace_peak_abs_mg(trace);
    if (peak > need) {
        need = peak;
    }
    if (limit_mg > need) {
        need = limit_mg;
    }
    need += need / 8;
    if (need < GRAPH_MIN_SCALE_MG) {
        need = GRAPH_MIN_SCALE_MG;
    }
    need = (need + 4) / 5 * 5;
    if (need < 1) {
        need = 1;
    }
    return need;
}

static int value_to_y(int32_t v, int32_t scale) {
    int32_t half = GRAPH_H / 2;
    int32_t mid = GRAPH_Y + half;
    int32_t y = mid - (v * half) / scale;
    if (y < GRAPH_Y) {
        y = GRAPH_Y;
    }
    if (y > GRAPH_Y + GRAPH_H - 1) {
        y = GRAPH_Y + GRAPH_H - 1;
    }
    return (int)y;
}

static void span_of(const trace_t *trace, int axis, int col, int32_t scale, int *x, int *y0, int *y1) {
    int16_t mn, mx;
    if (!trace_column_range(trace, axis, col, GRAPH_W, &mn, &mx)) {
        *x = -1;
        return;
    }
    *y0 = value_to_y(mn, scale);
    *y1 = value_to_y(mx, scale);
    *x = GRAPH_X + col;
}

static void draw_traces(uint16_t *fb, const trace_t *trace, int32_t scale) {
    const uint16_t color[3] = {UI_COLOR_X, UI_COLOR_Y, UI_COLOR_Z};
    /* Z, then Y, then X, so a taller axis still shows beyond a shorter one. */
    for (int axis = 2; axis >= 0; axis--) {
        for (int col = 0; col < GRAPH_W; col++) {
            int x, y0, y1;
            span_of(trace, axis, col, scale, &x, &y0, &y1);
            if (x < 0) {
                continue;
            }
            vline(fb, x, y0, y1, color[axis]);
        }
    }
    /* A cap in each axis color marks that axis even where the fills overlap. */
    for (int axis = 0; axis < 3; axis++) {
        for (int col = 0; col < GRAPH_W; col++) {
            int x, y0, y1;
            span_of(trace, axis, col, scale, &x, &y0, &y1);
            if (x < 0) {
                continue;
            }
            px(fb, x, y0, color[axis]);
            px(fb, x, y1, color[axis]);
            if (x + 1 < GRAPH_X + GRAPH_W) {
                px(fb, x + 1, y0, color[axis]);
                px(fb, x + 1, y1, color[axis]);
            }
        }
    }
}

static void draw_dashed_h(uint16_t *fb, int y, uint16_t color) {
    for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x++) {
        if (((x / 3) & 1) == 0) {
            px(fb, x, y, color);
        }
    }
}

void ui_draw(uint16_t *fb, const trace_t *trace, const ui_status_t *status) {
    const uint16_t bg = ui_rgb565(5, 8, 14);
    const uint16_t plot = ui_rgb565(10, 14, 24);
    const uint16_t grid = ui_rgb565(38, 48, 64);
    const uint16_t zero = ui_rgb565(78, 90, 110);
    const uint16_t text = ui_rgb565(226, 230, 236);
    const uint16_t dim = ui_rgb565(132, 144, 160);
    const uint16_t red = ui_rgb565(255, 64, 64);
    const int r2 = 116 * 116;

    for (int y = 0; y < UI_H; y++) {
        int dy = y - 120;
        for (int x = 0; x < UI_W; x++) {
            int dx = x - 120;
            fb[y * UI_W + x] = (dx * dx + dy * dy <= r2) ? bg : 0;
        }
    }

    draw_centered(fb, 120, 14, "5 MIN ACCEL", text, 1);

    char buf[24];
    char axis_txt[3][12];
    const uint16_t axis_color[3] = {UI_COLOR_X, UI_COLOR_Y, UI_COLOR_Z};
    const char *axis_name[3] = {"X", "Y", "Z"};
    int axis_w = 0;
    for (int a = 0; a < 3; a++) {
        snprintf(axis_txt[a], sizeof axis_txt[a], "%s%+ld", axis_name[a], (long)status->dyn_mg[a]);
        axis_w += text_px(axis_txt[a], 1);
        if (a != 2) {
            axis_w += 8;
        }
    }
    int x = 120 - axis_w / 2;
    for (int a = 0; a < 3; a++) {
        x = draw_text(fb, x, 28, axis_txt[a], axis_color[a], 1);
        x += 8;
    }

    uint16_t frame = status->alarm_on ? red : grid;
    fill_rect(fb, GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, plot);
    hline(fb, GRAPH_X, GRAPH_X + GRAPH_W - 1, GRAPH_Y, frame);
    hline(fb, GRAPH_X, GRAPH_X + GRAPH_W - 1, GRAPH_Y + GRAPH_H - 1, frame);
    vline(fb, GRAPH_X, GRAPH_Y, GRAPH_Y + GRAPH_H - 1, frame);
    vline(fb, GRAPH_X + GRAPH_W - 1, GRAPH_Y, GRAPH_Y + GRAPH_H - 1, frame);

    for (int m = 1; m < 5; m++) {
        int gx = GRAPH_X + (m * GRAPH_W) / 5;
        vline(fb, gx, GRAPH_Y + 1, GRAPH_Y + GRAPH_H - 2, grid);
    }
    int mid = GRAPH_Y + GRAPH_H / 2;
    hline(fb, GRAPH_X + 1, GRAPH_X + GRAPH_W - 2, mid, zero);

    int32_t scale = ui_scale_mg(trace, status->limit_mg);
    if (status->imu_ok) {
        int y_lim = value_to_y(status->limit_mg, scale);
        int y_neg = value_to_y(-status->limit_mg, scale);
        draw_dashed_h(fb, y_lim, UI_COLOR_ALARM);
        draw_dashed_h(fb, y_neg, UI_COLOR_ALARM);
        draw_traces(fb, trace, scale);
    } else {
        draw_centered(fb, 120, GRAPH_Y + GRAPH_H / 2 - 4, "IMU NOT FOUND", red, 1);
    }

    snprintf(buf, sizeof buf, "+%ld", (long)scale);
    draw_text(fb, GRAPH_X + GRAPH_W - text_px(buf, 1) - 3, GRAPH_Y + 3, buf, dim, 1);

    int ty = GRAPH_Y + GRAPH_H + 3;
    draw_text(fb, GRAPH_X, ty, "-5m", dim, 1);
    draw_text(fb, GRAPH_X + GRAPH_W - text_px("now", 1), ty, "now", dim, 1);

    if (!status->settled && status->imu_ok) {
        draw_centered(fb, 120, 160, "SETTLING", dim, 1);
    } else {
        draw_centered(fb, 120, 158, "LIM", dim, 1);
    }

    snprintf(buf, sizeof buf, "%ld", (long)status->limit_mg);
    uint16_t lim_color = status->alarm_on ? red : UI_COLOR_ALARM;
    draw_centered(fb, 120, 168, buf, lim_color, 2);

    draw_text(fb, 48, 172, "-", text, 2);
    draw_text(fb, 180, 172, "+", text, 2);

    snprintf(buf, sizeof buf, "PK %ld", (long)status->peak_mg);
    uint16_t pk_color = (status->mag_mg >= status->limit_mg) ? red : text;
    int pk_x = 120 - text_px(buf, 1) - 8;
    draw_text(fb, pk_x, 192, buf, pk_color, 1);

    if (status->alarm_on) {
        draw_text(fb, 128, 192, "PULSE", red, 1);
    } else {
        snprintf(buf, sizeof buf, "GP%d", status->alarm_gpio);
        draw_text(fb, 128, 192, buf, dim, 1);
    }
}
