/*
 * P9.1 SPI slave link service task.
 *
 * Owns the PL axi_quad_spi slave (base from the XSA: XPAR_XSPI_0_BASEADDR at
 * 0x4080_0000), the frame ring (la_spi_core) and the JA-1 data-ready GPIO
 * (PS EMIO GPIO[0] = bank 2 pin 54). The ESP32 is the SPI master; this task
 * keeps the 256-byte TX FIFO fed with whole encoded la_proto frames so the
 * ESP32 sees the same wire frames as the UART path. The task is
 * interrupt-driven (DTR underrun, TX half-empty while data is queued) with a
 * 1 ms poll fallback; the ISR only latches the IP status, clears the
 * write-1-to-clear bits and notifies the task.
 *
 * The MOSI channel is reserved for future SPI commands (P9.1 keeps commands
 * on UART); its FIFO is left to overflow and DRR_NOT_EMPTY is never enabled,
 * because the level-persistent flag storms the CPU and draining per byte
 * costs the same MMIO budget as the TX path. The data-ready line is high
 * while a frame is buffered or the FIFO holds bytes; underruns are counted
 * and the ESP32 detects a corrupted frame through the la_proto length/CRC
 * check.
 *
 * Bench verification (10/20 MHz, 64 MB drain) is a P9.1 gate; host tests
 * cover the register driver (la_native_spi) and the ring core
 * (la_native_spi_core).
 */

#include "la_app.h"

#include "la_spi_core.h"
#include "la_spi_link.h"
#include "la_trace.h"

#include "xil_io.h"
#include "xgpiops.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"

#include "semphr.h"

#include <string.h>

/* Data-ready rise thresholds (P9.1 bench 2026-09-13): the master starts a
 * capture only once a head start is buffered, so the producer has time to keep
 * the 256-byte FIFO fed while the first chunks leave. A small capture that
 * never reaches the threshold is released once the producer has been quiet for
 * LA_SPI_READY_QUIET_MS with data queued. */
#define LA_SPI_READY_ON_BYTES 4096u
#define LA_SPI_READY_QUIET_MS 5u

typedef struct la_spi_ctx {
    la_app_t *app;
    la_spi_dev_t dev;
    la_spi_core_t core;
    uint8_t ring[LA_SPI_RING_BYTES];
    XGpioPs gpio;
    uint8_t gpio_ok;
    TaskHandle_t task;
    uint32_t irq_mask;
    volatile uint32_t isr_status;
    volatile uint32_t rx_bytes;
    volatile uint32_t tx_bytes;
    volatile uint32_t rx_overruns;
    volatile uint32_t errors;
    volatile uint32_t stack_hwm;
    volatile uint32_t isr_tx_bytes;
    /* P9.1 bench diagnostics: why the master read stalls and where the error
     * bits come from (never a silent counter). */
    volatile uint32_t irq_events;
    volatile uint32_t isr_underrun;
    volatile uint32_t isr_half_empty;
    volatile uint32_t isr_other;
    volatile uint32_t ready_rises;
    volatile uint32_t ready_falls;
    volatile uint32_t task_passes;
    uint8_t ready;
} la_spi_ctx_t;

static la_spi_ctx_t s_spi;
static StackType_t s_spi_stack[LA_STACK_SPI];
static StaticTask_t s_spi_tcb;
static StaticSemaphore_t s_space_sem;
static SemaphoreHandle_t s_space;
static volatile uint32_t s_flush_request;
static volatile uint32_t s_flush_done;
static volatile TickType_t s_last_enqueue;

static uint32_t spi_read32(uintptr_t address)
{
    return Xil_In32((UINTPTR)address);
}

static void spi_write32(uintptr_t address, uint32_t value)
{
    Xil_Out32((UINTPTR)address, value);
}

/* Push as many bytes as the TX FIFO can take straight from the ring. Safe in
 * the ISR (IRQs are masked there) and in the task inside a critical section;
 * the producer's la_spi_core_push also masks interrupts, so the ring stays
 * consistent. Returns the number of bytes pushed.
 *
 * The task-only refill could not bridge the half-empty window at 10/20 MHz
 * (GIC + scheduler latency before the first MMIO write): the sweep measured
 * 2 underruns per 16 KB capture at 10 MHz and ~150 per capture at 20 MHz,
 * corrupting whole chunks. Feeding in the ISR cuts that latency to the
 * handler entry. */
