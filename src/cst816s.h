#pragma once

#include <stdbool.h>

/* I2C must already be configured. */
bool cst816s_init(void);

/* down is 1 while a finger is reported. x and y are 0..239. */
bool cst816s_read(int *x, int *y, int *down, int *gesture);
