#ifndef LA_APP_H
#define LA_APP_H

/*
 * P4.4-P4.9 Zynq application context.
 *
 * Owns every static FreeRTOS object (tasks, queues, stream buffers) and the
 * driver instances, and defines the message types exchanged between the
 * tasks. Ownership rules follow docs/rtos-design.md:
 *
 *   AcqTask   (6): owns la_acq_fsm, la_regs_drv writes, la_dma and the
 *                  capture buffer. Consumes ctrl_q (ARM/ABORT from CmdTask)
 *                  and the PL/DMA notifications. Produces job_q (one TxJob)
 *                  and notice_q (lifecycle reconciliation for CmdTask).
 *   CmdTask   (5): owns the la_proto parser and la_cmd_dispatch. Consumes
 *                  rx_sb (UART RX ISR) and notice_q. Produces tx_q
 *                  (control/status frames), ack_q (burst ACK/NAK routing)
 *                  and ctrl_q.
 *   TxTask    (4): owns la_tx_win, la_tx_pack and the single-producer TX
 *                  StreamBuffer. Consumes job_q, tx_q and ack_q. Produces
 *                  the UART bytes through la_link_uart_tx_kick().
 *   StatusTask(1): periodic stack/counter reporting on the console.
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#include "la_dma.h"
#include "la_irq.h"
#include "la_link_uart.h"
#include "la_live_ring.h"
#include "la_decoder_core.h"
#include "la_event_batch.h"
#include "la_proto.h"
#include "la_regs_drv.h"
#include "la_segment_pool.h"

#include <stdint.h>

#define LA_RX_STREAM_SIZE 8192u
#define LA_TX_STREAM_SIZE 4096u
#define LA_CTRL_Q_DEPTH 4u
#define LA_NOTICE_Q_DEPTH 8u
#define LA_TX_Q_DEPTH 16u
#define LA_ACK_Q_DEPTH 32u
#define LA_TX_FRAME_PAYLOAD 64u

#define LA_DECODER_COUNT LA_PROTO_DECODER_MAX_SLOTS
#define LA_CFG_Q_DEPTH 2u
#define LA_SEG_Q_DEPTH 8u
#define LA_EV_Q_DEPTH 8u
#define LA_EVENT_BLOCK_COUNT 12u

#define LA_STACK_ACQ 1024u
#define LA_STACK_CMD 1024u
#define LA_STACK_TX 1024u
#define LA_STACK_STATUS 512u
#define LA_STACK_DECODER 1024u

#define LA_PRIO_ACQ 6u
#define LA_PRIO_CMD 5u
#define LA_PRIO_DECODER 5u
#define LA_PRIO_TX 4u
#define LA_PRIO_STATUS 1u

#define LA_FW_VERSION UINT32_C(0x00070000)

typedef enum la_ctrl_kind {
    LA_CTRL_ARM = 1,
    LA_CTRL_ABORT = 2,
    LA_CTRL_LIVE_START = 3,
    LA_CTRL_LIVE_STOP = 4
} la_ctrl_kind_t;

typedef struct la_ctrl_msg {
    la_ctrl_kind_t kind;
    la_proto_config_t config;
    uint8_t config_valid;
    la_proto_decoder_config_t decoders[LA_PROTO_DECODER_MAX_SLOTS];
    uint8_t decoder_valid[LA_PROTO_DECODER_MAX_SLOTS];
    uint8_t decoder_count;
} la_ctrl_msg_t;

typedef enum la_notice_kind {
    LA_NOTICE_ARMED = 0,
    LA_NOTICE_TRIGGERED,
    LA_NOTICE_DONE,
    LA_NOTICE_ABORTED,
    LA_NOTICE_STREAMING,
    LA_NOTICE_OVERRUN
} la_notice_kind_t;

typedef struct la_notice {
    la_notice_kind_t kind;
    uint64_t trig_index;
    uint64_t word_count;
    uint32_t status;
    uint32_t dropped;   /* valid when kind == LA_NOTICE_OVERRUN */
    uint8_t stage;      /* EVT_OVERRUN stage when kind == LA_NOTICE_OVERRUN */
} la_notice_t;

typedef struct la_tx_frame {
    uint8_t type;
    uint8_t sequence;
    uint16_t length;
    uint8_t payload[LA_TX_FRAME_PAYLOAD];
} la_tx_frame_t;

