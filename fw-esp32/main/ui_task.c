#include "ui_task.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "la_board.h"
#include "la_link.h"
#include "la_net.h"
#include "la_usb.h"
#include "la_web.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#endif

static const char *TAG = "la_ui";

#if defined(CONFIG_IDF_TARGET_ESP32S3)
/* The DevKitC-1 RGB LED is a WS2812 that powers up in an undefined state and
 * is not driven by the rest of the firmware, so write one black frame at boot.
 * Both known LED GPIOs (v1.0/v1.1) are driven; the wrong pin just pulses. */
static void rgb_led_off(int gpio)
{
    rmt_tx_channel_config_t tx = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 64,
        .resolution_hz = 10000000u, /* 0.1 us per tick */
        .trans_queue_depth = 1,
    };
    rmt_copy_encoder_config_t copy;
    rmt_transmit_config_t transmit = { .loop_count = 0 };
    rmt_symbol_word_t symbols[24];
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;
    int index;

    if (rmt_new_tx_channel(&tx, &channel) != ESP_OK ||
        rmt_new_copy_encoder(&copy, &encoder) != ESP_OK ||
        rmt_enable(channel) != ESP_OK) {
        return;
    }
    for (index = 0; index < 24; ++index) {
        symbols[index].level0 = 1;
        symbols[index].duration0 = 4; /* 0.4 us high */
        symbols[index].level1 = 0;
        symbols[index].duration1 = 9; /* 0.9 us low */
    }
    if (rmt_transmit(channel, encoder, symbols, sizeof(symbols), &transmit) == ESP_OK) {
        (void)rmt_tx_wait_all_done(channel, 100);
    }
    (void)rmt_disable(channel);
    (void)rmt_del_encoder(encoder);
    (void)rmt_del_channel(channel);
}
#endif

static StackType_t s_ui_stack[LA_BOARD_UI_TASK_STACK];
static StaticTask_t s_ui_tcb;

static const char *policy_state_name(la_net_policy_state_t state)
{
    switch (state) {
    case LA_NET_POLICY_CONNECTING:
        return "CONNECTING";
    case LA_NET_POLICY_BACKOFF:
        return "BACKOFF";
    case LA_NET_POLICY_CONNECTED:
        return "CONNECTED";
    case LA_NET_POLICY_CLIENT_ACTIVE:
        return "CLIENT";
    default:
        return "IDLE";
    }
}

