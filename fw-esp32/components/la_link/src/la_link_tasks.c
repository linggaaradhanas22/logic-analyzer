#include "la_link.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "la_board.h"
#include "la_link_uart.h"
#include "la_proto.h"

static const char *TAG = "la_link";

/* P8.2: 64 KiB absorbs ~530 ms of a 120 kB/s event stream (the old 32 KiB
 * absorbed ~270 ms and the 60 s gate dropped frames under Wi-Fi stalls). */
#define LA_LINK_TO_NET_BYTES (64u * 1024u)
#define LA_LINK_NET_TO_LINK_BYTES (16u * 1024u)
#define LA_LINK_LIVE_BYTES (8u * 1024u)

typedef struct la_link_state {
    RingbufHandle_t to_net;
    RingbufHandle_t net_to_link;
    RingbufHandle_t live;
    QueueHandle_t uart_queue;
    la_relay_t relay_uart_to_net;
    la_relay_t relay_net_to_uart;
    la_proto_parser_t parser;
    la_link_counters_t counters;
    uint8_t control_sequence;
    uint8_t overrun_sequence;
    uint8_t frame[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    uint8_t control_frame[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    uint8_t overrun_frame[LA_PROTO_FRAME_OVERHEAD + LA_PROTO_EVT_OVERRUN_WIRE_SIZE];
} la_link_state_t;

static la_link_state_t s_link;

static uint8_t s_to_net_storage[LA_LINK_TO_NET_BYTES];
static StaticRingbuffer_t s_to_net_struct;
static uint8_t s_net_to_link_storage[LA_LINK_NET_TO_LINK_BYTES];
static StaticRingbuffer_t s_net_to_link_struct;
static uint8_t s_live_storage[LA_LINK_LIVE_BYTES];
static StaticRingbuffer_t s_live_struct;

static StackType_t s_rx_stack[LA_BOARD_LINK_RX_TASK_STACK];
static StaticTask_t s_rx_tcb;
static StackType_t s_tx_stack[LA_BOARD_LINK_TX_TASK_STACK];
static StaticTask_t s_tx_tcb;

static int to_net_capacity(void *context, size_t frame_bytes)
{
    la_link_state_t *link = (la_link_state_t *)context;

    return xRingbufferGetCurFreeSize(link->to_net) >= frame_bytes;
}

static void mirror_frame(la_link_state_t *link, uint8_t type, size_t frame_bytes)
{
    BaseType_t mirrored;

    if (!la_relay_is_mirror(&link->relay_uart_to_net, type)) {
        return;
    }
    mirrored = xRingbufferSend(link->live, link->frame, frame_bytes, 0);
    (void)la_relay_report(&link->relay_uart_to_net,
                          type,
                          mirrored == pdTRUE ? LA_RELAY_MIRROR_OK : LA_RELAY_MIRROR_FAIL);
}

/* P8.4: turn accumulated best-effort drops into one EVT_OVERRUN(stage=link)
 * for the host. The report is best-effort too: when the ring is still full it
 * stays pending and is retried on the next accepted frame. */
static void report_link_overrun(la_link_state_t *link)
{
    uint32_t dropped = la_relay_pending_drops(&link->relay_uart_to_net);
    uint8_t payload[LA_PROTO_EVT_OVERRUN_WIRE_SIZE];
    size_t frame_bytes;

    if (dropped == 0u) {
        return;
    }
    if (la_proto_pack_evt_overrun(dropped, 2u /* link stage */, payload,
                                  sizeof(payload)) != sizeof(payload)) {
        return;
    }
    frame_bytes = la_proto_encode(LA_PROTO_EVT_OVERRUN, link->overrun_sequence,
                                  payload, (uint16_t)sizeof(payload),
                                  link->overrun_frame, sizeof(link->overrun_frame));
    if (frame_bytes == 0u) {
        return;
    }
    if (xRingbufferSend(link->to_net, link->overrun_frame, frame_bytes, 0) == pdTRUE) {
        la_relay_clear_pending_drops(&link->relay_uart_to_net);
        link->overrun_sequence = (uint8_t)(link->overrun_sequence + 1u);
        link->counters.overrun_reports += 1u;
    }
}

/* P8.2: track the peak link_to_net occupancy so UiTask can report how close
 * the relay ran to the drop threshold. */
static void track_ring_usage(la_link_state_t *link)
{
    size_t free_bytes = xRingbufferGetCurFreeSize(link->to_net);
    uint32_t used = (uint32_t)(LA_LINK_TO_NET_BYTES - free_bytes);

    if (used > link->counters.to_net_max_used) {
        link->counters.to_net_max_used = used;
    }
}

static void on_uart_frame(void *context,
                          uint8_t type,
                          uint8_t sequence,
                          const uint8_t *payload,
                          uint16_t length)
{
    la_link_state_t *link = (la_link_state_t *)context;
    la_relay_decision_t decision = LA_RELAY_DROP_BEST_EFFORT;
    size_t frame_bytes;

    frame_bytes = la_proto_encode(type, sequence, payload, length, link->frame,
                                  sizeof(link->frame));
    if (frame_bytes == 0u) {
        return;
    }
    if (la_relay_accept(&link->relay_uart_to_net, type, frame_bytes,
                        to_net_capacity, link, &decision) != 0) {
        return;
    }
    if (decision == LA_RELAY_DROP_BEST_EFFORT) {
        track_ring_usage(link);
        report_link_overrun(link);
        return;
    }
    if (decision == LA_RELAY_PRESSURE) {
        /* Control frame: block until the net leg has room instead of dropping. */
        while (xRingbufferSend(link->to_net, link->frame, frame_bytes,
                               pdMS_TO_TICKS(100)) != pdTRUE) {
            esp_task_wdt_reset();
        }
    } else if (xRingbufferSend(link->to_net, link->frame, frame_bytes, 0) != pdTRUE) {
        /* The NOSPLIT ring can refuse a fragmented put after a positive
         * capacity answer; report the real outcome, never a silent drop. */
        (void)la_relay_report(&link->relay_uart_to_net, type, LA_RELAY_FWD_FAIL);
        return;
    }
    (void)la_relay_report(&link->relay_uart_to_net, type, LA_RELAY_FWD_OK);
    mirror_frame(link, type, frame_bytes);
    track_ring_usage(link);
    report_link_overrun(link);
}

static void link_rx_task(void *argument)
{
    la_link_state_t *link = &s_link;
    uart_event_t event;
    uint8_t chunk[512];

    (void)argument;
    esp_task_wdt_add(NULL);
    for (;;) {
        if (xQueueReceive(link->uart_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (event.type) {
            case UART_DATA: {
                size_t remaining = event.size;
                while (remaining > 0u) {
                    size_t take = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
                    int read = la_link_uart_read(chunk, take, pdMS_TO_TICKS(100));
                    if (read <= 0) {
                        break;
                    }
                    link->counters.uart_rx_bytes += (uint32_t)read;
                    (void)la_proto_parser_feed(&link->parser, chunk, (size_t)read,
                                               on_uart_frame, link);
                    remaining -= (size_t)read;
                }
                break;
            }
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                /* P5.5 requires this counter to stay zero: a nonzero value is a
                 * real overrun, so resync and record it instead of hiding it. */
                link->counters.fifo_ovf_events += 1u;
                la_link_uart_flush_input();
                la_proto_parser_init(&link->parser);
                ESP_LOGW(TAG, "UART overrun: flush and parser resync");
                break;
            case UART_FRAME_ERR:
            case UART_PARITY_ERR:
            case UART_BREAK:
                link->counters.line_error_events += 1u;
                break;
            default:
                break;
            }
            link->counters.frames_parsed = link->parser.frames;
            link->counters.parse_crc_errors = link->parser.crc_errors;
            link->counters.parse_format_errors = link->parser.format_errors;
            link->counters.parse_dropped_bytes = link->parser.dropped_bytes;
        }
        link->counters.rx_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
    }
}

static void link_tx_task(void *argument)
{
    la_link_state_t *link = &s_link;

    (void)argument;
    esp_task_wdt_add(NULL);
    for (;;) {
        size_t length = 0u;
        uint8_t *item = xRingbufferReceive(link->net_to_link, &length,
                                           pdMS_TO_TICKS(100));
        if (item != NULL) {
            if (la_link_uart_write_frame(item, length) == ESP_OK) {
                link->counters.uart_tx_bytes += (uint32_t)length;
            }
            vRingbufferReturnItem(link->net_to_link, item);
        }
        link->counters.tx_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
    }
}

esp_err_t la_link_start(void)
{
    esp_err_t result;

    memset(&s_link, 0, sizeof(s_link));
    s_link.to_net = xRingbufferCreateStatic(LA_LINK_TO_NET_BYTES,
                                            RINGBUF_TYPE_NOSPLIT,
                                            s_to_net_storage,
                                            &s_to_net_struct);
    s_link.net_to_link = xRingbufferCreateStatic(LA_LINK_NET_TO_LINK_BYTES,
                                                 RINGBUF_TYPE_NOSPLIT,
                                                 s_net_to_link_storage,
                                                 &s_net_to_link_struct);
    s_link.live = xRingbufferCreateStatic(LA_LINK_LIVE_BYTES,
                                          RINGBUF_TYPE_NOSPLIT,
                                          s_live_storage,
                                          &s_live_struct);
    if (s_link.to_net == NULL || s_link.net_to_link == NULL || s_link.live == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (la_relay_init(&s_link.relay_uart_to_net, LA_RELAY_ROLE_UART_TO_NET,
                      (const uint8_t[]){LA_PROTO_EVT_DECODED}, 1,
                      (const uint8_t[]){LA_PROTO_EVT_DECODED}, 1) != 0 ||
        la_relay_init(&s_link.relay_net_to_uart, LA_RELAY_ROLE_NET_TO_UART,
                      NULL, 0, NULL, 0) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    la_proto_parser_init(&s_link.parser);

    result = la_link_uart_init(&s_link.uart_queue);
    if (result != ESP_OK) {
        return result;
    }

    if (xTaskCreateStaticPinnedToCore(link_rx_task, "LinkRx",
                                      LA_BOARD_LINK_RX_TASK_STACK, NULL,
                                      LA_BOARD_LINK_RX_TASK_PRIO,
                                      s_rx_stack, &s_rx_tcb,
                                      LA_BOARD_LINK_RX_TASK_CORE) == NULL ||
        xTaskCreateStaticPinnedToCore(link_tx_task, "LinkTx",
                                      LA_BOARD_LINK_TX_TASK_STACK, NULL,
                                      LA_BOARD_LINK_TX_TASK_PRIO,
                                      s_tx_stack, &s_tx_tcb,
                                      LA_BOARD_LINK_TX_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "link tasks started (to_net %u B, net_to_link %u B, live %u B)",
             (unsigned)LA_LINK_TO_NET_BYTES,
             (unsigned)LA_LINK_NET_TO_LINK_BYTES,
             (unsigned)LA_LINK_LIVE_BYTES);
    return ESP_OK;
}

esp_err_t la_link_send_to_zynq(const uint8_t *frame, size_t length, TickType_t timeout)
{
    TickType_t start;

    if (frame == NULL || length == 0u) {
        return ESP_ERR_INVALID_ARG;
    }
    start = xTaskGetTickCount();
    for (;;) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        TickType_t remaining = elapsed < timeout ? timeout - elapsed : 0;
        if (xRingbufferSend(s_link.net_to_link, frame, length, remaining) == pdTRUE) {
            return ESP_OK;
        }
        if (elapsed >= timeout) {
            return ESP_ERR_TIMEOUT;
        }
        esp_task_wdt_reset();
    }
}

esp_err_t la_link_send_control(uint8_t type)
{
    size_t frame_bytes;
    esp_err_t result;

    frame_bytes = la_proto_encode(type, s_link.control_sequence, NULL, 0u,
                                  s_link.control_frame, sizeof(s_link.control_frame));
    if (frame_bytes == 0u) {
        return ESP_ERR_INVALID_ARG;
    }
    result = la_link_send_to_zynq(s_link.control_frame, frame_bytes, pdMS_TO_TICKS(2000));
    if (result == ESP_OK) {
        s_link.control_sequence = (uint8_t)(s_link.control_sequence + 1u);
    } else {
        s_link.counters.control_send_failures += 1u;
        ESP_LOGW(TAG, "control frame 0x%02X not queued: %s", type,
                 esp_err_to_name(result));
    }
    return result;
}

size_t la_link_receive_to_host(uint8_t **frame, TickType_t timeout)
{
    size_t length = 0u;

    if (frame == NULL) {
        return 0u;
    }
    *frame = xRingbufferReceive(s_link.to_net, &length, timeout);
    if (*frame == NULL) {
        return 0u;
    }
    return length;
}

void la_link_release_to_host(uint8_t *frame)
{
    if (frame != NULL) {
        vRingbufferReturnItem(s_link.to_net, frame);
    }
}

void la_link_stats(la_relay_stats_t *uart_to_net,
                   la_relay_stats_t *net_to_uart,
                   la_link_counters_t *counters)
{
    if (uart_to_net != NULL) {
        la_relay_snapshot(&s_link.relay_uart_to_net, uart_to_net);
    }
    if (net_to_uart != NULL) {
        la_relay_snapshot(&s_link.relay_net_to_uart, net_to_uart);
    }
    if (counters != NULL) {
        memcpy(counters, &s_link.counters, sizeof(*counters));
    }
}
