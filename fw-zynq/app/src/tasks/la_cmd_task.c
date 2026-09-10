#include "la_app.h"

#include "la_cmd_dispatch.h"
#include "la_mem_geom.h"
#include "la_regs.h"

#include <stdio.h>
#include <string.h>

#define LA_CMD_RX_CHUNK 128u
#define LA_CMD_QUEUE_TIMEOUT pdMS_TO_TICKS(20)

typedef struct la_cmd_ctx {
    la_app_t *app;
    la_proto_parser_t parser;
    la_cmd_dispatch_t dispatch;
    uint8_t rx_chunk[LA_CMD_RX_CHUNK];
} la_cmd_ctx_t;

static void cmd_post_frame(la_cmd_ctx_t *ctx, uint8_t type, uint8_t sequence,
                           const uint8_t *payload, uint16_t length)
{
    la_app_t *app = ctx->app;
    la_tx_frame_t frame;

    if (length > sizeof(frame.payload)) {
        app->tx_drops++;
        return;
    }
    frame.type = type;
    frame.sequence = sequence;
    frame.length = length;
    if (length != 0u) {
        memcpy(frame.payload, payload, length);
    }
    if (xQueueSend(app->tx_q, &frame, LA_CMD_QUEUE_TIMEOUT) != pdTRUE) {
        app->tx_drops++;
        return;
    }
    (void)xTaskNotify(app->tx_task, LA_NOTIFY_TX, eSetBits);
}

static void cmd_post_ack(la_cmd_ctx_t *ctx, uint8_t type, uint8_t sequence)
{
    uint8_t payload[LA_PROTO_ACK_WIRE_SIZE];

    (void)la_proto_pack_ack(type, sequence, payload, sizeof(payload));
    cmd_post_frame(ctx, LA_PROTO_ACK, la_app_next_sequence(ctx->app), payload,
                   LA_PROTO_ACK_WIRE_SIZE);
}

static void cmd_post_nak(la_cmd_ctx_t *ctx, uint8_t type, uint8_t sequence,
                         uint8_t error)
{
    uint8_t payload[LA_PROTO_NAK_WIRE_SIZE];

    (void)la_proto_pack_nak(type, sequence, error, payload, sizeof(payload));
    cmd_post_frame(ctx, LA_PROTO_NAK, la_app_next_sequence(ctx->app), payload,
                   LA_PROTO_NAK_WIRE_SIZE);
}

static void cmd_post_status(la_cmd_ctx_t *ctx)
{
    la_app_t *app = ctx->app;
    la_proto_status_t status;
    uint8_t payload[LA_PROTO_STATUS_WIRE_SIZE];

    memset(&status, 0, sizeof(status));
    status.state = (uint8_t)la_cmd_dispatch_view(&ctx->dispatch);
    status.status_reg = la_regs_read_status(&app->regs);
    status.word_count = app->word_count;
    status.trig_index = app->trig_index;
    status.cpu_load_permille = app->cpu_load_permille;
    status.min_free_stack = app->min_free_stack;
    if (la_proto_pack_status(&status, payload, sizeof(payload)) !=
        LA_PROTO_STATUS_WIRE_SIZE) {
        app->tx_drops++;
        return;
    }
    cmd_post_frame(ctx, LA_PROTO_STATUS, la_app_next_sequence(app), payload,
                   LA_PROTO_STATUS_WIRE_SIZE);
}

static void cmd_post_log(la_cmd_ctx_t *ctx, const char *text)
{
    size_t length = strlen(text);

    if (length > sizeof(((la_tx_frame_t *)0)->payload)) {
        length = sizeof(((la_tx_frame_t *)0)->payload);
    }
    cmd_post_frame(ctx, LA_PROTO_LOG, la_app_next_sequence(ctx->app),
                   (const uint8_t *)text, (uint16_t)length);
}

static void cmd_forward_ctrl(la_cmd_ctx_t *ctx, la_ctrl_kind_t kind,
                             const la_proto_config_t *config)
{
    la_app_t *app = ctx->app;
    la_ctrl_msg_t message;

    memset(&message, 0, sizeof(message));
    message.kind = kind;
    if (config != NULL) {
        message.config = *config;
        message.config_valid = 1u;
    }
    if (xQueueSend(app->ctrl_q, &message, 0) != pdTRUE) {
        cmd_post_log(ctx, "ctrl queue full");
        return;
    }
    (void)xTaskNotify(app->acq_task, LA_NOTIFY_CTRL, eSetBits);
}

static void cmd_post_ack_msg(la_cmd_ctx_t *ctx, const la_cmd_result_t *result)
{
    la_app_t *app = ctx->app;
    la_ack_msg_t message;

    memset(&message, 0, sizeof(message));
    switch (result->outcome) {
    case LA_CMD_OUT_CHUNK_ACK:
        message.kind = LA_ACK_CHUNK;
        break;
    case LA_CMD_OUT_CHUNK_NAK:
        message.kind = LA_ACK_CHUNK_NAK;
        break;
    case LA_CMD_OUT_BURST_NAK:
        message.kind = LA_ACK_BURST_NAK;
        break;
    default:
        return;
    }
    message.type = result->type;
    message.sequence = result->sequence;
    message.error = result->error;
    if (xQueueSend(app->ack_q, &message, 0) == pdTRUE) {
        (void)xTaskNotify(app->tx_task, LA_NOTIFY_TX, eSetBits);
    } else {
        app->tx_drops++;
    }
}

