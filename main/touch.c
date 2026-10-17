#include "touch.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_log.h"

#include "board_e32r28t.h"

static const char *TAG = "touch";

#if BOARD_TOUCH_ENABLE
static esp_lcd_touch_handle_t s_touch;
#endif
static bool s_available;

/* The driver scales raw ADC values with x_max/y_max, so ask for the raw range to
 * keep the conversion neutral (raw 0..4095 -> 0..4095) and map the axes here. */
#define TOUCH_ADC_MAX 4095

#if BOARD_TOUCH_ENABLE
static int scale_axis(uint16_t value, int raw_min, int raw_max, int length)
{
    if (value < raw_min) {
        value = raw_min;
    }
    if (value > raw_max) {
        value = raw_max;
    }
    if (raw_max <= raw_min) {
        return 0;
    }
    return (int)((int32_t)(value - raw_min) * (length - 1) / (raw_max - raw_min));
}
#endif /* BOARD_TOUCH_ENABLE */

esp_err_t touch_init(void)
{
#if !BOARD_TOUCH_ENABLE
    ESP_LOGW(TAG, "touch disabled in board_e32r28t.h");
    return ESP_ERR_NOT_SUPPORTED;
#else
    const spi_bus_config_t bus_config = {
        .mosi_io_num = BOARD_TOUCH_PIN_MOSI,
        .miso_io_num = BOARD_TOUCH_PIN_MISO,
        .sclk_io_num = BOARD_TOUCH_PIN_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 64,
    };

    ESP_LOGI(TAG, "SPI%d for XPT2046: SCK=%d MOSI=%d MISO=%d CS=%d",
             BOARD_TOUCH_HOST, BOARD_TOUCH_PIN_SCLK, BOARD_TOUCH_PIN_MOSI,
             BOARD_TOUCH_PIN_MISO, BOARD_TOUCH_PIN_CS);

    esp_err_t err = spi_bus_initialize(BOARD_TOUCH_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(BOARD_TOUCH_PIN_CS);
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_TOUCH_HOST, &io_config, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi: %s", esp_err_to_name(err));
        return err;
    }

    const esp_lcd_touch_config_t touch_config = {
        .x_max = TOUCH_ADC_MAX,
        .y_max = TOUCH_ADC_MAX,
        .rst_gpio_num = GPIO_NUM_NC, /* the XPT2046 shares the board reset */
        .int_gpio_num = GPIO_NUM_NC, /* polled, PENIRQ (GPIO36) is unreliable on this board */
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    err = esp_lcd_touch_new_spi_xpt2046(io, &touch_config, &s_touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_touch_new_spi_xpt2046: %s", esp_err_to_name(err));
        return err;
    }

    s_available = true;
    ESP_LOGI(TAG, "XPT2046 ready, raw axes %d..%d / %d..%d, swap=%d mirror=%d/%d",
             BOARD_TOUCH_RAW_MIN_X, BOARD_TOUCH_RAW_MAX_X, BOARD_TOUCH_RAW_MIN_Y, BOARD_TOUCH_RAW_MAX_Y,
             BOARD_TOUCH_SWAP_XY, BOARD_TOUCH_MIRROR_X, BOARD_TOUCH_MIRROR_Y);
    return ESP_OK;
#endif
}

bool touch_is_available(void)
{
    return s_available;
}

esp_err_t touch_read(touch_state_t *state)
{
    memset(state, 0, sizeof(*state));

    if (!s_available) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_lcd_touch_read_data(s_touch);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t points = 0;
    esp_lcd_touch_point_data_t point = {0};
    err = esp_lcd_touch_get_data(s_touch, &point, &points, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (points == 0) {
        return ESP_OK; /* nothing pressed */
    }
    if (point.strength < BOARD_TOUCH_Z_THRESHOLD) {
        return ESP_OK; /* too light, treat as noise */
    }

    const uint16_t raw_x = point.x;
    const uint16_t raw_y = point.y;
    const uint16_t strength = point.strength;
    state->raw_x = raw_x;
    state->raw_y = raw_y;
    state->strength = strength;

    int x;
    int y;
    if (BOARD_TOUCH_SWAP_XY) {
        x = scale_axis(raw_y, BOARD_TOUCH_RAW_MIN_Y, BOARD_TOUCH_RAW_MAX_Y, BOARD_SCREEN_WIDTH);
        y = scale_axis(raw_x, BOARD_TOUCH_RAW_MIN_X, BOARD_TOUCH_RAW_MAX_X, BOARD_SCREEN_HEIGHT);
    } else {
        x = scale_axis(raw_x, BOARD_TOUCH_RAW_MIN_X, BOARD_TOUCH_RAW_MAX_X, BOARD_SCREEN_WIDTH);
        y = scale_axis(raw_y, BOARD_TOUCH_RAW_MIN_Y, BOARD_TOUCH_RAW_MAX_Y, BOARD_SCREEN_HEIGHT);
    }

    if (BOARD_TOUCH_MIRROR_X) {
        x = BOARD_SCREEN_WIDTH - 1 - x;
    }
    if (BOARD_TOUCH_MIRROR_Y) {
        y = BOARD_SCREEN_HEIGHT - 1 - y;
    }

    state->x = x;
    state->y = y;
    state->pressed = true;
    return ESP_OK;
}
