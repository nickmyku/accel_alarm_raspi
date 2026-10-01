#pragma once

#include <stdint.h>

int32_t settings_load(void);

/* core1_running must be true once the sampler core has started, so flash
 * programming can stop that core while the sector is erased. */
void settings_save(int32_t limit_mg, int core1_running);
