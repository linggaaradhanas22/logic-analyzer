#ifndef LA_DMA_H
#define LA_DMA_H

/*
 * P4.6 AXI DMA burst driver (ordinary scatter-gather, S2MM only).
 *
 * Wraps the XAxiDma SG ring for one finite preposted capture chain per ARM:
 * la_dma_prepare() resets the channel, rebuilds the BD ring inside the
 * non-cached SG pool (la_mem_geom), programs exactly the BDs the burst plan
 * computed and starts the S2MM channel. la_dma_collect() drains the
 * completed BDs (actual lengths, RXEOF) and la_dma_stop() resets the
 * channel so a partially filled chain never leaks into the next capture.
 *
 * No XAxiDma_SimpleTransfer and no cyclic mode anywhere; the chain is
 * finite and always rebuilt after a reset. The DMA instance and the ring
 * live in non-cached DDR, so no cache maintenance is needed (P4.3).
 *
 * ISR contract: la_dma_isr() is called from the GIC DMA ISR; it only reads
 * and acknowledges the channel interrupt and latches error/IOC flags. The
 * caller (la_irq) notifies AcqTask, which owns all ring bookkeeping.
 */

#include "la_burst_plan.h"

#include "xaxidma.h"

#include <stdint.h>

/* Descriptor size used for the finite burst chain: 64 KiB per BD keeps the
 * chain short (128 BDs cover the 8 MiB pool) while staying far below the
 * 26-bit SG length field. */
#define LA_DMA_BD_MAX_BYTES UINT32_C(65536)

typedef struct la_dma {
    XAxiDma dma;
    XAxiDma_BdRing *ring;
    uintptr_t bd_space;
    uint32_t bd_space_size;
    uint32_t bd_max_bytes;
    int ready;
    volatile uint32_t error;     /* sticky: channel error or BD error status */
    volatile uint32_t ioc_count; /* completion interrupts seen */
    uint32_t last_bd_count;
    uint32_t last_actual_bytes;
} la_dma_t;

/* bd_space must be 64-byte aligned (the non-cached SG pool base is).
 * Returns 0, -1 on a missing config, -2 on CfgInitialize failure or
 * -3 when the core is not in SG mode. */
int la_dma_init(la_dma_t *dma, uintptr_t bd_space, uint32_t bd_space_size,
                uint32_t bd_max_bytes);

/* Rebuilds and starts the finite chain described by plan, writing into
 * buffer_base + bd_offset. Returns 0 or a negative error (not ready, plan
 * too large for the pool, ring create/alloc/free failures). */
int la_dma_prepare(la_dma_t *dma, const la_burst_plan_t *plan, uintptr_t buffer_base);

/* Drains the completed BDs: sums actual lengths, reports whether the
 * closing RXEOF beat was seen and frees the completed BDs. Returns the
 * number of completed BDs. */
int la_dma_collect(la_dma_t *dma, uint32_t *bytes_out, int *eof_out);

/* Resets the channel and waits for the reset to complete. Safe to call
 * when idle. */
void la_dma_stop(la_dma_t *dma);

/* GIC ISR body (no FreeRTOS calls). */
void la_dma_isr(la_dma_t *dma);

#endif /* LA_DMA_H */
