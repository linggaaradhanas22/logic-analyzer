#ifndef LA_BURST_PLAN_H
#define LA_BURST_PLAN_H

/*
 * P4.6/P4.7 burst sizing and finite SG chain plan (pure logic).
 *
 * Computes, before an ARM is accepted, how many 32-bit words the burst
 * capture can emit in the worst case, whether the capture pool is large
 * enough, and how the finite descriptor chain is laid out. The plan is the
 * contract between CmdTask (which rejects an over-capacity ARM) and
 * AcqTask (which preposts exactly this chain).
 *
 * Word accounting: raw packing emits ceil(samples / 4) words; transition
 * encoding emits at most one word per sample (every sample an edge).
 * Both are 4 bytes per word on the wire.
 *
 * Host tests: tests/native/burst_plan_host_tests.c (la_native_plan).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_PLAN_API __declspec(dllexport)
#else
#define LA_PLAN_API
#endif

/* Upper bound on BDs in one capture chain. 8 MiB / 64 KiB = 128 for the
 * current pool; the margin covers smaller BD sizes. */
#define LA_BURST_PLAN_MAX_BDS 256u

/* XAXIDMA SG length field is 26 bits wide in this design. */
#define LA_BURST_PLAN_BD_MAX_BYTES UINT32_C(0x03FFFFFF)

typedef enum la_burst_plan_result {
    LA_BURST_PLAN_OK = 0,
    LA_BURST_PLAN_ERR_ARGS = -1,
    LA_BURST_PLAN_ERR_OFFSET = -2,   /* byte offset cannot be a u32 */
    LA_BURST_PLAN_ERR_CAPACITY = -3, /* capture pool too small */
    LA_BURST_PLAN_ERR_BD_LIMIT = -4, /* chain needs more than MAX_BDS */
    LA_BURST_PLAN_ERR_BD_MAX = -5    /* BD length limit invalid */
} la_burst_plan_result_t;

typedef struct la_burst_plan {
    uint32_t bd_count;
    uint64_t total_words; /* worst-case words the PL can emit */
    uint64_t total_bytes; /* total_words * 4 */
    uint32_t bd_length[LA_BURST_PLAN_MAX_BDS];
    uint64_t bd_offset[LA_BURST_PLAN_MAX_BDS]; /* from the capture pool base */
} la_burst_plan_t;

/* Worst-case emitted words for a burst configuration. Returns 0 on invalid
 * arguments (encoding other than 0/1 or a NULL output). */
LA_PLAN_API int la_burst_plan_words(uint8_t encoding,
                                    uint32_t pre_samples,
                                    uint32_t post_samples,
                                    uint64_t *words_out);

/* Splits total_words into a finite BD chain of at most max_bds descriptors
 * whose lengths never exceed bd_max_bytes. A zero-word capture still gets
 * one guard BD (bd_max_bytes long) so the channel can be started and the
 * PL completion drained. Returns a la_burst_plan_result_t code. */
LA_PLAN_API int la_burst_plan_compute(uint64_t total_words,
                                      uint32_t bd_max_bytes,
                                      uint32_t pool_bytes,
                                      uint32_t max_bds,
                                      la_burst_plan_t *plan);

/* Trigger timeout in sample clocks -> milliseconds, rounded up (0 keeps the
 * disabled-timeout meaning). */
LA_PLAN_API uint32_t la_burst_plan_timeout_ms(uint32_t timeout_samples,
                                              uint32_t sample_div);

#endif /* LA_BURST_PLAN_H */
