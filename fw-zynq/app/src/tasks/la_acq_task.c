#include "la_app.h"

#include "la_acq_fsm.h"
#include "la_burst_plan.h"
#include "la_mem_geom.h"
#include "la_regs.h"
#include "xiltimer.h"

#include <string.h>

#define LA_ACQ_COMPLETE_POLLS 50u

typedef struct la_acq_ctx {
    la_app_t *app;
    la_acq_fsm_t fsm;
    la_burst_plan_t plan;
    uint8_t buffer_busy;
    uint8_t encoding;
    uint32_t sample_rate_hz;
    uint32_t timeout_ms;
    uint64_t trig_index;
    uint64_t word_count;
    uint32_t status;
    uint8_t live;
    uint8_t overflow_seen;
    uint8_t decoder_enabled[LA_PROTO_DECODER_MAX_SLOTS];
    uint32_t enabled_decoders;
} la_acq_ctx_t;

static void acq_post_overrun(la_acq_ctx_t *ctx, uint32_t dropped, uint8_t stage)
{
    la_notice_t notice;

    memset(&notice, 0, sizeof(notice));
    notice.kind = LA_NOTICE_OVERRUN;
    notice.dropped = dropped;
    notice.stage = stage;
    notice.status = ctx->status;
    (void)xQueueSend(ctx->app->notice_q, &notice, 0);
    ctx->app->live_overflows++;
}

static void acq_wait_buffer_free(la_acq_ctx_t *ctx)
{
    while (ctx->buffer_busy != 0u) {
        uint32_t bits = 0u;

        if (xTaskNotifyWait(0u, UINT32_MAX, &bits, pdMS_TO_TICKS(100)) == pdTRUE) {
            la_irq_note_task_wake(&ctx->app->irq);
            if ((bits & LA_NOTIFY_JOB_DONE) != 0u) {
                ctx->buffer_busy = 0u;
            }
        }
    }
}

static void acq_post_notice(la_acq_ctx_t *ctx, la_notice_kind_t kind)
{
    la_notice_t notice;

    notice.kind = kind;
    notice.trig_index = ctx->trig_index;
    notice.word_count = ctx->word_count;
    notice.status = ctx->status;
    (void)xQueueSend(ctx->app->notice_q, &notice, 0);
}

static void acq_apply_config(la_app_t *app, const la_proto_config_t *config)
{
    la_regs_write32(&app->regs, LA_REG_SAMPLE_DIV_OFFSET, config->sample_div);
    la_regs_write32(&app->regs, LA_REG_MODE_OFFSET,
                    config->mode & LA_REG_MODE_ALLOWED_MASK);
    la_regs_write32(&app->regs, LA_REG_CHAN_EN_OFFSET, config->chan_en);
    la_regs_write32(&app->regs, LA_REG_TRIG_MASK_OFFSET, config->trig_mask);
    la_regs_write32(&app->regs, LA_REG_TRIG_VALUE_OFFSET, config->trig_value);
    la_regs_write32(&app->regs, LA_REG_TRIG_RISE_OFFSET, config->trig_rise);
    la_regs_write32(&app->regs, LA_REG_TRIG_FALL_OFFSET, config->trig_fall);
    la_regs_write32(&app->regs, LA_REG_PRE_SAMPLES_OFFSET, config->pre_samples);
    la_regs_write32(&app->regs, LA_REG_POST_SAMPLES_OFFSET, config->post_samples);
    la_regs_write32(&app->regs, LA_REG_TIMEOUT_OFFSET, config->timeout);
    la_regs_write32(&app->regs, LA_REG_TXN_MAX_RUN_OFFSET, config->txn_max_run);
    la_regs_write32(&app->regs, LA_REG_LIVE_FLUSH_CYCLES_OFFSET,
                    config->live_flush_cycles);
    /* Sticky CTRL bits only; ARM/ABORT/SOFT_RESET are pulse commands. */
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET,
                    config->ctrl_flags &
                        (LA_REG_CTRL_SELFTEST_EN_MASK | LA_REG_CTRL_STREAM_MASK));
}

