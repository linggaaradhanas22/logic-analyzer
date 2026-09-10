#include "la_burst_plan.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static int test_word_accounting(void)
{
    uint64_t words = 0u;

    CHECK(la_burst_plan_words(0u, 0u, 0u, &words) == 0 && words == 0u,
          "empty raw capture is zero words");
    CHECK(la_burst_plan_words(0u, 0u, 1u, &words) == 0 && words == 1u,
          "one raw sample is one word");
    CHECK(la_burst_plan_words(0u, 0u, 4u, &words) == 0 && words == 1u,
          "four raw samples pack to one word");
    CHECK(la_burst_plan_words(0u, 0u, 5u, &words) == 0 && words == 2u,
          "five raw samples need two words");
    CHECK(la_burst_plan_words(0u, 1000u, 234u, &words) == 0 && words == 309u,
          "pre and post samples both count");
    CHECK(la_burst_plan_words(1u, 0u, 1u, &words) == 0 && words == 1u,
          "transition worst case is one word per sample");
    CHECK(la_burst_plan_words(1u, 512u, 512u, &words) == 0 && words == 1024u,
          "transition pre plus post");
    CHECK(la_burst_plan_words(2u, 0u, 1u, &words) != 0,
          "invalid encoding rejected");
    CHECK(la_burst_plan_words(0u, 0u, 1u, NULL) != 0, "NULL output rejected");
    return 0;
}

static int test_plan_exact_split(void)
{
    la_burst_plan_t plan;
    uint32_t index;

    memset(&plan, 0, sizeof(plan));
    CHECK(la_burst_plan_compute(4096u, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_OK,
          "16 KiB in four 4 KiB BDs plans");
    CHECK(plan.bd_count == 4u && plan.total_words == 4096u &&
              plan.total_bytes == 16384u,
          "plan totals");
    for (index = 0u; index < plan.bd_count; ++index) {
        CHECK(plan.bd_length[index] == 4096u, "each BD is the max size");
        CHECK(plan.bd_offset[index] == (uint64_t)index * 4096u, "BD offset");
    }
    return 0;
}

static int test_plan_short_tail(void)
{
    la_burst_plan_t plan;

    memset(&plan, 0, sizeof(plan));
    CHECK(la_burst_plan_compute(1000u, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_OK,
          "short capture plans");
    CHECK(plan.bd_count == 1u && plan.bd_length[0] == 4000u,
          "single partial BD");

    memset(&plan, 0, sizeof(plan));
    CHECK(la_burst_plan_compute(1025u, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_OK,
          "capture with tail plans");
    CHECK(plan.bd_count == 2u && plan.bd_length[0] == 4096u &&
              plan.bd_length[1] == 4u && plan.bd_offset[1] == 4096u,
          "full BD plus four-byte tail");
    return 0;
}

static int test_plan_guard_bd_and_limits(void)
{
    la_burst_plan_t plan;

    memset(&plan, 0, sizeof(plan));
    CHECK(la_burst_plan_compute(0u, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_OK,
          "empty capture still gets a guard BD");
    CHECK(plan.bd_count == 1u && plan.bd_length[0] == 4096u &&
              plan.total_bytes == 0u,
          "guard BD is max length");

    CHECK(la_burst_plan_compute(1u << 18, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_OK,
          "1 MiB of words fits the 1 MiB pool");
    CHECK(plan.bd_count == 256u, "1 MiB is exactly 256 BDs at 4 KiB");
    CHECK(la_burst_plan_compute((1u << 18) + 1u, 4096u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_ERR_CAPACITY,
          "over-pool capture rejected");
    CHECK(la_burst_plan_compute(5000u, 4096u, 1u << 20, 4u, &plan) ==
              LA_BURST_PLAN_ERR_BD_LIMIT,
          "over-BD-count chain rejected");

    CHECK(la_burst_plan_compute(1u << 18, 4096u, 1u << 20, 0u, &plan) ==
              LA_BURST_PLAN_ERR_ARGS,
          "zero max BDs rejected");
    CHECK(la_burst_plan_compute(1u << 18, 0u, 1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_ERR_ARGS,
          "zero BD size rejected");
    CHECK(la_burst_plan_compute(1u << 18, LA_BURST_PLAN_BD_MAX_BYTES + 1u,
                                1u << 20, 256u, &plan) ==
              LA_BURST_PLAN_ERR_BD_MAX,
          "BD larger than the 26-bit length field rejected");
    CHECK(la_burst_plan_compute(UINT64_C(0x40000000), 4096u, 0xFFFFFFFFu, 256u,
                                &plan) == LA_BURST_PLAN_ERR_OFFSET,
          "capture beyond the u32 offset space rejected");
    CHECK(la_burst_plan_compute(1u << 18, 4096u, 1u << 20, 257u, &plan) ==
              LA_BURST_PLAN_ERR_ARGS,
          "max BDs beyond the plan array rejected");
    CHECK(la_burst_plan_compute(1u << 18, 4096u, 1u << 20, 256u, NULL) ==
              LA_BURST_PLAN_ERR_ARGS,
          "NULL plan rejected");
    return 0;
}

static int test_timeout_ms(void)
{
    CHECK(la_burst_plan_timeout_ms(0u, 0u) == 0u, "zero timeout stays disabled");
    CHECK(la_burst_plan_timeout_ms(100000u, 0u) == 1u,
          "100k clocks at 100 MHz is one millisecond");
    CHECK(la_burst_plan_timeout_ms(100001u, 0u) == 2u,
          "partial milliseconds round up");
    CHECK(la_burst_plan_timeout_ms(100000u, 9u) == 10u,
          "sample_div scales the timeout");
    CHECK(la_burst_plan_timeout_ms(UINT32_MAX, UINT32_MAX) == UINT32_MAX,
          "overflow saturates");
    return 0;
}

int main(void)
{
    if (test_word_accounting() != 0 || test_plan_exact_split() != 0 ||
        test_plan_short_tail() != 0 || test_plan_guard_bd_and_limits() != 0 ||
        test_timeout_ms() != 0) {
        return 1;
    }
    puts("PASS: la_native_plan");
    return 0;
}
