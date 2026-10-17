/*
 * First bring-up test for the QDtech / LCDWiki "2.8inch ESP32-32E Display"
 * (SKU E32R28T, "Cheap Yellow Display"):
 *
 *   ESP32-D0WD-V3, ILI9341V (320x240, 4-wire SPI), XPT2046 touch, RGB LED.
 *
 * What the test does:
 *   1. brings up the LCD over SPI, dims the backlight up with LEDC
 *   2. draws a test pattern (colour bars, gradients, geometry, text) from a
 *      full screen framebuffer
 *   3. animates a status line (uptime, frames/s, heap) and a sweeping bar
 *   4. paints a dot wherever the touchscreen is pressed
 *   5. pressing the BOOT key (GPIO0) redraws the clean test pattern
 *
 * Every step is logged over the serial console, so the boot log shows what
 * worked even if the display stays dark.
 */

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

#include "board_e32r28t.h"
#include "gfx.h"
#include "sensor.h"
#include "storage.h"
#include "touch.h"

static const char *TAG = "cyd";

static esp_lcd_panel_handle_t s_panel;
static gfx_canvas_t s_canvas;

/* Row layout of the test pattern (landscape: BOARD_SCREEN_WIDTH x BOARD_SCREEN_HEIGHT) */
#define ROW_TITLE_Y    0
#define ROW_TITLE_H    20
#define ROW_BARS_Y     24
#define ROW_BARS_H     96
#define ROW_GRAD_Y     124
#define ROW_GRAD_H     24
#define ROW_GEOM_Y     152
#define ROW_GEOM_H     48
#define ROW_STATUS_Y   204
#define ROW_STATUS_H   36

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

#if BOARD_LCD_LANDSCAPE
    err = esp_lcd_panel_swap_xy(s_panel, true);
    if (err == ESP_OK) {
        err = esp_lcd_panel_mirror(s_panel, BOARD_LCD_MIRROR_X, BOARD_LCD_MIRROR_Y);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel orientation: %s", esp_err_to_name(err));
        return err;
    }
#endif

    err = esp_lcd_panel_invert_color(s_panel, BOARD_LCD_INVERT_COLORS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_invert_color: %s", esp_err_to_name(err));
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

/* ------------------------------------------------------------------------- */
/* RGB status LED (common anode: low = on)                                   */
/* ------------------------------------------------------------------------- */

static void rgb_led_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << BOARD_LED_PIN_R) | (1ULL << BOARD_LED_PIN_G) | (1ULL << BOARD_LED_PIN_B),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(BOARD_LED_PIN_R, 1);
    gpio_set_level(BOARD_LED_PIN_G, 1);
    gpio_set_level(BOARD_LED_PIN_B, 1);
}

static void rgb_led_set(bool red, bool green, bool blue)
{
    gpio_set_level(BOARD_LED_PIN_R, red ? 0 : 1);
    gpio_set_level(BOARD_LED_PIN_G, green ? 0 : 1);
    gpio_set_level(BOARD_LED_PIN_B, blue ? 0 : 1);
}

/*
 * Light every channel once, so a missing colour is visible right after boot.
 * The LED on the back of the board carries three separate chips (one per
 * colour), a dark channel points at a wrong GPIO or a missing resistor.
 */
