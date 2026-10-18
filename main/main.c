/*
 * Room thermometer / hygrometer / barometer on the QDtech / LCDWiki
 * "2.8inch ESP32-32E Display" (SKU E32R28T, "Cheap Yellow Display"):
 *
 *   ESP32-D0WD-V3, ILI9341V (240x320 portrait), XPT2046 touch, BME280.
 *
 * The application shows a wallpaper chosen by season and time of day and the
 * current temperature as "xx,x°" in the large glass tile.  The wallpaper files
 * are packed into the littlefs partition "storage" at build time (data/).
 *
 * The hardware test screen (test pattern, touch dots, uptime) lives in
 * testscreen.c and can be switched on with APP_SHOW_TESTSCREEN below, or
 * removed from the project altogether.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "background.h"
#include "bigdigit.h"
#include "board_e32r28t.h"
#include "gfx.h"
#include "layout.h"
#include "sensor.h"
#include "statusled.h"
#include "storage.h"
#include "testscreen.h"
#include "touch.h"
#include "ttf.h"
#include "wallpaper.h"

static const char *TAG = "cyd";

/* 1 = hardware test screen instead of the thermometer readout */
#define APP_SHOW_TESTSCREEN 0

/* Wallpaper slot shown for now; later derived from the NTP date and time */
#define APP_SEASON           SEASON_SUMMER
#define APP_DAYTIME          DAYTIME_EVENING
#define APP_BACKGROUND_INDEX 1 /* ocean-sunset.jpg */

/* Seconds between sensor reads (a thermometer does not need more) */
#define SENSOR_INTERVAL_S 10

static esp_lcd_panel_handle_t s_panel;
static gfx_canvas_t s_canvas;

/* ------------------------------------------------------------------------- */
/* Backlight                                                                 */
/* ------------------------------------------------------------------------- */