typedef enum la_ack_kind {
    LA_ACK_CHUNK = 1,     /* ACK of a DATA_CHUNK sequence */
    LA_ACK_CHUNK_NAK = 2, /* NAK of a DATA_CHUNK sequence (retransmit) */
    LA_ACK_BURST_NAK = 3, /* NAK of another frame type: abandon the burst */
    LA_ACK_ABORT = 4      /* local abort: abandon the burst */
} la_ack_kind_t;

typedef struct la_ack_msg {
    la_ack_kind_t kind;
    uint8_t type;
    uint8_t sequence;
    uint8_t error;
} la_ack_msg_t;

typedef struct la_tx_job {
    uintptr_t buffer; /* capture pool base for this job */
    uint64_t total_words;
    uint8_t encoding;
    uint32_t sample_rate_hz;
    uint32_t status;
    uint64_t trig_index;
} la_tx_job_t;

/* P7 live pipeline messages. A published segment travels by handle; the
 * decoder task reads the payload at slot_address(slot) while it owns the
 * reference and releases the handle when done. */
typedef struct la_live_segment_msg {
    la_segment_handle_t handle;
    uint32_t slot;
    uint32_t length;
    uint32_t sequence;
} la_live_segment_msg_t;

typedef struct la_decoder_config_msg {
    la_proto_decoder_config_t config;
    uint8_t valid;
} la_decoder_config_msg_t;

struct la_app;

typedef struct la_decoder_ctx {
    struct la_app *app;
    uint8_t slot;
    QueueHandle_t cfg_q;
    QueueHandle_t seg_q;
    TaskHandle_t task;
    la_decoder_core_t core;
    la_event_batch_t *current; /* block being filled, NULL when none held */
    uint32_t batches;
    uint32_t dropped_batches;
    uint32_t dropped_events;
} la_decoder_ctx_t;

typedef struct la_app {
    /* Drivers and register block */
    la_regs_dev_t regs;
    la_dma_t dma;
    la_irq_t irq;
    la_link_uart_t link;

    /* FreeRTOS objects */
    StreamBufferHandle_t rx_sb;
    StreamBufferHandle_t tx_sb;
    QueueHandle_t ctrl_q;
    QueueHandle_t notice_q;
    QueueHandle_t tx_q;
    QueueHandle_t ack_q;
    QueueHandle_t job_q;
    TaskHandle_t acq_task;
    TaskHandle_t cmd_task;
    TaskHandle_t tx_task;
    TaskHandle_t status_task;

    /* Single Zynq sequence space shared by CmdTask and TxTask. */
    volatile uint8_t tx_sequence;

    /* P7 live pipeline: the pool and ring are owned by AcqTask, the event
     * block queue is multi-producer (decoder tasks) / single-consumer
     * (TxTask), and the counters are written by the owning task and read
     * for STATUS snapshots. */
    la_segment_pool_t pool;
    la_live_ring_t live_ring;
    QueueHandle_t block_q; /* free la_event_batch_t blocks */
    QueueHandle_t ev_q;    /* filled blocks to TxTask */
    la_decoder_ctx_t decoders[LA_DECODER_COUNT];
    volatile uint32_t decoder_events[LA_DECODER_COUNT];
    volatile uint32_t decoder_dropped_segments[LA_DECODER_COUNT];
    volatile uint32_t decoder_dropped_events[LA_DECODER_COUNT];
    volatile uint32_t decoder_queue_drops[LA_DECODER_COUNT];
    volatile uint32_t live_overflows;
    volatile uint8_t live_active;

    /* STATUS snapshot shared with CmdTask (single writer per field). */
    volatile uint32_t status_reg;
    volatile uint64_t word_count;
    volatile uint64_t trig_index;
    volatile uint16_t min_free_stack;
    volatile uint16_t cpu_load_permille;
    volatile uint32_t tx_frames;
    volatile uint32_t tx_retransmits;
    volatile uint32_t rx_frames;
    volatile uint32_t tx_drops;
    volatile uint32_t pl_irqs;
    volatile uint32_t dma_irqs;
} la_app_t;

/* Creates every object and driver and starts the UART/DMA interrupts.
 * Returns 0 or a negative error; the scheduler is started by main(). */
int la_app_init(la_app_t *app);

/* Next frame sequence number (modulo 256) from the shared Zynq sequence
 * space; atomic across CmdTask and TxTask. */
uint8_t la_app_next_sequence(la_app_t *app);

void la_acq_task(void *parameters);
void la_cmd_task(void *parameters);
void la_tx_task(void *parameters);
void la_status_task(void *parameters);
void la_decoder_task(void *parameters);

#endif /* LA_APP_H */
