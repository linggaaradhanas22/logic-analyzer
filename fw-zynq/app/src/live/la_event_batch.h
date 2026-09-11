#ifndef LA_EVENT_BATCH_H
#define LA_EVENT_BATCH_H

/*
 * P7.4 EVT_DECODED batch builder (pure logic).
 *
 * A decoder task fills one batch with complete EVT_DECODED records
 * (`slot:u8, kind:u8, t_sample:u64 LE, len:u8, data[len]`, protocol.md) and
 * hands the batch to TxTask when it is full or when the 2 ms flush timer
 * fires. A record that does not fit in the remaining payload capacity is
 * dropped whole (drop-newest, never a truncated record); the caller counts
 * the drop and reports EVT_OVERRUN(stage=decoder) separately.
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
    uint8_t data[LA_EVENT_BATCH_CAPACITY];
} la_event_batch_t;

LA_EVENT_API void la_event_batch_init(la_event_batch_t *batch);

/* Appends one event record. Returns 1 when appended, 0 when the batch has no
 * room for the complete record (batch unchanged) or -1 on invalid
 * arguments/event length. */
LA_EVENT_API int la_event_batch_add(la_event_batch_t *batch,
                                    uint8_t slot,
                                    const la_decode_event_t *event);

LA_EVENT_API int la_event_batch_empty(const la_event_batch_t *batch);
LA_EVENT_API uint16_t la_event_batch_length(const la_event_batch_t *batch);
LA_EVENT_API uint16_t la_event_batch_records(const la_event_batch_t *batch);

#endif /* LA_EVENT_BATCH_H */
