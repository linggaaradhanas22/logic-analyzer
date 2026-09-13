#include "la_spi_core.h"

#include <string.h>

void la_spi_core_init(la_spi_core_t *core, uint8_t *buffer, size_t capacity)
{
    if (core == NULL) {
        return;
    }
    memset(core, 0, sizeof(*core));
    if (buffer != NULL) {
        core->buffer = buffer;
        core->capacity = capacity;
    }
}

size_t la_spi_core_space(const la_spi_core_t *core)
{
    if (core == NULL) {
        return 0u;
    }
    return core->capacity - core->used;
}

size_t la_spi_core_used(const la_spi_core_t *core)
{
    return core == NULL ? 0u : core->used;
}

int la_spi_core_ready(const la_spi_core_t *core)
{
    return core != NULL && core->used > 0u;
}

int la_spi_core_push(la_spi_core_t *core, const uint8_t *frame, size_t length)
{
    if (core == NULL || core->buffer == NULL || frame == NULL || length == 0u) {
        return -1;
    }
    if (length > core->capacity - core->used) {
        core->frames_dropped += 1u;
        return -1;
    }
    if (core->tail + length > core->capacity) {
        /* The frame does not fit at the tail even though the ring has room:
         * compact the buffered bytes to the start. */
        if (core->used > 0u) {
            memmove(core->buffer, core->buffer + core->head, core->used);
        }
        core->head = 0u;
        core->tail = core->used;
    }
    memcpy(core->buffer + core->tail, frame, length);
    core->tail += length;
    if (core->tail == core->capacity) {
        core->tail = 0u;
    }
    core->used += length;
    core->frames_queued += 1u;
    return 0;
}

size_t la_spi_core_feed(la_spi_core_t *core, uint8_t *out, size_t max_bytes)
{
    size_t take;

    if (core == NULL || out == NULL || max_bytes == 0u || core->used == 0u) {
        return 0u;
    }
    take = core->used < max_bytes ? core->used : max_bytes;
    if (core->head + take <= core->capacity) {
        memcpy(out, core->buffer + core->head, take);
        core->head += take;
        if (core->head == core->capacity) {
            core->head = 0u;
        }
    } else {
        size_t first = core->capacity - core->head;

        memcpy(out, core->buffer + core->head, first);
        memcpy(out + first, core->buffer, take - first);
        core->head = take - first;
    }
    core->used -= take;
    core->bytes_fed += (uint32_t)take;
    return take;
}

void la_spi_core_note_underrun(la_spi_core_t *core)
{
    if (core != NULL) {
        core->underruns += 1u;
    }
}
