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

static const char *TAG = "la_ui";

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
    uint32_t led_phase = 0u;

    (void)argument;
    esp_task_wdt_add(NULL);

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << LA_BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&led);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        la_link_stats(&up, &down, &link);
        la_net_stats(&policy, &state, &net);

#if LA_BOARD_LED_ACTIVE_LOW
        int level = (state == LA_NET_POLICY_CLIENT_ACTIVE) ? 0 : (int)(led_phase & 1u);
#else
        int level = (state == LA_NET_POLICY_CLIENT_ACTIVE) ? 1 : (int)(led_phase & 1u);
#endif
        (void)gpio_set_level(LA_BOARD_LED_GPIO, level);
        led_phase += 1u;

        if ((led_phase & 3u) != 0u) {
            esp_task_wdt_reset();
            continue;
        }
        ESP_LOGI(TAG,
                 "link fwd=%u drop=%u press=%u mirror=%u/%u fifo_ovf=%u line_err=%u "
                 "touse=%u rep=%u rx_bytes=%u tx_bytes=%u | net %s clients=%u/%u "
                 "disc=%u wifidrop=%u rx_frames=%u tx_frames=%u to_host_discard=%u | "
                 "rssi=%d heap=%u stacks rx=%u tx=%u net=%u ui=%u",
                 (unsigned)up.frames_forwarded, (unsigned)up.best_effort_dropped,
                 (unsigned)up.control_pressure_events, (unsigned)up.frames_mirrored,
                 (unsigned)up.mirror_dropped, (unsigned)link.fifo_ovf_events,
                 (unsigned)link.line_error_events, (unsigned)link.to_net_max_used,
                 (unsigned)link.overrun_reports, (unsigned)link.uart_rx_bytes,
                 (unsigned)link.uart_tx_bytes, policy_state_name(state),
                 (unsigned)policy.clients_accepted, (unsigned)policy.clients_rejected,
                 (unsigned)policy.client_disconnects, (unsigned)net.wifi_event_drops,
                 (unsigned)net.client_rx_frames, (unsigned)net.client_tx_frames,
                 (unsigned)net.to_host_discarded, (int)net.rssi,
                 (unsigned)esp_get_free_heap_size(), (unsigned)link.rx_stack_hwm,
                 (unsigned)link.tx_stack_hwm, (unsigned)net.net_stack_hwm,
                 (unsigned)uxTaskGetStackHighWaterMark(NULL));
        esp_task_wdt_reset();
    }
}

esp_err_t la_ui_start(void)
{
    if (xTaskCreateStaticPinnedToCore(ui_task, "UiTask",
                                      LA_BOARD_UI_TASK_STACK, NULL,
                                      LA_BOARD_UI_TASK_PRIO,
                                      s_ui_stack, &s_ui_tcb,
                                      LA_BOARD_UI_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
