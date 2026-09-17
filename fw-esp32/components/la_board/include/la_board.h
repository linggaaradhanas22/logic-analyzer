#ifndef LA_BOARD_H
#define LA_BOARD_H

#include "sdkconfig.h"

/*
 * Board pin maps for the analyzer gateway.
 *
 * P9.1 gateway (2026-09-17): ESP32-S3-DevKitC-1 with an ESP32-S3-WROOM-1-N16R8
 * module. The S3 map below matches the bench wiring and the official
 * DevKitC-1 v1.1 header map (J1 = left header, pin 1 at the 3V3 end):
 *
 *   Zybo JD-1  Zynq TX   -> GPIO16 RX    (J1-9)
 *   Zybo JD-2  Zynq RX   <- GPIO17 TX    (J1-10)
 *   Zybo JD-3  Zynq RTS  -> GPIO14 CTS   (J1-20)
 *   Zybo JD-4  Zynq CTS  <- GPIO15 RTS   (J1-8)
 *   Zybo JD-7  SPI SCK   <- GPIO12       (J1-18, FSPICLK)
 *   Zybo JD-8  SPI SS_N  <- GPIO10       (J1-16, FSPICS0)
 *   Zybo JD-9  SPI MOSI  <- GPIO11       (J1-17, FSPID)
 *   Zybo JD-10 SPI MISO  -> GPIO13       (J1-19, FSPIQ)
 *   Zybo JA-1  data rdy  -> GPIO9        (J1-15, FSPIHD)
 *
 * Reserved on the S3: GPIO19/20 (native USB D-/D+, the P9.1 CDC host
 * transport), GPIO26-37 (module flash and octal PSRAM), GPIO0/3/45/46
 * (strapping), GPIO43/44 (UART0 console through the CP2102 bridge). GPIO9-14
 * are the SPI2/FSPI IOMUX pins, so the bulk link uses the controller's native
 * pin path; the S3 has no SPI3, only SPI2_HOST.
 *
 * The classic ESP32 (DOIT DevKit V1) map is kept for the previous gateway and
 * is selected when the IDF target is esp32.
 */

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#define LA_BOARD_LINK_UART 2

#define LA_BOARD_LINK_TX_GPIO 17
#define LA_BOARD_LINK_RX_GPIO 16
#define LA_BOARD_LINK_RTS_GPIO 15
#define LA_BOARD_LINK_CTS_GPIO 14

#define LA_BOARD_SPI_HOST 1 /* SPI2_HOST (FSPI) */
#define LA_BOARD_SPI_SCK_GPIO 12
#define LA_BOARD_SPI_MOSI_GPIO 11
#define LA_BOARD_SPI_MISO_GPIO 13
#define LA_BOARD_SPI_CS_GPIO 10
#define LA_BOARD_SPI_READY_GPIO 9

/* The DevKitC-1 RGB LED is a WS2812 (GPIO38 on v1.1, GPIO48 on v1.0), not a
 * plain GPIO LED, so the UI heartbeat is disabled on this board. */
#define LA_BOARD_LED_GPIO (-1)
#define LA_BOARD_LED_ACTIVE_LOW 0

#else

/* P5.1 confirmed gateway board: DOIT ESP32 DevKit V1 (classic ESP32-WROOM-32,
 * dual core, CP2102 USB-UART, 4 MB flash, no PSRAM).
 *
 *   JD-1  Zynq TX  -> GPIO16  (UART2 RX)
 *   JD-2  Zynq RX  <- GPIO17  (UART2 TX)
 *   JD-3  Zynq RTS -> GPIO21  (UART2 CTS)
 *   JD-4  Zynq CTS <- GPIO22  (UART2 RTS)
 *   JD-5/11 GND    -- common ground
 */
#define LA_BOARD_LINK_UART 2

#define LA_BOARD_LINK_TX_GPIO 17
#define LA_BOARD_LINK_RX_GPIO 16
#define LA_BOARD_LINK_RTS_GPIO 22
#define LA_BOARD_LINK_CTS_GPIO 21

#define LA_BOARD_SPI_HOST 2 /* SPI3_HOST (VSPI) on the classic ESP32 */
#define LA_BOARD_SPI_SCK_GPIO 18
#define LA_BOARD_SPI_MOSI_GPIO 23
#define LA_BOARD_SPI_MISO_GPIO 19
#define LA_BOARD_SPI_CS_GPIO 4
#define LA_BOARD_SPI_READY_GPIO 27

/* Onboard blue LED of the DOIT DevKit V1, active low. */
#define LA_BOARD_LED_GPIO 2
#define LA_BOARD_LED_ACTIVE_LOW 1

#endif

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

/* Task placement (dual-core gateway). */
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

/* P10.1 live web view: the HTTP server runs its own task; LiveViewTask only
 * formats and broadcasts the mirrored events, so it stays below the relay. */
#define LA_BOARD_WEB_TASK_CORE 0
#define LA_BOARD_WEB_TASK_PRIO 5
#define LA_BOARD_WEB_TASK_STACK 3072
#define LA_BOARD_WEB_PORT 80
#define LA_BOARD_WEB_MAX_CLIENTS 4

/* P9.1 USB CDC host transport (la_usb): drains the relay ring into the native
 * USB port. Core 0 keeps it off the Zybo link core. */
#define LA_BOARD_USB_TASK_CORE 0
#define LA_BOARD_USB_TASK_PRIO 13
#define LA_BOARD_USB_TASK_STACK 4096

/* P9.1 SPI bulk link (docs/spi-link.md): the Zynq is the slave, the ESP32 is
 * the master. The UART above keeps commands/ACKs; this link carries the bulk
 * Zynq->ESP32 direction. */
#ifdef CONFIG_LA_SPI_CLOCK_HZ
#define LA_BOARD_SPI_CLOCK_HZ CONFIG_LA_SPI_CLOCK_HZ
#else
#define LA_BOARD_SPI_CLOCK_HZ 10000000
#endif
/* SPI master read chunk: the master clocks a fixed-size DMA chunk whenever
 * the data-ready line is high and feeds it to the incremental la_proto parser.
 * The parser resyncs on the next SOF, so a chunk boundary or an underrun can
 * never leave the master with a persistent framing offset (P9.1 bench
 * 2026-09-13: a 5-byte header/body reader lost sync after one offset and
 * collapsed to ~5 bytes per transaction). */
#define LA_BOARD_SPI_CHUNK_BYTES 1024
#define LA_BOARD_SPI_TASK_CORE 1
#define LA_BOARD_SPI_TASK_PRIO 19
#define LA_BOARD_SPI_TASK_STACK 4096

#endif /* LA_BOARD_H */