static void acq_abort(la_acq_ctx_t *ctx, int from_timeout)
{
    la_app_t *app = ctx->app;
    la_acq_event_t event;

    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET, LA_REG_CTRL_ABORT_MASK);
    la_dma_stop(&app->dma);
    if (from_timeout) {
        if (la_acq_fsm_timeout(&ctx->fsm, &event) != 0) {
            return;
        }
    } else {
        if (la_acq_fsm_abort(&ctx->fsm, &event) != 0) {
            return;
        }
    }
    ctx->word_count = 0u;
    ctx->status = la_regs_read_status(&app->regs);
    app->status_reg = ctx->status;
    /* Discard partial PL data (FIFO/packer/ring) so it cannot leak into the
     * next capture's descriptor chain. The soft reset also clears the
     * register block; the next ARM re-applies the full configuration. */
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET, LA_REG_CTRL_SOFT_RESET_MASK);
    acq_post_notice(ctx, LA_NOTICE_ABORTED);
}

static void acq_complete(la_acq_ctx_t *ctx)
{
    la_app_t *app = ctx->app;
    la_acq_event_t event;
    la_tx_job_t job;
    uint32_t bytes = 0u;
    int eof = 0;
    uint32_t attempt;

    /* The PL reports DONE when the closing word has entered the FIFO; the
     * DMA may still be draining the tail into the last BD. Poll until the
     * RXEOF beat lands (or the DMA errors). */
    for (attempt = 0u; attempt < LA_ACQ_COMPLETE_POLLS; ++attempt) {
        (void)la_dma_collect(&app->dma, &bytes, &eof);
        if (eof || app->dma.error != 0u) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (app->dma.error != 0u) {
        acq_abort(ctx, 0);
        return;
    }

    ctx->word_count = (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_HI_OFFSET)
                          << 32 |
                      (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_LO_OFFSET);
    ctx->status = la_regs_read_status(&app->regs);
    la_dma_stop(&app->dma);
    if (la_acq_fsm_complete(&ctx->fsm, ctx->word_count, ctx->status, &event) != 0) {
        return;
    }
    app->status_reg = ctx->status;
    app->word_count = ctx->word_count;
    app->trig_index = ctx->trig_index;

    /* Empty jobs still emit DATA_BEGIN + DATA_END with the empty CRC, so
     * the host always completes the burst instead of timing out. */
    job.buffer = la_mem_capture_base();
    job.total_words = ctx->word_count;
    job.encoding = ctx->encoding;
    job.sample_rate_hz = ctx->sample_rate_hz;
    job.status = ctx->status;
    job.trig_index = ctx->trig_index;
    if (xQueueSend(app->job_q, &job, 0) == pdTRUE) {
        ctx->buffer_busy = 1u;
        (void)xTaskNotify(app->tx_task, LA_NOTIFY_TX, eSetBits);
    }
    acq_post_notice(ctx, LA_NOTICE_DONE);
}

static int acq_live_repost(la_acq_ctx_t *ctx)
{
    la_app_t *app = ctx->app;
    int result = 0;

    taskENTER_CRITICAL();
    while (la_dma_free_bd_count(&app->dma) > 0u) {
        uint32_t slot = 0u;

        if (la_live_ring_repost(&app->live_ring, &slot) < 0) {
            break;
        }
        if (la_dma_post_live(&app->dma, slot,
                             la_live_ring_slot_address(&app->live_ring, slot),
                             LA_LIVE_SEGMENT_BYTES) != 0) {
            result = -1;
            break;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

static void acq_start_live(la_acq_ctx_t *ctx, const la_ctrl_msg_t *message)
{
    la_app_t *app = ctx->app;
    uintptr_t addresses[LA_LIVE_SEGMENT_COUNT];
    uint32_t slot;
    uint8_t d;

    acq_wait_buffer_free(ctx);

    acq_apply_config(app, &message->config);
    la_regs_write32(&app->regs, LA_REG_IRQ_STAT_OFFSET,
                    LA_REG_IRQ_STAT_ALLOWED_MASK);
    la_regs_write32(&app->regs, LA_REG_IRQ_EN_OFFSET,
                    LA_REG_IRQ_EN_DONE_MASK | LA_REG_IRQ_EN_TRIGGERED_MASK |
                        LA_REG_IRQ_EN_OVERFLOW_MASK);

    (void)la_live_ring_abort(&app->live_ring);
    if (la_live_ring_start(&app->live_ring) != (int)LA_LIVE_SEGMENT_COUNT) {
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }
    for (slot = 0u; slot < LA_LIVE_SEGMENT_COUNT; ++slot) {
        addresses[slot] = la_live_ring_slot_address(&app->live_ring, slot);
    }
    if (la_dma_prepare_live(&app->dma, addresses, LA_LIVE_SEGMENT_COUNT,
                            LA_LIVE_SEGMENT_BYTES) != 0) {
        (void)la_live_ring_abort(&app->live_ring);
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }

    ctx->enabled_decoders = 0u;
    for (d = 0u; d < LA_PROTO_DECODER_MAX_SLOTS; ++d) {
        ctx->decoder_enabled[d] = 0u;
        if (message->decoder_valid[d] != 0u &&
            message->decoders[d].type != LA_PROTO_DECODER_OFF) {
            la_decoder_config_msg_t config;

            memset(&config, 0, sizeof(config));
            config.config = message->decoders[d];
            config.valid = 1u;
            if (xQueueSend(app->decoders[d].cfg_q, &config, 0) == pdTRUE) {
                ctx->decoder_enabled[d] = 1u;
                ctx->enabled_decoders++;
                (void)xTaskNotify(app->decoders[d].task, LA_NOTIFY_DECODER, eSetBits);
            }
        }
    }

    if (la_acq_fsm_configure(&ctx->fsm, 1) != 0 ||
        la_acq_fsm_arm(&ctx->fsm) != 0) {
        (void)la_live_ring_abort(&app->live_ring);
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }
    ctx->live = 1u;
    ctx->overflow_seen = 0u;
    ctx->trig_index = 0u;
    app->live_active = 1u;
    app->word_count = 0u;
    app->trig_index = 0u;
    /* The latency maximum accumulates across live sessions until reboot so
     * repeated shots can be bounded without console sampling races. */
    app->live_sample_div = message->config.sample_div;
    XTime_GetTime(&app->live_arm_time);
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET,
                    LA_REG_CTRL_ARM_MASK |
                        (message->config.ctrl_flags &
                         (LA_REG_CTRL_SELFTEST_EN_MASK | LA_REG_CTRL_STREAM_MASK)));
    acq_post_notice(ctx, LA_NOTICE_STREAMING);
}

static void acq_stop_live(la_acq_ctx_t *ctx, int report_overrun, uint32_t dropped)
{
    la_app_t *app = ctx->app;
    la_acq_event_t event;
    uint32_t guard;
    uint8_t d;

    ctx->live = 0u;
    app->live_active = 0u;
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET, LA_REG_CTRL_ABORT_MASK);
    la_dma_stop(&app->dma);

    for (d = 0u; d < LA_PROTO_DECODER_MAX_SLOTS; ++d) {
        if (app->decoders[d].task != NULL) {
            (void)xTaskNotify(app->decoders[d].task, LA_NOTIFY_LIVE_STOP, eSetBits);
        }
    }
    /* Consumers release their references as they drain; abort frees every
     * DMA-owned slot at once and reports the CPU-owned remainder. */
    for (guard = 0u; guard < 200u; ++guard) {
        uint32_t pending;

        taskENTER_CRITICAL();
        pending = (uint32_t)la_live_ring_abort(&app->live_ring);
        taskEXIT_CRITICAL();
        if (pending == 0u) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    taskENTER_CRITICAL();
    (void)la_live_ring_abort(&app->live_ring);
    taskEXIT_CRITICAL();

    ctx->word_count =
        (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_HI_OFFSET) << 32 |
        (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_LO_OFFSET);
    ctx->status = la_regs_read_status(&app->regs);
    app->status_reg = ctx->status;
    app->word_count = ctx->word_count;
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET, LA_REG_CTRL_SOFT_RESET_MASK);

    if (report_overrun != 0) {
        acq_post_overrun(ctx, dropped, 0u);
    }
    if (la_acq_fsm_abort(&ctx->fsm, &event) == 0) {
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
    }
}

static void acq_handle_live_irq(la_acq_ctx_t *ctx)
{
    la_app_t *app = ctx->app;
    la_dma_live_bd_t entries[LA_LIVE_SEGMENT_COUNT];
    int count;
    int index;
    int status;

    count = la_dma_collect_live(&app->dma, entries, LA_LIVE_SEGMENT_COUNT);
    if (count < 0 || app->dma.error != 0u) {
        acq_stop_live(ctx, 1, 0u);
        return;
    }
    ctx->word_count =
        (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_HI_OFFSET) << 32 |
        (uint64_t)la_regs_read32(&app->regs, LA_REG_WORD_COUNT_LO_OFFSET);
    app->word_count = ctx->word_count;

    for (index = 0; index < count; ++index) {
        la_live_segment_msg_t message;
        uint8_t d;

        if (entries[index].id >= LA_LIVE_SEGMENT_COUNT) {
            acq_stop_live(ctx, 1, 0u);
            return;
        }
        message.slot = entries[index].id;
        message.length = entries[index].length > LA_LIVE_SEGMENT_BYTES
                             ? LA_LIVE_SEGMENT_BYTES
                             : entries[index].length;
        taskENTER_CRITICAL();
        status = la_live_ring_complete(&app->live_ring, message.slot,
                                       message.length, ctx->enabled_decoders);
        if (status == 0) {
            message.handle = app->live_ring.slots[message.slot].handle;
            message.sequence = la_live_ring_slot_sequence(&app->live_ring,
                                                          message.slot);
        }
        taskEXIT_CRITICAL();
        if (status != 0) {
            continue;
        }
        for (d = 0u; d < LA_PROTO_DECODER_MAX_SLOTS; ++d) {
            if (ctx->decoder_enabled[d] == 0u) {
                continue;
            }
            if (xQueueSend(app->decoders[d].seg_q, &message, 0) == pdTRUE) {
                (void)xTaskNotify(app->decoders[d].task, LA_NOTIFY_DECODER, eSetBits);
            } else {
                /* Queue-full releases only this decoder's reference. */
                (void)la_live_ring_release(&app->live_ring, message.slot);
                app->decoder_queue_drops[d]++;
            }
        }
    }

    if (acq_live_repost(ctx) != 0) {
        acq_stop_live(ctx, 1, 0u);
    }
    /* A ring with no posted descriptor is not itself an error: consumers
     * release their references within a few milliseconds and the next
     * SEGMENT_FREE notification reposts. If they fall too far behind, the
     * PL FIFO overflows, stops and reports DONE - that path carries the
     * accurate overflow event. */
}

static void acq_handle_pl(la_acq_ctx_t *ctx)
{
    la_app_t *app = ctx->app;
    uint32_t pending;

    taskENTER_CRITICAL();
    pending = app->irq.pl_pending;
    app->irq.pl_pending = 0u;
    taskEXIT_CRITICAL();
    if (pending == 0u) {
        return;
    }
    if ((pending & LA_REG_IRQ_STAT_TRIGGERED_MASK) != 0u) {
        uint32_t index = la_regs_read32(&app->regs, LA_REG_TRIG_INDEX_OFFSET);
        la_acq_event_t event;

        if (la_acq_fsm_trigger(&ctx->fsm, index, &event) == 0) {
            ctx->trig_index = index;
            app->trig_index = index;
            acq_post_notice(ctx, LA_NOTICE_TRIGGERED);
        }
    }
    if ((pending & LA_REG_IRQ_STAT_OVERFLOW_MASK) != 0u) {
        ctx->overflow_seen = 1u;
    }
    if ((pending & LA_REG_IRQ_STAT_DONE_MASK) != 0u) {
        if (ctx->live != 0u) {
            /* In stream mode DONE follows an overflow stop and drain. */
            acq_handle_live_irq(ctx);
            if (ctx->live != 0u) {
                acq_stop_live(ctx, 1, 0u);
            }
        } else {
            acq_complete(ctx);
        }
    }
}

static void acq_handle_ctrl(la_acq_ctx_t *ctx, const la_ctrl_msg_t *message)
{
    la_app_t *app = ctx->app;
    uint64_t words;

    if (message->kind == LA_CTRL_ABORT) {
        if (ctx->live != 0u) {
            acq_stop_live(ctx, 0, 0u);
            return;
        }
        if (la_acq_fsm_state(&ctx->fsm) != LA_ACQ_IDLE) {
            acq_abort(ctx, 0);
        }
        return;
    }
    if (message->kind == LA_CTRL_LIVE_STOP) {
        if (ctx->live != 0u) {
            acq_stop_live(ctx, 0, 0u);
        }
        return;
    }
    if (message->kind == LA_CTRL_LIVE_START) {
        if (ctx->live == 0u && la_acq_fsm_state(&ctx->fsm) == LA_ACQ_IDLE &&
            message->config_valid != 0u) {
            acq_start_live(ctx, message);
        }
        return;
    }
    if (message->kind != LA_CTRL_ARM || message->config_valid == 0u) {
        return;
    }
    acq_wait_buffer_free(ctx);

    ctx->encoding = (uint8_t)(message->config.mode & LA_REG_MODE_ENCODING_MASK);
    ctx->sample_rate_hz = 100000000u / (message->config.sample_div + 1u);
    ctx->timeout_ms = la_burst_plan_timeout_ms(message->config.timeout,
                                               message->config.sample_div);
    ctx->trig_index = 0u;
    if (la_burst_plan_words(ctx->encoding, message->config.pre_samples,
                            message->config.post_samples, &words) != 0 ||
        la_burst_plan_compute(words, app->dma.bd_max_bytes,
                              LA_MEM_CAPTURE_SIZE, LA_BURST_PLAN_MAX_BDS,
                              &ctx->plan) != LA_BURST_PLAN_OK) {
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }

    acq_apply_config(app, &message->config);
    la_regs_write32(&app->regs, LA_REG_IRQ_STAT_OFFSET,
                    LA_REG_IRQ_STAT_ALLOWED_MASK);
    la_regs_write32(&app->regs, LA_REG_IRQ_EN_OFFSET,
                    LA_REG_IRQ_EN_DONE_MASK | LA_REG_IRQ_EN_TRIGGERED_MASK |
                        LA_REG_IRQ_EN_OVERFLOW_MASK);
    if (la_dma_prepare(&app->dma, &ctx->plan, la_mem_capture_base()) != 0) {
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }
    (void)la_acq_fsm_configure(&ctx->fsm, 0);
    if (la_acq_fsm_arm(&ctx->fsm) != 0) {
        la_dma_stop(&app->dma);
        acq_post_notice(ctx, LA_NOTICE_ABORTED);
        return;
    }
    /* ARM together with the sticky CTRL bits: a plain ARM write would
     * clear SELFTEST_EN/STREAM because the register block keeps only the
     * sticky mask (P2.11). */
    la_regs_write32(&app->regs, LA_REG_CTRL_OFFSET,
                    LA_REG_CTRL_ARM_MASK |
                        (message->config.ctrl_flags &
                         (LA_REG_CTRL_SELFTEST_EN_MASK | LA_REG_CTRL_STREAM_MASK)));
    acq_post_notice(ctx, LA_NOTICE_ARMED);
}

static la_acq_ctx_t s_acq_ctx;

void la_acq_task(void *parameters)
{
    la_app_t *app = (la_app_t *)parameters;
    la_acq_ctx_t *ctx = &s_acq_ctx;
    TickType_t wait_ticks = portMAX_DELAY;

    if (app == NULL) {
        vTaskDelete(NULL);
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->app = app;
    ctx->sample_rate_hz = 100000000u;
    la_acq_fsm_init(&ctx->fsm);

    for (;;) {
        uint32_t bits = 0u;

        if (xTaskNotifyWait(0u, UINT32_MAX, &bits, wait_ticks) == pdTRUE) {
            la_irq_note_task_wake(&app->irq);
            if ((bits & LA_NOTIFY_JOB_DONE) != 0u) {
                ctx->buffer_busy = 0u;
            }
            if ((bits & LA_NOTIFY_PL_IRQ) != 0u) {
                acq_handle_pl(ctx);
            }
            if (ctx->live != 0u && (bits & LA_NOTIFY_DMA_IRQ) != 0u) {
                acq_handle_live_irq(ctx);
            }
            if (ctx->live != 0u && (bits & LA_NOTIFY_SEGMENT_FREE) != 0u) {
                if (acq_live_repost(ctx) != 0) {
                    acq_stop_live(ctx, 1, 0u);
                }
            }
        } else if (la_acq_fsm_state(&ctx->fsm) == LA_ACQ_ARMED) {
            acq_abort(ctx, 1);
        }

        for (;;) {
            la_ctrl_msg_t message;

            if (xQueueReceive(app->ctrl_q, &message, 0) != pdTRUE) {
                break;
            }
            acq_handle_ctrl(ctx, &message);
        }

        if (la_acq_fsm_state(&ctx->fsm) == LA_ACQ_ARMED && ctx->timeout_ms != 0u) {
            wait_ticks = pdMS_TO_TICKS(ctx->timeout_ms);
            if (wait_ticks == 0u) {
                wait_ticks = 1u;
            }
        } else {
            wait_ticks = portMAX_DELAY;
        }
    }
}
