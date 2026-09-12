#ifndef LA_LINK_UART_H
#define LA_LINK_UART_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Installs the UART driver on the board link port and returns the event queue
 * through *event_queue. */
esp_err_t la_link_uart_init(QueueHandle_t *event_queue);

int la_link_uart_read(uint8_t *buffer, size_t length, TickType_t timeout);

/* Bytes currently buffered by the UART driver (RX). Used to keep draining
 * when the driver's event queue loses an event under overflow. */
size_t la_link_uart_available(void);

/* Copies a whole frame into the TX ring without blocking on the driver: waits
 * (bounded, watchdog-safe) until the ring has room, then writes. */
esp_err_t la_link_uart_write_frame(const uint8_t *frame, size_t length);

void la_link_uart_flush_input(void);

#endif /* LA_LINK_UART_H */
