#ifndef LA_IRQ_H
#define LA_IRQ_H

/*
 * P4.5 interrupt plumbing.
 *
 * Installs the two fabric interrupt handlers through the FreeRTOS port
 * (xPortInstallInterruptHandler + vPortEnableInterrupt, the SDT-flow entry
 * points) and keeps the ISR bodies to notification-only work:
 *
 *   - PL IRQ: reads IRQ_STAT, write-1-clears the pending bits and notifies
 *     AcqTask; the sticky STATUS/IRQ_STAT bits stay readable by the task.
 *   - DMA S2MM IRQ: acknowledges the channel interrupt through la_dma_isr()
 *     and notifies AcqTask.
 *
 * The PL interrupt has no generated SDT interrupt ID because la_top_0 is a
 * module reference; the ID is derived from the P3 xlconcat wiring:
 * la_top_0/irq_o -> In0 -> IRQ_F2P[0] -> GIC 61, encoded as 61-32 = 29 in
 * the SDT numbering (the DMA on In1 -> IRQ_F2P[1] -> GIC 62 is
 * XPAR_FABRIC_AXI_DMA_0_INTR = 30, which pins the mapping). The DMA ID is
 * taken from the driver config, not hardcoded.
 *
 * Latency: every ISR records its own entry-to-exit duration; AcqTask calls
 * la_irq_note_task_wake() after each notification wake to record the
 * ISR-exit-to-task latency. Both are reported through LOG/console, not
 * through the v1 STATUS payload (which has no latency field).
 */

#include "FreeRTOS.h"
#include "task.h"

#include "la_dma.h"
#include "la_irq_stats.h"
#include "la_regs_drv.h"

#include <stdint.h>

/* AcqTask/decoder-task notification bits (ISRs, CmdTask and peer tasks). */
#define LA_NOTIFY_PL_IRQ ((uint32_t)0x00000001u)
#define LA_NOTIFY_DMA_IRQ ((uint32_t)0x00000002u)
#define LA_NOTIFY_CTRL ((uint32_t)0x00000004u)
#define LA_NOTIFY_TX ((uint32_t)0x00000008u)
#define LA_NOTIFY_JOB_DONE ((uint32_t)0x00000010u)
#define LA_NOTIFY_SEGMENT_FREE ((uint32_t)0x00000020u)
#define LA_NOTIFY_LIVE_STOP ((uint32_t)0x00000040u)
#define LA_NOTIFY_DECODER ((uint32_t)0x00000080u)

typedef struct la_irq {
    la_regs_dev_t regs;
    la_dma_t *dma;
    TaskHandle_t acq_task;
    la_irq_stats_t isr_stats;
    la_irq_stats_t wake_stats;
    volatile uint64_t last_isr_exit;
    volatile uint32_t pl_pending; /* IRQ_STAT bits latched by the PL ISR */
    volatile uint32_t pl_count;
    volatile uint32_t dma_count;
    volatile uint32_t spurious;
} la_irq_t;

/* Installs and enables both fabric interrupts. Returns 0 or -1 when a
 * handler cannot be installed. */
int la_irq_init(la_irq_t *irq, const la_regs_dev_t *regs, la_dma_t *dma,
                TaskHandle_t acq_task);

/* Connects one GIC handler through the FreeRTOS port. The SDT BSP's
 * XConnectToInterruptCntrl() silently skips every connection until
 * XSetupInterruptSystem() has set its internal ScuGicInitialized flag, and the
 * port's own initialisation path (XConfigInterruptCntrl) never does. Without
 * this workaround every handler stays StubHandler, the first asserted level
 * interrupt (e.g. UART TXEMPTY) never clears and the CPU is starved in the IRQ
 * vector. The first call in the image therefore goes through
 * XSetupInterruptSystem() (which also sets the priority/trigger); later calls
 * use xPortInstallInterruptHandler(). Both leave the line disabled, so
 * vPortEnableInterrupt() must follow. Returns 0 or -1. */
int la_irq_connect(uint32_t encoded_id, void (*handler)(void *), void *reference,
                   uintptr_t controller, uint8_t priority);

/* Records the ISR-exit-to-task wake latency for the notification that woke
 * AcqTask. Call once per AcqTask wake. */
void la_irq_note_task_wake(la_irq_t *irq);

#endif /* LA_IRQ_H */
