#include "la_app.h"

#include "la_mem_geom.h"
#include "la_trace.h"
#include "la_tx_pack.h"
#include "la_tx_win.h"
#include "xil_cache.h"
#include "xiltimer.h"

#include <string.h>

#define LA_TX_SEND_TIMEOUT pdMS_TO_TICKS(100)

typedef struct la_tx_slot_map {
    uint8_t sequence;
    uint8_t used;
    uint64_t chunk;
} la_tx_slot_map_t;

typedef struct la_tx_ctx {
    la_app_t *app;
    la_tx_pack_t pack;
    la_tx_win_t win;
    la_tx_slot_map_t map[LA_TX_WIN_DEPTH];
    la_tx_job_t job;
    uint8_t active;
    uint8_t begin_sent;
    uint8_t end_sent;
    uint64_t next_chunk;
    uint32_t jobs;
    uint32_t abandons;
    uint32_t reported_drops[LA_DECODER_COUNT];
} la_tx_ctx_t;

static void tx_map_add(la_tx_ctx_t *ctx, uint8_t sequence, uint64_t chunk)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (ctx->map[index].used == 0u) {
            ctx->map[index].sequence = sequence;
            ctx->map[index].chunk = chunk;
            ctx->map[index].used = 1u;
            return;
        }
    }
}

static int tx_map_find(la_tx_ctx_t *ctx, uint8_t sequence, uint64_t *chunk)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (ctx->map[index].used != 0u && ctx->map[index].sequence == sequence) {
            *chunk = ctx->map[index].chunk;
            return 1;
        }
    }
    return 0;
}

static void tx_map_remove(la_tx_ctx_t *ctx, uint8_t sequence)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (ctx->map[index].used != 0u && ctx->map[index].sequence == sequence) {
            ctx->map[index].used = 0u;
            return;
        }
    }
}

static const uint8_t *tx_read(void *context, uint64_t byte_offset)
{
    la_tx_ctx_t *ctx = (la_tx_ctx_t *)context;
    uint64_t total_bytes = ctx->job.total_words * 4u;

    if (byte_offset >= total_bytes) {
        return NULL;
    }
    return (const uint8_t *)(ctx->job.buffer + (uintptr_t)byte_offset);
}

