#ifndef LA_LIVE_RING_H
#define LA_LIVE_RING_H

/*
 * P7.2 live segment ring (pure logic over la_segment_pool).
 *
 * Owns the firmware-side view of the live pool: one slot per segment index
 * (slot i is the 256 KiB region at base + i * segment_bytes), the pool handle
 * that currently backs it, its published length and its lifecycle state.
 * AcqTask is the single writer for DMA completion/publish/repost; decoder
 * tasks only signal reference releases through the same task, so every state
 * transition passes through this core under the pool lock.
 *
 * States mirror the pool: a slot is FREE (segment released, safe to repost),
 * DMA (posted to hardware), or CPU (published to one or more decoders). A
 * completion with zero consumers recycles immediately; a queue-full for one
 * decoder releases only that reference, never the whole buffer. A repost is
 * only possible while the slot is FREE, so a descriptor is never republished
 * while any consumer owns its payload.
 *
 * The core performs no DMA, FreeRTOS or queue calls; it only acquires,
 * publishes, releases and aborts pool handles. AcqTask programs the BD whose
 * ID carries the segment index.
 */

#include "la_segment_pool.h"

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_LIVE_API __declspec(dllexport)
#else
#define LA_LIVE_API
#endif

#define LA_LIVE_SEGMENT_COUNT 16u
#define LA_LIVE_SEGMENT_BYTES (256u * 1024u)
#define LA_LIVE_SEGMENT_WORDS (LA_LIVE_SEGMENT_BYTES / 4u)
#define LA_LIVE_MAX_CONSUMERS 3u

typedef enum la_live_slot_state {
    LA_LIVE_SLOT_FREE = 0,
    LA_LIVE_SLOT_DMA = 1,
    LA_LIVE_SLOT_CPU = 2
} la_live_slot_state_t;

typedef struct la_live_slot {
    la_segment_handle_t handle;
    uint32_t length;
    la_live_slot_state_t state;
} la_live_slot_t;

typedef struct la_live_ring {
    la_segment_pool_t *pool;
    uintptr_t base;
    uint32_t segment_count;
    uint32_t segment_bytes;
    uint32_t dma_posted;
    uint32_t published;
    la_live_slot_t slots[LA_LIVE_SEGMENT_COUNT];
} la_live_ring_t;

/* Validates the geometry and clears every slot. The pool must outlive the
 * ring and may already be initialized. segment_count <= 16 and
 * segment_bytes <= the pool segment capacity. Returns 0 or -1. */
LA_LIVE_API int la_live_ring_init(la_live_ring_t *ring,
                                  la_segment_pool_t *pool,
                                  uintptr_t base,
                                  uint32_t segment_count,
                                  uint32_t segment_bytes);

/* Acquires every slot from the pool and marks it DMA-owned. Returns the
 * number of posted slots (== segment_count) or -1 on invalid state; on a
 * partial acquire the already-acquired handles are aborted, so the pool is
 * left as before. */
LA_LIVE_API int la_live_ring_start(la_live_ring_t *ring);

/* Completes one DMA slot with actual_length bytes and consumer_count
 * references. consumer_count == 0 frees the segment immediately. Returns 0,
 * -1 on bad arguments or -2 when the slot is not DMA-owned. */
LA_LIVE_API int la_live_ring_complete(la_live_ring_t *ring,
                                      uint32_t slot,
                                      uint32_t actual_length,
                                      uint32_t consumer_count);

/* Drops one consumer reference. When the last reference goes, the pool frees
 * the segment and the slot becomes FREE (repostable). Returns 0, -1 on bad
 * arguments or -2 when the slot is not CPU-owned. */
LA_LIVE_API int la_live_ring_release(la_live_ring_t *ring, uint32_t slot);

/* Finds the lowest FREE slot, re-acquires it from the pool and marks it
 * DMA-owned. Returns the slot index, or -1 when no slot is free (pool
 * exhaustion) / invalid arguments. */
LA_LIVE_API int la_live_ring_repost(la_live_ring_t *ring, uint32_t *slot_out);

/* Aborts every DMA-owned slot. CPU-owned slots are left untouched (their
 * consumers still own the payload); the return value is the number of slots
 * that are not FREE afterwards, so the caller can drain and retry. Returns
 * -1 on invalid arguments. */
LA_LIVE_API int la_live_ring_abort(la_live_ring_t *ring);

LA_LIVE_API uintptr_t la_live_ring_slot_address(const la_live_ring_t *ring, uint32_t slot);
LA_LIVE_API la_live_slot_state_t la_live_ring_slot_state(const la_live_ring_t *ring, uint32_t slot);
LA_LIVE_API uint32_t la_live_ring_slot_sequence(const la_live_ring_t *ring, uint32_t slot);
LA_LIVE_API uint32_t la_live_ring_dma_posted(const la_live_ring_t *ring);

#endif /* LA_LIVE_RING_H */
