#include "trace.h"

#include <string.h>

#if defined(__ARM_ARCH)
#include "hardware/sync.h"
#define TRACE_DMB() __dmb()
#else
#define TRACE_DMB() ((void)0)
#endif

_Static_assert(TRACE_BINS == (5 * 60 * 1000) / TRACE_BIN_MS, "5 minute window");
_Static_assert(TRACE_BINS > 0 && TRACE_BINS <= 4000, "history size");

static int32_t isqrt_u64(uint64_t x) {
    uint64_t op = x;
    uint64_t res = 0;
    uint64_t one = 1ull << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (int32_t)res;
}

int32_t vec_mag_i32(const int32_t v[TRACE_AXES]) {
    uint64_t s = (uint64_t)((int64_t)v[0] * v[0]) + (uint64_t)((int64_t)v[1] * v[1]) +
                 (uint64_t)((int64_t)v[2] * v[2]);
    return isqrt_u64(s);
}

int32_t limit_clamp(int32_t mg) {
    if (mg < ALARM_LIMIT_MIN_MG) {
        return ALARM_LIMIT_MIN_MG;
    }
    if (mg > ALARM_LIMIT_MAX_MG) {
        return ALARM_LIMIT_MAX_MG;
    }
    return mg;
}

void trace_init(trace_t *t) {
    memset(t, 0, sizeof(*t));
    for (int a = 0; a < TRACE_AXES; a++) {
        t->peak_abs[a] = -1;
    }
}

static void publish_bins(trace_t *t) {
    TRACE_DMB();
    t->published = t->head | (t->count << 16);
}

static void snapshot_bins(const trace_t *t, uint32_t *head, uint32_t *count) {
    uint32_t packed = t->published;
    TRACE_DMB();
    *head = packed & 0xFFFFu;
    *count = packed >> 16;
}

static void commit_bin(trace_t *t) {
    uint32_t idx = t->head;
    for (int a = 0; a < TRACE_AXES; a++) {
        int32_t v = (t->peak_abs[a] < 0) ? 0 : t->peak[a];
        if (v > 32767) {
            v = 32767;
        }
        if (v < -32768) {
            v = -32768;
        }
        t->hist[a][idx] = (int16_t)v;
        t->peak[a] = 0;
        t->peak_abs[a] = -1;
    }
    uint32_t head = idx + 1;
    if (head >= TRACE_BINS) {
        head = 0;
    }
    t->head = head;
    if (t->count < TRACE_BINS) {
        t->count++;
    }
    publish_bins(t);
}

void trace_push_sample(trace_t *t, uint32_t now_ms, const int32_t raw_mg[TRACE_AXES],
                       int32_t dyn_mg[TRACE_AXES]) {
    if (!t->seeded) {
        for (int a = 0; a < TRACE_AXES; a++) {
            t->baseline_q[a] = raw_mg[a] << BASELINE_SHIFT;
            dyn_mg[a] = 0;
        }
        t->seeded = true;
        t->bin_start_ms = now_ms;
        return;
    }

    for (int a = 0; a < TRACE_AXES; a++) {
        int32_t base = t->baseline_q[a] >> BASELINE_SHIFT;
        int32_t dyn = raw_mg[a] - base;
        t->baseline_q[a] += dyn;
        dyn_mg[a] = dyn;
        int32_t ad = dyn < 0 ? -dyn : dyn;
        if (t->peak_abs[a] < 0 || ad >= t->peak_abs[a]) {
            t->peak_abs[a] = ad;
            t->peak[a] = dyn;
        }
    }

    int closed = 0;
    while ((uint32_t)(now_ms - t->bin_start_ms) >= TRACE_BIN_MS && closed < 50) {
        commit_bin(t);
        t->bin_start_ms += TRACE_BIN_MS;
        closed++;
    }
    if ((uint32_t)(now_ms - t->bin_start_ms) >= TRACE_BIN_MS) {
        t->bin_start_ms = now_ms;
    }
}

int32_t trace_peak_abs_mg(const trace_t *t) {
    uint32_t count;
    uint32_t head;
    snapshot_bins(t, &head, &count);
    int32_t peak = 0;
    for (uint32_t n = 0; n < count; n++) {
        uint32_t idx = (head + TRACE_BINS - count + n) % TRACE_BINS;
        for (int a = 0; a < TRACE_AXES; a++) {
            int32_t v = t->hist[a][idx];
            if (v < 0) {
                v = -v;
            }
            if (v > peak) {
                peak = v;
            }
        }
    }
    return peak;
}

bool trace_column_range(const trace_t *t, int axis, int col, int ncols, int16_t *mn,
                        int16_t *mx) {
    if (axis < 0 || axis >= TRACE_AXES || col < 0 || col >= ncols || ncols <= 0) {
        return false;
    }
    uint32_t count;
    uint32_t head;
    snapshot_bins(t, &head, &count);
    if (count == 0) {
        return false;
    }
    uint32_t age_lo = (uint32_t)(TRACE_BINS - ((col + 1) * TRACE_BINS) / ncols);
    uint32_t age_hi = (uint32_t)(TRACE_BINS - (col * TRACE_BINS) / ncols);
    if (age_lo >= count || age_lo >= age_hi) {
        return false;
    }
    if (age_hi > count) {
        age_hi = count;
    }
    int16_t lo = 32767;
    int16_t hi = -32768;
    for (uint32_t age = age_lo; age < age_hi; age++) {
        uint32_t idx = (head + TRACE_BINS - 1 - age) % TRACE_BINS;
        int16_t v = t->hist[axis][idx];
        if (v < lo) {
            lo = v;
        }
        if (v > hi) {
            hi = v;
        }
    }
    *mn = lo;
    *mx = hi;
    return true;
}

void alarm_reset(alarm_t *a) {
    a->state = 0;
    a->started_ms = 0;
    a->pin = 0;
}

int alarm_update(alarm_t *a, uint32_t now_ms, int32_t mag_mg, int32_t limit_mg,
                 int32_t hyst_mg, uint32_t pulse_ms) {
    if (limit_mg < 1) {
        limit_mg = 1;
    }
    if (hyst_mg < 0) {
        hyst_mg = 0;
    }
    if (hyst_mg >= limit_mg) {
        hyst_mg = limit_mg - 1;
    }
    if (pulse_ms == 0) {
        pulse_ms = 1;
    }
    int32_t rearm = limit_mg - hyst_mg;

    switch (a->state) {
    case 0: /* idle, pin low */
        a->pin = 0;
        if (mag_mg >= limit_mg) {
            a->state = 1;
            a->started_ms = now_ms;
            a->pin = 1;
        }
        break;
    case 1: /* pulse high */
        a->pin = 1;
        if ((uint32_t)(now_ms - a->started_ms) >= pulse_ms) {
            a->pin = 0;
            a->state = 2;
        }
        break;
    case 2: /* wait until the shock has fallen back */
        a->pin = 0;
        if (mag_mg < rearm) {
            a->state = 0;
        }
        break;
    default:
        a->state = 0;
        a->pin = 0;
        break;
    }
    return a->pin;
}