static int tx_write(la_tx_ctx_t *ctx, uint8_t type, uint8_t sequence,
                    const uint8_t *payload, uint16_t length)
{
    uint8_t wire[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    size_t encoded = la_proto_encode(type, sequence, payload, length, wire,
                                     sizeof(wire));
    size_t sent;

    if (encoded == 0u) {
        ctx->app->tx_drops++;
        return -1;
    }
    sent = xStreamBufferSend(ctx->app->tx_sb, wire, encoded,
                             LA_TX_SEND_TIMEOUT);
    if (sent != encoded) {
        ctx->app->tx_drops++;
        return -1;
    }
    la_trace_mark(LA_TRACE_EVENT_MARK_TX_FRAME, type);
    la_link_uart_tx_kick(&ctx->app->link);
    return 0;
}

static void tx_release_job(la_tx_ctx_t *ctx)
{
    ctx->active = 0u;
    ctx->begin_sent = 0u;
    ctx->end_sent = 0u;
    (void)xTaskNotify(ctx->app->acq_task, LA_NOTIFY_JOB_DONE, eSetBits);
}

static void tx_abandon(la_tx_ctx_t *ctx)
{
    if (ctx->active != 0u) {
        ctx->abandons++;
        tx_release_job(ctx);
    }
}

static void tx_pump(la_tx_ctx_t *ctx)
{
    uint8_t payload[LA_PROTO_DATA_CHUNK_MAX + LA_PROTO_DATA_CHUNK_FIELD_SIZE];
    la_tx_pack_out_t out;
    uint8_t sequence;

    if (ctx->active == 0u) {
        return;
    }

    while (la_tx_win_pop_retransmit(&ctx->win, &sequence) == 0) {
        uint64_t chunk;

        if (tx_map_find(ctx, sequence, &chunk) == 0) {
            tx_abandon(ctx);
            return;
        }
        if (la_tx_pack_chunk_at(&ctx->pack, chunk, tx_read, ctx, payload,
                                sizeof(payload), &out) != 0 ||
            out.kind != LA_TX_PACK_CHUNK) {
            tx_abandon(ctx);
            return;
        }
        if (tx_write(ctx, LA_PROTO_DATA_CHUNK, sequence, payload,
                     (uint16_t)(LA_PROTO_DATA_CHUNK_FIELD_SIZE +
                                out.data_length)) != 0) {
            tx_abandon(ctx);
            return;
        }
        ctx->app->tx_retransmits++;
    }

    while (ctx->active != 0u &&
           la_tx_win_outstanding(&ctx->win) < LA_TX_WIN_DEPTH) {
        if (la_tx_pack_next(&ctx->pack, tx_read, ctx, payload, sizeof(payload),
                            &out) != 0) {
            break;
        }
        sequence = la_app_next_sequence(ctx->app);
        if (out.kind == LA_TX_PACK_BEGIN) {
            if (tx_write(ctx, LA_PROTO_DATA_BEGIN, sequence, payload,
                         LA_PROTO_DATA_BEGIN_WIRE_SIZE) != 0) {
                tx_abandon(ctx);
                return;
            }
            ctx->begin_sent = 1u;
        } else if (out.kind == LA_TX_PACK_CHUNK) {
            if (la_tx_win_push(&ctx->win, sequence) != 0) {
                tx_abandon(ctx);
                return;
            }
            tx_map_add(ctx, sequence, ctx->next_chunk);
            ctx->next_chunk++;
            if (tx_write(ctx, LA_PROTO_DATA_CHUNK, sequence, payload,
                         (uint16_t)(LA_PROTO_DATA_CHUNK_FIELD_SIZE +
                                    out.data_length)) != 0) {
                tx_abandon(ctx);
                return;
            }
            ctx->app->tx_frames++;
        } else {
            if (tx_write(ctx, LA_PROTO_DATA_END, sequence, payload,
                         LA_PROTO_DATA_END_WIRE_SIZE) != 0) {
                tx_abandon(ctx);
                return;
            }
            ctx->end_sent = 1u;
            break;
        }
    }

    if (ctx->active != 0u && ctx->end_sent != 0u &&
        la_tx_win_outstanding(&ctx->win) == 0u) {
        ctx->jobs++;
        tx_release_job(ctx);
    }
}

static void tx_start_job(la_tx_ctx_t *ctx, const la_tx_job_t *job)
{
    if (ctx->active != 0u) {
        tx_abandon(ctx);
    }
    ctx->job = *job;
#if LA_MEM_LIVE_CACHED
    /* P8.5: the burst payload shares the cacheable live-pool prefix; drop the
     * stale lines from the previous CPU read before DMA-written data is
     * packed. total_words is bounded by the 8 MiB capture pool. */
    if (job->total_words != 0u) {
        uint32_t bytes = (uint32_t)(job->total_words * 4u);
        if (bytes > LA_MEM_CAPTURE_SIZE) {
            bytes = LA_MEM_CAPTURE_SIZE;
        }
        Xil_DCacheInvalidateRange((INTPTR)job->buffer, (bytes + 63u) & ~63u);
    }
#endif
    if (la_tx_pack_init(&ctx->pack, job->total_words, job->encoding,
                        job->sample_rate_hz) != 0) {
        (void)xTaskNotify(ctx->app->acq_task, LA_NOTIFY_JOB_DONE, eSetBits);
        return;
    }
    memset(&ctx->win, 0, sizeof(ctx->win));
    memset(ctx->map, 0, sizeof(ctx->map));
    ctx->active = 1u;
    ctx->begin_sent = 0u;
    ctx->end_sent = 0u;
    ctx->next_chunk = 0u;
    tx_pump(ctx);
}

static void tx_handle_ack(la_tx_ctx_t *ctx, const la_ack_msg_t *message)
{
    switch (message->kind) {
    case LA_ACK_CHUNK:
        if (la_tx_win_ack(&ctx->win, message->sequence) == 0) {
            tx_map_remove(ctx, message->sequence);
        }
        break;
    case LA_ACK_CHUNK_NAK:
        (void)la_tx_win_nak(&ctx->win, message->sequence);
        break;
    case LA_ACK_BURST_NAK:
    case LA_ACK_ABORT:
        tx_abandon(ctx);
        return;
    default:
        return;
    }
    tx_pump(ctx);
}

static void tx_send_frame(la_tx_ctx_t *ctx, const la_tx_frame_t *frame)
{
    (void)tx_write(ctx, frame->type, frame->sequence, frame->payload,
                   frame->length);
}

static void tx_report_decoder_overruns(la_tx_ctx_t *ctx)
{
    uint8_t slot;

    for (slot = 0u; slot < LA_DECODER_COUNT; ++slot) {
        uint32_t current = ctx->app->decoder_dropped_events[slot];

        if (current != ctx->reported_drops[slot]) {
            uint8_t payload[LA_PROTO_EVT_OVERRUN_WIRE_SIZE];
            uint32_t delta = current - ctx->reported_drops[slot];

            ctx->reported_drops[slot] = current;
            if (la_proto_pack_evt_overrun(delta, 1u, payload, sizeof(payload)) ==
                LA_PROTO_EVT_OVERRUN_WIRE_SIZE) {
                (void)tx_write(ctx, LA_PROTO_EVT_OVERRUN,
                               la_app_next_sequence(ctx->app), payload,
                               (uint16_t)sizeof(payload));
            }
        }
    }
}

/* P8.2: worst UART-byte-to-send latency for the live stream. The last record
 * in the block is the newest event; its t_sample is the wire time in
 * (sample_div+1) sample clocks and the ARM XTime is the epoch. */
static void tx_track_latency(la_tx_ctx_t *ctx, const la_event_batch_t *block)
{
    uint64_t arm = ctx->app->live_arm_time;
    uint32_t offset = 0u;
    uint64_t t_sample = 0u;
    uint64_t elapsed_us;
    uint64_t sample_us;
    uint32_t latency_us;
    XTime now;

    if (arm == 0u || block == NULL || block->length == 0u) {
        return;
    }
    while (offset + 11u <= block->length) {
        uint64_t t = 0u;
        uint32_t index;
        uint8_t length;

        for (index = 0u; index < 8u; ++index) {
            t |= (uint64_t)block->data[offset + 2u + index] << (8u * index);
        }
        length = block->data[offset + 10u];
        t_sample = t;
        offset += 11u + (uint32_t)length;
    }
    XTime_GetTime(&now);
    elapsed_us = ((uint64_t)(now - arm) * 1000000u) / (uint64_t)COUNTS_PER_SECOND;
    sample_us = t_sample * (uint64_t)(ctx->app->live_sample_div + 1u) / 100u;
    if (elapsed_us > sample_us) {
        latency_us = (uint32_t)(elapsed_us - sample_us);
        if (latency_us > ctx->app->live_max_latency_us) {
            ctx->app->live_max_latency_us = latency_us;
        }
    }
}

static void tx_drain_events(la_tx_ctx_t *ctx)
{
    la_event_batch_t *block;

    /* Report drops before draining the backlog: under overload the ev_q is
     * never empty, so a report placed after the drain would only be sent once
     * the stream ends - too late for the host to see it live. */
    tx_report_decoder_overruns(ctx);
    while (xQueueReceive(ctx->app->ev_q, &block, 0) == pdTRUE) {
        if (block != NULL && block->length != 0u) {
            size_t needed = (size_t)block->length + LA_PROTO_FRAME_OVERHEAD;

            /* Bound the drain by the TX stream buffer: under overload the
             * producers keep the ev_q non-empty, so an unbounded drain never
             * returns and starves the control path (jobs, ACKs, STATUS).
             * Put the block back and let the UART drain before retrying. */
            if (xStreamBufferSpacesAvailable(ctx->app->tx_sb) < needed) {
                if (xQueueSendToFront(ctx->app->ev_q, &block, 0) != pdTRUE) {
                    ctx->app->tx_drops++;
                    if (xQueueSend(ctx->app->block_q, &block, 0) != pdTRUE) {
                        ctx->app->tx_drops++;
                    }
                }
                return;
            }
            tx_track_latency(ctx, block);
            (void)tx_write(ctx, LA_PROTO_EVT_DECODED,
                           la_app_next_sequence(ctx->app), block->data,
                           block->length);
        }
        if (block != NULL &&
            xQueueSend(ctx->app->block_q, &block, 0) != pdTRUE) {
            ctx->app->tx_drops++;
        }
    }
}

static la_tx_ctx_t s_tx_ctx;

void la_tx_task(void *parameters)
{
    la_app_t *app = (la_app_t *)parameters;
    la_tx_ctx_t *ctx = &s_tx_ctx;

    if (app == NULL) {
        vTaskDelete(NULL);
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;

    for (;;) {
        (void)xTaskNotifyWait(0u, UINT32_MAX, NULL, pdMS_TO_TICKS(2));

        for (;;) {
            la_tx_job_t job;

            if (xQueueReceive(app->job_q, &job, 0) != pdTRUE) {
                break;
            }
            tx_start_job(ctx, &job);
        }
        for (;;) {
            la_ack_msg_t ack;

            if (xQueueReceive(app->ack_q, &ack, 0) != pdTRUE) {
                break;
            }
            tx_handle_ack(ctx, &ack);
        }
        for (;;) {
            la_tx_frame_t frame;

            if (xQueueReceive(app->tx_q, &frame, 0) != pdTRUE) {
                break;
            }
            tx_send_frame(ctx, &frame);
        }
        tx_drain_events(ctx);
        tx_pump(ctx);
    }
}
