#include "la_event_batch.h"

#include <string.h>

void la_event_batch_init(la_event_batch_t *batch)
{
    if (batch == NULL) {
        return;
    }
    batch->length = 0u;
    batch->records = 0u;
}

int la_event_batch_add(la_event_batch_t *batch,
                       uint8_t slot,
                       const la_decode_event_t *event)
{
    uint16_t record_size;

    if (batch == NULL || event == NULL || event->length > LA_DECODE_MAX_EVENT_DATA) {
        return -1;
    }
    record_size = (uint16_t)(LA_EVENT_BATCH_RECORD_HEADER + event->length);
    if ((uint32_t)batch->length + record_size > LA_EVENT_BATCH_CAPACITY) {
        return 0;
    }
    batch->data[batch->length + 0u] = slot;
    batch->data[batch->length + 1u] = event->kind;
    batch->data[batch->length + 2u] = (uint8_t)(event->t_sample & 0xFFu);
    batch->data[batch->length + 3u] = (uint8_t)((event->t_sample >> 8) & 0xFFu);
    batch->data[batch->length + 4u] = (uint8_t)((event->t_sample >> 16) & 0xFFu);
    batch->data[batch->length + 5u] = (uint8_t)((event->t_sample >> 24) & 0xFFu);
    batch->data[batch->length + 6u] = (uint8_t)((event->t_sample >> 32) & 0xFFu);
    batch->data[batch->length + 7u] = (uint8_t)((event->t_sample >> 40) & 0xFFu);
    batch->data[batch->length + 8u] = (uint8_t)((event->t_sample >> 48) & 0xFFu);
    batch->data[batch->length + 9u] = (uint8_t)((event->t_sample >> 56) & 0xFFu);
    batch->data[batch->length + 10u] = event->length;
    if (event->length != 0u) {
        memcpy(&batch->data[batch->length + LA_EVENT_BATCH_RECORD_HEADER],
               event->data, event->length);
    }
    batch->length = (uint16_t)(batch->length + record_size);
    batch->records++;
    return 1;
}

int la_event_batch_empty(const la_event_batch_t *batch)
{
    return batch == NULL || batch->length == 0u;
}

uint16_t la_event_batch_length(const la_event_batch_t *batch)
{
    return batch != NULL ? batch->length : 0u;
}

uint16_t la_event_batch_records(const la_event_batch_t *batch)
{
    return batch != NULL ? batch->records : 0u;
}
