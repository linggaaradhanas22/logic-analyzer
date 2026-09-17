#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "la_link.h"
#include "la_net.h"
#include "la_usb.h"
#include "la_web.h"
#include "ui_task.h"

void app_main(void)
{
    esp_err_t result = nvs_flash_init();

    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    ESP_ERROR_CHECK(la_link_start());
    ESP_ERROR_CHECK(la_usb_start());
    ESP_ERROR_CHECK(la_net_start());
    ESP_ERROR_CHECK(la_web_start());
    ESP_ERROR_CHECK(la_ui_start());
}
