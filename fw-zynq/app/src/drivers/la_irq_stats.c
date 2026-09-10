#include "la_irq_stats.h"

#include <stddef.h>

void la_irq_stats_init(la_irq_stats_t *stats, uint32_t counts_per_us)
{
    if (stats == NULL) {
        return;
    }
    stats->samples = 0u;
    stats->min_counts = 0u;
    stats->max_counts = 0u;
    stats->total_counts = 0u;
    stats->counts_per_us = counts_per_us;
}

void la_irq_stats_record(la_irq_stats_t *stats, uint32_t delta_counts)
{
    if (stats == NULL || stats->counts_per_us == 0u) {
        return;
    }
    if (stats->samples == 0u || delta_counts < stats->min_counts) {
        stats->min_counts = delta_counts;
    }
    if (delta_counts > stats->max_counts) {
        stats->max_counts = delta_counts;
    }
    stats->total_counts += delta_counts;
    stats->samples++;
}

uint32_t la_irq_stats_min_us(const la_irq_stats_t *stats)
{
    if (stats == NULL || stats->samples == 0u || stats->counts_per_us == 0u) {
        return 0u;
    }
    return stats->min_counts / stats->counts_per_us;
}

uint32_t la_irq_stats_max_us(const la_irq_stats_t *stats)
{
    if (stats == NULL || stats->samples == 0u || stats->counts_per_us == 0u) {
        return 0u;
    }
    return stats->max_counts / stats->counts_per_us;
}

uint32_t la_irq_stats_avg_us(const la_irq_stats_t *stats)
{
    if (stats == NULL || stats->samples == 0u || stats->counts_per_us == 0u) {
        return 0u;
    }
    return (uint32_t)(stats->total_counts / stats->samples / stats->counts_per_us);
}
