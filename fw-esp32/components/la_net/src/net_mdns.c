#include "net_mdns.h"

#include "esp_log.h"
#include "la_board.h"
#include "mdns.h"

static const char *TAG = "la_net_mdns";

esp_err_t la_net_mdns_start(void)
{
    esp_err_t result = mdns_init();

    if (result != ESP_OK) {
        return result;
    }
    result = mdns_hostname_set(LA_BOARD_MDNS_HOSTNAME);
    if (result == ESP_OK) {
        result = mdns_instance_name_set("RTOS Logic Analyzer gateway");
    }
    if (result == ESP_OK) {
        result = mdns_service_add(NULL, LA_BOARD_MDNS_SERVICE, LA_BOARD_MDNS_PROTO,
                                  LA_BOARD_TCP_PORT, NULL, 0);
    }
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "%s.local advertises %s.%s on :%d",
                 LA_BOARD_MDNS_HOSTNAME, LA_BOARD_MDNS_SERVICE,
                 LA_BOARD_MDNS_PROTO, LA_BOARD_TCP_PORT);
    }
    return result;
}
