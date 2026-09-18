#include "la_usb_trace.h"

#include <stddef.h>

void la_usb_trace_init(la_usb_trace_t *trace)
{
    uint32_t index;
    uint32_t word;

    if (trace == NULL) {
        return;
    }
    trace->head = 0u;
    for (index = 0u; index < LA_USB_TRACE_CAPACITY; index++) {
        for (word = 0u; word < LA_USB_TRACE_WORDS; word++) {
            trace->words[index][word] = 0u;
        }
    }
}

void la_usb_trace_push(la_usb_trace_t *trace, uint32_t tag, uint32_t a,
                       uint32_t b, uint32_t c)
{
    uint32_t slot;

    if (trace == NULL) {
        return;
    }
    slot = trace->head % LA_USB_TRACE_CAPACITY;
    trace->words[slot][1] = a;
    trace->words[slot][2] = b;
    trace->words[slot][3] = c;
    /* Publish the tag last, then advance head: a reader starting from head
     * sees either the previous complete entry or this one. */
    trace->words[slot][0] = tag;
    trace->head = trace->head + 1u;
}

uint32_t la_usb_trace_count(const la_usb_trace_t *trace)
{
    if (trace == NULL) {
        return 0u;
    }
    return (trace->head < LA_USB_TRACE_CAPACITY) ? trace->head
                                                 : LA_USB_TRACE_CAPACITY;
}

uint32_t la_usb_trace_lost(const la_usb_trace_t *trace)
{
    if (trace == NULL || trace->head <= LA_USB_TRACE_CAPACITY) {
        return 0u;
    }
    return trace->head - LA_USB_TRACE_CAPACITY;
}
