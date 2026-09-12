#include "la_event_batch.h"

#include <string.h>

static void put_u16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_u64(uint8_t *output, uint64_t value)
{
    unsigned index;

    for (index = 0u; index < 8u; ++index) {
        output[index] = (uint8_t)((value >> (8u * index)) & 0xFFu);
    }
}

static uint64_t get_u64(const uint8_t *input)
{
    uint64_t value = 0u;
    unsigned index;

    for (index = 0u; index < 8u; ++index) {
        value |= (uint64_t)input[index] << (8u * index);
    }
    return value;
}

void la_event_batch_init(la_event_batch_t *batch)
{
    if (batch == NULL) {
        return;
    }
    batch->length = 0u;
    batch->records = 0u;
    batch->type = LA_PROTO_EVT_DECODED;
    batch->format = LA_PROTO_EVT_FORMAT_RECORDS;
    batch->slot = 0u;
    batch->t_base = 0u;
    batch->last_t = 0u;
}

void la_event_batch_init_block(la_event_batch_t *batch, uint8_t slot)
{
    if (batch == NULL) {
        return;
    }
    batch->length = 0u;
    batch->records = 0u;
    batch->type = LA_PROTO_EVT_BLOCK;
    batch->format = LA_PROTO_EVT_FORMAT_BLOCK;
    batch->slot = slot;
    batch->t_base = 0u;
    batch->last_t = 0u;
}

static int batch_add_records(la_event_batch_t *batch, uint8_t slot,
                             const la_decode_event_t *event)
{
    uint16_t record_size =
        (uint16_t)(LA_EVENT_BATCH_RECORD_HEADER + event->length);

    if ((uint32_t)batch->length + record_size > LA_EVENT_BATCH_CAPACITY) {
        return 0;
    }
    batch->data[batch->length + 0u] = slot;
    batch->data[batch->length + 1u] = event->kind;
    put_u64(&batch->data[batch->length + 2u], event->t_sample);
    batch->data[batch->length + 10u] = event->length;
    if (event->length != 0u) {
        memcpy(&batch->data[batch->length + LA_EVENT_BATCH_RECORD_HEADER],
               event->data, event->length);
    }
    batch->length = (uint16_t)(batch->length + record_size);
    batch->records++;
    return 1;
}

static int batch_add_block(la_event_batch_t *batch,
                           const la_decode_event_t *event)
{
    uint64_t delta;
    uint16_t record_size;
    uint16_t offset;

    if (batch->records == 0u) {
        batch->t_base = event->t_sample;
        batch->last_t = event->t_sample;
        batch->data[0] = LA_PROTO_EVT_BLOCK_VERSION;
        batch->data[1] = batch->slot;
        put_u16(&batch->data[2], 0u);
        put_u64(&batch->data[4], batch->t_base);
        batch->length = LA_PROTO_EVT_BLOCK_HEADER_SIZE;
    }
    delta = event->t_sample - batch->last_t;
    record_size = (uint16_t)((delta >= LA_PROTO_EVT_BLOCK_DELTA_ESCAPE ? 12u : 4u) +
                             event->length);
    if ((uint32_t)batch->length + record_size > LA_EVENT_BATCH_CAPACITY) {
        return 0;
    }
    offset = batch->length;
    if (delta >= LA_PROTO_EVT_BLOCK_DELTA_ESCAPE) {
        put_u16(&batch->data[offset], LA_PROTO_EVT_BLOCK_DELTA_ESCAPE);
        put_u64(&batch->data[offset + 2u], event->t_sample);
        offset = (uint16_t)(offset + 10u);
    } else {
        put_u16(&batch->data[offset], (uint16_t)delta);
        offset = (uint16_t)(offset + 2u);
    }
    batch->data[offset] = event->kind;
    batch->data[offset + 1u] = event->length;
    if (event->length != 0u) {
        memcpy(&batch->data[offset + 2u], event->data, event->length);
    }
    batch->length = (uint16_t)(batch->length + record_size);
    batch->records++;
    batch->last_t = event->t_sample;
    put_u16(&batch->data[2], batch->records);
    return 1;
}

int la_event_batch_add(la_event_batch_t *batch,
                       uint8_t slot,
                       const la_decode_event_t *event)
{
    if (batch == NULL || event == NULL || event->length > LA_DECODE_MAX_EVENT_DATA) {
        return -1;
    }
    if (batch->format == LA_PROTO_EVT_FORMAT_BLOCK) {
        return batch_add_block(batch, event);
    }
    return batch_add_records(batch, slot, event);
}

int la_event_batch_empty(const la_event_batch_t *batch)
{
    return batch == NULL || batch->records == 0u;
}

uint16_t la_event_batch_length(const la_event_batch_t *batch)
{
    return batch != NULL ? batch->length : 0u;
}

uint16_t la_event_batch_records(const la_event_batch_t *batch)
{
    return batch != NULL ? batch->records : 0u;
}

int la_event_batch_last_sample(const la_event_batch_t *batch, uint64_t *t_sample)
{
    uint32_t offset;

    if (batch == NULL || t_sample == NULL || batch->records == 0u) {
        return -1;
    }
    if (batch->format == LA_PROTO_EVT_FORMAT_BLOCK) {
        *t_sample = batch->last_t;
        return 0;
    }
    offset = 0u;
    while (offset + LA_EVENT_BATCH_RECORD_HEADER <= batch->length) {
        uint8_t length = batch->data[offset + 10u];

        *t_sample = get_u64(&batch->data[offset + 2u]);
        offset += LA_EVENT_BATCH_RECORD_HEADER + length;
    }
    return 0;
}
