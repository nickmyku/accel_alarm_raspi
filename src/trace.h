#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#define TRACE_AXES 3

typedef struct {
    int16_t hist[TRACE_AXES][TRACE_BINS];
    /* Writer-only counters. Readers use `published` (head in the low half,
     * count in the high half), which is stored once after the bin is filled. */
    uint32_t count;
    uint32_t head;
    volatile uint32_t published;
    int32_t baseline_q[TRACE_AXES];
    int32_t peak[TRACE_AXES];
    int32_t peak_abs[TRACE_AXES];
    uint32_t bin_start_ms;
    bool seeded;
} trace_t;

typedef struct {
    int state;
    uint32_t started_ms;
    int pin;
} alarm_t;

void trace_init(trace_t *t);

/* raw_mg is sensor acceleration including gravity. dyn_mg receives the
 * high-pass (gravity removed) sample in milli-g. */
void trace_push_sample(trace_t *t, uint32_t now_ms, const int32_t raw_mg[TRACE_AXES],
                       int32_t dyn_mg[TRACE_AXES]);

int32_t vec_mag_i32(const int32_t v[TRACE_AXES]);

/* Largest |sample| currently inside the 5-minute window. */
int32_t trace_peak_abs_mg(const trace_t *t);

/* Column 0 is the oldest edge of the 5-minute window, column ncols-1 is now.
 * Returns false when that column has no samples yet. mn/mx are milli-g. */
bool trace_column_range(const trace_t *t, int axis, int col, int ncols, int16_t *mn,
                        int16_t *mx);

void alarm_reset(alarm_t *a);

/* Returns the pin level, 1 while the momentary pulse is active. */
int alarm_update(alarm_t *a, uint32_t now_ms, int32_t mag_mg, int32_t limit_mg,
                 int32_t hyst_mg, uint32_t pulse_ms);

int32_t limit_clamp(int32_t mg);
