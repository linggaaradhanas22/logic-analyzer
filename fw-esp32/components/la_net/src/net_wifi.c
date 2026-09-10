#include "net_wifi.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "la_board.h"

static const char *TAG = "la_net_wifi";

static QueueHandle_t s_event_queue;
static uint32_t s_event_drops;

static void post_event(la_net_policy_event_t event)
{
    if (s_event_queue == NULL) {
        return;
    }
    if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
        s_event_drops += 1u;
    }
}

static void wifi_event_handler(void *argument,
                               esp_event_base_t base,
                               int32_t id,
                               void *data)
{
    (void)argument;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *info =
            (const wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "station disconnected (reason %d)", info->reason);
        post_event(LA_NET_POLICY_EVT_WIFI_LOST);
    }
}

static void ip_event_handler(void *argument,
                             esp_event_base_t base,
                             int32_t id,
                             void *data)
{
    (void)argument;

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *info = (const ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&info->ip_info.ip));
        post_event(LA_NET_POLICY_EVT_WIFI_UP);
    }
}

esp_err_t la_net_wifi_init(QueueHandle_t event_queue)
{
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t station_config;
    esp_err_t result;

    if (event_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_event_queue = event_queue;

    result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }
    result = esp_wifi_init(&init_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(result));
        return result;
    }
    result = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &wifi_event_handler, NULL, NULL);
    if (result == ESP_OK) {
        result = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                     &ip_event_handler, NULL, NULL);
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "event registration failed: %s", esp_err_to_name(result));
        return result;
    }

    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result == ESP_OK) {
        result = esp_wifi_set_ps(WIFI_PS_NONE);
    }
    if (result == ESP_OK) {
        result = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "station setup failed: %s", esp_err_to_name(result));
        return result;
    }

    memset(&station_config, 0, sizeof(station_config));
    if (CONFIG_LA_WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "CONFIG_LA_WIFI_SSID is empty; set credentials with "
                      "`idf.py -B build/esp32 menuconfig` before the network gate");
    } else {
        strlcpy((char *)station_config.sta.ssid, CONFIG_LA_WIFI_SSID,
                sizeof(station_config.sta.ssid));
        strlcpy((char *)station_config.sta.password, CONFIG_LA_WIFI_PASSWORD,
                sizeof(station_config.sta.password));
        station_config.sta.threshold.authmode =
            CONFIG_LA_WIFI_PASSWORD[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        result = esp_wifi_set_config(WIFI_IF_STA, &station_config);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(result));
            return result;
        }
    }

    result = esp_wifi_start();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(result));
        return result;
    }
    ESP_LOGI(TAG, "station started (ssid \"%s\")", CONFIG_LA_WIFI_SSID);
    return ESP_OK;
}

esp_err_t la_net_wifi_connect(void)
{
    esp_err_t result = esp_wifi_connect();

    if (result != ESP_OK && result != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_connect: %s", esp_err_to_name(result));
    }
    return result;
}

esp_err_t la_net_wifi_stop(void)
{
    return esp_wifi_stop();
}

int la_net_wifi_rssi(void)
{
    wifi_ap_record_t record;

    if (esp_wifi_sta_get_ap_info(&record) == ESP_OK) {
        return record.rssi;
    }
    return -127;
}

uint32_t la_net_wifi_event_drops(void)
{
    return s_event_drops;
}
