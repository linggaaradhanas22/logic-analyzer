#include "la_link.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/stream_buffer.h"
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
/* P9.1: SPI bytes are handed from LinkSpiTask to LinkRxTask through this
 * stream buffer so the relay keeps a single producer. Sized so a 10-20 MHz
 * SPI burst (up to ~2.5 MB/s) can fill it while LinkRxTask wakes on a short
 * UART-event timeout. */
#define LA_LINK_SPI_RX_BYTES (16u * 1024u)
/* P9.1: after this much SPI silence the parser state is dropped (a capture is
 * seconds away from the next one, while a live stream never idles this long). */
#define LA_LINK_SPI_IDLE_RESET_MS 50u
/* P9.4: a control frame waits at most this long for relay-ring room before it
 * is dropped and counted. An unbounded wait parked LinkRxTask when the host
 * stopped reading; its full UART RX buffer then held RTS low, which stalled
 * the Zynq TX path and deadlocked the whole link. */
#define LA_LINK_PRESSURE_TIMEOUT_MS 2000u

typedef struct la_link_state {
    RingbufHandle_t to_net;
    RingbufHandle_t net_to_link;
    RingbufHandle_t live;
    QueueHandle_t uart_queue;
    StreamBufferHandle_t spi_rx_sb;
    la_relay_t relay_uart_to_net;
    la_relay_t relay_net_to_uart;
    la_proto_parser_t parser;
    la_proto_parser_t spi_parser;
    la_link_counters_t counters;
    uint8_t control_sequence;
    uint8_t overrun_sequence;
    uint8_t spi_parser_active;   /* bytes were fed since the last idle gap */
    uint32_t spi_last_rx_ms;     /* last SPI byte arrival (session boundary) */
    uint8_t frame[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    uint8_t spi_frame[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
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
static uint8_t s_spi_rx_storage[LA_LINK_SPI_RX_BYTES];
static StaticStreamBuffer_t s_spi_rx_struct;

static StackType_t s_rx_stack[LA_BOARD_LINK_RX_TASK_STACK];
static StaticTask_t s_rx_tcb;
static StackType_t s_tx_stack[LA_BOARD_LINK_TX_TASK_STACK];
static StaticTask_t s_tx_tcb;
static StackType_t s_spi_stack[LA_BOARD_SPI_TASK_STACK];
static StaticTask_t s_spi_tcb;
static volatile uint32_t s_spi_clock_request;

static int to_net_capacity(void *context, size_t frame_bytes)
{
    la_link_state_t *link = (la_link_state_t *)context;

    return xRingbufferGetCurFreeSize(link->to_net) >= frame_bytes;
}

static void mirror_frame(la_link_state_t *link, const uint8_t *frame, uint8_t type,
                         size_t frame_bytes)
{
    BaseType_t mirrored;

    if (!la_relay_is_mirror(&link->relay_uart_to_net, type)) {
        return;
    }
    mirrored = xRingbufferSend(link->live, (void *)frame, frame_bytes, 0);
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

/* Re-encodes one parsed frame into the relay and applies the P5.2 policy.
 * Both the UART and the SPI parser call this; each passes its own scratch
 * buffer, and LinkRxTask is the only caller so the relay stays
 * single-producer. */
static void relay_frame(la_link_state_t *link, uint8_t type, uint8_t sequence,
                        const uint8_t *payload, uint16_t length,
                        uint8_t *scratch, size_t scratch_size)
{
    la_relay_decision_t decision = LA_RELAY_DROP_BEST_EFFORT;
    size_t frame_bytes;

    frame_bytes = la_proto_encode(type, sequence, payload, length, scratch,
                                  scratch_size);
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
    /* Control frame: block until the net leg has room instead of dropping, but
     * never forever (see LA_LINK_PRESSURE_TIMEOUT_MS). The capacity check is
     * optimistic for a NOSPLIT ring: when the ring wraps, xRingbufferSend can
     * fail even with enough total free space, so both the pressure decision
     * and a failed send retry here. A silent FWD_FAIL here dropped 39 of 94
     * parsed frames in the 4 MB drain attempt (bench 2026-09-13). */
    {
        TickType_t start = xTaskGetTickCount();

        for (;;) {
            if (xRingbufferSend(link->to_net, scratch, frame_bytes,
                                pdMS_TO_TICKS(100)) == pdTRUE) {
                break;
            }
            esp_task_wdt_reset();
            if ((xTaskGetTickCount() - start) >
                pdMS_TO_TICKS(LA_LINK_PRESSURE_TIMEOUT_MS)) {
                link->counters.pressure_timeouts += 1u;
                (void)la_relay_report(&link->relay_uart_to_net, type,
                                      LA_RELAY_FWD_FAIL);
                return;
            }
        }
    }
    (void)la_relay_report(&link->relay_uart_to_net, type, LA_RELAY_FWD_OK);
    mirror_frame(link, scratch, type, frame_bytes);
    track_ring_usage(link);
    report_link_overrun(link);
}

static void on_uart_frame(void *context, uint8_t type, uint8_t sequence,
                          const uint8_t *payload, uint16_t length)
{
    la_link_state_t *link = (la_link_state_t *)context;

    relay_frame(link, type, sequence, payload, length, link->frame,
                sizeof(link->frame));
}

static void on_spi_frame(void *context, uint8_t type, uint8_t sequence,
                         const uint8_t *payload, uint16_t length)
{
    la_link_state_t *link = (la_link_state_t *)context;

    relay_frame(link, type, sequence, payload, length, link->spi_frame,
                sizeof(link->spi_frame));
}

static void drain_buffered(la_link_state_t *link, uint8_t *chunk, size_t chunk_size,
                           int count_rescue)
{
    size_t available = la_link_uart_available();

    while (available > 0u) {
        size_t take = available < chunk_size ? available : chunk_size;
        int read = la_link_uart_read(chunk, take, 0);

        if (read <= 0) {
            break;
        }
        link->counters.uart_rx_bytes += (uint32_t)read;
        if (count_rescue != 0) {
            link->counters.rx_rescue_bytes += (uint32_t)read;
        }
        (void)la_proto_parser_feed(&link->parser, chunk, (size_t)read,
                                   on_uart_frame, link);
        available = la_link_uart_available();
    }
}

static void link_rx_task(void *argument)
{
    la_link_state_t *link = &s_link;
    uart_event_t event;
    uint8_t chunk[512];

    (void)argument;
    esp_task_wdt_add(NULL);
#if CONFIG_LA_LINK_WDT_TEST_HOOK
    /* P8.4 fault injection: stop servicing the watchdog so the 5 s task WDT
     * panics and resets the gateway. Never enabled in a normal build. */
    ESP_LOGE(TAG, "P8.4 WDT test hook active: hanging LinkRxTask");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif
    for (;;) {
        int handled_data = 0;

        /* Short timeout: the SPI stream buffer drain below must not wait for
         * a UART event (P9.1), and the event-less rescue drain stays prompt. */
        if (xQueueReceive(link->uart_queue, &event, pdMS_TO_TICKS(5)) == pdTRUE) {
            switch (event.type) {
            case UART_DATA: {
                size_t remaining = event.size;
                handled_data = 1;
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
        }
        /* The driver's event queue can lose a UART_DATA event when it overflows.
         * Always drain the driver buffer so the RX path (and the RTS flow
         * control that depends on it) cannot deadlock with a stalled host. */
        drain_buffered(link, chunk, sizeof(chunk), handled_data == 0 ? 1 : 0);
        /* P9.1: drain the SPI bulk bytes handed over by LinkSpiTask. The
         * parser is reset after an idle gap so a partial frame left by an
         * aborted capture can never swallow the next capture's DATA_BEGIN
         * (the Zynq also flushes its ring at every burst job start). */
        for (;;) {
            size_t spi_bytes = xStreamBufferReceive(link->spi_rx_sb, chunk,
                                                    sizeof(chunk), 0);
            if (spi_bytes == 0u) {
                break;
            }
            link->spi_parser_active = 1u;
            link->spi_last_rx_ms = (uint32_t)(esp_timer_get_time() / 1000);
            (void)la_proto_parser_feed(&link->spi_parser, chunk, spi_bytes,
                                       on_spi_frame, link);
        }
        if (link->spi_parser_active != 0u &&
            ((uint32_t)(esp_timer_get_time() / 1000) - link->spi_last_rx_ms) >
                LA_LINK_SPI_IDLE_RESET_MS) {
            link->counters.spi_frames_total += link->spi_parser.frames;
            link->counters.spi_crc_total += link->spi_parser.crc_errors +
                                            link->spi_parser.format_errors;
            la_proto_parser_init(&link->spi_parser);
            link->spi_parser_active = 0u;
            link->counters.spi_parser_resets += 1u;
        }
        link->counters.frames_parsed = link->parser.frames;
        link->counters.parse_crc_errors = link->parser.crc_errors;
        link->counters.parse_format_errors = link->parser.format_errors;
        link->counters.parse_dropped_bytes = link->parser.dropped_bytes;
        link->counters.spi_frames = link->spi_parser.frames;
        link->counters.spi_crc_errors = link->spi_parser.crc_errors +
                                        link->spi_parser.format_errors;
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

/* P9.1 SPI master: while the JA-1 data-ready line is high it clocks fixed-size
 * DMA chunks and hands every byte to LinkRxTask through the SPI stream buffer
 * (the relay keeps a single producer). The incremental la_proto parser
 * resyncs on the next SOF, so chunk boundaries, data-ready drops and underrun
 * zeros cannot leave the master with a persistent framing offset (bench
 * 2026-09-13: the old 5-byte header/body reader lost sync after one offset
 * and collapsed to ~5 bytes per transaction, starving the Zynq ring). */
static void link_spi_task(void *argument)
{
    la_link_state_t *link = &s_link;
    spi_bus_config_t bus = {
        .mosi_io_num = LA_BOARD_SPI_MOSI_GPIO,
        .miso_io_num = LA_BOARD_SPI_MISO_GPIO,
        .sclk_io_num = LA_BOARD_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LA_BOARD_SPI_CHUNK_BYTES,
    };
    spi_device_interface_config_t device = {
        .clock_speed_hz = LA_BOARD_SPI_CLOCK_HZ,
        .mode = 0,
        /* CS is driven manually around each chunk: the axi_quad_spi slave only
         * enables its MISO driver after spisel asserts, and the driver's
         * cs_ena_pretrans only applies to half-duplex transactions. */
        .spics_io_num = -1,
        .queue_size = 1,
    };
    gpio_config_t cs = {
        .pin_bit_mask = 1ULL << LA_BOARD_SPI_CS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config_t ready = {
        .pin_bit_mask = 1ULL << LA_BOARD_SPI_READY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    spi_device_handle_t handle = NULL;
    uint8_t *body = heap_caps_malloc(LA_BOARD_SPI_CHUNK_BYTES, MALLOC_CAP_DMA);

    (void)argument;
    esp_task_wdt_add(NULL);
    (void)gpio_config(&cs);
    gpio_set_level(LA_BOARD_SPI_CS_GPIO, 1);
    (void)gpio_config(&ready);
    if (body == NULL ||
        spi_bus_initialize((spi_host_device_t)LA_BOARD_SPI_HOST, &bus,
                           SPI_DMA_CH_AUTO) != ESP_OK ||
        spi_bus_add_device((spi_host_device_t)LA_BOARD_SPI_HOST, &device,
                           &handle) != ESP_OK) {
        ESP_LOGE(TAG, "SPI master init failed");
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "SPI master %u Hz sck=%d mosi=%d miso=%d cs=%d ready=%d chunk=%d",
             (unsigned)LA_BOARD_SPI_CLOCK_HZ, LA_BOARD_SPI_SCK_GPIO,
             LA_BOARD_SPI_MOSI_GPIO, LA_BOARD_SPI_MISO_GPIO,
             LA_BOARD_SPI_CS_GPIO, LA_BOARD_SPI_READY_GPIO,
             (int)LA_BOARD_SPI_CHUNK_BYTES);
    {
        uint32_t last_bytes = 0u;
        TickType_t last_report = xTaskGetTickCount();

        for (;;) {
            int level = gpio_get_level(LA_BOARD_SPI_READY_GPIO);
            int do_read = 1;

            /* P9.1 bench: apply a LINK_CFG clock change before the next chunk.
             * Re-adding the device is the only way to change the ESP-IDF SPI
             * master clock; no transaction is in flight here. */
            {
                uint32_t requested = s_spi_clock_request;

                if (requested != 0u &&
                    requested != (uint32_t)device.clock_speed_hz) {
                    if (spi_bus_remove_device(handle) == ESP_OK) {
                        device.clock_speed_hz = (int)requested;
                        if (spi_bus_add_device((spi_host_device_t)LA_BOARD_SPI_HOST,
                                               &device, &handle) == ESP_OK) {
                            ESP_LOGI(TAG, "SPI clock -> %u Hz",
                                     (unsigned)requested);
                        } else {
                            ESP_LOGE(TAG, "SPI clock change failed");
                        }
                    }
                    s_spi_clock_request = 0u;
                }
            }

#if CONFIG_LA_SPI_READY_REQUIRED
            do_read = level != 0;
#endif
            if (do_read != 0) {
                spi_transaction_t chunk_tx = {
                    .length = LA_BOARD_SPI_CHUNK_BYTES * 8u,
                    .rx_buffer = body,
                };

                gpio_set_level(LA_BOARD_SPI_CS_GPIO, 0);
                esp_rom_delay_us(2);
                if (spi_device_transmit(handle, &chunk_tx) == ESP_OK) {
                    size_t written;

                    link->counters.spi_chunks += 1u;
                    written = xStreamBufferSend(link->spi_rx_sb, body,
                                                LA_BOARD_SPI_CHUNK_BYTES,
                                                pdMS_TO_TICKS(100));
                    link->counters.spi_bytes += (uint32_t)written;
                    if (written != LA_BOARD_SPI_CHUNK_BYTES) {
                        link->counters.spi_send_timeout += 1u;
                    }
                } else {
                    link->counters.spi_chunk_fail += 1u;
                }
                gpio_set_level(LA_BOARD_SPI_CS_GPIO, 1);
            } else {
                link->counters.spi_ready_low += 1u;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            if ((xTaskGetTickCount() - last_report) > pdMS_TO_TICKS(5000)) {
                last_report = xTaskGetTickCount();
                if (link->counters.spi_bytes == last_bytes) {
                    ESP_LOGW(TAG, "SPI idle: ready=%d bytes=%u", level,
                             (unsigned)link->counters.spi_bytes);
                }
                last_bytes = link->counters.spi_bytes;
            }
            link->counters.spi_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
            esp_task_wdt_reset();
        }
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
    la_proto_parser_init(&s_link.spi_parser);
    s_link.spi_rx_sb = xStreamBufferCreateStatic(LA_LINK_SPI_RX_BYTES, 1u,
                                                 s_spi_rx_storage,
                                                 &s_spi_rx_struct);
    if (s_link.spi_rx_sb == NULL) {
        return ESP_ERR_NO_MEM;
    }

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
#if CONFIG_LA_SPI_ENABLE
    if (xTaskCreateStaticPinnedToCore(link_spi_task, "LinkSpi",
                                      LA_BOARD_SPI_TASK_STACK, NULL,
                                      LA_BOARD_SPI_TASK_PRIO,
                                      s_spi_stack, &s_spi_tcb,
                                      LA_BOARD_SPI_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
#endif
    ESP_LOGI(TAG, "link tasks started (to_net %u B, net_to_link %u B, live %u B, spi_rx %u B)",
             (unsigned)LA_LINK_TO_NET_BYTES,
             (unsigned)LA_LINK_NET_TO_LINK_BYTES,
             (unsigned)LA_LINK_LIVE_BYTES,
             (unsigned)LA_LINK_SPI_RX_BYTES);
    return ESP_OK;
}

esp_err_t la_link_spi_set_clock(uint32_t hz)
{
    if (hz != 0u && hz < 100000u) {
        return ESP_ERR_INVALID_ARG;
    }
    s_spi_clock_request = hz;
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
