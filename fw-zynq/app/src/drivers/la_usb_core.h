#ifndef LA_USB_CORE_H
#define LA_USB_CORE_H

/*
 * U02 direct-USB portable core: session lifecycle and completion hand-off.
 *
 * This module is pure C (no Xilinx or FreeRTOS calls) so the host tests can
 * exercise every branch. The firmware wrappers (la_usb_dev.c / la_usb_task.c)
 * add the XUsbPs driver, the GIC interrupt and the FreeRTOS notification.
 *
 * Ownership contract (docs/usb-link.md, "Frozen integration contract"):
 *   - The interrupt callback pops a completed driver buffer with
 *     XUsbPs_EpBufferReceive and pushes a {generation, ptr, len, handle}
 *     record into la_usb_ring_t; it performs no class processing and no
 *     payload copy. The driver's own RX handler advances its descriptor
 *     cursor per callback, so the pop must happen in interrupt context.
 *   - The task drains the ring, drops records whose generation is not the
 *     current session generation, copies valid payloads to app->rx_sb,
 *     releases each handle and re-primes the endpoint.
 *   - la_usb_ring_t is single-producer (interrupt) / single-consumer (task);
 *     the firmware wrapper guards the index updates with a critical section.
 */

#include <stddef.h>
#include <stdint.h>

#define LA_USB_RING_CAPACITY 16u /* >= EP1 OUT NumBufs (16) and TX pool (8) */
#define LA_USB_GENERATION_INIT 1u

typedef enum la_usb_state {
    LA_USB_STATE_DETACHED = 0,
    LA_USB_STATE_DEFAULT,   /* bus reset seen, address 0 */
    LA_USB_STATE_ADDRESSED,
    LA_USB_STATE_CONFIGURED,
    LA_USB_STATE_SUSPENDED
} la_usb_state_t;

typedef enum la_usb_event {
    LA_USB_EV_DISCONNECT = 0,
    LA_USB_EV_BUS_RESET,
    LA_USB_EV_SET_ADDRESS,
    LA_USB_EV_SET_CONFIGURATION,
    LA_USB_EV_SUSPEND,
    LA_USB_EV_RESUME
} la_usb_event_t;

/* Action bits returned by la_usb_session_event(). The caller executes them in
 * task context; the session never touches acquisition or parser state. */
#define LA_USB_ACT_NONE 0x00u
#define LA_USB_ACT_INVALIDATE 0x01u       /* generation bumped: drop stale work */
#define LA_USB_ACT_CLEAR_PARSER 0x02u     /* discard partial la_proto input */
#define LA_USB_ACT_STOP_ACQUISITION 0x04u /* request capture/live teardown */
#define LA_USB_ACT_START_ENDPOINTS 0x08u
#define LA_USB_ACT_STOP_ENDPOINTS 0x10u
#define LA_USB_ACT_GATE_SUBMISSIONS 0x20u /* suspend: hold new IN submissions */

typedef struct la_usb_session {
    la_usb_state_t state;
    la_usb_state_t resume_state;
    uint32_t generation;
    uint8_t address;
    uint8_t configured;
    uint32_t protocol_errors;
} la_usb_session_t;

typedef struct la_usb_completion {
    uint32_t generation;
    uint32_t handle; /* opaque XUsbPs dTD handle; 0 for TX completions */
    uint8_t *ptr;
    uint32_t len;
    uint8_t ep;
} la_usb_completion_t;

typedef struct la_usb_ring {
    la_usb_completion_t entries[LA_USB_RING_CAPACITY];
    uint32_t head; /* next push index */
    uint32_t tail; /* next pop index */
    uint32_t used;
    uint32_t pushed;
    uint32_t popped;
    uint32_t stale_drops;
    uint32_t overflows;
} la_usb_ring_t;

void la_usb_session_init(la_usb_session_t *session);

/* Applies one bus/session event and returns the LA_USB_ACT_* set the caller
 * must execute. Invalid transitions are counted and return ACT_NONE. */
unsigned la_usb_session_event(la_usb_session_t *session, la_usb_event_t event,
                              uint8_t arg);

void la_usb_ring_init(la_usb_ring_t *ring);

/* Interrupt-context push. Returns 0 on success, -1 when full (counted). */
int la_usb_ring_push(la_usb_ring_t *ring, const la_usb_completion_t *completion);

/* Task-context pop. Returns 1 with *out filled, 0 when empty. Records whose
 * generation differs from `generation` are dropped and counted. */
int la_usb_ring_pop(la_usb_ring_t *ring, uint32_t generation,
                    la_usb_completion_t *out);

uint32_t la_usb_ring_used(const la_usb_ring_t *ring);

#endif /* LA_USB_CORE_H */