static void backlight_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = BOARD_BL_LEDC_MODE,
        .duty_resolution = BOARD_BL_LEDC_RESOLUTION,
        .timer_num = BOARD_BL_LEDC_TIMER,
        .freq_hz = BOARD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    const ledc_channel_config_t channel_config = {
        .gpio_num = BOARD_LCD_PIN_BACKLIGHT,
        .speed_mode = BOARD_BL_LEDC_MODE,
        .channel = BOARD_BL_LEDC_CHANNEL,
        .timer_sel = BOARD_BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

static void backlight_set(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t duty = (uint32_t)((percent * BOARD_BL_MAX_DUTY) / 100);
    ESP_ERROR_CHECK(ledc_set_duty(BOARD_BL_LEDC_MODE, BOARD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BOARD_BL_LEDC_MODE, BOARD_BL_LEDC_CHANNEL));
}

/* ------------------------------------------------------------------------- */
/* LCD                                                                       */
/* ------------------------------------------------------------------------- */

static esp_err_t lcd_init(void)
{
    const spi_bus_config_t bus_config = {
        .sclk_io_num = BOARD_LCD_PIN_SCLK,
        .mosi_io_num = BOARD_LCD_PIN_MOSI,
        .miso_io_num = BOARD_LCD_PIN_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BOARD_SCREEN_WIDTH * GFX_FLUSH_BAND_LINES * sizeof(uint16_t),
    };

    ESP_LOGI(TAG, "SPI%d for ILI9341: SCLK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=EN",
             BOARD_LCD_HOST, BOARD_LCD_PIN_SCLK, BOARD_LCD_PIN_MOSI, BOARD_LCD_PIN_MISO,
             BOARD_LCD_PIN_CS, BOARD_LCD_PIN_DC);

    esp_err_t err = spi_bus_initialize(BOARD_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BOARD_LCD_PIN_CS,
        .dc_gpio_num = BOARD_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST, &io_config, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi: %s", esp_err_to_name(err));
        return err;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_PIN_RST, /* GPIO_NUM_NC: shared with the ESP32 EN line */
        .rgb_ele_order = BOARD_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_ili9341(io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_ili9341: %s", esp_err_to_name(err));
        return err;
    }

    if ((err = esp_lcd_panel_reset(s_panel)) != ESP_OK ||
        (err = esp_lcd_panel_init(s_panel)) != ESP_OK) {
        ESP_LOGE(TAG, "panel reset/init: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_lcd_panel_swap_xy(s_panel, BOARD_LCD_LANDSCAPE);
    if (err == ESP_OK) {
        err = esp_lcd_panel_mirror(s_panel, BOARD_LCD_MIRROR_X, BOARD_LCD_MIRROR_Y);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_invert_color(s_panel, BOARD_LCD_INVERT_COLORS);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel orientation: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_disp_on_off: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ILI9341 ready, %dx%d %s, %d MHz SPI clock, %s element order",
             BOARD_SCREEN_WIDTH, BOARD_SCREEN_HEIGHT,
             BOARD_LCD_LANDSCAPE ? "landscape" : "portrait",
             BOARD_LCD_PIXEL_CLOCK_HZ / 1000000,
             BOARD_LCD_BGR ? "BGR" : "RGB");
    return ESP_OK;
}

static void boot_button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_PIN_BOOT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

/* ------------------------------------------------------------------------- */
/* Thermometer screen                                                        */
/* ------------------------------------------------------------------------- */

static int s_shown_tenths;

/* Draw wallpaper and the temperature tile, then send the frame to the panel. */
static esp_err_t draw_thermometer(const char *path, int tenths, uint16_t text_color)
{
    const int64_t start_us = esp_timer_get_time();
    const esp_err_t err = wallpaper_load(&s_canvas, path);
    if (err != ESP_OK) {
        return err;
    }
    const int64_t decoded_us = esp_timer_get_time();

    const int used = bigdigit_draw_number(&s_canvas, LAYOUT_TEMP_X, LAYOUT_TEMP_BASELINE_Y,
                                          LAYOUT_TEMP_MAX_WIDTH, tenths, text_color);
    bigdigit_draw_degree(&s_canvas, LAYOUT_TEMP_DEGREE_X, LAYOUT_TEMP_DEGREE_Y,
                         LAYOUT_TEMP_DIGIT_HEIGHT, text_color);
    const esp_err_t flush_err = gfx_flush_all(&s_canvas, s_panel);
    if (flush_err != ESP_OK) {
        return flush_err;
    }

    ESP_LOGI(TAG, "frame: %d,%d degC, text from (%d,%d), %d px wide, degree at (%d,%d), decode %lld ms, total %lld ms",
             tenths / 10, tenths % 10, LAYOUT_TEMP_X, LAYOUT_TEMP_BASELINE_Y, used,
             LAYOUT_TEMP_DEGREE_X, LAYOUT_TEMP_DEGREE_Y,
             (long long)((decoded_us - start_us) / 1000),
             (long long)((esp_timer_get_time() - start_us) / 1000));
    return ESP_OK;
}

static void thermometer_task(void)
{
    char path[128];
    wallpaper_slot_path(path, sizeof(path), APP_SEASON, APP_DAYTIME, APP_BACKGROUND_INDEX);
    const uint16_t text_color = backgrounds[APP_SEASON][APP_DAYTIME][APP_BACKGROUND_INDEX].textcolor;
    ESP_LOGI(TAG, "wallpaper slot %d/%d/%d: %s, text colour 0x%04X", APP_SEASON, APP_DAYTIME,
             APP_BACKGROUND_INDEX, path, text_color);

    sensor_reading_t reading;
    if (sensor_is_available() && sensor_read(&reading) == ESP_OK) {
        s_shown_tenths = (int)lroundf(reading.temperature_c * 10.0f);
    } else {
        ESP_LOGW(TAG, "no sensor reading yet, showing 00,0");
        s_shown_tenths = 0;
    }

    if (draw_thermometer(path, s_shown_tenths, text_color) != ESP_OK) {
        ESP_LOGE(TAG, "could not draw the thermometer screen");
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_S * 1000));

        if (!sensor_is_available() || sensor_read(&reading) != ESP_OK) {
            ESP_LOGW(TAG, "sensor read failed");
            continue;
        }

        const int tenths = (int)lroundf(reading.temperature_c * 10.0f);
        ESP_LOGI(TAG, "BME280: %.2f C, %.1f %%rH, %.1f hPa", reading.temperature_c,
                 reading.humidity_rh, reading.pressure_hpa);

        if (tenths == s_shown_tenths) {
            continue;
        }
        s_shown_tenths = tenths;
        if (draw_thermometer(path, tenths, text_color) != ESP_OK) {
            ESP_LOGW(TAG, "redraw failed");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Application                                                               */
/* ------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "--- E32R28T room thermometer, ESP-IDF %s ---", IDF_VER);
    ESP_LOGI(TAG, "heap: %d bytes free, largest free block %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             (int)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    backlight_init();
    backlight_set(0);
    statusled_init();
    statusled_self_test();
    boot_button_init();

    if (lcd_init() != ESP_OK) {
        ESP_LOGE(TAG, "LCD bring-up failed, stopping here");
        return;
    }

    if (gfx_canvas_init(&s_canvas, BOARD_SCREEN_WIDTH, BOARD_SCREEN_HEIGHT) != ESP_OK) {
        ESP_LOGE(TAG, "not enough memory for the %dx%d canvas", BOARD_SCREEN_WIDTH, BOARD_SCREEN_HEIGHT);
        return;
    }

    if (storage_init() == ESP_OK) {
        storage_log_summary();
    } else {
        ESP_LOGW(TAG, "wallpaper partition not available");
    }

    if (sensor_init() != ESP_OK) {
        ESP_LOGW(TAG, "BME280 not available, continuing without sensor values");
    }

    if (touch_init() != ESP_OK) {
        ESP_LOGW(TAG, "touch not available");
    }

    if (ttf_init_builtin() != ESP_OK) {
        ESP_LOGE(TAG, "font could not be loaded, the readout stays empty");
    }

    ESP_LOGI(TAG, "%d free heap bytes after initialisation", (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

#if APP_SHOW_TESTSCREEN
    backlight_set(100);
    statusled_set(false, true, false);
    testscreen_run(&s_canvas, s_panel); /* never returns */
#endif

    backlight_set(100);
    statusled_set(false, true, false);
    thermometer_task(); /* never returns */
}
