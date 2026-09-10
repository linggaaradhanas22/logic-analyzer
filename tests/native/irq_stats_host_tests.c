#include "la_irq_stats.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static int test_empty_stats(void)
{
    la_irq_stats_t stats;

    la_irq_stats_init(&stats, 333u);
    CHECK(stats.samples == 0u, "init clears samples");
    CHECK(la_irq_stats_min_us(&stats) == 0u, "empty min is zero");
    CHECK(la_irq_stats_max_us(&stats) == 0u, "empty max is zero");
    CHECK(la_irq_stats_avg_us(&stats) == 0u, "empty average is zero");
    return 0;
}

static int test_min_max_average(void)
{
    la_irq_stats_t stats;

    la_irq_stats_init(&stats, 333u); /* counts per microsecond */
    la_irq_stats_record(&stats, 3330u); /* 10 us */
    CHECK(la_irq_stats_min_us(&stats) == 10u && la_irq_stats_max_us(&stats) == 10u,
          "first sample sets min and max");
    la_irq_stats_record(&stats, 666u); /* 2 us */
    la_irq_stats_record(&stats, 33300u); /* 100 us */
    CHECK(stats.samples == 3u, "sample count");
    CHECK(la_irq_stats_min_us(&stats) == 2u, "minimum updated");
    CHECK(la_irq_stats_max_us(&stats) == 100u, "maximum updated");
    CHECK(la_irq_stats_avg_us(&stats) == 37u, "average of 10, 2 and 100 us");
    return 0;
}

static int test_guards(void)
{
    la_irq_stats_t stats;

    la_irq_stats_init(&stats, 0u); /* disabled */
    la_irq_stats_record(&stats, 100u);
    CHECK(stats.samples == 0u, "zero counts_per_us disables recording");
    CHECK(la_irq_stats_min_us(&stats) == 0u, "disabled min is zero");

    la_irq_stats_init(&stats, 100u);
    la_irq_stats_record(&stats, 50u); /* 0.5 us rounds down */
    CHECK(la_irq_stats_min_us(&stats) == 0u, "sub-microsecond rounds down");
    CHECK(la_irq_stats_avg_us(&stats) == 0u, "sub-microsecond average");

    la_irq_stats_init(NULL, 100u);
    la_irq_stats_record(NULL, 100u);
    CHECK(la_irq_stats_min_us(NULL) == 0u, "NULL guards");
    return 0;
}

int main(void)
{
    if (test_empty_stats() != 0 || test_min_max_average() != 0 ||
        test_guards() != 0) {
        return 1;
    }
    puts("PASS: la_native_irqstats");
    return 0;
}
