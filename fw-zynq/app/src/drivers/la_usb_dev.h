#ifndef LA_USB_DEV_H
#define LA_USB_DEV_H

/*
 * U03 direct-USB CDC-ACM device driver (XUsbPs wrapper).
 *
 * Interrupt discipline (docs/usb-link.md section 9): the GIC handler is the
 * vendor XUsbPs_IntrHandler, which performs bounded register work and invokes
 * the endpoint callbacks below. The callbacks only (a) copy the 8-byte setup
 * packet, (b) pop a completed OUT buffer with XUsbPs_EpBufferReceive - which
 * must happen in interrupt context because the vendor RX dispatcher advances
 * its descriptor cursor per callback - and (c) push a completion record and
 * notify UsbTask. Class processing, payload copies, stream-buffer writes,
 * submissions and re-priming all run in UsbTask.
 *
 * Buffer ownership: the driver's DMA pool lives in the non-cached DDR window;
 * RX completions travel by handle and are released only by UsbTask. TX frames
 * are copied into the static pool by TxTask (la_usb_enqueue), submitted by
 * UsbTask and returned by the IN completion callback through the counting
 * semaphore. A partial frame is never submitted.
 */

#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"

#include "la_usb_core.h"
#include "la_usb_trace.h"
#include "xparameters.h"
#include "xusbps.h"

#include <stddef.h>
#include <stdint.h>

#define LA_USB_EP0_BUF_SIZE 64u
#define LA_USB_EP0_BUF_COUNT 2u
#define LA_USB_BULK_BUF_SIZE 512u
#define LA_USB_BULK_BUF_COUNT 16u
#define LA_USB_NOTIFY_BUF_COUNT 2u
#define LA_USB_NOTIFY_BUF_SIZE 16u
#define LA_USB_DMA_POOL_BYTES (64u * 1024u)
#define LA_USB_TX_FRAME_BYTES 4160u
/* 16 slots: one full OUT dTD ring worth of ACKs can be in flight while the
 * host is still writing (2026-09-19 200-record burst). */
#define LA_USB_TX_FRAME_COUNT 16u
#define LA_USB_TX_WAIT_MS 500u
#define LA_USB_GIC_ENCODED XPAR_XUSBPS_0_INTERRUPTS

/* UsbTask notification bits (interrupt callbacks and TxTask). */
#define LA_USB_NOTIFY_EP0_SETUP ((uint32_t)0x00000001u)
#define LA_USB_NOTIFY_RX_DONE ((uint32_t)0x00000002u)
#define LA_USB_NOTIFY_TX_DONE ((uint32_t)0x00000004u)
#define LA_USB_NOTIFY_SESSION ((uint32_t)0x00000008u)
#define LA_USB_NOTIFY_TX_FRAME ((uint32_t)0x00000010u)
#define LA_USB_NOTIFY_ALL ((uint32_t)0x0000001Fu)

/* Matches the OUT dTD ring depth: a full controller burst can be staged
 * before the task drains it (2026-09-19 200-record burst). */
#define LA_USB_STAGE_COUNT 16u

typedef struct la_usb_stage {
    uint8_t data[LA_USB_BULK_BUF_SIZE] __attribute__((aligned(64)));
    uint16_t len;
    uint8_t ep;
} la_usb_stage_t;

