/* XPT2046 resistive touch panel of the E32R28T board. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool pressed;
    uint16_t raw_x; /* raw 12 bit ADC value, useful for calibrating the board header */
    uint16_t raw_y;
    uint16_t strength; /* Z / pressure value, 0 when released */
    int x;             /* screen coordinate, only valid when pressed */
    int y;
} touch_state_t;

/** Bring up the touch SPI bus and the controller. */
esp_err_t touch_init(void);

/** True once touch_init() succeeded. */
bool touch_is_available(void);

/**
 * Read the panel.  Returns ESP_OK also when nothing is pressed
 * (state->pressed is false in that case).
 */
esp_err_t touch_read(touch_state_t *state);
