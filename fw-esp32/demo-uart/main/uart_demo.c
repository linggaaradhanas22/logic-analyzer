/* Standalone external stimulus; the analyzer observes GPIO17 through JC-1.
 * No application queues, callbacks or ISRs are used. The UART driver owns its
 * internal RX buffer (unused here); synchronous TX waits before each idle gap.
 */
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LA_UART_DEMO";

void app_main(void)
{
    const uart_port_t port = UART_NUM_1;
    const uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    static const char message[] = "LA-S3\n";

    ESP_ERROR_CHECK(uart_driver_install(port, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &config));
    ESP_ERROR_CHECK(uart_set_pin(port, 17, 16,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "READY TX=GPIO17 RX=GPIO16(unused) baud=115200 format=8N1 "
                  "payload=4C 41 2D 53 33 0A gap_ms=2");

    unsigned long batches = 0;
    TickType_t last_report = xTaskGetTickCount();
    for (;;) {
        int written = uart_write_bytes(port, message, sizeof(message) - 1);
        ESP_ERROR_CHECK(written == sizeof(message) - 1 ? ESP_OK : ESP_FAIL);
        ESP_ERROR_CHECK(uart_wait_tx_done(port, pdMS_TO_TICKS(100)));
        ++batches;
        vTaskDelay(pdMS_TO_TICKS(2));
        TickType_t now = xTaskGetTickCount();
        if (now - last_report >= pdMS_TO_TICKS(1000)) {
            ESP_LOGI(TAG, "RUNNING batches=%lu TX=GPIO17 baud=115200", batches);
            last_report = now;
        }
    }
}