static void ui_task(void *argument)
{
    la_relay_stats_t up;
    la_relay_stats_t down;
    la_link_counters_t link;
    la_net_policy_stats_t policy;
    la_net_policy_state_t state;
    la_net_counters_t net;
    la_usb_counters_t usb;
    la_web_counters_t web;
    uint32_t led_phase = 0u;
    uint32_t stats_phase = 0u;

    (void)argument;
    esp_task_wdt_add(NULL);

#if LA_BOARD_LED_GPIO >= 0
    gpio_config_t led = {
        .pin_bit_mask = 1ULL << LA_BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&led);
#endif

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        la_link_stats(&up, &down, &link);
        la_net_stats(&policy, &state, &net);
        la_usb_stats(&usb);
        la_web_stats(&web);

#if LA_BOARD_LED_GPIO >= 0
#if LA_BOARD_LED_ACTIVE_LOW
        int level = (state == LA_NET_POLICY_CLIENT_ACTIVE) ? 0 : (int)(led_phase & 1u);
#else
        int level = (state == LA_NET_POLICY_CLIENT_ACTIVE) ? 1 : (int)(led_phase & 1u);
#endif
        (void)gpio_set_level(LA_BOARD_LED_GPIO, level);
#endif
        led_phase += 1u;

        if ((stats_phase++ % 40u) == 0u) {
            la_net_diag_dump();
        }

        if ((led_phase & 3u) != 0u) {
            esp_task_wdt_reset();
            continue;
        }
        ESP_LOGI(TAG,
                 "link fwd=%u ffail=%u drop=%u press=%u mirror=%u/%u fifo_ovf=%u line_err=%u "
                 "ptimeout=%u rrescue=%u touse=%u rep=%u rx_bytes=%u tx_bytes=%u "
                 "spi=%u/%u/%u | net %s clients=%u/%u "
                 "disc=%u wifiat=%u/%u wifidrop=%u rx_frames=%u tx_frames=%u to_host_discard=%u "
                 "open=%u accfail=%u/%d closefail=%u/%d closemax=%u sretry=%u "
                 "drains=%u loops=%u sready=%u serr=%u refresh=%u usbrej=%u | "
                 "usb %d at=%u det=%u disc=%u rx=%u/%u tx=%u/%u ffail=%u part=%u "
                 "stall=%u wto=%u rxd=%u rxerr=%u hwm=%u | "
                 "rssi=%d heap=%u stacks rx=%u tx=%u net=%u ui=%u "
                 "spi2 lo=%u ch=%u cf=%u sto=%u rst=%u frm=%u crc=%u "
                 "web=%u/%u/%u/%u",
                 (unsigned)up.frames_forwarded, (unsigned)up.frames_forward_failed,
                 (unsigned)up.best_effort_dropped,
                 (unsigned)up.control_pressure_events, (unsigned)up.frames_mirrored,
                 (unsigned)up.mirror_dropped, (unsigned)link.fifo_ovf_events,
                 (unsigned)link.line_error_events, (unsigned)link.pressure_timeouts,
                 (unsigned)link.rx_rescue_bytes, (unsigned)link.to_net_max_used,
                 (unsigned)link.overrun_reports, (unsigned)link.uart_rx_bytes,
                 (unsigned)link.uart_tx_bytes, (unsigned)link.spi_bytes,
                 (unsigned)link.spi_frames, (unsigned)link.spi_crc_errors,
                 policy_state_name(state),
                 (unsigned)policy.clients_accepted, (unsigned)policy.clients_rejected,
                 (unsigned)policy.client_disconnects,
                 (unsigned)policy.wifi_attempts, (unsigned)policy.wifi_successes,
                 (unsigned)net.wifi_event_drops,
                 (unsigned)net.client_rx_frames, (unsigned)net.client_tx_frames,
                 (unsigned)net.to_host_discarded, (unsigned)net.sockets_open,
                 (unsigned)net.accept_failures, (int)net.accept_errno,
                 (unsigned)net.close_failures, (int)net.close_errno,
                 (unsigned)net.close_max_ms, (unsigned)net.send_retries,
                 (unsigned)net.accept_drains, (unsigned)net.loops,
                 (unsigned)net.select_ready, (unsigned)net.select_errors,
                 (unsigned)net.listener_refreshes, (unsigned)net.usb_rejections,
                 la_usb_host_active(),
                 (unsigned)usb.attaches, (unsigned)usb.detaches,
                 (unsigned)usb.discarded, (unsigned)usb.host_rx_frames,
                 (unsigned)usb.host_rx_bytes, (unsigned)usb.host_tx_frames,
                 (unsigned)usb.host_tx_bytes, (unsigned)usb.host_frame_failures,
                 (unsigned)usb.tx_partial, (unsigned)usb.write_stalls,
                 (unsigned)usb.write_timeouts, (unsigned)usb.rx_dropped,
                 (unsigned)usb.rx_errors, (unsigned)usb.usb_stack_hwm,
                 (int)net.rssi,
                 (unsigned)esp_get_free_heap_size(), (unsigned)link.rx_stack_hwm,
                 (unsigned)link.tx_stack_hwm, (unsigned)net.net_stack_hwm,
                 (unsigned)uxTaskGetStackHighWaterMark(NULL),
                 (unsigned)link.spi_ready_low, (unsigned)link.spi_chunks,
                 (unsigned)link.spi_chunk_fail, (unsigned)link.spi_send_timeout,
                 (unsigned)link.spi_parser_resets,
                 (unsigned)link.spi_frames_total, (unsigned)link.spi_crc_total,
                 (unsigned)web.ws_clients, (unsigned)web.frames_sent,
                 (unsigned)web.frames_dropped, (unsigned)web.web_stack_hwm);
        esp_task_wdt_reset();
    }
}

esp_err_t la_ui_start(void)
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    rgb_led_off(LA_BOARD_RGB_LED_GPIO);
    rgb_led_off(LA_BOARD_RGB_LED_GPIO_ALT);
#endif
    if (xTaskCreateStaticPinnedToCore(ui_task, "UiTask",
                                      LA_BOARD_UI_TASK_STACK, NULL,
                                      LA_BOARD_UI_TASK_PRIO,
                                      s_ui_stack, &s_ui_tcb,
                                      LA_BOARD_UI_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
