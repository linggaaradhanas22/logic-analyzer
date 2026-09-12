#include "la_trace_core.h"

#include <string.h>

void la_trace_core_init(la_trace_core_t *trace)
{
    if (trace == NULL) {
        return;
    }
    memset(trace, 0, sizeof(*trace));
}

void la_trace_core_start(la_trace_core_t *trace)
{
    if (trace == NULL) {
        return;
    }
    trace->head = 0u;
    trace->total = 0u;
    trace->enabled = 1u;
}

void la_trace_core_stop(la_trace_core_t *trace)
{
    if (trace == NULL) {
        return;
    }
    trace->enabled = 0u;
}

int la_trace_core_enabled(const la_trace_core_t *trace)
{
    return trace != NULL && trace->enabled != 0u;
}

int la_trace_core_register(la_trace_core_t *trace,
                           uint32_t id,
                           uint8_t kind,
                           uint8_t number,
                           const char *name)
{
    la_trace_object_t *entry = NULL;
    uint8_t index;
    size_t length;

    if (trace == NULL || name == NULL || id == 0u) {
        return -1;
    }
    for (index = 0u; index < trace->object_count; ++index) {
        if (trace->objects[index].id == id) {
            entry = &trace->objects[index];
            break;
        }
    }
    if (entry == NULL) {
        if (trace->object_count >= LA_TRACE_OBJECT_MAX) {
            return -1;
        }
        entry = &trace->objects[trace->object_count++];
        memset(entry, 0, sizeof(*entry));
        entry->id = id;
    }
    entry->kind = kind;
    entry->number = number;
    length = strlen(name);
    if (length > LA_TRACE_NAME_MAX - 1u) {
        length = LA_TRACE_NAME_MAX - 1u;
    }
    memset(entry->name, 0, sizeof(entry->name));
    memcpy(entry->name, name, length);
    return 0;
}

uint8_t la_trace_core_task_number(const la_trace_core_t *trace, uint32_t id)
{
    uint8_t index;

    if (trace == NULL) {
        return 0u;
    }
    for (index = 0u; index < trace->object_count; ++index) {
        if (trace->objects[index].id == id &&
            trace->objects[index].kind == LA_TRACE_OBJECT_TASK) {
            return trace->objects[index].number;
        }
    }
    return 0u;
}

int la_trace_core_record(la_trace_core_t *trace,
                         uint8_t event,
                         uint32_t id,
                         uint8_t object,
                         uint16_t value,
                         uint64_t time)
{
    la_trace_record_t *record;

    if (trace == NULL || trace->enabled == 0u) {
        return -1;
    }
    record = &trace->records[trace->head];
    record->time = time;
    record->id = id;
    record->event = event;
    record->object = object;
    record->value = value;
    trace->head = (trace->head + 1u) % LA_TRACE_CAPACITY;
    trace->total++;
    return 0;
}

uint32_t la_trace_core_count(const la_trace_core_t *trace)
{
    if (trace == NULL) {
        return 0u;
    }
    return trace->total < LA_TRACE_CAPACITY ? trace->total : LA_TRACE_CAPACITY;
}

uint32_t la_trace_core_total(const la_trace_core_t *trace)
{
    return trace != NULL ? trace->total : 0u;
}

uint32_t la_trace_core_dropped(const la_trace_core_t *trace)
{
    return la_trace_core_total(trace) - la_trace_core_count(trace);
}

const la_trace_record_t *la_trace_core_record_at(const la_trace_core_t *trace,
                                                 uint32_t index)
{
    uint32_t count;
    uint32_t oldest;

    if (trace == NULL) {
        return NULL;
    }
    count = la_trace_core_count(trace);
    if (index >= count) {
        return NULL;
    }
    oldest = (trace->head + LA_TRACE_CAPACITY - count) % LA_TRACE_CAPACITY;
    return &trace->records[(oldest + index) % LA_TRACE_CAPACITY];
}

const la_trace_object_t *la_trace_core_object_at(const la_trace_core_t *trace,
                                                 uint32_t index)
{
    if (trace == NULL || index >= trace->object_count) {
        return NULL;
    }
    return &trace->objects[index];
}
