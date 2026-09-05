#include "segment_pool.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#if defined(_WIN32)
/* The byte array is intentionally opaque in the ABI; only handles cross tasks. */
typedef char la_lock_size_check[(sizeof(CRITICAL_SECTION) <= sizeof(((la_segment_pool_t *)0)->lock_storage)) ? 1 : -1];
static CRITICAL_SECTION *pool_lock(la_segment_pool_t *pool)
{
    return (CRITICAL_SECTION *)pool->lock_storage;
}

static void lock_pool(la_segment_pool_t *pool)
{
    EnterCriticalSection(pool_lock(pool));
}

static void unlock_pool(la_segment_pool_t *pool)
{
    LeaveCriticalSection(pool_lock(pool));
}
#else

static pthread_mutex_t *pool_lock(la_segment_pool_t *pool)
{
    return (pthread_mutex_t *)pool->lock_storage;
}

static void lock_pool(la_segment_pool_t *pool)
{
    (void)pthread_mutex_lock(pool_lock(pool));
}

static void unlock_pool(la_segment_pool_t *pool)
{
    (void)pthread_mutex_unlock(pool_lock(pool));
}

#endif

static la_segment_handle_t invalid_handle(void)
{
    la_segment_handle_t handle;
    handle.slot = LA_SEGMENT_INVALID_SLOT;
    handle.reserved = 0u;
    handle.generation = 0u;
    return handle;
}

static int handle_valid(const la_segment_pool_t *pool, la_segment_handle_t handle)
{
    return handle.slot < LA_SEGMENT_POOL_CAPACITY &&
           pool->slots[handle.slot].generation == handle.generation &&
           pool->slots[handle.slot].state != LA_SEGMENT_FREE;
}

int la_segment_pool_init(la_segment_pool_t *pool, uint32_t segment_capacity)
{
    uint32_t index;

    if (pool == NULL || segment_capacity == 0u) {
        return -1;
    }
    memset(pool, 0, sizeof(*pool));
    pool->segment_capacity = segment_capacity;
#if defined(_WIN32)
    InitializeCriticalSection(pool_lock(pool));
#else
    if (pthread_mutex_init(pool_lock(pool), NULL) != 0) {
        return -1;
    }
#endif
    for (index = 0u; index < LA_SEGMENT_POOL_CAPACITY; ++index) {
        pool->slots[index].state = LA_SEGMENT_FREE;
    }
    return 0;
}

void la_segment_pool_destroy(la_segment_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }
#if defined(_WIN32)
    DeleteCriticalSection(pool_lock(pool));
#else
    (void)pthread_mutex_destroy(pool_lock(pool));
#endif
}

int la_segment_pool_acquire(la_segment_pool_t *pool, la_segment_handle_t *handle)
{
    uint32_t index;

    if (pool == NULL || handle == NULL) {
        return -1;
    }
    *handle = invalid_handle();
    lock_pool(pool);
    for (index = 0u; index < LA_SEGMENT_POOL_CAPACITY; ++index) {
        la_segment_slot_t *slot = &pool->slots[index];
        if (slot->state == LA_SEGMENT_FREE) {
            ++slot->generation;
            if (slot->generation == 0u) {
                slot->generation = 1u;
            }
            ++pool->next_sequence;
            slot->sequence = pool->next_sequence;
            slot->length = 0u;
            slot->references = 0u;
            slot->state = LA_SEGMENT_DMA_OWNED;
            handle->slot = (uint16_t)index;
            handle->generation = slot->generation;
            unlock_pool(pool);
            return 0;
        }
    }
    unlock_pool(pool);
    return -2;
}

int la_segment_pool_publish(la_segment_pool_t *pool,
                            la_segment_handle_t handle,
                            uint32_t actual_length,
                            uint32_t consumer_count)
{
    la_segment_slot_t *slot;

    if (pool == NULL || actual_length > pool->segment_capacity || consumer_count > 3u) {
        return -1;
    }
    lock_pool(pool);
    if (!handle_valid(pool, handle) || pool->slots[handle.slot].state != LA_SEGMENT_DMA_OWNED) {
        unlock_pool(pool);
        return -2;
    }
    slot = &pool->slots[handle.slot];
    slot->length = actual_length;
    slot->references = consumer_count;
    slot->state = consumer_count == 0u ? LA_SEGMENT_FREE : LA_SEGMENT_CPU_OWNED;
    unlock_pool(pool);
    return 0;
}

int la_segment_pool_release(la_segment_pool_t *pool, la_segment_handle_t handle)
{
    la_segment_slot_t *slot;

    if (pool == NULL) {
        return -1;
    }
    lock_pool(pool);
    if (!handle_valid(pool, handle) || pool->slots[handle.slot].state != LA_SEGMENT_CPU_OWNED) {
        unlock_pool(pool);
        return -2;
    }
    slot = &pool->slots[handle.slot];
    if (slot->references == 0u) {
        unlock_pool(pool);
        return -3;
    }
    --slot->references;
    if (slot->references == 0u) {
        slot->length = 0u;
        slot->state = LA_SEGMENT_FREE;
    }
    unlock_pool(pool);
    return 0;
}

int la_segment_pool_abort(la_segment_pool_t *pool, la_segment_handle_t handle)
{
    la_segment_slot_t *slot;

    if (pool == NULL) {
        return -1;
    }
    lock_pool(pool);
    if (!handle_valid(pool, handle)) {
        unlock_pool(pool);
        return -2;
    }
    slot = &pool->slots[handle.slot];
    if (slot->state == LA_SEGMENT_CPU_OWNED && slot->references != 0u) {
        unlock_pool(pool);
        return -3;
    }
    slot->length = 0u;
    slot->references = 0u;
    slot->state = LA_SEGMENT_FREE;
    unlock_pool(pool);
    return 0;
}

int la_segment_pool_snapshot(la_segment_pool_t *pool,
                             la_segment_handle_t handle,
                             la_segment_state_t *state,
                             uint32_t *references,
                             uint32_t *sequence,
                             uint32_t *length)
{
    la_segment_slot_t *slot;

    if (pool == NULL || state == NULL || references == NULL || sequence == NULL || length == NULL) {
        return -1;
    }
    lock_pool(pool);
    if (!handle_valid(pool, handle)) {
        unlock_pool(pool);
        return -2;
    }
    slot = &pool->slots[handle.slot];
    *state = slot->state;
    *references = slot->references;
    *sequence = slot->sequence;
    *length = slot->length;
    unlock_pool(pool);
    return 0;
}
