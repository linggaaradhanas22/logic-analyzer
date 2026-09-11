#include "la_app.h"

#include "la_mem_geom.h"
#include "la_regs.h"

#include "xil_io.h"
#include "xparameters.h"

#include <string.h>

static uint32_t la_mmio_read32(uintptr_t address)
{
    return Xil_In32((UINTPTR)address);
}

static void la_mmio_write32(uintptr_t address, uint32_t value)
{
    Xil_Out32((UINTPTR)address, value);
}

static StaticStreamBuffer_t s_rx_sb_struct;
static uint8_t s_rx_sb_storage[LA_RX_STREAM_SIZE + 1u];
static StaticStreamBuffer_t s_tx_sb_struct;
static uint8_t s_tx_sb_storage[LA_TX_STREAM_SIZE + 1u];

static StaticQueue_t s_ctrl_q_struct;
static la_ctrl_msg_t s_ctrl_q_storage[LA_CTRL_Q_DEPTH];
static StaticQueue_t s_notice_q_struct;
static la_notice_t s_notice_q_storage[LA_NOTICE_Q_DEPTH];
static StaticQueue_t s_tx_q_struct;
static la_tx_frame_t s_tx_q_storage[LA_TX_Q_DEPTH];
static StaticQueue_t s_ack_q_struct;
static la_ack_msg_t s_ack_q_storage[LA_ACK_Q_DEPTH];
static StaticQueue_t s_job_q_struct;
static la_tx_job_t s_job_q_storage[1];

static StaticQueue_t s_block_q_struct;
static la_event_batch_t *s_block_q_storage[LA_EVENT_BLOCK_COUNT];
static la_event_batch_t s_event_blocks[LA_EVENT_BLOCK_COUNT];
static StaticQueue_t s_ev_q_struct;
static la_event_batch_t *s_ev_q_storage[LA_EV_Q_DEPTH];
static StaticQueue_t s_cfg_q_struct[LA_DECODER_COUNT];
static la_decoder_config_msg_t s_cfg_q_storage[LA_DECODER_COUNT][LA_CFG_Q_DEPTH];
static StaticQueue_t s_seg_q_struct[LA_DECODER_COUNT];
static la_live_segment_msg_t s_seg_q_storage[LA_DECODER_COUNT][LA_SEG_Q_DEPTH];

static StaticTask_t s_acq_tcb;
static StackType_t s_acq_stack[LA_STACK_ACQ];
static StaticTask_t s_cmd_tcb;
static StackType_t s_cmd_stack[LA_STACK_CMD];
static StaticTask_t s_tx_tcb;
static StackType_t s_tx_stack[LA_STACK_TX];
static StaticTask_t s_status_tcb;
static StackType_t s_status_stack[LA_STACK_STATUS];
static StaticTask_t s_decoder_tcb[LA_DECODER_COUNT];
static StackType_t s_decoder_stack[LA_DECODER_COUNT][LA_STACK_DECODER];

static const char *const s_decoder_names[LA_DECODER_COUNT] = {
    "la_dec0", "la_dec1", "la_dec2"};

uint8_t la_app_next_sequence(la_app_t *app)
{
    return __atomic_fetch_add(&app->tx_sequence, 1u, __ATOMIC_RELAXED);
}

