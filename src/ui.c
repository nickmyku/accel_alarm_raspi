#include "ui.h"

#include "font5x7.h"

#include <stdio.h>
#include <string.h>

/*
 * 240x240 round glass. Content is clipped to CLIP_R so the dead corners
 * of the square panel stay black. The trace sits in the wide middle band,
 * whose left and right edges are the circle itself. Minus and plus are
 * round keys in the side bulges, on the same left/right touch zones.
 * The limit readout occupies the lower cap, where the chord is still wide
 * enough for a large numeral; the live axis values occupy the upper cap.
 */
enum {
    CX = 120,
    CY = 120,
    CLIP_R = 116,
    BAND_R = 111,
    BAND_TOP = 36,
    BAND_BOT = 158,
    PLOT_X = 46,
    PLOT_W = 148,
    PLOT_Y = 47,
    PLOT_H = 108,
    BTN_R = 14,
    BTN_LX = 27,
    BTN_RX = 213
};

static void px(uint16_t *fb, int x, int y, uint16_t color) {
    if ((unsigned)x >= UI_W || (unsigned)y >= UI_H) {
        return;
    }
    int dx = x - CX;
    int dy = y - CY;
    if (dx * dx + dy * dy > CLIP_R * CLIP_R) {
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

static void draw_glyph_centered(uint16_t *fb, int cx, int cy, char ch, uint16_t color, int scale) {
    char s[2] = {ch, 0};
    int w = 5 * scale;
    int h = 7 * scale;
    draw_text(fb, cx - w / 2, cy - h / 2, s, color, scale);
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

static void ring(uint16_t *fb, int cx, int cy, int r, uint16_t color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        px(fb, cx + x, cy + y, color);
        px(fb, cx - x, cy + y, color);
        px(fb, cx + x, cy - y, color);
        px(fb, cx - x, cy - y, color);
        px(fb, cx + y, cy + x, color);
        px(fb, cx - y, cy + x, color);
        px(fb, cx + y, cy - x, color);
        px(fb, cx - y, cy - x, color);
        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
}

static void fill_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color) {
    int r2 = r * r;
    for (int y = -r; y <= r; y++) {
        int span = 0;
        int y2 = y * y;
        while ((span + 1) * (span + 1) + y2 <= r2) {
            span++;
        }
        hline(fb, cx - span, cx + span, cy + y, color);
    }
}

static void chord(uint16_t *fb, int y, int radius, uint16_t color) {
    int dy = y - CY;
    int room = radius * radius - dy * dy;
    if (room < 0) {
        return;
    }
    int dx = 0;
    while ((dx + 1) * (dx + 1) <= room) {
        dx++;
    }
    hline(fb, CX - dx, CX + dx, y, color);
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
    int32_t half = PLOT_H / 2;
    int32_t mid = PLOT_Y + half;
    int32_t y = mid - (v * half) / scale;
    if (y < PLOT_Y) {
        y = PLOT_Y;
    }
    if (y > PLOT_Y + PLOT_H - 1) {
        y = PLOT_Y + PLOT_H - 1;
    }
    return (int)y;
}

static void span_of(const trace_t *trace, int axis, int col, int32_t scale, int *x, int *y0, int *y1) {
    int16_t mn, mx;
    if (!trace_column_range(trace, axis, col, PLOT_W, &mn, &mx)) {
        *x = -1;
        return;
    }
    *y0 = value_to_y(mn, scale);
    *y1 = value_to_y(mx, scale);
    *x = PLOT_X + col;
}

static void draw_traces(uint16_t *fb, const trace_t *trace, int32_t scale) {
    const uint16_t color[3] = {UI_COLOR_X, UI_COLOR_Y, UI_COLOR_Z};
    /* Z, then Y, then X, so a taller axis still shows beyond a shorter one. */
    for (int axis = 2; axis >= 0; axis--) {
        for (int col = 0; col < PLOT_W; col++) {
            int x, y0, y1;
            span_of(trace, axis, col, scale, &x, &y0, &y1);
            if (x < 0) {
                continue;
            }
            vline(fb, x, y0, y1, color[axis]);
        }
    }
    for (int axis = 0; axis < 3; axis++) {
        for (int col = 0; col < PLOT_W; col++) {
            int x, y0, y1;
            span_of(trace, axis, col, scale, &x, &y0, &y1);
            if (x < 0) {
                continue;
            }
            px(fb, x, y0, color[axis]);
            px(fb, x, y1, color[axis]);
            if (x + 1 < PLOT_X + PLOT_W) {
                px(fb, x + 1, y0, color[axis]);
                px(fb, x + 1, y1, color[axis]);
            }
        }
    }
}

static void draw_dashed_h(uint16_t *fb, int y, uint16_t color) {
    for (int x = PLOT_X; x < PLOT_X + PLOT_W; x++) {
        if (((x / 3) & 1) == 0) {
            px(fb, x, y, color);
        }
    }
}

static void draw_key(uint16_t *fb, int cx, int cy, char glyph, uint16_t face, uint16_t ink,
                     uint16_t ring_color) {
    fill_circle(fb, cx, cy, BTN_R, face);
    ring(fb, cx, cy, BTN_R, ring_color);
    ring(fb, cx, cy, BTN_R - 1, ring_color);
    draw_glyph_centered(fb, cx, cy, glyph, ink, 2);
}

void ui_draw(uint16_t *fb, const trace_t *trace, const ui_status_t *status) {
    const uint16_t bg = ui_rgb565(5, 8, 14);
    const uint16_t plot = ui_rgb565(14, 22, 38);
    const uint16_t grid = ui_rgb565(42, 54, 72);
    const uint16_t zero = ui_rgb565(86, 98, 118);
    const uint16_t text = ui_rgb565(226, 230, 236);
    const uint16_t dim = ui_rgb565(132, 144, 160);
    const uint16_t red = ui_rgb565(255, 64, 64);
    const uint16_t steel = ui_rgb565(168, 178, 194);
    const uint16_t key = ui_rgb565(18, 28, 46);
    const int clip2 = CLIP_R * CLIP_R;
    const int band2 = BAND_R * BAND_R;
    const uint16_t bezel = status->alarm_on ? red : steel;

    for (int y = 0; y < UI_H; y++) {
        int dy = y - CY;
        int in_band = y >= BAND_TOP && y < BAND_BOT;
        for (int x = 0; x < UI_W; x++) {
            int dx = x - CX;
            int r2 = dx * dx + dy * dy;
            uint16_t c = 0;
            if (r2 <= clip2) {
                c = (in_band && r2 <= band2) ? plot : bg;
            }
            fb[y * UI_W + x] = c;
        }
    }

    chord(fb, BAND_TOP, BAND_R, grid);
    chord(fb, BAND_BOT - 1, BAND_R, grid);

    for (int m = 1; m < 5; m++) {
        int gx = PLOT_X + (m * PLOT_W) / 5;
        vline(fb, gx, PLOT_Y, PLOT_Y + PLOT_H - 1, grid);
    }
    int mid = PLOT_Y + PLOT_H / 2;
    hline(fb, PLOT_X, PLOT_X + PLOT_W - 1, mid, zero);

    int32_t scale = ui_scale_mg(trace, status->limit_mg);
    if (status->imu_ok) {
        draw_traces(fb, trace, scale);
        int y_lim = value_to_y(status->limit_mg, scale);
        int y_neg = value_to_y(-status->limit_mg, scale);
        draw_dashed_h(fb, y_lim, UI_COLOR_ALARM);
        draw_dashed_h(fb, y_neg, UI_COLOR_ALARM);
    } else {
        draw_centered(fb, CX, mid - 4, "IMU NOT FOUND", red, 1);
    }

    char buf[24];
    snprintf(buf, sizeof buf, "+%ld", (long)scale);
    draw_text(fb, PLOT_X + PLOT_W - text_px(buf, 1) - 2, BAND_TOP + 3, buf, dim, 1);
    draw_text(fb, PLOT_X + 2, BAND_BOT + 4, "-5m", dim, 1);
    draw_text(fb, PLOT_X + PLOT_W - text_px("now", 1) - 2, BAND_BOT + 4, "now", dim, 1);

    draw_key(fb, BTN_LX, mid, '-', key, text, steel);
    draw_key(fb, BTN_RX, mid, '+', key, text, steel);

    char axis_txt[3][12];
    const uint16_t axis_color[3] = {UI_COLOR_X, UI_COLOR_Y, UI_COLOR_Z};
    const char *axis_name[3] = {"X", "Y", "Z"};
    int glyphs = 0;
    for (int a = 0; a < 3; a++) {
        snprintf(axis_txt[a], sizeof axis_txt[a], "%s%+ld", axis_name[a], (long)status->dyn_mg[a]);
        glyphs += text_px(axis_txt[a], 1);
    }
    /* The top of the glass is narrow. Pull the three readings together
     * when a large value would otherwise meet the bezel. */
    int gap = (glyphs + 12 > 110) ? 2 : 6;
    int x = CX - (glyphs + 2 * gap) / 2;
    for (int a = 0; a < 3; a++) {
        x = draw_text(fb, x, 26, axis_txt[a], axis_color[a], 1);
        if (a != 2) {
            x += gap;
        }
    }

    if (!status->settled && status->imu_ok) {
        draw_centered(fb, CX, 176, "SETTLING", dim, 1);
    } else {
        draw_centered(fb, CX, 176, "LIM", dim, 1);
    }

    snprintf(buf, sizeof buf, "%ld", (long)status->limit_mg);
    uint16_t lim_color = status->alarm_on ? red : UI_COLOR_ALARM;
    draw_centered(fb, CX, 186, buf, lim_color, 3);

    snprintf(buf, sizeof buf, "PK %ld", (long)status->peak_mg);
    uint16_t pk_color = (status->mag_mg >= status->limit_mg) ? red : text;
    int pk_x = CX - text_px(buf, 1) - 6;
    draw_text(fb, pk_x, 212, buf, pk_color, 1);
    if (status->alarm_on) {
        draw_text(fb, CX + 8, 212, "PULSE", red, 1);
    } else {
        snprintf(buf, sizeof buf, "GP%d", status->alarm_gpio);
        draw_text(fb, CX + 8, 212, buf, dim, 1);
    }

    ring(fb, CX, CY, CLIP_R - 2, bezel);
    ring(fb, CX, CY, CLIP_R - 3, bezel);
    if (status->alarm_on) {
        ring(fb, CX, CY, CLIP_R - 4, red);
    }
}
