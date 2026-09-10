#ifndef LA_IRQ_STATS_H
#define LA_IRQ_STATS_H

/*
 * P4.5 ISR latency statistics (pure logic).
 *
 * The IRQ plumbing records one entry-to-exit duration (or ISR-exit to task
 * wake duration) per event in XTime counts; this core keeps the min/max/mean
 * in microseconds for STATUS/LOG reporting. No FreeRTOS or Xilinx calls, so
 * host tests exercise every branch (la_native_irqstats).
 */

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_IRQSTATS_API __declspec(dllexport)
#else
#define LA_IRQSTATS_API
#endif

typedef struct la_irq_stats {
    uint32_t samples;
    uint32_t min_counts;
    uint32_t max_counts;
    uint64_t total_counts;
    uint32_t counts_per_us;
} la_irq_stats_t;

/* counts_per_us is the XTime tick rate divided by one million (333 for the
 * Zynq A9 global timer). Zero is rejected and keeps the stats disabled. */
LA_IRQSTATS_API void la_irq_stats_init(la_irq_stats_t *stats,
                                       uint32_t counts_per_us);

LA_IRQSTATS_API void la_irq_stats_record(la_irq_stats_t *stats,
                                         uint32_t delta_counts);

LA_IRQSTATS_API uint32_t la_irq_stats_min_us(const la_irq_stats_t *stats);
LA_IRQSTATS_API uint32_t la_irq_stats_max_us(const la_irq_stats_t *stats);
LA_IRQSTATS_API uint32_t la_irq_stats_avg_us(const la_irq_stats_t *stats);

#endif /* LA_IRQ_STATS_H */