int la_app_init(la_app_t *app)
{
    uint8_t slot;

    if (app == NULL) {
        return -1;
    }
    memset(app, 0, sizeof(*app));
    la_regs_dev_init(&app->regs, LA_REG_BASE_ADDRESS, la_mmio_read32,
                     la_mmio_write32);

    app->rx_sb = xStreamBufferCreateStatic(LA_RX_STREAM_SIZE, 1u,
                                           s_rx_sb_storage, &s_rx_sb_struct);
    app->tx_sb = xStreamBufferCreateStatic(LA_TX_STREAM_SIZE, 1u,
                                           s_tx_sb_storage, &s_tx_sb_struct);
    app->ctrl_q = xQueueCreateStatic(LA_CTRL_Q_DEPTH, sizeof(la_ctrl_msg_t),
                                     (uint8_t *)s_ctrl_q_storage,
                                     &s_ctrl_q_struct);
    app->notice_q = xQueueCreateStatic(LA_NOTICE_Q_DEPTH, sizeof(la_notice_t),
                                       (uint8_t *)s_notice_q_storage,
                                       &s_notice_q_struct);
    app->tx_q = xQueueCreateStatic(LA_TX_Q_DEPTH, sizeof(la_tx_frame_t),
                                   (uint8_t *)s_tx_q_storage, &s_tx_q_struct);
    app->ack_q = xQueueCreateStatic(LA_ACK_Q_DEPTH, sizeof(la_ack_msg_t),
                                    (uint8_t *)s_ack_q_storage, &s_ack_q_struct);
    app->job_q = xQueueCreateStatic(1u, sizeof(la_tx_job_t),
                                    (uint8_t *)s_job_q_storage,
                                    &s_job_q_struct);
    app->block_q = xQueueCreateStatic(LA_EVENT_BLOCK_COUNT,
                                      sizeof(la_event_batch_t *),
                                      (uint8_t *)s_block_q_storage,
                                      &s_block_q_struct);
    app->ev_q = xQueueCreateStatic(LA_EV_Q_DEPTH, sizeof(la_event_batch_t *),
                                   (uint8_t *)s_ev_q_storage, &s_ev_q_struct);
    if (app->rx_sb == NULL || app->tx_sb == NULL || app->ctrl_q == NULL ||
        app->notice_q == NULL || app->tx_q == NULL || app->ack_q == NULL ||
        app->job_q == NULL || app->block_q == NULL || app->ev_q == NULL) {
        return -1;
    }

    for (slot = 0u; slot < LA_DECODER_COUNT; ++slot) {
        la_decoder_ctx_t *decoder = &app->decoders[slot];

        decoder->app = app;
        decoder->slot = slot;
        decoder->cfg_q = xQueueCreateStatic(LA_CFG_Q_DEPTH,
                                            sizeof(la_decoder_config_msg_t),
                                            (uint8_t *)s_cfg_q_storage[slot],
                                            &s_cfg_q_struct[slot]);
        decoder->seg_q = xQueueCreateStatic(LA_SEG_Q_DEPTH,
                                            sizeof(la_live_segment_msg_t),
                                            (uint8_t *)s_seg_q_storage[slot],
                                            &s_seg_q_struct[slot]);
        if (decoder->cfg_q == NULL || decoder->seg_q == NULL) {
            return -1;
        }
        la_decoder_core_init(&decoder->core);
    }
    for (slot = 0u; slot < LA_EVENT_BLOCK_COUNT; ++slot) {
        la_event_batch_t *block = &s_event_blocks[slot];

        la_event_batch_init(block);
        if (xQueueSend(app->block_q, &block, 0) != pdTRUE) {
            return -1;
        }
    }
    if (la_segment_pool_init(&app->pool, LA_LIVE_SEGMENT_BYTES) != 0) {
        return -1;
    }
    if (la_live_ring_init(&app->live_ring, &app->pool, la_mem_capture_base(),
                          LA_LIVE_SEGMENT_COUNT, LA_LIVE_SEGMENT_BYTES) != 0) {
        return -1;
    }

    app->acq_task = xTaskCreateStatic(la_acq_task, "la_acq", LA_STACK_ACQ, app,
                                      LA_PRIO_ACQ, s_acq_stack, &s_acq_tcb);
    app->cmd_task = xTaskCreateStatic(la_cmd_task, "la_cmd", LA_STACK_CMD, app,
                                      LA_PRIO_CMD, s_cmd_stack, &s_cmd_tcb);
    app->tx_task = xTaskCreateStatic(la_tx_task, "la_tx", LA_STACK_TX, app,
                                     LA_PRIO_TX, s_tx_stack, &s_tx_tcb);
    app->status_task = xTaskCreateStatic(
        la_status_task, "la_status", LA_STACK_STATUS, app, LA_PRIO_STATUS,
        s_status_stack, &s_status_tcb);
    if (app->acq_task == NULL || app->cmd_task == NULL ||
        app->tx_task == NULL || app->status_task == NULL) {
        return -1;
    }
    for (slot = 0u; slot < LA_DECODER_COUNT; ++slot) {
        app->decoders[slot].task = xTaskCreateStatic(
            la_decoder_task, s_decoder_names[slot], LA_STACK_DECODER,
            &app->decoders[slot], LA_PRIO_DECODER, s_decoder_stack[slot],
            &s_decoder_tcb[slot]);
        if (app->decoders[slot].task == NULL) {
            return -1;
        }
    }

    if (la_link_uart_init(&app->link, XPAR_XUARTPS_0_BASEADDR, 921600u,
                          app->rx_sb, app->tx_sb) != 0) {
        return -2;
    }
    if (la_dma_init(&app->dma, la_mem_sg_base(), LA_MEM_SG_SIZE,
                    LA_DMA_BD_MAX_BYTES) != 0) {
        return -3;
    }
    if (la_irq_init(&app->irq, &app->regs, &app->dma, app->acq_task) != 0) {
        return -4;
    }
    return 0;
}
