#include "statusled.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_e32r28t.h"

static const char *TAG = "statusled";

/* The backlight already owns timer 0 (20 kHz, 10 bit); the LED uses three more
 * channels of that timer, so there is only one frequency to reason about */
#define LED_TIMER    BOARD_BL_LEDC_TIMER
#define LED_MODE     BOARD_BL_LEDC_MODE
#define LED_MAX_DUTY BOARD_BL_MAX_DUTY

#define LED_CHANNEL_R LEDC_CHANNEL_1
#define LED_CHANNEL_G LEDC_CHANNEL_2
#define LED_CHANNEL_B LEDC_CHANNEL_3

/* Update interval of the task: 50 ms gives 1200 steps per colour turn */
#define LED_TICK_MS 50

static bool s_manual; /* set by statusled_set(): leave the LED alone */
static bool s_night;  /* ambilight on */

/* Write one channel; the common anode needs the duty inverted, so `level` is
 * the on-brightness in duty units. */
static void led_write(int red, int green, int blue)
{
    const int levels[3] = {red, green, blue};
    const ledc_channel_t channels[3] = {LED_CHANNEL_R, LED_CHANNEL_G, LED_CHANNEL_B};

    for (size_t i = 0; i < 3; i++) {
        const uint32_t duty = (uint32_t)(LED_MAX_DUTY - (levels[i] < 0 ? 0 : levels[i]));
        ledc_set_duty(LED_MODE, channels[i], duty);
        ledc_update_duty(LED_MODE, channels[i]);
    }
}

/* Hue wheel, h in 0..1535 (six sectors of 256), full saturation and value */
static void hue_to_rgb(int h, int *red, int *green, int *blue)
{
    const int sector = (h >> 8) % 6;
    const int fraction = h & 0xFF;

    switch (sector) {
    case 0: /* red -> yellow */
        *red = 255;
        *green = fraction;
        *blue = 0;
        break;
    case 1: /* yellow -> green */
        *red = 255 - fraction;
        *green = 255;
        *blue = 0;
        break;
    case 2: /* green -> cyan */
        *red = 0;
        *green = 255;
        *blue = fraction;
        break;
    case 3: /* cyan -> blue */
        *red = 0;
        *green = 255 - fraction;
        *blue = 255;
        break;
    case 4: /* blue -> magenta */
        *red = fraction;
        *green = 0;
        *blue = 255;
        break;
    default: /* magenta -> red */
        *red = 255;
        *green = 0;
        *blue = 255 - fraction;
        break;
    }
}

/* Scale a 0..255 colour by a percentage into duty units */
static int scaled_level(int value, int percent)
{
    return value * LED_MAX_DUTY * percent / (255 * 100);
}

static void statusled_task(void *argument)
{
    (void)argument;

    for (;;) {
        if (!s_manual) {
            if (s_night) {
                /* hue from the run time, so the fade keeps running smoothly
                 * across redraws of the display */
                const int64_t milliseconds = esp_timer_get_time() / 1000;
                const int phase = (int)(milliseconds % STATUSLED_AMBIENT_PERIOD_MS);
                const int hue = phase * 6 * 256 / STATUSLED_AMBIENT_PERIOD_MS;

                int red = 0;
                int green = 0;
                int blue = 0;
                hue_to_rgb(hue, &red, &green, &blue);
                led_write(scaled_level(red, STATUSLED_AMBIENT_BRIGHTNESS),
                          scaled_level(green, STATUSLED_AMBIENT_BRIGHTNESS),
                          scaled_level(blue, STATUSLED_AMBIENT_BRIGHTNESS));
            } else {
                const int white = LED_MAX_DUTY * STATUSLED_IDLE_BRIGHTNESS / 100;
                led_write(white, white, white);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));
    }
}

void statusled_init(void)
{
    /* The backlight timer normally exists already; configuring it again with the
     * same values keeps this module usable on its own. */
    const ledc_timer_config_t timer = {
        .speed_mode = LED_MODE,
        .duty_resolution = BOARD_BL_LEDC_RESOLUTION,
        .timer_num = LED_TIMER,
        .freq_hz = BOARD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    const struct {
        ledc_channel_t channel;
        gpio_num_t pin;
    } outputs[] = {
        {LED_CHANNEL_R, BOARD_LED_PIN_R},
        {LED_CHANNEL_G, BOARD_LED_PIN_G},
        {LED_CHANNEL_B, BOARD_LED_PIN_B},
    };

    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
        const ledc_channel_config_t channel = {
            .gpio_num = outputs[i].pin,
            .speed_mode = LED_MODE,
            .channel = outputs[i].channel,
            .timer_sel = LED_TIMER,
            .duty = LED_MAX_DUTY, /* inverted: off */
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
    }

    led_write(0, 0, 0);
}

void statusled_set(bool red, bool green, bool blue)
{
    s_manual = true;
    led_write(red ? LED_MAX_DUTY : 0, green ? LED_MAX_DUTY : 0, blue ? LED_MAX_DUTY : 0);
}

void statusled_ambilight(bool night)
{
    s_manual = false;
    s_night = night;
}

void statusled_start(void)
{
    if (xTaskCreate(statusled_task, "statusled", 2560, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "no memory for the LED task, the LED stays off");
    }
}

void statusled_self_test(void)
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
        statusled_set(steps[i].red, steps[i].green, steps[i].blue);
        ESP_LOGI(TAG, "RGB LED: %s", steps[i].name);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}
