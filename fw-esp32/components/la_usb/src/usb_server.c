#include "la_usb.h"

#include <string.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "la_board.h"
#include "la_link.h"
#include "la_proto.h"

static const char *TAG = "la_usb";

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"

#define LA_USB_RX_STREAM_BYTES (4u * 1024u)
#define LA_USB_RX_CB_BYTES 256u
#define LA_USB_PARSE_CHUNK 512u
#define LA_USB_TX_BUDGET 64u
#define LA_USB_FORWARD_TIMEOUT_MS 2000u
#define LA_USB_FLUSH_WAIT_TICKS pdMS_TO_TICKS(50)

typedef struct la_usb_state {
    StreamBufferHandle_t rx_sb;
    la_proto_parser_t parser;
    la_usb_counters_t counters;
    volatile int dtr;    /* host has the CDC port open (line-state callback) */
    volatile int active; /* task has processed the attach */
    uint8_t rx_cb_buf[LA_USB_RX_CB_BYTES];
    uint8_t parse_buf[LA_USB_PARSE_CHUNK];
} la_usb_state_t;

static la_usb_state_t s_usb;
static StaticStreamBuffer_t s_rx_sb_struct;
static uint8_t s_rx_sb_storage[LA_USB_RX_STREAM_BYTES];
static StackType_t s_usb_stack[LA_BOARD_USB_TASK_STACK];
static StaticTask_t s_usb_tcb;

static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    size_t rx_size = 0u;
    size_t sent;

    (void)event;
    if (tinyusb_cdcacm_read(itf, s_usb.rx_cb_buf, sizeof(s_usb.rx_cb_buf),
                            &rx_size) != ESP_OK) {
        s_usb.counters.rx_errors += 1u;
        return;
    }
    if (rx_size == 0u) {
        return;
    }
    sent = xStreamBufferSend(s_usb.rx_sb, s_usb.rx_cb_buf, rx_size, 0);
    if (sent != rx_size) {
        s_usb.counters.rx_dropped += (uint32_t)(rx_size - sent);
    }
}

static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    s_usb.dtr = event->line_state_changed_data.dtr ? 1 : 0;
}

/* A new host session must never inherit the previous session's tail: drop any
 * frames left in the relay ring and reset the command parser (same rule as
 * the TCP leg's client disconnect). */
static void host_attached(void)
{
    uint8_t *frame = NULL;
    size_t length;

    xStreamBufferReset(s_usb.rx_sb);
    la_proto_parser_init(&s_usb.parser);
    while ((length = la_link_receive_to_host(&frame, 0)) != 0u) {
        la_link_release_to_host(frame);
        s_usb.counters.discarded += 1u;
    }
    s_usb.active = 1;
    s_usb.counters.attaches += 1u;
    ESP_LOGI(TAG, "host attached: USB CDC owns the analyzer stream");
}

/* Mirror the TCP client-disconnect policy: a host that vanishes mid-burst or
 * mid-live must stop the Zynq instead of leaving it streaming into a full
 * ring. */
static void host_detached(void)
{
    s_usb.active = 0;
    s_usb.counters.detaches += 1u;
    (void)la_link_send_control(LA_PROTO_ABORT);
    (void)la_link_send_control(LA_PROTO_LIVE_STOP);
    ESP_LOGI(TAG, "host detached");
}

static void on_host_frame(void *context, uint8_t type, uint8_t sequence,
                          const uint8_t *payload, uint16_t length)
{
    (void)context;
    if (la_link_forward_host_frame(type, sequence, payload, length,
                                   pdMS_TO_TICKS(LA_USB_FORWARD_TIMEOUT_MS)) != ESP_OK) {
        s_usb.counters.host_frame_failures += 1u;
    } else {
        s_usb.counters.host_rx_frames += 1u;
    }
}

static void pump_rx(void)
{
    for (;;) {
        size_t received = xStreamBufferReceive(s_usb.rx_sb, s_usb.parse_buf,
                                               sizeof(s_usb.parse_buf), 0);

        if (received == 0u) {
            return;
        }
        s_usb.counters.host_rx_bytes += (uint32_t)received;
        (void)la_proto_parser_feed(&s_usb.parser, s_usb.parse_buf, received,
                                   on_host_frame, NULL);
    }
}

