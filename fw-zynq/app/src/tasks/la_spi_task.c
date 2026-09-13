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
    uint8_t ready;
} la_spi_ctx_t;

static la_spi_ctx_t s_spi;
static StackType_t s_spi_stack[LA_STACK_SPI];
static StaticTask_t s_spi_tcb;
static StaticSemaphore_t s_space_sem;
static SemaphoreHandle_t s_space;

static uint32_t spi_read32(uintptr_t address)
{
    return Xil_In32((UINTPTR)address);
}

static void spi_write32(uintptr_t address, uint32_t value)
{
    Xil_Out32((UINTPTR)address, value);
}

/* Notification-only ISR: latch the write-1-to-clear status and wake the task
 * (the task counts and acts on it). */
static void la_spi_isr(void *reference)
{
    la_spi_ctx_t *ctx = (la_spi_ctx_t *)reference;
    BaseType_t woken = pdFALSE;
    uint32_t status = la_spi_irq_status(&ctx->dev);

    if (status != 0u) {
        la_spi_irq_clear(&ctx->dev, status);
        ctx->isr_status |= status;
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
        uint32_t occupancy;

        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
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
        /* Bulk TX refill: read the FIFO room from TFO once and push that many
         * bytes. The 10 MHz master leaves ~0.8 us per byte, so a per-byte
         * status read (the la_spi_tx_push helper) would double the MMIO cost
         * and starve the FIFO mid-burst. */
        occupancy = la_spi_tx_occupancy(&ctx->dev);
        while (occupancy < XPAR_XSPI_0_FIFO_SIZE &&
               la_spi_core_ready(&ctx->core) != 0) {
            size_t room = (size_t)(XPAR_XSPI_0_FIFO_SIZE - occupancy);
            size_t want = room < sizeof(chunk) ? room : sizeof(chunk);
            size_t count = la_spi_core_feed(&ctx->core, chunk, want);
            size_t index;

            for (index = 0u; index < count; ++index) {
                la_spi_write32(&ctx->dev, LA_SPI_SPIDTR_OFFSET,
                               (uint32_t)chunk[index]);
                ctx->tx_bytes += 1u;
            }
            occupancy += (uint32_t)count;
        }
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
        /* Data-ready hysteresis: only start a master read once a full FIFO of
         * data is buffered (the first frame alone is drained in ~20 us and
         * the master then clocks the next header before the next frame has
         * been queued - a one-byte underrun that costs the frame; bench
         * 2026-09-13). Once started, stay ready until the ring and FIFO are
         * both empty so the capture tail (small END frames) still drains. */
        {
            uint32_t buffered = (uint32_t)la_spi_core_used(&ctx->core) +
                                la_spi_tx_occupancy(&ctx->dev);

            if (ctx->ready == 0u) {
                spi_set_ready(ctx, buffered >= XPAR_XSPI_0_FIFO_SIZE);
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

int la_spi_enqueue(const uint8_t *frame, size_t length)
{
    int result = la_spi_core_push(&s_spi.core, frame, length);

    if (result == 0 && s_spi.task != NULL) {
        /* Wake the service task so it re-enables the TX interrupt promptly. */
        (void)xTaskNotifyGive(s_spi.task);
    }
    return result;
}

void la_spi_snapshot(uint32_t *tx_bytes, uint32_t *rx_bytes,
                     uint32_t *underruns, uint32_t *errors)
{
    if (tx_bytes != NULL) {
        *tx_bytes = s_spi.tx_bytes;
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
