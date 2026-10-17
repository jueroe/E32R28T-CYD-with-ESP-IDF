/* BME280 temperature / humidity / pressure sensor on the I2C bus. */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    float temperature_c; /* °C */
    float humidity_rh;   /* % relative humidity */
    float pressure_hpa;  /* hPa */
} sensor_reading_t;

/** Bring up the I2C bus and the sensor. */
esp_err_t sensor_init(void);

/** True once sensor_init() succeeded. */
bool sensor_is_available(void);

/** Read all three values.  Returns ESP_ERR_INVALID_STATE if not initialised. */
esp_err_t sensor_read(sensor_reading_t *reading);
