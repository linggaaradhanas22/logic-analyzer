#ifndef LA_EVENT_BATCH_H
#define LA_EVENT_BATCH_H

/*
 * P7.4/P9.2 event batch builder (pure logic).
 *
 * A decoder task fills one batch with complete event records and hands the
 * batch to TxTask when it is full or when the 2 ms flush timer fires. Two
 * encodings are supported, selected by EVT_CFG before LIVE_START:
 *
 *  - v1 records (`la_event_batch_init`): the protocol.md EVT_DECODED payload
 *    `slot:u8, kind:u8, t_sample:u64 LE, len:u8, data[len]`, 11 bytes of
 *    header per record.
 *  - compact blocks (`la_event_batch_init_block`): the protocol.md EVT_BLOCK
 *    v1 payload with a 12-byte header (version, slot, count, t_base) and
 *    per-record `t_delta:u16, kind:u8, len:u8, data[len]`. A delta above
 *    0xFFFF uses the escape (0xFFFF + absolute t_sample:u64). The block
 *    carries one slot, so records omit it; timestamps are byte-exact.
 *
 * A record that does not fit in the remaining payload capacity is dropped
 * whole (drop-newest, never a truncated record); the caller counts the drop
 * and reports EVT_OVERRUN(stage=decoder) separately.
 */

#include "la_decode.h"
#include "la_proto.h"

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_EVENT_API __declspec(dllexport)
#else
#define LA_EVENT_API
#endif

#define LA_EVENT_BATCH_CAPACITY ((uint16_t)LA_PROTO_MAX_PAYLOAD)
#define LA_EVENT_BATCH_RECORD_HEADER ((uint16_t)11u)

typedef struct la_event_batch {
    uint16_t length;
    uint16_t records;
    uint8_t type;   /* LA_PROTO_EVT_DECODED or LA_PROTO_EVT_BLOCK */
    uint8_t format; /* LA_PROTO_EVT_FORMAT_RECORDS or _BLOCK */
    uint8_t slot;   /* block header slot (block format only) */
    uint8_t reserved;
    uint64_t t_base; /* block header first timestamp */
    uint64_t last_t; /* last record timestamp (block deltas) */
    uint8_t data[LA_EVENT_BATCH_CAPACITY];
} la_event_batch_t;

/* v1 records encoding (EVT_DECODED). */
LA_EVENT_API void la_event_batch_init(la_event_batch_t *batch);

/* Compact block encoding (EVT_BLOCK v1) for one decoder slot. */
LA_EVENT_API void la_event_batch_init_block(la_event_batch_t *batch, uint8_t slot);

/* Appends one event record. Returns 1 when appended, 0 when the batch has no
 * room for the complete record (batch unchanged) or -1 on invalid
 * arguments/event length. */
LA_EVENT_API int la_event_batch_add(la_event_batch_t *batch,
                                    uint8_t slot,
                                    const la_decode_event_t *event);

LA_EVENT_API int la_event_batch_empty(const la_event_batch_t *batch);
LA_EVENT_API uint16_t la_event_batch_length(const la_event_batch_t *batch);
LA_EVENT_API uint16_t la_event_batch_records(const la_event_batch_t *batch);

/* Timestamp of the newest record in the batch (P8.2 latency tracking), for
 * either encoding. Returns 0, or -1 on invalid arguments/empty batch. */
LA_EVENT_API int la_event_batch_last_sample(const la_event_batch_t *batch,
                                            uint64_t *t_sample);

#endif /* LA_EVENT_BATCH_H */
