#include "wifi.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "connect.h"

static const char *TAG = "wifi";

#define WIFI_GOT_IP_BIT  BIT0
#define WIFI_FAILED_BIT  BIT1

/* Tries before startup gives up; once the link was up, reconnects never stop,
 * so a router reboot does not need a reboot of the thermometer */
#define WIFI_MAX_RETRIES 5

static EventGroupHandle_t s_events;
static bool s_connected;
static bool s_was_connected;
static int s_retries;
static char s_ip[16];

static void clear_ip(void)
{
    snprintf(s_ip, sizeof(s_ip), "%s", "-");
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        clear_ip();
        const wifi_event_sta_disconnected_t *event = (const wifi_event_sta_disconnected_t *)data;

        if (s_retries < WIFI_MAX_RETRIES || s_was_connected) {
            s_retries++;
            ESP_LOGW(TAG, "disconnected from \"%s\" (reason %d), retry %d", CONNECT_WIFI_SSID,
                     event->reason, s_retries);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "cannot reach \"%s\" (reason %d, e.g. 201 = network not found, "
                          "15 = wrong password), continuing without network",
                     CONNECT_WIFI_SSID, event->reason);
            xEventGroupSetBits(s_events, WIFI_FAILED_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        s_was_connected = true;
        s_retries = 0;
        ESP_LOGI(TAG, "connected to \"%s\", IP %s", CONNECT_WIFI_SSID, s_ip);
        if (s_events != NULL) {
            xEventGroupSetBits(s_events, WIFI_GOT_IP_BIT);
        }
    }
}

bool wifi_connect(uint32_t timeout_ms)
{
    clear_ip();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS had to be erased (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    s_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif != NULL) {
        esp_netif_set_hostname(netif, CONNECT_HOSTNAME);
    }

    const wifi_init_config_t wifi_init_configuration = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_configuration));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        event_handler, NULL, NULL));

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = CONNECT_WIFI_SSID,
            .password = CONNECT_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration));

    ESP_LOGI(TAG, "connecting to \"%s\" ...", CONNECT_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());

    const EventBits_t bits = xEventGroupWaitBits(s_events, WIFI_GOT_IP_BIT | WIFI_FAILED_BIT,
                                                 pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    if ((bits & WIFI_GOT_IP_BIT) != 0) {
        return true;
    }
    if ((bits & WIFI_FAILED_BIT) == 0) {
        ESP_LOGW(TAG, "no IP address within %u ms, continuing without network",
                 (unsigned)timeout_ms);
    }
    return false;
}

bool wifi_is_connected(void)
{
    return s_connected;
}

const char *wifi_ip_address(void)
{
    return s_ip;
}

void wifi_log_link(void)
{
    wifi_ap_record_t ap;
    if (!s_connected || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        ESP_LOGI(TAG, "not connected");
        return;
    }
    ESP_LOGI(TAG, "link to \"%s\": channel %d, RSSI %d dBm, IP %s", (const char *)ap.ssid,
             ap.primary, ap.rssi, s_ip);
}
