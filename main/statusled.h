/* RGB status LED of the board (common anode, one chip per colour). */

#pragma once

#include <stdbool.h>

/* Configure the three pins and switch all colours off. */
void statusled_init(void);

/* Low level = colour on (common anode). */
void statusled_set(bool red, bool green, bool blue);

/* Light every channel once so a broken colour is visible right after boot. */
void statusled_self_test(void);
