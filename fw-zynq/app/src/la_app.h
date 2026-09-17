#ifndef LA_APP_H
#define LA_APP_H

/* Build-time transport profile: 0 = ESP32 gateway over UART0 (default),
 * 1 = direct Zynq USB CDC-ACM. Selected by `fw-zynq -- --profile usb`. */
#ifndef LA_TRANSPORT_USB
#define LA_TRANSPORT_USB 0
#endif

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

#if LA_TRANSPORT_USB
#include "la_usb_dev.h"
#endif

#include <stddef.h>
#include <stdint.h>

/* Zynq UART0 analyzer link rate (plan: 921600 -> 2 Mbps with RTS/CTS). The
 * PS UART0 reference clock is 100 MHz, so 2 Mbps divides exactly (divisor 50).
 * Must match the gateway's CONFIG_LA_LINK_BAUD. */
#define LA_LINK_BAUD 2000000u

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
#define LA_STACK_STATUS 1024u
#define LA_STACK_DECODER 1024u
#define LA_STACK_SPI 1024u
#define LA_STACK_USB 3072u

/* P9.1 SPI slave service (la_spi_task.c): the task runs above AcqTask and
 * wakes on the IP's TX-half-empty interrupt so the 256-byte FIFO (102 us at
 * 20 MHz) is refilled before it drains. */
#define LA_SPI_RING_BYTES 8192u
#define LA_PRIO_SPI 7u
#define LA_SPI_READY_GPIO_PIN 54u /* PS EMIO GPIO[0] -> bank 2 bit 0 (JA-1) */
#define LA_SPI_INTR_ID 31u        /* xlconcat In2 -> IRQ_F2P[2] -> GIC 63 */

/* P8.3 trace task numbers (stable ids in the trace dump). */
#define LA_TRACE_TASK_ACQ 1u
#define LA_TRACE_TASK_CMD 2u
#define LA_TRACE_TASK_TX 3u
#define LA_TRACE_TASK_STATUS 4u
#define LA_TRACE_TASK_DECODER_BASE 5u

#define LA_PRIO_ACQ 6u
#define LA_PRIO_CMD 5u
#define LA_PRIO_DECODER 5u
/* P8.1 review: TxTask is the consumer of the decoder event queue. Below the
 * decoders (4) it starved under sustained overload - the decoders never
 * blocked because AcqTask kept seg_q non-empty - so no event or overrun
 * frame was ever sent. Equal priority gives FreeRTOS round-robin drain. */
#define LA_PRIO_TX 5u
#define LA_PRIO_USB 5u
#define LA_PRIO_STATUS 1u

#define LA_FW_VERSION UINT32_C(0x00090000)

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
    uint8_t evt_format; /* LA_PROTO_EVT_FORMAT_* for this live session (P9.2) */
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
    LA_NOTICE_OVERRUN,
    LA_NOTICE_STOP_ON
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
    uint8_t evt_format; /* live event encoding for this decoder (P9.2) */
} la_decoder_config_msg_t;

struct la_app;

typedef struct la_decoder_ctx {
    struct la_app *app;
    uint8_t slot;
    uint8_t evt_format; /* live event encoding from the latched EVT_CFG */
    QueueHandle_t cfg_q;
    QueueHandle_t seg_q;
    TaskHandle_t task;
    la_decoder_core_t core;
    la_event_batch_t *current; /* block being filled, NULL when none held */
    uint32_t batches;
    uint32_t dropped_batches;
    uint32_t dropped_events;
    uint8_t stop_on_reported; /* P7.7: match already handed to AcqTask */
} la_decoder_ctx_t;

typedef struct la_app {
    /* Drivers and register block */
    la_regs_dev_t regs;
    la_dma_t dma;
    la_irq_t irq;
    la_link_uart_t link;
#if LA_TRANSPORT_USB
    /* U03 direct-USB CDC-ACM device and its service task. */
    la_usb_dev_t usb;
    TaskHandle_t usb_task;
#endif

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

    /* P9.1: LINK_CFG latch. 1 routes burst DATA_* frames over the PL SPI
     * slave; 0 keeps them on the UART. Written by CmdTask, read by TxTask. */
    volatile uint8_t spi_bulk;

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
    /* P8.5 throughput instrumentation: transition words fed to each decoder
     * and the cumulative XTime spent inside la_decoder_core_feed_segment
     * (microseconds, 32-bit; wraps after about 71 minutes at full load).
     * Monotonic since boot, printed by StatusTask for bench comparisons. */
    volatile uint32_t decoder_words[LA_DECODER_COUNT];
    volatile uint32_t decoder_busy_us[LA_DECODER_COUNT];
    volatile uint32_t live_overflows;
    volatile uint8_t live_active;
    /* P7.7 stop-on match: the first decoder to complete its pattern latches
     * the slot and the matching sample index here (critical section), then
     * notifies AcqTask with LA_NOTIFY_STOP_ON. AcqTask clears the flag when
     * the live session ends. */
    volatile uint8_t live_stop_on;
    volatile uint8_t live_stop_slot;
    volatile uint64_t live_stop_index;
    /* P8.2 live latency: XTime counts at the live ARM, the latched sample
     * divider and the worst byte-to-send latency in microseconds (published
     * by StatusTask). */
    volatile uint64_t live_arm_time;
    volatile uint32_t live_sample_div;
    volatile uint32_t live_max_latency_us;

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

#if LA_TRANSPORT_USB
/* U03: owns EP0 class requests, the RX StreamBuffer producer side and the
 * USB TX frame pool consumer side. */
void la_usb_task(void *parameters);
#endif

/* P9.1: creates the static SPI slave service task (the task owns the
 * axi_quad_spi device, the frame ring and the JA-1 data-ready GPIO). */
int la_spi_service_start(la_app_t *app);

/* P9.1: queues one complete encoded frame for the SPI link. Producer context
 * (TxTask or the boot path); all-or-nothing, returns 0 or -1 when full. */
int la_spi_enqueue(const uint8_t *frame, size_t length);

/* P9.1: blocks until the SPI service task has freed ring space (or the timeout
 * expires). Lets TxTask pace the producer to the link rate instead of a 1 ms
 * poll, which under-feeds the 10 MHz master. Returns 0 when signalled. */
int la_spi_wait_space(TickType_t ticks);

/* P9.1: drops any queued/partial SPI frame data and resets the slave FIFOs.
 * Called at every burst job start so a new capture cannot inherit the tail of
 * an aborted one. Returns 0 when the service task completed the flush. */
int la_spi_flush(TickType_t ticks);

/* P9.1 counters for StatusTask/console reporting. */
void la_spi_snapshot(uint32_t *tx_bytes, uint32_t *rx_bytes,
                     uint32_t *underruns, uint32_t *errors);

/* P9.1 bench diagnostics: current ring/FIFO/ready state and the per-source
 * interrupt counts (why the master read stalled, which error bits fire). */
typedef struct la_spi_debug {
    uint32_t ring_used;
    uint32_t fifo_occ;
    uint32_t ready;
    uint32_t isr_events;
    uint32_t isr_underrun;
    uint32_t isr_half_empty;
    uint32_t isr_other;
    uint32_t ready_rises;
    uint32_t ready_falls;
    uint32_t frames_dropped;
    uint32_t task_passes;
    uint32_t buffered;
} la_spi_debug_t;

void la_spi_debug(la_spi_debug_t *out);

#endif /* LA_APP_H */
