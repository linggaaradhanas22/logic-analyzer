#ifndef LA_RELAY_H
#define LA_RELAY_H

/*
 * P5.2 ESP32 link relay policy core (pure logic, portable).
 *
 * The ESP32 gateway relays protocol frames verbatim between the Zynq UART
 * leg and the host TCP leg. The FreeRTOS tasks own the byte rings and the
 * UART/TCP drivers; this module owns the routing decision and the drop
 * counters for one directed leg, so the policy in docs/rtos-design.md
 * ("ESP32 link relay core (P5.2)") is deterministic and host-testable:
 *
 *  - Best-effort frame types (the live decoded-event stream, EVT_DECODED
 *    on the Zynq -> host leg) are drop-newest when the destination ring
 *    lacks space for the frame.
 *  - Control frames (everything else, including unknown parseable types so
 *    a newer peer can be proxied) are preserved: when the ring lacks space
 *    the decision is PRESSURE and the caller blocks/retries instead of
 *    dropping. Every dropped best-effort frame is counted, never silent.
 *  - Mirror types (EVT_DECODED toward the P10 live view) are additionally
 *    mirrored after a successful forward; a failed mirror is counted.
 *
 * The module performs no ring, FreeRTOS, ESP-IDF, parser or la_proto calls:
 * type tables are supplied by the caller task at init (the task includes
 * la_proto.h and passes LA_PROTO_EVT_DECODED and friends), the caller asks
 * capacity through a callback and reports every real outcome through
 * la_relay_report(). Frame bytes are the caller's encoded frame size; the
 * relay counts frames, not bytes.
 *
 * The Host -> Zynq leg is configured with empty best-effort/mirror tables,
 * which makes every accepted frame control traffic that is never dropped
 * by policy (its UART TX still obeys RTS/CTS backpressure).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_RELAY_API __declspec(dllexport)
#else
#define LA_RELAY_API
#endif

#define LA_RELAY_MAX_TYPES 4u

typedef enum la_relay_role {
    LA_RELAY_ROLE_UART_TO_NET = 0, /* Zynq -> host leg (LinkRxTask) */
    LA_RELAY_ROLE_NET_TO_UART = 1  /* host -> Zynq leg (LinkTxTask) */
} la_relay_role_t;

typedef enum la_relay_decision {
    LA_RELAY_FORWARD = 0,       /* write the frame to the destination ring */
    LA_RELAY_DROP_BEST_EFFORT,  /* best-effort frame dropped (ring full) */
    LA_RELAY_PRESSURE           /* control frame: no space; caller blocks or retries */
} la_relay_decision_t;

typedef enum la_relay_result {
    LA_RELAY_FWD_OK = 0,    /* the frame was placed in the destination ring */
    LA_RELAY_FWD_FAIL,      /* placement failed after a FORWARD decision */
    LA_RELAY_MIRROR_OK,     /* a mirror type was placed in the live-view ring */
    LA_RELAY_MIRROR_FAIL    /* mirror placement failed (counted, never silent) */
} la_relay_result_t;

/* Returns nonzero when a frame of frame_bytes can be placed now. */
typedef int (*la_relay_capacity_fn)(void *context, size_t frame_bytes);

typedef struct la_relay_stats {
    uint32_t frames_forwarded;
    uint32_t frames_forward_failed;
    uint32_t best_effort_dropped;
    uint32_t control_pressure_events;
    uint32_t frames_mirrored;
    uint32_t mirror_dropped;
} la_relay_stats_t;

typedef struct la_relay {
    la_relay_role_t role;
    uint8_t best_effort_types[LA_RELAY_MAX_TYPES];
    uint8_t best_effort_count;
    uint8_t mirror_types[LA_RELAY_MAX_TYPES];
    uint8_t mirror_count;
    la_relay_stats_t stats;
} la_relay_t;

/* Resets the instance and installs the caller's type tables. type arrays
 * of NULL with count 0 select no types. Returns 0, or -1 on invalid
 * arguments (counts beyond LA_RELAY_MAX_TYPES or NULL arrays with nonzero
 * counts). */
LA_RELAY_API int la_relay_init(la_relay_t *relay,
                               la_relay_role_t role,
                               const uint8_t *best_effort_types,
                               size_t best_effort_count,
                               const uint8_t *mirror_types,
                               size_t mirror_count);

/* 1 when the frame type is in the best-effort table (mirror types are a
 * subset decision the caller also needs). */
LA_RELAY_API int la_relay_is_best_effort(const la_relay_t *relay, uint8_t type);

/* 1 when the frame type is in the mirror table. */
LA_RELAY_API int la_relay_is_mirror(const la_relay_t *relay, uint8_t type);

/* Routes one accepted frame of frame_bytes. Returns 0 with *decision set,
 * or -1 on invalid arguments (NULL relay/decision/capacity, zero frame
 * bytes). A DROP_BEST_EFFORT decision increments best_effort_dropped and
 * nothing is placed or mirrored; a PRESSURE decision increments
 * control_pressure_events once per no-room attempt (the caller then blocks
 * or retries). FORWARD is counted only after the caller reports its real
 * outcome with la_relay_report(). */
LA_RELAY_API int la_relay_accept(la_relay_t *relay,
                                 uint8_t type,
                                 size_t frame_bytes,
                                 la_relay_capacity_fn capacity,
                                 void *context,
                                 la_relay_decision_t *decision);

/* Reports a real outcome for the most recent accept of this type. Mirror
 * results on non-mirror types are rejected with -1. */
LA_RELAY_API int la_relay_report(la_relay_t *relay,
                                 uint8_t type,
                                 la_relay_result_t result);

LA_RELAY_API void la_relay_snapshot(const la_relay_t *relay, la_relay_stats_t *stats);

/* Zeroes the counters without touching the configuration. */
LA_RELAY_API void la_relay_reset_counters(la_relay_t *relay);

#endif /* LA_RELAY_H */
