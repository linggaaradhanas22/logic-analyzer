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
#include "la_proto.h"
#include "la_regs_drv.h"

#include <stdint.h>

#define LA_RX_STREAM_SIZE 8192u
#define LA_TX_STREAM_SIZE 4096u
#define LA_CTRL_Q_DEPTH 4u
#define LA_NOTICE_Q_DEPTH 8u
#define LA_TX_Q_DEPTH 16u
#define LA_ACK_Q_DEPTH 32u
#define LA_TX_FRAME_PAYLOAD 64u

#define LA_STACK_ACQ 1024u
#define LA_STACK_CMD 1024u
#define LA_STACK_TX 1024u
#define LA_STACK_STATUS 512u

#define LA_PRIO_ACQ 6u
#define LA_PRIO_CMD 5u
#define LA_PRIO_TX 4u
#define LA_PRIO_STATUS 1u

#define LA_FW_VERSION UINT32_C(0x00040000)

typedef enum la_ctrl_kind {
    LA_CTRL_ARM = 1,
    LA_CTRL_ABORT = 2
} la_ctrl_kind_t;

typedef struct la_ctrl_msg {
    la_ctrl_kind_t kind;
    la_proto_config_t config;
    uint8_t config_valid;
} la_ctrl_msg_t;

typedef enum la_notice_kind {
    LA_NOTICE_ARMED = 0,
    LA_NOTICE_TRIGGERED,
    LA_NOTICE_DONE,
    LA_NOTICE_ABORTED
} la_notice_kind_t;

typedef struct la_notice {
    la_notice_kind_t kind;
    uint64_t trig_index;
    uint64_t word_count;
    uint32_t status;
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

#endif /* LA_APP_H */