static uint32_t spi_feed_fifo(la_spi_ctx_t *ctx)
{
    uint8_t chunk[32];
    uint32_t occupancy = la_spi_tx_occupancy(&ctx->dev);
    uint32_t total = 0u;

    while (occupancy < XPAR_XSPI_0_FIFO_SIZE &&
           la_spi_core_ready(&ctx->core) != 0) {
        size_t room = (size_t)(XPAR_XSPI_0_FIFO_SIZE - occupancy);
        size_t want = room < sizeof(chunk) ? room : sizeof(chunk);
        size_t count = la_spi_core_feed(&ctx->core, chunk, want);
        size_t index;

        for (index = 0u; index < count; ++index) {
            la_spi_write32(&ctx->dev, LA_SPI_SPIDTR_OFFSET,
                           (uint32_t)chunk[index]);
        }
        occupancy += (uint32_t)count;
        total += (uint32_t)count;
    }
    return total;
}

/* Notification-only ISR for the status bookkeeping, plus the emergency FIFO
 * refill that keeps the slave from underrunning between task wake-ups. */
static void la_spi_isr(void *reference)
{
    la_spi_ctx_t *ctx = (la_spi_ctx_t *)reference;
    BaseType_t woken = pdFALSE;
    uint32_t status = la_spi_irq_status(&ctx->dev);

    if (status != 0u) {
        la_spi_irq_clear(&ctx->dev, status);
        ctx->isr_status |= status;
        ctx->irq_events += 1u;
        if ((status & LA_SPI_ISR_DTR_UNDERRUN) != 0u) {
            ctx->isr_underrun += 1u;
        }
        if ((status & LA_SPI_ISR_TX_FIFO_HALF_EMPTY) != 0u) {
            ctx->isr_half_empty += 1u;
        }
        if ((status & (LA_SPI_ISR_ERROR_MASK &
                       ~(LA_SPI_ISR_DTR_UNDERRUN | LA_SPI_ISR_DRR_OVERRUN))) != 0u) {
            ctx->isr_other += 1u;
        }
        if ((status & (LA_SPI_ISR_TX_FIFO_HALF_EMPTY | LA_SPI_ISR_DTR_UNDERRUN |
                       LA_SPI_ISR_DTR_EMPTY)) != 0u) {
            ctx->isr_tx_bytes += spi_feed_fifo(ctx);
        }
    }
    vTaskNotifyGiveFromISR(ctx->task, &woken);
    portYIELD_FROM_ISR(woken);
}

static void spi_set_ready(la_spi_ctx_t *ctx, int ready)
{
    uint8_t value = ready != 0 ? 1u : 0u;

    if (value == ctx->ready) {
        return;
    }
    ctx->ready = value;
    if (value != 0u) {
        ctx->ready_rises += 1u;
    } else {
        ctx->ready_falls += 1u;
    }
    if (ctx->gpio_ok != 0u) {
        XGpioPs_WritePin(&ctx->gpio, LA_SPI_READY_GPIO_PIN, value);
    }
}

