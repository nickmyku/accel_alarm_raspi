#include "settings.h"

#include "config.h"
#include "trace.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"

#include <string.h>

#define SETTINGS_MAGIC 0x314D4C41u

typedef struct {
    uint32_t magic;
    int32_t limit_mg;
    uint32_t crc;
} settings_rec_t;

static uint32_t settings_crc(int32_t limit_mg) {
    uint32_t c = 0x811C9DC5u ^ SETTINGS_MAGIC;
    c *= 16777619u;
    c ^= (uint32_t)limit_mg;
    return c;
}

int32_t settings_load(void) {
    const settings_rec_t *rec =
        (const settings_rec_t *)(XIP_BASE + SETTINGS_FLASH_OFFSET);
    if (rec->magic == SETTINGS_MAGIC && rec->crc == settings_crc(rec->limit_mg)) {
        return limit_clamp(rec->limit_mg);
    }
    return ALARM_LIMIT_MG_DEFAULT;
}

void settings_save(int32_t limit_mg, int core1_running) {
    limit_mg = limit_clamp(limit_mg);
    settings_rec_t rec;
    rec.magic = SETTINGS_MAGIC;
    rec.limit_mg = limit_mg;
    rec.crc = settings_crc(limit_mg);

    static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(256)));
    memset(page, 0xFF, sizeof page);
    memcpy(page, &rec, sizeof rec);

    if (core1_running) {
        multicore_lockout_start_blocking();
    }
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SETTINGS_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    if (core1_running) {
        multicore_lockout_end_blocking();
    }
}
