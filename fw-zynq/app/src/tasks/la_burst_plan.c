#include "la_burst_plan.h"

int la_burst_plan_words(uint8_t encoding,
                        uint32_t pre_samples,
                        uint32_t post_samples,
                        uint64_t *words_out)
{
    uint64_t samples;

    if (words_out == NULL || encoding > 1u) {
        return -1;
    }
    samples = (uint64_t)pre_samples + (uint64_t)post_samples;
    if (encoding == 0u) {
        *words_out = (samples + 3u) / 4u;
    } else {
        *words_out = samples;
    }
    return 0;
}

int la_burst_plan_compute(uint64_t total_words,
                          uint32_t bd_max_bytes,
                          uint32_t pool_bytes,
                          uint32_t max_bds,
                          la_burst_plan_t *plan)
{
    uint64_t total_bytes;
    uint32_t full_bds;
    uint32_t remainder;
    uint32_t index;
    uint64_t offset;

    if (plan == NULL || bd_max_bytes == 0u || pool_bytes == 0u ||
        max_bds == 0u || max_bds > LA_BURST_PLAN_MAX_BDS) {
        return LA_BURST_PLAN_ERR_ARGS;
    }
    if (bd_max_bytes > LA_BURST_PLAN_BD_MAX_BYTES) {
        return LA_BURST_PLAN_ERR_BD_MAX;
    }
    total_bytes = total_words * 4u;
    if (total_bytes > UINT32_MAX) {
        return LA_BURST_PLAN_ERR_OFFSET;
    }
    if (total_bytes > pool_bytes) {
        return LA_BURST_PLAN_ERR_CAPACITY;
    }

    plan->total_words = total_words;
    plan->total_bytes = total_bytes;

    if (total_bytes == 0u) {
        plan->bd_count = 1u;
        plan->bd_length[0] = bd_max_bytes;
        plan->bd_offset[0] = 0u;
        return LA_BURST_PLAN_OK;
    }

    full_bds = (uint32_t)(total_bytes / bd_max_bytes);
    remainder = (uint32_t)(total_bytes % bd_max_bytes);
    plan->bd_count = full_bds + (remainder != 0u ? 1u : 0u);
    if (plan->bd_count > max_bds) {
        return LA_BURST_PLAN_ERR_BD_LIMIT;
    }

    offset = 0u;
    for (index = 0u; index < plan->bd_count; ++index) {
        plan->bd_offset[index] = offset;
        if (index < full_bds) {
            plan->bd_length[index] = bd_max_bytes;
        } else {
            plan->bd_length[index] = remainder;
        }
        offset += plan->bd_length[index];
    }
    return LA_BURST_PLAN_OK;
}

uint32_t la_burst_plan_timeout_ms(uint32_t timeout_samples, uint32_t sample_div)
{
    uint64_t clocks;
    uint64_t ms;

    if (timeout_samples == 0u) {
        return 0u;
    }
    clocks = (uint64_t)timeout_samples * ((uint64_t)sample_div + 1u);
    ms = (clocks + 99999u) / 100000u; /* ceil at 100 MHz */
    if (ms > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)ms;
}