/* The CDC FIFO accepts partial writes: queue what fits, flush, and retry the
 * remainder. The ring item stays valid until released, so no staging copy is
 * needed for the bulk direction. */
static void pump_tx(void)
{
    unsigned budget = LA_USB_TX_BUDGET;

    while (budget-- > 0u && s_usb.active != 0) {
        uint8_t *frame = NULL;
        size_t length = la_link_receive_to_host(&frame, pdMS_TO_TICKS(2));
        size_t offset = 0u;

        if (length == 0u) {
            return;
        }
        while (offset < length && s_usb.active != 0) {
            size_t queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                                       frame + offset,
                                                       length - offset);
            if (queued == 0u) {
                s_usb.counters.write_stalls += 1u;
                if (tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0,
                                               LA_USB_FLUSH_WAIT_TICKS) ==
                    ESP_ERR_TIMEOUT) {
                    s_usb.counters.write_timeouts += 1u;
                }
                continue;
            }
            offset += queued;
            (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
        }
        la_link_release_to_host(frame);
        if (offset == length) {
            s_usb.counters.host_tx_frames += 1u;
            s_usb.counters.host_tx_bytes += (uint32_t)length;
        } else {
            s_usb.counters.tx_partial += 1u;
        }
    }
}

static void usb_host_task(void *argument)
{
    tinyusb_config_cdcacm_t acm = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = cdc_rx_callback,
        .callback_line_state_changed = cdc_line_state_callback,
    };
    tinyusb_config_t tusb = TINYUSB_DEFAULT_CONFIG();

    (void)argument;
    /* The default TinyUSB task priority (5) is below every network task; a
     * Wi-Fi reconnect storm has been observed to keep the control endpoint
     * unresponsive while the host tries to open the port. */
    tusb.task.priority = LA_BOARD_USB_TINYUSB_TASK_PRIO;
    if (tinyusb_driver_install(&tusb) != ESP_OK ||
        tinyusb_cdcacm_init(&acm) != ESP_OK) {
        ESP_LOGE(TAG, "USB CDC init failed");
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "USB CDC ready on the native USB port (full speed)");
    esp_task_wdt_add(NULL);

    for (;;) {
        if (s_usb.dtr != 0 && s_usb.active == 0) {
            host_attached();
        } else if (s_usb.dtr == 0 && s_usb.active != 0) {
            host_detached();
        }
        if (s_usb.active != 0) {
            pump_rx();
            pump_tx();
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        s_usb.counters.usb_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
    }
}

esp_err_t la_usb_start(void)
{
    memset(&s_usb, 0, sizeof(s_usb));
    la_proto_parser_init(&s_usb.parser);
    s_usb.rx_sb = xStreamBufferCreateStatic(LA_USB_RX_STREAM_BYTES, 1u,
                                            s_rx_sb_storage, &s_rx_sb_struct);
    if (s_usb.rx_sb == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreateStaticPinnedToCore(usb_host_task, "UsbHost",
                                      LA_BOARD_USB_TASK_STACK, NULL,
                                      LA_BOARD_USB_TASK_PRIO,
                                      s_usb_stack, &s_usb_tcb,
                                      LA_BOARD_USB_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

int la_usb_host_active(void)
{
    return s_usb.dtr;
}

void la_usb_stats(la_usb_counters_t *counters)
{
    if (counters != NULL) {
        memcpy(counters, &s_usb.counters, sizeof(*counters));
    }
}

#else /* !CONFIG_IDF_TARGET_ESP32S3: stub for boards without a native USB port */

esp_err_t la_usb_start(void)
{
    return ESP_OK;
}

int la_usb_host_active(void)
{
    return 0;
}

void la_usb_stats(la_usb_counters_t *counters)
{
    if (counters != NULL) {
        memset(counters, 0, sizeof(*counters));
    }
}

#endif
