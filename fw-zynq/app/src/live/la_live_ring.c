#include "la_live_ring.h"

#include <string.h>

static int slot_valid(const la_live_ring_t *ring, uint32_t slot)
{
    return ring != NULL && slot < ring->segment_count;
}

int la_live_ring_init(la_live_ring_t *ring,
                      la_segment_pool_t *pool,
                      uintptr_t base,
                      uint32_t segment_count,
                      uint32_t segment_bytes)
{
    uint32_t index;

    if (ring == NULL || pool == NULL || segment_count == 0u ||
        segment_count > LA_LIVE_SEGMENT_COUNT || segment_bytes == 0u ||
        segment_bytes > pool->segment_capacity || base == 0u) {
        return -1;
    }
    memset(ring, 0, sizeof(*ring));
    ring->pool = pool;
    ring->base = base;
    ring->segment_count = segment_count;
    ring->segment_bytes = segment_bytes;
    for (index = 0u; index < segment_count; ++index) {
        ring->slots[index].state = LA_LIVE_SLOT_FREE;
    }
    return 0;
}

int la_live_ring_start(la_live_ring_t *ring)
{
    uint32_t index;

    if (ring == NULL || ring->pool == NULL) {
        return -1;
    }
    for (index = 0u; index < ring->segment_count; ++index) {
        la_segment_handle_t handle;

        if (ring->slots[index].state != LA_LIVE_SLOT_FREE) {
            return -1;
        }
        if (la_segment_pool_acquire(ring->pool, &handle) != 0) {
            uint32_t rollback;

            for (rollback = 0u; rollback < index; ++rollback) {
                (void)la_segment_pool_abort(ring->pool, ring->slots[rollback].handle);
                ring->slots[rollback].state = LA_LIVE_SLOT_FREE;
            }
            ring->dma_posted = 0u;
            return -1;
        }
        ring->slots[index].handle = handle;
        ring->slots[index].length = 0u;
        ring->slots[index].state = LA_LIVE_SLOT_DMA;
    }
    ring->dma_posted = ring->segment_count;
    ring->published = 0u;
    return (int)ring->segment_count;
}

int la_live_ring_complete(la_live_ring_t *ring,
                          uint32_t slot,
                          uint32_t actual_length,
                          uint32_t consumer_count)
{
    if (!slot_valid(ring, slot) || consumer_count > LA_LIVE_MAX_CONSUMERS) {
        return -1;
    }
    if (ring->slots[slot].state != LA_LIVE_SLOT_DMA) {
        return -2;
    }
    if (la_segment_pool_publish(ring->pool, ring->slots[slot].handle,
                                actual_length, consumer_count) != 0) {
        return -2;
    }
    if (ring->dma_posted != 0u) {
        --ring->dma_posted;
    }
    ring->slots[slot].length = actual_length;
    if (consumer_count == 0u) {
        ring->slots[slot].state = LA_LIVE_SLOT_FREE;
        return 0;
    }
    ring->slots[slot].state = LA_LIVE_SLOT_CPU;
    ++ring->published;
    return 0;
}

int la_live_ring_release(la_live_ring_t *ring, uint32_t slot)
{
    la_segment_state_t pool_state = LA_SEGMENT_FREE;
    uint32_t references = 0u;
    uint32_t sequence = 0u;
    uint32_t length = 0u;

    if (!slot_valid(ring, slot)) {
        return -1;
    }
    if (ring->slots[slot].state != LA_LIVE_SLOT_CPU) {
        return -2;
    }
    if (la_segment_pool_release(ring->pool, ring->slots[slot].handle) != 0) {
        return -2;
    }
    if (la_segment_pool_snapshot(ring->pool, ring->slots[slot].handle, &pool_state,
                                 &references, &sequence, &length) != 0) {
        /* The last release freed the segment; the handle is invalid now. */
        pool_state = LA_SEGMENT_FREE;
    }
    if (pool_state == LA_SEGMENT_FREE) {
        ring->slots[slot].state = LA_LIVE_SLOT_FREE;
        ring->slots[slot].length = 0u;
        if (ring->published != 0u) {
            --ring->published;
        }
    }
    return 0;
}

int la_live_ring_repost(la_live_ring_t *ring, uint32_t *slot_out)
{
    uint32_t index;

    if (ring == NULL || ring->pool == NULL || slot_out == NULL) {
        return -1;
    }
    for (index = 0u; index < ring->segment_count; ++index) {
        la_segment_handle_t handle;

        if (ring->slots[index].state != LA_LIVE_SLOT_FREE) {
            continue;
        }
        if (la_segment_pool_acquire(ring->pool, &handle) != 0) {
            return -1;
        }
        if (handle.slot != index) {
            /* The pool hands out the lowest free slot, which must match the
             * slot this ring believes is free; a mismatch means the ring and
             * the pool diverged, so fail closed. */
            (void)la_segment_pool_abort(ring->pool, handle);
            return -1;
        }
        ring->slots[index].handle = handle;
        ring->slots[index].length = 0u;
        ring->slots[index].state = LA_LIVE_SLOT_DMA;
        ++ring->dma_posted;
        *slot_out = index;
        return (int)index;
    }
    return -1;
}

int la_live_ring_abort(la_live_ring_t *ring)
{
    uint32_t index;
    uint32_t pending = 0u;

    if (ring == NULL || ring->pool == NULL) {
        return -1;
    }
    for (index = 0u; index < ring->segment_count; ++index) {
        if (ring->slots[index].state == LA_LIVE_SLOT_DMA) {
            (void)la_segment_pool_abort(ring->pool, ring->slots[index].handle);
            ring->slots[index].state = LA_LIVE_SLOT_FREE;
            ring->slots[index].length = 0u;
            if (ring->dma_posted != 0u) {
                --ring->dma_posted;
            }
        } else if (ring->slots[index].state == LA_LIVE_SLOT_CPU) {
            ++pending;
        }
    }
    return (int)pending;
}

uintptr_t la_live_ring_slot_address(const la_live_ring_t *ring, uint32_t slot)
{
    if (!slot_valid(ring, slot)) {
        return 0u;
    }
    return ring->base + (uintptr_t)slot * (uintptr_t)ring->segment_bytes;
}

la_live_slot_state_t la_live_ring_slot_state(const la_live_ring_t *ring, uint32_t slot)
{
    if (!slot_valid(ring, slot)) {
        return LA_LIVE_SLOT_FREE;
    }
    return ring->slots[slot].state;
}

uint32_t la_live_ring_slot_sequence(const la_live_ring_t *ring, uint32_t slot)
{
    uint32_t sequence = 0u;
    uint32_t references = 0u;
    uint32_t length = 0u;
    la_segment_state_t state = LA_SEGMENT_FREE;

    if (!slot_valid(ring, slot) ||
        la_segment_pool_snapshot(ring->pool, ring->slots[slot].handle, &state,
                                 &references, &sequence, &length) != 0) {
        return 0u;
    }
    return sequence;
}

uint32_t la_live_ring_dma_posted(const la_live_ring_t *ring)
{
    return ring != NULL ? ring->dma_posted : 0u;
}
