#include "statusled.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_e32r28t.h"

static const char *TAG = "statusled";

void statusled_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << BOARD_LED_PIN_R) | (1ULL << BOARD_LED_PIN_G) | (1ULL << BOARD_LED_PIN_B),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    statusled_set(false, false, false);
}

void statusled_set(bool red, bool green, bool blue)
{
    gpio_set_level(BOARD_LED_PIN_R, red ? 0 : 1);
    gpio_set_level(BOARD_LED_PIN_G, green ? 0 : 1);
    gpio_set_level(BOARD_LED_PIN_B, blue ? 0 : 1);
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
