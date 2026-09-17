#ifndef LA_USB_H
#define LA_USB_H

/*
 * P9.1 USB CDC-ACM host transport (ESP32-S3 native USB port).
 *
 * The analyzer host leg over TCP (:5555) is bandwidth-limited by Wi-Fi. This
 * component adds a second host leg over the S3's native USB OTG port: the
 * gateway enumerates as a USB CDC-ACM serial device and the host opens it with
 * the existing la-cli/e2e serial transport (`serial://COMx`).
 *
 * Ownership: while the host has the CDC port open (DTR asserted), the USB leg
 * owns the analyzer stream. NetServerTask stops consuming the relay ring and
 * refuses TCP analyzer clients so frames are never split between legs. When
 * the port closes, the TCP leg resumes. The P10 web view uses the separate
 * live mirror ring and is unaffected.
 *
 * Only the esp32s3 target implements the transport; other targets compile a
 * stub so the net server can call the same API.
 */

#include <stdint.h>

#include "esp_err.h"

typedef struct la_usb_counters {
    uint32_t attaches;           /* host opened the CDC port */
    uint32_t detaches;           /* host closed the CDC port */
    uint32_t discarded;          /* stale frames dropped at attach */
    uint32_t host_rx_frames;     /* host -> Zynq frames queued */
    uint32_t host_rx_bytes;
    uint32_t host_tx_frames;     /* Zynq -> host frames written */
    uint32_t host_tx_bytes;
    uint32_t host_frame_failures;
    uint32_t tx_partial;         /* frames cut short by a detach */
    uint32_t write_stalls;       /* CDC FIFO full events */
    uint32_t write_timeouts;     /* flush timed out while stalled */
    uint32_t rx_dropped;         /* callback could not buffer received bytes */
    uint32_t rx_errors;          /* tinyusb_cdcacm_read failed */
    uint32_t usb_stack_hwm;
} la_usb_counters_t;

/* Starts the USB host task (TinyUSB install + CDC init happen in the task).
 * Call once from app_main after la_link_start(). */
esp_err_t la_usb_start(void);

/* 1 while the host has the CDC port open (DTR). The TCP leg checks this
 * before consuming the relay ring or accepting analyzer clients. */
int la_usb_host_active(void);

void la_usb_stats(la_usb_counters_t *counters);

#endif /* LA_USB_H */
