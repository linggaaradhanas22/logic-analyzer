#include "la_app.h"

#include "la_acq_fsm.h"
#include "la_burst_plan.h"
#include "la_mem_geom.h"
#include "la_regs.h"

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
} la_acq_ctx_t;

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
            acq_post_notice(ctx, LA_NOTICE_TRIGGERED);
        }
    }
    if ((pending & LA_REG_IRQ_STAT_DONE_MASK) != 0u) {
        acq_complete(ctx);
    }
}

static void acq_handle_ctrl(la_acq_ctx_t *ctx, const la_ctrl_msg_t *message)
{
    la_app_t *app = ctx->app;
    uint64_t words;

    if (message->kind == LA_CTRL_ABORT) {
        if (la_acq_fsm_state(&ctx->fsm) != LA_ACQ_IDLE) {
            acq_abort(ctx, 0);
        }
        return;
    }
    if (message->kind != LA_CTRL_ARM || message->config_valid == 0u) {
        return;
    }
    while (ctx->buffer_busy != 0u) {
        uint32_t bits = 0u;

        if (xTaskNotifyWait(0u, UINT32_MAX, &bits, pdMS_TO_TICKS(100)) == pdTRUE) {
            la_irq_note_task_wake(&app->irq);
            if ((bits & LA_NOTIFY_JOB_DONE) != 0u) {
                ctx->buffer_busy = 0u;
            }
        }
    }

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
