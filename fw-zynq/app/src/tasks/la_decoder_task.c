#include "la_app.h"

#include "la_irq.h"

#include <string.h>

/* P7.3 decoder task: one per decoder slot. Consumes la_decoder_config_msg_t
 * from cfg_q (LIVE_START) and la_live_segment_msg_t from seg_q, feeds
 * la_decoder_core and batches the emitted events into static event blocks
 * (block_q -> ev_q). The batch flush timer is the task's 2 ms notify timeout
 * plus a full-batch flush from the event callback; a dropped batch or event
 * is counted and reported by TxTask as EVT_OVERRUN(stage=decoder). */

#define LA_DECODER_WAIT_TICKS pdMS_TO_TICKS(2)
#define LA_DECODER_FLUSH_TICKS pdMS_TO_TICKS(2)

static void decoder_recycle(la_decoder_ctx_t *ctx, la_event_batch_t *block)
{
    if (block == NULL) {
        return;
    }
    if (xQueueSend(ctx->app->block_q, &block, 0) != pdTRUE) {
        /* The free queue is sized to the whole block pool, so this is a
         * structural error; count it and leak rather than corrupt memory. */
        ctx->dropped_batches++;
    }
}

static void decoder_flush(la_decoder_ctx_t *ctx)
{
    la_event_batch_t *block = ctx->current;

    if (block == NULL) {
        return;
    }
    ctx->current = NULL;
    if (la_event_batch_empty(block)) {
        decoder_recycle(ctx, block);
        return;
    }
    if (xQueueSend(ctx->app->ev_q, &block, 0) != pdTRUE) {
        ctx->dropped_batches++;
        ctx->dropped_events += la_event_batch_records(block);
        ctx->app->decoder_dropped_events[ctx->slot] += la_event_batch_records(block);
        decoder_recycle(ctx, block);
        return;
    }
    ctx->batches++;
    (void)xTaskNotify(ctx->app->tx_task, LA_NOTIFY_TX, eSetBits);
}

static la_event_batch_t *decoder_block_get(la_decoder_ctx_t *ctx)
{
    la_event_batch_t *block = NULL;

    if (ctx->current != NULL) {
        return ctx->current;
    }
    if (xQueueReceive(ctx->app->block_q, &block, 0) != pdTRUE) {
        ctx->dropped_events++;
        ctx->app->decoder_dropped_events[ctx->slot]++;
        return NULL;
    }
    la_event_batch_init(block);
    ctx->current = block;
    return block;
}

static void decoder_event(void *context, const la_decode_event_t *event)
{
    la_decoder_ctx_t *ctx = (la_decoder_ctx_t *)context;
    la_event_batch_t *block = decoder_block_get(ctx);

    if (block == NULL) {
        return;
    }
    if (la_event_batch_add(block, ctx->slot, event) == 0) {
        decoder_flush(ctx);
        block = decoder_block_get(ctx);
        if (block == NULL) {
            return;
        }
        if (la_event_batch_add(block, ctx->slot, event) == 0) {
            /* A single record always fits an empty 1 KiB block. */
            ctx->dropped_events++;
            ctx->app->decoder_dropped_events[ctx->slot]++;
        }
    }
}

static void decoder_apply_config(la_decoder_ctx_t *ctx)
{
    la_decoder_config_msg_t message;

    while (xQueueReceive(ctx->cfg_q, &message, 0) == pdTRUE) {
        if (message.valid != 0u) {
            (void)la_decoder_core_configure(&ctx->core, &message.config);
            ctx->app->decoder_events[ctx->slot] = 0u;
            ctx->app->decoder_dropped_segments[ctx->slot] = 0u;
        }
    }
}

#define LA_DECODER_SEGMENTS_PER_TURN 4u

static void decoder_drain_segments(la_decoder_ctx_t *ctx)
{
    la_live_segment_msg_t message;
    uint32_t drained = 0u;

    /* Bound the per-turn work so a LIVE_STOP notification is handled within
     * a few segment times even when the queue is backed up under overload. */
    while (drained < LA_DECODER_SEGMENTS_PER_TURN &&
           xQueueReceive(ctx->seg_q, &message, 0) == pdTRUE) {
        drained++;
        if (la_decoder_core_enabled(&ctx->core) && message.length >= 4u) {
            const uint32_t *words = (const uint32_t *)(uintptr_t)
                la_live_ring_slot_address(&ctx->app->live_ring, message.slot);
            uint32_t word_count = message.length / 4u;

            (void)la_decoder_core_feed_segment(&ctx->core, words, word_count,
                                               message.sequence, decoder_event,
                                               ctx);
        }
        /* The ring owns the slot state; the pool lock inside is taken under
         * this critical section so AcqTask's repost never interleaves. */
        taskENTER_CRITICAL();
        (void)la_live_ring_release(&ctx->app->live_ring, message.slot);
        taskEXIT_CRITICAL();
        (void)xTaskNotify(ctx->app->acq_task, LA_NOTIFY_SEGMENT_FREE, eSetBits);
        ctx->app->decoder_events[ctx->slot] = ctx->core.stats.events;
        ctx->app->decoder_dropped_segments[ctx->slot] =
            ctx->core.stats.dropped_segments;
    }
}

void la_decoder_task(void *parameters)
{
    la_decoder_ctx_t *ctx = (la_decoder_ctx_t *)parameters;
    TickType_t last_flush;

    if (ctx == NULL || ctx->app == NULL) {
        vTaskDelete(NULL);
        return;
    }
    last_flush = xTaskGetTickCount();
    for (;;) {
        uint32_t bits = 0u;

        (void)xTaskNotifyWait(0u, UINT32_MAX, &bits, LA_DECODER_WAIT_TICKS);
        decoder_apply_config(ctx);
        decoder_drain_segments(ctx);
        if (ctx->current != NULL &&
            ((bits & LA_NOTIFY_LIVE_STOP) != 0u ||
             xTaskGetTickCount() - last_flush >= LA_DECODER_FLUSH_TICKS)) {
            decoder_flush(ctx);
            last_flush = xTaskGetTickCount();
        }
    }
}
