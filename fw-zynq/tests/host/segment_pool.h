#ifndef LA_SEGMENT_POOL_H
#define LA_SEGMENT_POOL_H

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_SEGMENT_API __declspec(dllexport)
#else
#define LA_SEGMENT_API
#endif

#define LA_SEGMENT_POOL_CAPACITY 16u
#define LA_SEGMENT_INVALID_SLOT UINT16_MAX

typedef enum la_segment_state {
    LA_SEGMENT_FREE = 0,
    LA_SEGMENT_DMA_OWNED = 1,
    LA_SEGMENT_CPU_OWNED = 2
} la_segment_state_t;

typedef struct la_segment_handle {
    uint16_t slot;
    uint16_t reserved;
    uint32_t generation;
} la_segment_handle_t;

typedef struct la_segment_slot {
    uint32_t generation;
    uint32_t sequence;
    uint32_t length;
    uint32_t references;
    la_segment_state_t state;
} la_segment_slot_t;

typedef struct la_segment_pool {
    uint32_t segment_capacity;
    uint32_t next_sequence;
    la_segment_slot_t slots[LA_SEGMENT_POOL_CAPACITY];
#if defined(_WIN32)
    void *lock_storage[5];
#else
    void *lock_storage[8];
#endif
} la_segment_pool_t;

/* The opaque lock storage keeps the public test ABI stable across host builds. */
LA_SEGMENT_API int la_segment_pool_init(la_segment_pool_t *pool, uint32_t segment_capacity);
LA_SEGMENT_API void la_segment_pool_destroy(la_segment_pool_t *pool);
LA_SEGMENT_API int la_segment_pool_acquire(la_segment_pool_t *pool, la_segment_handle_t *handle);
LA_SEGMENT_API int la_segment_pool_publish(la_segment_pool_t *pool,
                                           la_segment_handle_t handle,
                                           uint32_t actual_length,
                                           uint32_t consumer_count);
LA_SEGMENT_API int la_segment_pool_release(la_segment_pool_t *pool, la_segment_handle_t handle);
LA_SEGMENT_API int la_segment_pool_abort(la_segment_pool_t *pool, la_segment_handle_t handle);
LA_SEGMENT_API int la_segment_pool_snapshot(la_segment_pool_t *pool,
                                            la_segment_handle_t handle,
                                            la_segment_state_t *state,
                                            uint32_t *references,
                                            uint32_t *sequence,
                                            uint32_t *length);

#endif /* LA_SEGMENT_POOL_H */