static void la_spi_task(void *parameters)
{
    la_spi_ctx_t *ctx = (la_spi_ctx_t *)parameters;
    uint8_t chunk[32];

    la_spi_dev_init(&ctx->dev, XPAR_XSPI_0_BASEADDR, spi_read32, spi_write32);
    (void)la_spi_reset(&ctx->dev);
    (void)la_spi_start(&ctx->dev);
    la_spi_irq_clear(&ctx->dev, 0xFFFFFFFFu);
    /* The TX half-empty flag is level-persistent: enabling it while the FIFO
     * is idle re-asserts the PL interrupt forever and starves the UART link
     * tasks. It is enabled only while there is data to feed (see the loop). */
    ctx->irq_mask = LA_SPI_ISR_DTR_UNDERRUN;
    la_spi_irq_enable(&ctx->dev, ctx->irq_mask);
    if (la_irq_connect(LA_SPI_INTR_ID, la_spi_isr, ctx,
                       (uintptr_t)XPAR_XSCUGIC_0_BASEADDR,
                       XINTERRUPT_DEFAULT_PRIORITY) != 0) {
        xil_printf("LA SPI: interrupt connect failed\r\n");
    }
    /* The first la_irq_connect (analyzer/DMA) owns the GIC setup; later
     * callers only install the port handler. This line still needs the same
     * level/priority and GIC enable as the other fabric IRQs, or the TX
     * half-empty refill never fires and the 256-byte FIFO starves inside a
     * master burst (observed on the 2026-09-13 bench). */
    (void)XSetPriorityTriggerType(LA_SPI_INTR_ID, XINTERRUPT_DEFAULT_PRIORITY,
                                  (uintptr_t)XPAR_XSCUGIC_0_BASEADDR);
    vPortEnableInterrupt(LA_SPI_INTR_ID);

    {
        XGpioPs_Config *gpio_config =
            XGpioPs_LookupConfig(XPAR_XGPIOPS_0_BASEADDR);

        if (gpio_config != NULL &&
            XGpioPs_CfgInitialize(&ctx->gpio, gpio_config,
                                  gpio_config->BaseAddr) == XST_SUCCESS) {
            XGpioPs_SetDirectionPin(&ctx->gpio, LA_SPI_READY_GPIO_PIN, 1u);
            XGpioPs_SetOutputEnablePin(&ctx->gpio, LA_SPI_READY_GPIO_PIN, 1u);
            XGpioPs_WritePin(&ctx->gpio, LA_SPI_READY_GPIO_PIN, 0u);
            ctx->gpio_ok = 1u;
        } else {
            xil_printf("LA SPI: data-ready GPIO init failed\r\n");
        }
    }
    xil_printf("LA SPI slave: %s, FIFO %u B, ring %u B\r\n",
               la_spi_check_name(la_spi_check(&ctx->dev)),
               (unsigned)XPAR_XSPI_0_FIFO_SIZE,
               (unsigned)LA_SPI_RING_BYTES);

    for (;;) {
        uint32_t status;

        ctx->task_passes += 1u;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
        if (s_flush_request != 0u) {
            /* P9.1: a new burst job must never inherit a partial frame from an
             * aborted capture (the ESP32 master would relay the stale tail as
             * data). Drop the ring and reset both FIFOs; the master sees
             * data-ready fall and its parser resyncs on the next SOF. */
            s_flush_request = 0u;
            la_spi_core_init(&ctx->core, ctx->ring, sizeof(ctx->ring));
            (void)la_spi_reset(&ctx->dev);
            (void)la_spi_start(&ctx->dev);
            la_spi_irq_clear(&ctx->dev, 0xFFFFFFFFu);
            ctx->irq_mask = LA_SPI_ISR_DTR_UNDERRUN;
            la_spi_irq_enable(&ctx->dev, ctx->irq_mask);
            spi_set_ready(ctx, 0);
            s_flush_done = 1u;
            continue;
        }
        status = ctx->isr_status;
        ctx->isr_status = 0u;
        if ((status & LA_SPI_ISR_DTR_UNDERRUN) != 0u) {
            la_spi_core_note_underrun(&ctx->core);
        }
        if ((status & LA_SPI_ISR_DRR_OVERRUN) != 0u) {
            ctx->rx_overruns += 1u;
        }
        if ((status & (LA_SPI_ISR_ERROR_MASK & ~LA_SPI_ISR_DRR_OVERRUN)) != 0u) {
            ctx->errors += 1u;
        }
        /* The MOSI channel is reserved for future commands (P9.1 keeps
         * commands on UART). Its FIFO is left to overflow: draining costs
         * the same MMIO budget as the TX path, and a SPICR read-modify-write
         * to reset it mid-transfer disturbs the slave (bench 2026-09-13:
         * the resets correlated with TX underruns). The overrun flag is
         * excluded from the error count above. */
        /* Bulk TX refill (the ISR already bridges the half-empty window): the
         * task tops the FIFO up whenever it runs, with the ring feed inside a
         * critical section because TxTask's la_spi_core_push may compact the
         * ring concurrently. */
        taskENTER_CRITICAL();
        ctx->tx_bytes += spi_feed_fifo(ctx);
        taskEXIT_CRITICAL();
        {
            uint32_t mask = LA_SPI_ISR_DTR_UNDERRUN;

            if (la_spi_core_ready(&ctx->core) != 0 ||
                la_spi_tx_occupancy(&ctx->dev) > 0u) {
                mask |= LA_SPI_ISR_TX_FIFO_HALF_EMPTY;
            }
            if (mask != ctx->irq_mask) {
                ctx->irq_mask = mask;
                la_spi_irq_enable(&ctx->dev, mask);
            }
        }
        /* Data-ready hysteresis: start a capture only once a head start is
         * buffered (a single frame is drained in ~200 us at 10 MHz, and the
         * master then clocks zeros until the producer catches up - the
         * underruns that corrupted chunks in the 64 KB gate; bench
         * 2026-09-13). Once started, stay ready until the ring and FIFO are
         * both empty so the capture tail (small END frames) still drains. */
        {
            uint32_t buffered = (uint32_t)la_spi_core_used(&ctx->core) +
                                la_spi_tx_occupancy(&ctx->dev);

            if (ctx->ready == 0u) {
                int rise = buffered >= LA_SPI_READY_ON_BYTES;

                if (rise == 0 && buffered > 0u &&
                    (xTaskGetTickCount() - s_last_enqueue) >
                        pdMS_TO_TICKS(LA_SPI_READY_QUIET_MS)) {
                    /* Small capture: the producer is done, release the tail. */
                    rise = 1;
                }
                spi_set_ready(ctx, rise);
            } else {
                spi_set_ready(ctx, buffered > 0u);
            }
        }
        if (s_space != NULL) {
            (void)xSemaphoreGive(s_space);
        }
        ctx->stack_hwm = uxTaskGetStackHighWaterMark(NULL);
    }
}

