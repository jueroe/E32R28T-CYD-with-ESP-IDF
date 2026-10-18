#include "sensor.h"

#include <string.h>

#include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#include "board_e32r28t.h"

static const char *TAG = "sensor";

static i2c_master_bus_handle_t s_bus_handle;
static i2c_bus_handle_t s_bus;
static bme280_handle_t s_bme280;
static bool s_available;

/* Last successful reading, so the HTTP endpoint can answer without touching the
 * I2C bus (the display task reads once every 10 s). */
static sensor_reading_t s_last;
static int64_t s_last_us;
static bool s_last_valid;
static SemaphoreHandle_t s_last_lock;

esp_err_t sensor_init(void)
{
    if (s_last_lock == NULL) {
        s_last_lock = xSemaphoreCreateMutex();
        if (s_last_lock == NULL) {
            ESP_LOGE(TAG, "no memory for the reading lock");
            return ESP_ERR_NO_MEM;
        }
    }
    /* The bus is created here instead of inside i2c_bus_create(): the latter
     * probes an existing bus with i2c_master_get_bus_handle(), which logs an
     * error when the port is not initialised yet.  Creating it first is the
     * supported path ("uses the externally initialized bus handle") and keeps
     * the boot log clean. */
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = BOARD_I2C_HOST,
        .sda_io_num = BOARD_I2C_PIN_SDA,
        .scl_io_num = BOARD_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, /* the breakout has 10k pull-ups, this is a fallback */
    };

    ESP_LOGI(TAG, "I2C%d for BME280: SDA=%d SCL=%d, %d kHz, address 0x%02X", BOARD_I2C_HOST,
             BOARD_I2C_PIN_SDA, BOARD_I2C_PIN_SCL, BOARD_I2C_FREQ_HZ / 1000, BOARD_BME280_ADDRESS);

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_PIN_SDA,
        .scl_io_num = BOARD_I2C_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BOARD_I2C_FREQ_HZ,
    };

    s_bus = i2c_bus_create(BOARD_I2C_HOST, &config);
    if (s_bus == NULL) {
        ESP_LOGE(TAG, "i2c_bus_create failed");
        return ESP_FAIL;
    }

    s_bme280 = bme280_create(s_bus, BOARD_BME280_ADDRESS);
    if (s_bme280 == NULL) {
        ESP_LOGE(TAG, "bme280_create failed");
        return ESP_FAIL;
    }

    err = bme280_default_init(s_bme280);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no BME280 at 0x%02X (%s) - check SDA/SCL and the address jumper",
                 BOARD_BME280_ADDRESS, esp_err_to_name(err));
        return err;
    }

    /* bme280_default_init() leaves the sensor in normal mode with 16x
     * oversampling, the first conversion takes up to ~110 ms.  Wait until a
     * complete measurement can be read, so the caller always gets valid data. */
    s_available = true;
    sensor_reading_t probe;
    for (int attempt = 0; attempt < 10; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (sensor_read(&probe) == ESP_OK) {
            ESP_LOGI(TAG, "BME280 ready, first reading: %.2f C, %.1f %%rH, %.1f hPa",
                     probe.temperature_c, probe.humidity_rh, probe.pressure_hpa);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "BME280 answers but delivers no measurement");
    s_available = false;
    return ESP_FAIL;
}

bool sensor_is_available(void)
{
    return s_available;
}

esp_err_t sensor_read(sensor_reading_t *reading)
{
    if (!s_available) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(reading, 0, sizeof(*reading));

    esp_err_t err = bme280_read_temperature(s_bme280, &reading->temperature_c);
    if (err == ESP_OK) {
        err = bme280_read_humidity(s_bme280, &reading->humidity_rh);
    }
    if (err == ESP_OK) {
        err = bme280_read_pressure(s_bme280, &reading->pressure_hpa);
    }

    if (err == ESP_OK && s_last_lock != NULL) {
        xSemaphoreTake(s_last_lock, portMAX_DELAY);
        s_last = *reading;
        s_last_us = esp_timer_get_time();
        s_last_valid = true;
        xSemaphoreGive(s_last_lock);
    }
    return err;
}

bool sensor_last(sensor_reading_t *reading, int64_t *age_ms)
{
    if (s_last_lock == NULL) {
        return false;
    }

    xSemaphoreTake(s_last_lock, portMAX_DELAY);
    const bool valid = s_last_valid;
    if (valid) {
        if (reading != NULL) {
            *reading = s_last;
        }
        if (age_ms != NULL) {
            *age_ms = (esp_timer_get_time() - s_last_us) / 1000;
        }
    }
    xSemaphoreGive(s_last_lock);
    return valid;
}
