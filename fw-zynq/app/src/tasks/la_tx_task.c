#include "la_app.h"

#include "la_tx_pack.h"
#include "la_tx_win.h"

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
        tx_pump(ctx);
    }
}