int la_spi_service_start(la_app_t *app)
{
    if (app == NULL) {
        return -1;
    }
    memset(&s_spi, 0, sizeof(s_spi));
    s_spi.app = app;
    la_spi_core_init(&s_spi.core, s_spi.ring, sizeof(s_spi.ring));
    s_space = xSemaphoreCreateBinaryStatic(&s_space_sem);
    s_spi.task = xTaskCreateStatic(la_spi_task, "la_spi", LA_STACK_SPI, &s_spi,
                                   LA_PRIO_SPI, s_spi_stack, &s_spi_tcb);
    return s_spi.task != NULL ? 0 : -1;
}

int la_spi_wait_space(TickType_t ticks)
{
    if (s_space == NULL) {
        return -1;
    }
    return xSemaphoreTake(s_space, ticks) == pdTRUE ? 0 : -1;
}

int la_spi_flush(TickType_t ticks)
{
    TickType_t start;

    if (s_spi.task == NULL) {
        return -1;
    }
    s_flush_done = 0u;
    s_flush_request = 1u;
    (void)xTaskNotifyGive(s_spi.task);
    start = xTaskGetTickCount();
    while (s_flush_done == 0u) {
        if ((xTaskGetTickCount() - start) > ticks) {
            return -1;
        }
        vTaskDelay(1);
    }
    return 0;
}

int la_spi_enqueue(const uint8_t *frame, size_t length)
{
    int result;

    /* Mutual exclusion with the service task's la_spi_core_feed: the task can
     * preempt this push during its wrap compaction (see the refill loop). */
    taskENTER_CRITICAL();
    result = la_spi_core_push(&s_spi.core, frame, length);
    taskEXIT_CRITICAL();
    if (result == 0 && s_spi.task != NULL) {
        s_last_enqueue = xTaskGetTickCount();
        /* Wake the service task so it re-enables the TX interrupt promptly. */
        (void)xTaskNotifyGive(s_spi.task);
    }
    return result;
}

void la_spi_snapshot(uint32_t *tx_bytes, uint32_t *rx_bytes,
                     uint32_t *underruns, uint32_t *errors)
{
    if (tx_bytes != NULL) {
        *tx_bytes = s_spi.tx_bytes + s_spi.isr_tx_bytes;
    }
    if (rx_bytes != NULL) {
        *rx_bytes = s_spi.rx_bytes;
    }
    if (underruns != NULL) {
        *underruns = s_spi.core.underruns;
    }
    if (errors != NULL) {
        *errors = s_spi.errors + s_spi.rx_overruns;
    }
}

void la_spi_debug(la_spi_debug_t *out)
{
    if (out == NULL) {
        return;
    }
    out->ring_used = (uint32_t)la_spi_core_used(&s_spi.core);
    out->fifo_occ = la_spi_tx_occupancy(&s_spi.dev);
    out->ready = s_spi.ready;
    out->isr_events = s_spi.irq_events;
    out->isr_underrun = s_spi.isr_underrun;
    out->isr_half_empty = s_spi.isr_half_empty;
    out->isr_other = s_spi.isr_other;
    out->ready_rises = s_spi.ready_rises;
    out->ready_falls = s_spi.ready_falls;
    out->frames_dropped = s_spi.core.frames_dropped;
    out->task_passes = s_spi.task_passes;
    out->buffered = (uint32_t)la_spi_core_used(&s_spi.core) +
                    la_spi_tx_occupancy(&s_spi.dev);
}
