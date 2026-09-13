#ifndef LA_BOARD_H
#define LA_BOARD_H

#include "sdkconfig.h"

/*
 * P5.1 confirmed gateway board: DOIT ESP32 DevKit V1 (classic ESP32-WROOM-32,
 * dual core, CP2102 USB-UART, 4 MB flash, no PSRAM).
 *
 * The analyzer link uses UART2 on the right-hand header row so the Zybo JD
 * ribbon and the breadboard wiring stay on one side. The mapping replaces the
 * provisional GPIO16/17/14/15 from docs/wiring.md:
 *
 *   JD-1  Zynq TX  -> GPIO16  (UART2 RX)
 *   JD-2  Zynq RX  <- GPIO17  (UART2 TX)
 *   JD-3  Zynq RTS -> GPIO21  (UART2 CTS)
 *   JD-4  Zynq CTS <- GPIO22  (UART2 RTS)
 *   JD-5/11 GND    -- common ground
 *
 * GPIO16/17 are the UART2 defaults; GPIO21/22 avoid the GPIO15 strapping pin
 * and the flash pins. JD-7..10 stay reserved for the P9 SPI link.
 */

#define LA_BOARD_LINK_UART 2

#define LA_BOARD_LINK_TX_GPIO 17
#define LA_BOARD_LINK_RX_GPIO 16
#define LA_BOARD_LINK_RTS_GPIO 22
#define LA_BOARD_LINK_CTS_GPIO 21

/* Link rate; must match the Zynq LA_LINK_BAUD. P5.5 ran the loopback gate at
 * 921600 and then 2 Mbps through the LA_LINK_BAUD Kconfig option (sdkconfig,
 * never committed); the P7.6 live gate moved the default to 2 Mbps. */
#ifdef CONFIG_LA_LINK_BAUD
#define LA_BOARD_LINK_BAUD CONFIG_LA_LINK_BAUD
#else
#define LA_BOARD_LINK_BAUD 2000000
#endif

/* Driver buffers and thresholds (P5.1: 16 KiB UART RX buffer, ~64-byte RX
 * FIFO threshold, 4 KiB TX ring so a maximum 1031-byte frame always fits). */
#define LA_BOARD_LINK_RX_BUFFER 16384
#define LA_BOARD_LINK_TX_BUFFER 4096
#define LA_BOARD_LINK_RX_FIFO_THRESH 64
#define LA_BOARD_LINK_RX_TIMEOUT_SYMS 10
#define LA_BOARD_LINK_UART_EVENT_QUEUE 20

#define LA_BOARD_TCP_PORT 5555
#define LA_BOARD_MDNS_HOSTNAME "la-esp32"
#define LA_BOARD_MDNS_SERVICE "_la-analyzer"
#define LA_BOARD_MDNS_PROTO "_tcp"

/* Task placement per docs/plan.md P1.6 (classic dual-core ESP32). */
#define LA_BOARD_LINK_RX_TASK_CORE 1
#define LA_BOARD_LINK_RX_TASK_PRIO 20
#define LA_BOARD_LINK_RX_TASK_STACK 4096

#define LA_BOARD_LINK_TX_TASK_CORE 1
#define LA_BOARD_LINK_TX_TASK_PRIO 18
#define LA_BOARD_LINK_TX_TASK_STACK 3072

#define LA_BOARD_NET_TASK_CORE 0
#define LA_BOARD_NET_TASK_PRIO 12
#define LA_BOARD_NET_TASK_STACK 5120

#define LA_BOARD_UI_TASK_CORE 0
#define LA_BOARD_UI_TASK_PRIO 5
#define LA_BOARD_UI_TASK_STACK 3072

/* Onboard blue LED of the DOIT DevKit V1, active low. */
#define LA_BOARD_LED_GPIO 2
#define LA_BOARD_LED_ACTIVE_LOW 1

/* P9.1 SPI bulk link (docs/spi-link.md): the Zynq is the slave, the ESP32 is
 * the master. The UART above keeps commands/ACKs; this link carries the bulk
 * Zynq->ESP32 direction. Pins are provisional until the bench wiring check. */
#define LA_BOARD_SPI_HOST 2 /* SPI3_HOST (VSPI) on the classic ESP32 */
#define LA_BOARD_SPI_SCK_GPIO 18
#define LA_BOARD_SPI_MOSI_GPIO 23
#define LA_BOARD_SPI_MISO_GPIO 19
#define LA_BOARD_SPI_CS_GPIO 4
#define LA_BOARD_SPI_READY_GPIO 27
#ifdef CONFIG_LA_SPI_CLOCK_HZ
#define LA_BOARD_SPI_CLOCK_HZ CONFIG_LA_SPI_CLOCK_HZ
#else
#define LA_BOARD_SPI_CLOCK_HZ 10000000
#endif
/* Largest SPI frame body the master clocks in one transaction: one la_proto
 * payload (1024 B) plus its 2-byte CRC. LinkSpiTask reads the 5-byte header
 * first and sizes the body transaction from the frame length field, so a
 * data-ready drop between frames becomes an idle gap instead of zeros inside
 * a frame (P9.1 bench 2026-09-13). */
#define LA_BOARD_SPI_FRAME_BODY_MAX 1026
#define LA_BOARD_SPI_TASK_CORE 1
#define LA_BOARD_SPI_TASK_PRIO 19
#define LA_BOARD_SPI_TASK_STACK 4096

#endif /* LA_BOARD_H */
