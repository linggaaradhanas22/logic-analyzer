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

/* One completed live BD as reported to AcqTask. id is the segment slot the
 * descriptor was posted with (XAxiDma_BdSetId). */
typedef struct la_dma_live_bd {
    uint32_t id;
    uint32_t length;
    uint32_t status;
} la_dma_live_bd_t;

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

/* P7.2 live ring: rebuilds the BD ring with count descriptors, one per
 * segment, each segment_bytes long and pointing at addresses[i], with the BD
 * ID set to i. Ordinary SG, no cyclic mode; descriptors are re-posted as
 * they complete (la_dma_post_live). Returns 0 or a negative error. */
int la_dma_prepare_live(la_dma_t *dma, const uintptr_t *addresses,
                        uint32_t count, uint32_t segment_bytes);

/* Drains completed live BDs into entries (at most max_entries) and frees
 * them back to the driver free list. Returns the entry count, -1 on bad
 * arguments/not ready, or -2 when more BDs completed than max_entries (all
 * are still freed; the caller must size the array to the ring count). */
int la_dma_collect_live(la_dma_t *dma, la_dma_live_bd_t *entries,
                        uint32_t max_entries);

/* Allocates one free BD and posts it with the given address, length and ID.
 * Returns 0, -1 on bad arguments/not ready or -2 when no free BD exists. */
int la_dma_post_live(la_dma_t *dma, uint32_t id, uintptr_t address,
                     uint32_t length);

/* Number of BDs currently on the driver free list (postable). */
uint32_t la_dma_free_bd_count(const la_dma_t *dma);

/* Resets the channel and waits for the reset to complete. Safe to call
 * when idle. */
void la_dma_stop(la_dma_t *dma);

/* GIC ISR body (no FreeRTOS calls). */
void la_dma_isr(la_dma_t *dma);

#endif /* LA_DMA_H */
