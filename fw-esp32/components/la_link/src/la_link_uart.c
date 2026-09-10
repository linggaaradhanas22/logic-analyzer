#include "la_link_uart.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "la_board.h"

static const char *TAG = "la_link_uart";

esp_err_t la_link_uart_init(QueueHandle_t *event_queue)
{
    const uart_config_t config = {
        .baud_rate = LA_BOARD_LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = LA_BOARD_LINK_RX_FIFO_THRESH,
        .source_clk = UART_SCLK_DEFAULT,
    };
    QueueHandle_t queue = NULL;
    esp_err_t result;

    if (event_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *event_queue = NULL;

    result = uart_driver_install((uart_port_t)LA_BOARD_LINK_UART,
                                 LA_BOARD_LINK_RX_BUFFER,
                                 LA_BOARD_LINK_TX_BUFFER,
                                 LA_BOARD_LINK_UART_EVENT_QUEUE,
                                 &queue,
                                 0);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(result));
        return result;
    }
    result = uart_param_config((uart_port_t)LA_BOARD_LINK_UART, &config);
    if (result == ESP_OK) {
        result = uart_set_pin((uart_port_t)LA_BOARD_LINK_UART,
                              LA_BOARD_LINK_TX_GPIO,
                              LA_BOARD_LINK_RX_GPIO,
                              LA_BOARD_LINK_RTS_GPIO,
                              LA_BOARD_LINK_CTS_GPIO);
    }
    if (result == ESP_OK) {
        result = uart_set_rx_full_threshold((uart_port_t)LA_BOARD_LINK_UART,
                                            LA_BOARD_LINK_RX_FIFO_THRESH);
    }
    if (result == ESP_OK) {
        result = uart_set_rx_timeout((uart_port_t)LA_BOARD_LINK_UART,
                                     LA_BOARD_LINK_RX_TIMEOUT_SYMS);
    }
    if (result == ESP_OK) {
        /* uart_set_pin enables the CTS pull-up but not the RX one. Without a
         * powered peer the RX line floats and the driver reports a framing
         * error storm; hold it high (a driven push-pull TX overrides this). */
        (void)gpio_set_pull_mode((gpio_num_t)LA_BOARD_LINK_RX_GPIO,
                                 GPIO_PULLUP_ONLY);
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "link UART configuration failed: %s", esp_err_to_name(result));
        (void)uart_driver_delete((uart_port_t)LA_BOARD_LINK_UART);
        return result;
    }

    ESP_LOGI(TAG, "UART%d %d 8N1 RTS/CTS tx=%d rx=%d rts=%d cts=%d",
             LA_BOARD_LINK_UART,
             LA_BOARD_LINK_BAUD,
             LA_BOARD_LINK_TX_GPIO,
             LA_BOARD_LINK_RX_GPIO,
             LA_BOARD_LINK_RTS_GPIO,
             LA_BOARD_LINK_CTS_GPIO);
    *event_queue = queue;
    return ESP_OK;
}

int la_link_uart_read(uint8_t *buffer, size_t length, TickType_t timeout)
{
    if (buffer == NULL || length == 0u) {
        return -1;
    }
    return uart_read_bytes((uart_port_t)LA_BOARD_LINK_UART, buffer, length, timeout);
}

esp_err_t la_link_uart_write_frame(const uint8_t *frame, size_t length)
{
    if (frame == NULL || length == 0u || length > LA_BOARD_LINK_TX_BUFFER) {
        return ESP_ERR_INVALID_ARG;
    }
    for (;;) {
        size_t free_size = 0u;
        esp_err_t result = uart_get_tx_buffer_free_size((uart_port_t)LA_BOARD_LINK_UART,
                                                        &free_size);
        if (result == ESP_OK && free_size >= length) {
            break;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    int written = uart_write_bytes((uart_port_t)LA_BOARD_LINK_UART, frame, length);
    return written == (int)length ? ESP_OK : ESP_FAIL;
}

void la_link_uart_flush_input(void)
{
    (void)uart_flush_input((uart_port_t)LA_BOARD_LINK_UART);
}