static void rgb_led_self_test(void)
{
    static const struct {
        const char *name;
        bool red;
        bool green;
        bool blue;
    } steps[] = {
        {"red", true, false, false},
        {"green", false, true, false},
        {"blue", false, false, true},
        {"white (all three)", true, true, true},
        {"off", false, false, false},
    };

    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        rgb_led_set(steps[i].red, steps[i].green, steps[i].blue);
        ESP_LOGI(TAG, "RGB LED: %s", steps[i].name);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
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
/* Test pattern                                                              */
/* ------------------------------------------------------------------------- */

static void draw_test_pattern(void)
{
    gfx_clear(&s_canvas, GFX_BLACK);

    /* title bar */
    gfx_fill_rect(&s_canvas, 0, ROW_TITLE_Y, BOARD_SCREEN_WIDTH, ROW_TITLE_H, GFX_NAVY);
    gfx_text(&s_canvas, 4, ROW_TITLE_Y + 2, "E32R28T CYD   ILI9341V  320X240", GFX_WHITE, GFX_NAVY, 1, true);
    gfx_text(&s_canvas, 4, ROW_TITLE_Y + 11, "ESP32-D0WD-V3  " CONFIG_IDF_TARGET "  ESP-IDF " IDF_VER,
             GFX_GRAY, GFX_NAVY, 1, true);

    /* colour bars: white, yellow, cyan, green, magenta, red, blue, black */
    static const uint16_t bars[] = {GFX_WHITE, GFX_YELLOW, GFX_CYAN, GFX_GREEN,
                                    GFX_MAGENTA, GFX_RED, GFX_BLUE, GFX_BLACK};
    const int bar_width = BOARD_SCREEN_WIDTH / (int)(sizeof(bars) / sizeof(bars[0]));
    for (size_t i = 0; i < sizeof(bars) / sizeof(bars[0]); i++) {
        gfx_fill_rect(&s_canvas, (int)i * bar_width, ROW_BARS_Y, bar_width + 1, ROW_BARS_H, bars[i]);
    }

    /* ramps to judge the grey/colour reproduction */
    const int ramp_height = ROW_GRAD_H / 3;
    gfx_gradient_h(&s_canvas, 0, ROW_GRAD_Y, BOARD_SCREEN_WIDTH, ramp_height, 0, 0, 0, 255, 255, 255);
    gfx_gradient_h(&s_canvas, 0, ROW_GRAD_Y + ramp_height, BOARD_SCREEN_WIDTH, ramp_height, 0, 0, 0, 255, 0, 0);
    gfx_gradient_h(&s_canvas, 0, ROW_GRAD_Y + 2 * ramp_height, BOARD_SCREEN_WIDTH, ramp_height, 0, 0, 0, 0, 0, 255);

    /* geometry: outline, diagonals, circle, pixel grid */
    gfx_rect_outline(&s_canvas, 4, ROW_GEOM_Y, BOARD_SCREEN_WIDTH - 8, ROW_GEOM_H, GFX_WHITE);
    gfx_line(&s_canvas, 4, ROW_GEOM_Y, BOARD_SCREEN_WIDTH - 5, ROW_GEOM_Y + ROW_GEOM_H - 1, GFX_DARK_GRAY);
    gfx_line(&s_canvas, BOARD_SCREEN_WIDTH - 5, ROW_GEOM_Y, 4, ROW_GEOM_Y + ROW_GEOM_H - 1, GFX_DARK_GRAY);
    gfx_circle_filled(&s_canvas, BOARD_SCREEN_WIDTH / 2, ROW_GEOM_Y + ROW_GEOM_H / 2, 16, GFX_ORANGE);
    gfx_rect_outline(&s_canvas, BOARD_SCREEN_WIDTH / 2 - 17, ROW_GEOM_Y + ROW_GEOM_H / 2 - 17, 35, 35, GFX_WHITE);
    gfx_text(&s_canvas, 8, ROW_GEOM_Y + 4, "GEOMETRY / 1PX", GFX_WHITE, GFX_BLACK, 1, false);

    /* status area background, the text is refreshed every frame */
    gfx_fill_rect(&s_canvas, 0, ROW_STATUS_Y, BOARD_SCREEN_WIDTH, ROW_STATUS_H, GFX_DARK_GRAY);
}

static void draw_status(const char *line2, const char *line3, int sweep)
{
    const uint16_t background = GFX_DARK_GRAY;

    gfx_text(&s_canvas, 4, ROW_STATUS_Y + 2, line2, GFX_WHITE, background, 1, true);

    /* clear the whole row first, the touch line changes length */
    gfx_fill_rect(&s_canvas, 0, ROW_STATUS_Y + 11, BOARD_SCREEN_WIDTH, 8, background);
    gfx_text(&s_canvas, 4, ROW_STATUS_Y + 11, line3, GFX_WHITE, background, 1, false);

    /* sweeping bar: moves left to right once per cycle */
    gfx_fill_rect(&s_canvas, 0, ROW_STATUS_Y + 22, BOARD_SCREEN_WIDTH, 8, background);
    const int sweep_width = BOARD_SCREEN_WIDTH / 6;
    const int sweep_x = sweep * (BOARD_SCREEN_WIDTH - sweep_width) / 99;
    gfx_fill_rect(&s_canvas, sweep_x, ROW_STATUS_Y + 22, sweep_width, 8, GFX_GREEN);
    gfx_rect_outline(&s_canvas, 0, ROW_STATUS_Y + 22, BOARD_SCREEN_WIDTH, 8, GFX_WHITE);

    gfx_flush(&s_canvas, s_panel, 0, ROW_STATUS_Y, BOARD_SCREEN_WIDTH, ROW_STATUS_H);
}

/* ------------------------------------------------------------------------- */
/* Application                                                               */
/* ------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "--- E32R28T CYD first tests, ESP-IDF %s ---", IDF_VER);
    ESP_LOGI(TAG, "heap: %d bytes free, largest free block %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
             (int)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    backlight_init();
    backlight_set(0);
    rgb_led_init();
    rgb_led_self_test();
    boot_button_init();

    if (lcd_init() != ESP_OK) {
        ESP_LOGE(TAG, "LCD bring-up failed, stopping here");
        return;
    }

    if (gfx_canvas_init(&s_canvas, BOARD_SCREEN_WIDTH, BOARD_SCREEN_HEIGHT) != ESP_OK) {
        ESP_LOGE(TAG, "not enough memory for the %dx%d canvas", BOARD_SCREEN_WIDTH, BOARD_SCREEN_HEIGHT);
        return;
    }

    draw_test_pattern();
    if (gfx_flush_all(&s_canvas, s_panel) != ESP_OK) {
        ESP_LOGE(TAG, "first frame could not be sent");
    }
    backlight_set(100);
    rgb_led_set(false, true, false);
    ESP_LOGI(TAG, "test pattern drawn, backlight on - press BOOT (GPIO0) to redraw");

    if (touch_init() != ESP_OK) {
        ESP_LOGW(TAG, "touch not available, continuing without the touch test");
    }

    if (storage_init() == ESP_OK) {
        storage_log_summary();
    } else {
        ESP_LOGW(TAG, "wallpaper partition not available");
    }

    if (sensor_init() != ESP_OK) {
        ESP_LOGW(TAG, "BME280 not available, continuing without sensor values");
    }

    ESP_LOGI(TAG, "%d free heap bytes after initialisation", (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    const int64_t start_us = esp_timer_get_time();
    int64_t last_report_us = start_us;
    int64_t last_touch_log_us = 0;
    int64_t last_sensor_read_us = 0;
    int frames = 0;
    int fps = 0;
    char touch_line[48];
    snprintf(touch_line, sizeof(touch_line), "TOUCH: %s", touch_is_available() ? "READY" : "NOT AVAILABLE");

    for (;;) {
        const int64_t now_us = esp_timer_get_time();
        const int64_t uptime_us = now_us - start_us;
        const int sweep = (int)((uptime_us / 50000) % 100); /* 0..99, 5 s per cycle */

        /* touch: paint a dot on the panel and follow it in the status line */
        touch_state_t touch = {0};
        if (touch_is_available() && touch_read(&touch) == ESP_OK && touch.pressed) {
            gfx_circle_filled(&s_canvas, touch.x, touch.y, 5, GFX_MAGENTA);
            gfx_circle_filled(&s_canvas, touch.x, touch.y, 2, GFX_WHITE);
            gfx_flush(&s_canvas, s_panel, touch.x - 6, touch.y - 6, 13, 13);

            snprintf(touch_line, sizeof(touch_line), "TOUCH: RAW %4u/%4u -> %3d/%3d  Z%4u",
                     touch.raw_x, touch.raw_y, touch.x, touch.y, touch.strength);
            if (now_us - last_touch_log_us > 200000) {
                last_touch_log_us = now_us;
                ESP_LOGI(TAG, "%s", touch_line);
            }
        }

        /* BME280: log a reading every 2 s (the display layout follows later) */
        if (sensor_is_available() && now_us - last_sensor_read_us >= 2000000) {
            last_sensor_read_us = now_us;
            sensor_reading_t reading;
            const esp_err_t err = sensor_read(&reading);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "BME280: %.2f C, %.1f %%rH, %.1f hPa", reading.temperature_c,
                         reading.humidity_rh, reading.pressure_hpa);
            } else {
                ESP_LOGW(TAG, "BME280 read failed: %s", esp_err_to_name(err));
            }
        }

        char line2[48];
        char line3[48];
        snprintf(line2, sizeof(line2), "UPTIME %5d.%1ds  FPS %2d  HEAP %4d KB",
                 (int)(uptime_us / 1000000), (int)((uptime_us / 100000) % 10), fps,
                 (int)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
        snprintf(line3, sizeof(line3), "%s", touch_line);
        draw_status(line2, line3, sweep);

        /* BOOT key redraws the clean test pattern */
        if (gpio_get_level(BOARD_PIN_BOOT) == 0) {
            ESP_LOGI(TAG, "BOOT pressed, redrawing the test pattern");
            rgb_led_set(false, false, true);
            draw_test_pattern();
            gfx_flush_all(&s_canvas, s_panel);
            rgb_led_set(false, true, false);
            while (gpio_get_level(BOARD_PIN_BOOT) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        frames++;
        if (now_us - last_report_us >= 1000000) {
            fps = frames;
            frames = 0;
            last_report_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
