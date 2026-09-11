#ifndef LA_LINK_H
#define LA_LINK_H

/*
 * P5.2 ESP32 link adapter: UART2 with hardware RTS/CTS to the Zynq JD header,
 * the LinkRxTask/LinkTxTask pair, and the byte rings the relay policy writes
 * through. The pure routing policy lives in la_relay.{h,c}; this layer owns
 * the ESP-IDF driver, the FreeRTOS tasks, static rings and counters.
 *
 * Ring ownership (docs/rtos-design.md "ESP32 link relay core (P5.2)"):
 *   - link_to_net: LinkRxTask produces, NetServerTask consumes. NOSPLIT 32 KiB.
 *   - net_to_link: NetServerTask (the session task) is the only producer,
 *     LinkTxTask consumes. NOSPLIT 16 KiB.
 *   - live_rb: LinkRxTask mirrors EVT_DECODED for the P10 web view. The P10
 *     consumer is not implemented yet, so a full ring counts mirror_dropped.
 */

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "la_relay.h"

typedef struct la_link_counters {
    uint32_t uart_rx_bytes;
    uint32_t uart_tx_bytes;
    uint32_t frames_parsed;
    uint32_t parse_crc_errors;
    uint32_t parse_format_errors;
    uint32_t parse_dropped_bytes;
    uint32_t fifo_ovf_events;
    uint32_t line_error_events;
    uint32_t to_host_discarded;
    uint32_t control_send_failures;
    uint32_t to_net_max_used;  /* P8.2: peak link_to_net occupancy in bytes */
    uint32_t overrun_reports;  /* P8.4: EVT_OVERRUN(stage=2) frames sent */
    uint32_t rx_stack_hwm;
    uint32_t tx_stack_hwm;
} la_link_counters_t;

/* Installs the UART driver, creates the static rings and starts LinkRxTask and
 * LinkTxTask. Call once from app_main. */
esp_err_t la_link_start(void);

/* Encodes an empty-payload control frame (ABORT, LIVE_STOP, ...) with the
 * gateway's own sequence space and queues it for the Zynq. Called by the
 * session task only, so net_to_link keeps a single producer. */
esp_err_t la_link_send_control(uint8_t type);

/* Queues a verbatim encoded frame for the Zynq leg. Called by NetServerTask. */
esp_err_t la_link_send_to_zynq(const uint8_t *frame, size_t length, TickType_t timeout);

/* Returns the next Zynq -> host frame, or 0 when the ring is empty. The
 * returned buffer stays valid until la_link_release_to_host(). */
size_t la_link_receive_to_host(uint8_t **frame, TickType_t timeout);

void la_link_release_to_host(uint8_t *frame);

void la_link_stats(la_relay_stats_t *uart_to_net,
                   la_relay_stats_t *net_to_uart,
                   la_link_counters_t *counters);

#endif /* LA_LINK_H */
