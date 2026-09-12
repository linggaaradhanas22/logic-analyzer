#include "la_irq.h"

#include "la_regs.h"
#include "la_trace.h"
#include "xaxidma.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"
#include "xiltimer.h"

#define LA_PL_INTR_ID 29u

static void la_irq_record_isr(la_irq_t *irq, XTime start)
{
    XTime end;

    XTime_GetTime(&end);
    la_irq_stats_record(&irq->isr_stats, (uint32_t)(end - start));
    irq->last_isr_exit = end;
}

static void la_pl_isr(void *reference)
{
    la_irq_t *irq = (la_irq_t *)reference;
    BaseType_t woken = pdFALSE;
    XTime start;
    uint32_t status;

    XTime_GetTime(&start);
    la_trace_mark(LA_TRACE_EVENT_ISR_PL, 0u);
    status = irq->regs.read32(irq->regs.base + LA_REG_IRQ_STAT_OFFSET);
    if (status != 0u) {
        irq->regs.write32(irq->regs.base + LA_REG_IRQ_STAT_OFFSET,
                          status & LA_REG_IRQ_STAT_ALLOWED_MASK);
        irq->pl_pending |= status & LA_REG_IRQ_STAT_ALLOWED_MASK;
        irq->pl_count++;
        if (irq->acq_task != NULL) {
            xTaskNotifyFromISR(irq->acq_task, LA_NOTIFY_PL_IRQ, eSetBits, &woken);
        }
    } else {
        irq->spurious++;
    }
    la_irq_record_isr(irq, start);
    portYIELD_FROM_ISR(woken);
}

static void la_dma_irq_handler(void *reference)
{
    la_irq_t *irq = (la_irq_t *)reference;
    BaseType_t woken = pdFALSE;
    XTime start;

    XTime_GetTime(&start);
    la_trace_mark(LA_TRACE_EVENT_ISR_DMA, 0u);
    la_dma_isr(irq->dma);
    irq->dma_count++;
    if (irq->acq_task != NULL) {
        xTaskNotifyFromISR(irq->acq_task, LA_NOTIFY_DMA_IRQ, eSetBits, &woken);
    }
    la_irq_record_isr(irq, start);
    portYIELD_FROM_ISR(woken);
}

int la_irq_connect(uint32_t encoded_id, void (*handler)(void *), void *reference,
                   uintptr_t controller, uint8_t priority)
{
    static int controller_ready = 0;

    if (!controller_ready) {
        /* Sets the wrapper's ScuGicInitialized flag, which is what actually
         * lets XConnectToInterruptCntrl() connect handlers in the SDT BSP. */
        if (XSetupInterruptSystem(reference, (void *)handler, encoded_id,
                                  controller, (u16)priority) != XST_SUCCESS) {
            return -1;
        }
        controller_ready = 1;
        return 0;
    }
    if (xPortInstallInterruptHandler((uint16_t)encoded_id,
                                     (XInterruptHandler)handler,
                                     reference) != pdPASS) {
        return -1;
    }
    return 0;
}

int la_irq_init(la_irq_t *irq, const la_regs_dev_t *regs, la_dma_t *dma,
                TaskHandle_t acq_task)
{
    XAxiDma_Config *config;
    uint16_t dma_intr_id;

    if (irq == NULL || regs == NULL || dma == NULL) {
        return -1;
    }
    config = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
    if (config == NULL) {
        return -1;
    }
    dma_intr_id = config->IntrId[0];

    irq->regs = *regs;
    irq->dma = dma;
    irq->acq_task = acq_task;
    irq->pl_pending = 0u;
    irq->pl_count = 0u;
    irq->dma_count = 0u;
    irq->spurious = 0u;
    irq->last_isr_exit = 0u;
    la_irq_stats_init(&irq->isr_stats,
                      (uint32_t)(COUNTS_PER_SECOND / 1000000u));
    la_irq_stats_init(&irq->wake_stats,
                      (uint32_t)(COUNTS_PER_SECOND / 1000000u));

    if (la_irq_connect(LA_PL_INTR_ID, la_pl_isr, irq,
                       (uintptr_t)config->IntrParent,
                       XINTERRUPT_DEFAULT_PRIORITY) != 0) {
        return -1;
    }
    if (la_irq_connect(dma_intr_id, la_dma_irq_handler, irq,
                       (uintptr_t)config->IntrParent,
                       XINTERRUPT_DEFAULT_PRIORITY) != 0) {
        return -1;
    }
    /* The port installs handlers without touching the GIC priority; the
     * FreeRTOS ISR-safe API asserts unless the interrupt priority is at or
     * below configMAX_API_CALL_INTERRUPT_PRIORITY, so both fabric lines get
     * the Xilinx default (0xA0, numerically below the FreeRTOS mask). */
    XSetPriorityTriggerType(LA_PL_INTR_ID, XINTERRUPT_DEFAULT_PRIORITY,
                            config->IntrParent);
    XSetPriorityTriggerType(dma_intr_id, XINTERRUPT_DEFAULT_PRIORITY,
                            config->IntrParent);
    vPortEnableInterrupt(LA_PL_INTR_ID);
    vPortEnableInterrupt(dma_intr_id);
    return 0;
}

void la_irq_note_task_wake(la_irq_t *irq)
{
    XTime now;

    if (irq == NULL || irq->last_isr_exit == 0u) {
        return;
    }
    XTime_GetTime(&now);
    la_irq_stats_record(&irq->wake_stats, (uint32_t)(now - irq->last_isr_exit));
}
