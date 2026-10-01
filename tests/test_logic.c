#include "qmi8658.h"
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>

static int g_failed;

static void check(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", msg);
        g_failed++;
    }
}

static void push_at(trace_t *t, uint32_t now, int32_t x, int32_t y, int32_t z, int32_t dyn[3]) {
    int32_t raw[3] = {x, y, z};
    trace_push_sample(t, now, raw, dyn);
}

static void test_units(void) {
    check(qmi_raw_to_mg(16384) == 1000, "1 g converts to 1000 mg");
    check(qmi_raw_to_mg(-16384) == -1000, "-1 g converts to -1000 mg");
    check(qmi_raw_to_mg(8192) == 500, "0.5 g converts to 500 mg");
    check(qmi_prefer_big_endian(64, 16000) == 1, "swapped gravity selects big endian");
    check(qmi_prefer_big_endian(16000, 64) == 0, "little endian kept when it is 1 g");
    check(limit_clamp(1) == ALARM_LIMIT_MIN_MG, "limit lower bound");
    check(limit_clamp(9000) == ALARM_LIMIT_MAX_MG, "limit upper bound");
    check(limit_clamp(80) == 80, "limit in range");
}

static void test_alarm_pulse(void) {
    alarm_t a;
    alarm_reset(&a);
    check(alarm_update(&a, 0, 10, 50, 15, 250) == 0, "below limit stays low");
    check(alarm_update(&a, 10, 50, 50, 15, 250) == 1, "crossing the limit drives the pin");
    check(alarm_update(&a, 100, 90, 50, 15, 250) == 1, "pin stays high during the pulse");
    check(alarm_update(&a, 30, 0, 50, 15, 250) == 1, "pulse finishes even if the shock ends");
    check(alarm_update(&a, 260, 90, 50, 15, 250) == 0, "pin returns low after the pulse");
    check(alarm_update(&a, 300, 90, 50, 15, 250) == 0, "still over limit does not retrigger");
    check(alarm_update(&a, 400, 30, 50, 15, 250) == 0, "falling below hysteresis rearms");
    check(alarm_update(&a, 410, 50, 50, 15, 250) == 1, "a later shock pulses again");
}

static int column_has_at_least(const trace_t *t, int axis, int32_t need) {
    for (int col = 0; col < 180; col++) {
        int16_t mn, mx;
        if (!trace_column_range(t, axis, col, 180, &mn, &mx)) {
            continue;
        }
        if (mx >= need || -mn >= need) {
            return 1;
        }
    }
    return 0;
}

static void test_footstep_kept(void) {
    trace_t t;
    trace_init(&t);
    int32_t dyn[3];
    push_at(&t, 0, 0, 0, 0, dyn);
    push_at(&t, 4, 0, 0, 0, dyn);
    push_at(&t, 8, 0, 0, 80, dyn);
    check(dyn[2] >= 70, "footstep sample is not swallowed by the baseline");
    push_at(&t, 12, 0, 0, 0, dyn);
    push_at(&t, 100, 0, 0, 0, dyn);
    check(column_has_at_least(&t, 2, 70), "100 ms bin keeps the footstep peak");
}

static void test_gravity_and_slow_tilt(void) {
    trace_t t;
    alarm_t a;
    trace_init(&t);
    alarm_reset(&a);
    int32_t dyn[3];
    uint32_t now = 0;
    push_at(&t, now, 0, 0, 1000, dyn);
    int alarmed = 0;
    for (int i = 0; i < 250; i++) {
        now += 4;
        push_at(&t, now, 0, 0, 1000, dyn);
        if (alarm_update(&a, now, vec_mag_i32(dyn), 50, 15, 250)) {
            alarmed = 1;
        }
    }
    check(dyn[2] > -5 && dyn[2] < 5, "static gravity is removed");
    check(!alarmed, "resting gravity does not alarm");

    now += 4;
    push_at(&t, now, 0, 0, 1080, dyn);
    check(dyn[2] >= 70, "a sudden 80 mg step still comes through");
    check(alarm_update(&a, now, vec_mag_i32(dyn), 50, 15, 250) == 1, "that step trips the alarm");

    trace_init(&t);
    alarm_reset(&a);
    now = 0;
    int32_t z = 1000;
    push_at(&t, now, 0, 0, z, dyn);
    alarmed = 0;
    int32_t worst = 0;
    /* About 80 mg/s of tilt: +1 mg every third sample. */
    for (int i = 0; i < 2000; i++) {
        now += 4;
        if ((i % 3) == 2) {
            z += 1;
        }
        push_at(&t, now, 0, 0, z, dyn);
        int32_t mag = vec_mag_i32(dyn);
        if (i > 1000 && mag > worst) {
            worst = mag;
        }
        if (i > 500 && alarm_update(&a, now, mag, 50, 15, 250)) {
            alarmed = 1;
        }
    }
    check(worst < 40, "slow tilt stays under the footstep threshold");
    check(!alarmed, "slow tilt does not hold the alarm");
}

static void test_five_minute_column(void) {
    trace_t t;
    trace_init(&t);
    int32_t dyn[3];
    push_at(&t, 0, 0, 0, 0, dyn);
    for (int i = 0; i < TRACE_BINS; i++) {
        int32_t x = (i == 1499) ? 90 : 0;
        push_at(&t, (uint32_t)(i + 1) * TRACE_BIN_MS, x, 0, 0, dyn);
    }
    check(t.count == TRACE_BINS, "window holds 5 minutes of bins");
    int hot = 0;
    int hot_col = -1;
    for (int col = 0; col < 180; col++) {
        int16_t mn, mx;
        check(trace_column_range(&t, 0, col, 180, &mn, &mx), "full window has every column");
        if (mx >= 80) {
            hot++;
            hot_col = col;
        }
    }
    check(hot == 1, "one column carries the spike");
    check(hot_col > 40 && hot_col < 120, "a spike 2.5 minutes ago is near the middle");
    int16_t mn, mx;
    check(trace_column_range(&t, 0, 179, 180, &mn, &mx), "newest column exists");
    check(mx < 20 && mn > -20, "newest column is the quiet present");
}

int main(void) {
    test_units();
    test_alarm_pulse();
    test_footstep_kept();
    test_gravity_and_slow_tilt();
    test_five_minute_column();
    if (g_failed) {
        fprintf(stderr, "%d checks failed\n", g_failed);
        return 1;
    }
    printf("all logic checks passed\n");
    return 0;
}