typedef struct la_usb_dev {
    XUsbPs usb;
    XUsbPs_DeviceConfig device_config;
    TaskHandle_t task;
    StreamBufferHandle_t rx_sb;
    la_usb_session_t session;

    StaticSemaphore_t tx_sem_struct;
    SemaphoreHandle_t tx_free_sem;
    la_usb_stage_t stages[LA_USB_STAGE_COUNT];
    volatile uint32_t stage_head; /* ISR producer */
    volatile uint32_t stage_tail; /* task consumer */
    volatile uint32_t stage_count;
    volatile uint8_t setup_ready;
    XUsbPs_SetupData setup_data;
    uint8_t tx_frames[LA_USB_TX_FRAME_COUNT][LA_USB_TX_FRAME_BYTES]
        __attribute__((aligned(64)));
    volatile uint8_t tx_slot_state[LA_USB_TX_FRAME_COUNT]; /* 0 free, 1 ready, 2 in flight, 3 reserved */
    volatile uint32_t tx_slot_len[LA_USB_TX_FRAME_COUNT];
    volatile int32_t tx_inflight;
    /* FIFO of ready slot indices: the pump must send in enqueue order. A
     * lowest-free-slot scan reuses a slot freed mid-burst and lets a later
     * frame overtake an earlier one (bench 2026-09-19: DATA_CHUNK sequence
     * 14,16,15 reached the host). */
    volatile uint8_t tx_ready[LA_USB_TX_FRAME_COUNT];
    volatile uint32_t tx_ready_head;
    volatile uint32_t tx_ready_tail;

    volatile uint8_t setup_pending;
    volatile uint8_t session_pending;
    volatile uint32_t session_status;
    XUsbPs_SetupData setup;

    volatile uint8_t line_coding[7];
    volatile uint16_t control_line_state;

    volatile uint32_t rx_bytes;
    volatile uint32_t rx_overflows;
    volatile uint32_t rx_bad;    /* driver entries outside the DMA pool */
    volatile uint32_t rx_events; /* OUT completion callbacks serviced */
    volatile uint32_t tx_bytes;
    volatile uint32_t tx_drops;
    volatile uint32_t setup_count;
    volatile uint32_t unsupported;
    volatile uint32_t ep0_errors;
    volatile uint32_t session_events;
    volatile uint32_t isr_events;
} la_usb_dev_t;

/* Creates the driver objects, configures the endpoints from the non-cached
 * pool, installs the GIC handler and starts the controller. Returns 0 or a
 * negative error. The task handle is used for ISR notifications. */
int la_usb_dev_init(la_usb_dev_t *dev, StreamBufferHandle_t rx_sb,
                    TaskHandle_t task);

/* Re-initializes the endpoint ring after a bus reset and primes EP1 OUT. */
int la_usb_reconfigure(la_usb_dev_t *dev);

/* TX producer API (TxTask): copies one complete frame into a free slot and
 * notifies UsbTask. Bounded wait for a free slot; returns 0 or -1. */
int la_usb_enqueue(la_usb_dev_t *dev, const uint8_t *frame, size_t length);

/* TX service (UsbTask): submits the next ready frame when the endpoint is
 * idle. */
void la_usb_tx_pump(la_usb_dev_t *dev);

/* Drops queued/in-flight TX frames after a session invalidation and returns
 * every slot to the free pool. */
void la_usb_tx_reset(la_usb_dev_t *dev);

/* RX service (UsbTask): releases a popped completion and re-primes the
 * endpoint. */
void la_usb_rx_release(la_usb_dev_t *dev, const la_usb_completion_t *completion);

/* Endpoint service (UsbTask). The vendor RX interrupt loop requires the
 * endpoint callback to pop AND release every completed descriptor before it
 * advances, so the ISR copies each payload into the bounded staging ring,
 * releases and re-primes immediately (bench 2026-09-18: deferring the
 * release desynced the ring and dropped every other packet). The task
 * consumes one staged entry at a time and must release it before the next
 * pop; the entry pointer stays valid until then. */
const uint8_t *la_usb_stage_pop(la_usb_dev_t *dev, uint16_t *len, uint8_t *ep);
void la_usb_stage_release(la_usb_dev_t *dev);
int la_usb_setup_take(la_usb_dev_t *dev, XUsbPs_SetupData *setup);
void la_usb_sweep_rx(la_usb_dev_t *dev);

/* EP0 control responses (UsbTask). */
int la_usb_ep0_send(la_usb_dev_t *dev, const uint8_t *data, size_t length);
void la_usb_ep0_stall(la_usb_dev_t *dev, uint8_t direction);

/* True when the port negotiated high speed (read in task context). */
int la_usb_high_speed(la_usb_dev_t *dev);

/* U03 bounded event trace. la_usb_trace_ring is a global so the JTAG dump
 * tool can resolve it from the ELF symbol table; la_usb_trace_event() is
 * atomic against the USB ISR (interrupt-mask pair) and is safe from both ISR
 * and task context. The ring is retrieved over JTAG after a burst and is
 * never printed. */
extern la_usb_trace_t la_usb_trace_ring;
void la_usb_trace_event(uint32_t tag, uint32_t a, uint32_t b, uint32_t c);

/* Resets one endpoint direction's data toggle to DATA0 through the
 * documented EPCR TXR/RXR bits (docs/usb-link.md section 10). UsbTask only,
 * with the endpoint idle. */
void la_usb_toggle_reset(la_usb_dev_t *dev, uint8_t number, uint8_t direction);

#endif /* LA_USB_DEV_H */