static void cmd_handle_outcome(la_cmd_ctx_t *ctx, const la_cmd_result_t *result)
{
    switch (result->outcome) {
    case LA_CMD_OUT_ACK:
        cmd_post_ack(ctx, result->type, result->sequence);
        break;
    case LA_CMD_OUT_NAK:
        cmd_post_nak(ctx, result->type, result->sequence, result->error);
        break;
    case LA_CMD_OUT_PONG:
        cmd_post_frame(ctx, LA_PROTO_PONG, la_app_next_sequence(ctx->app),
                       result->payload, LA_PROTO_PONG_WIRE_SIZE);
        break;
    case LA_CMD_OUT_ARM:
        cmd_post_ack(ctx, result->type, result->sequence);
        cmd_forward_ctrl(ctx, LA_CTRL_ARM, la_cmd_dispatch_config(&ctx->dispatch));
        break;
    case LA_CMD_OUT_ABORT:
        cmd_post_ack(ctx, result->type, result->sequence);
        cmd_forward_ctrl(ctx, LA_CTRL_ABORT, NULL);
        break;
    case LA_CMD_OUT_REQUEST_STATUS:
        cmd_post_status(ctx);
        break;
    case LA_CMD_OUT_PATGEN:
        /* The dispatch result carries the request type/sequence; acknowledge
         * first, then latch the register (rtos-design.md P4.9). */
        cmd_post_ack(ctx, result->type, result->sequence);
        la_regs_write32(&ctx->app->regs, LA_REG_PATGEN_CTRL_OFFSET,
                        la_cmd_dispatch_patgen_ctrl(&ctx->dispatch));
        break;
    case LA_CMD_OUT_CHUNK_ACK:
    case LA_CMD_OUT_CHUNK_NAK:
    case LA_CMD_OUT_BURST_NAK:
        cmd_post_ack_msg(ctx, result);
        break;
    default:
        break;
    }
}

static void cmd_frame_callback(void *context, uint8_t type, uint8_t sequence,
                               const uint8_t *payload, uint16_t length)
{
    la_cmd_ctx_t *ctx = (la_cmd_ctx_t *)context;
    la_cmd_result_t result;
    uint64_t uptime_ms = (uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

    ctx->app->rx_frames++;
    if (la_cmd_dispatch_step(&ctx->dispatch, type, sequence, payload, length,
                             uptime_ms, &result) == 0) {
        cmd_handle_outcome(ctx, &result);
    }
}

static void cmd_handle_notice(la_cmd_ctx_t *ctx, const la_notice_t *notice)
{
    la_app_t *app = ctx->app;

    switch (notice->kind) {
    case LA_NOTICE_ARMED:
        la_cmd_dispatch_acq_notice(&ctx->dispatch, LA_CMD_ACQ_ARMED);
        break;
    case LA_NOTICE_TRIGGERED: {
        uint8_t payload[8];
        uint64_t index = notice->trig_index;
        unsigned index_byte;

        la_cmd_dispatch_acq_notice(&ctx->dispatch, LA_CMD_ACQ_TRIGGERED);
        for (index_byte = 0u; index_byte < 8u; ++index_byte) {
            payload[index_byte] = (uint8_t)(index & 0xFFu);
            index >>= 8;
        }
        cmd_post_frame(ctx, LA_PROTO_EVT_TRIGGERED, la_app_next_sequence(app),
                       payload, sizeof(payload));
        break;
    }
    case LA_NOTICE_DONE:
        la_cmd_dispatch_acq_notice(&ctx->dispatch, LA_CMD_ACQ_DONE);
        break;
    case LA_NOTICE_ABORTED: {
        char text[48];

        la_cmd_dispatch_acq_notice(&ctx->dispatch, LA_CMD_ACQ_ABORTED);
        (void)snprintf(text, sizeof(text), "acq aborted status=0x%08X",
                       (unsigned)notice->status);
        cmd_post_log(ctx, text);
        break;
    }
    default:
        break;
    }
}

static la_cmd_ctx_t s_cmd_ctx;

void la_cmd_task(void *parameters)
{
    la_app_t *app = (la_app_t *)parameters;
    la_cmd_ctx_t *ctx = &s_cmd_ctx;

    if (app == NULL) {
        vTaskDelete(NULL);
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    la_proto_parser_init(&ctx->parser);
    la_cmd_dispatch_init(&ctx->dispatch, LA_FW_VERSION,
                         la_regs_read_id(&app->regs));
    la_cmd_dispatch_set_pool(&ctx->dispatch, LA_MEM_CAPTURE_SIZE);

    for (;;) {
        size_t received = xStreamBufferReceive(app->rx_sb, ctx->rx_chunk,
                                               sizeof(ctx->rx_chunk),
                                               pdMS_TO_TICKS(5));

        if (received != 0u) {
            (void)la_proto_parser_feed(&ctx->parser, ctx->rx_chunk, received,
                                       cmd_frame_callback, ctx);
        }
        for (;;) {
            la_notice_t notice;

            if (xQueueReceive(app->notice_q, &notice, 0) != pdTRUE) {
                break;
            }
            cmd_handle_notice(ctx, &notice);
        }
    }
}
