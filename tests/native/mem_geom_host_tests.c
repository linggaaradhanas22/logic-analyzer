#include "la_mem_geom.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

typedef struct attr_recorder {
    uintptr_t bases[64];
    uint32_t count;
} attr_recorder_t;

static void record_attr(void *context, uintptr_t section_base)
{
    attr_recorder_t *recorder = (attr_recorder_t *)context;

    recorder->bases[recorder->count] = section_base;
    recorder->count += 1u;
}

static int test_constant_geometry_ok(void)
{
    attr_recorder_t recorder;
    uint32_t sections;
    uint32_t index;

    CHECK(la_mem_geometry_check(LA_MEM_DDR_BASE) == LA_MEM_CHECK_OK,
          "geometry with firmware below the NC window passes");
    CHECK(la_mem_nc_base() == LA_MEM_NC_BASE, "nc base accessor");
    CHECK(la_mem_capture_base() == LA_MEM_NC_BASE + LA_MEM_CAPTURE_OFFSET,
          "capture base accessor");
    CHECK(la_mem_sg_base() == LA_MEM_NC_BASE + LA_MEM_SG_OFFSET, "sg base accessor");
    CHECK(la_mem_capture_base() % LA_MEM_SECTION_SIZE == 0u, "capture 1 MiB aligned");
    CHECK(la_mem_sg_base() % LA_MEM_SECTION_SIZE == 0u, "sg pool 1 MiB aligned");

    memset(&recorder, 0, sizeof(recorder));
    sections = la_mem_apply_noncached(record_attr, &recorder);
    CHECK(sections == LA_MEM_NC_SIZE / LA_MEM_SECTION_SIZE, "one call per section");
    CHECK(recorder.count == sections, "recorder saw every section");
    for (index = 0u; index < sections; ++index) {
        CHECK(recorder.bases[index] == LA_MEM_NC_BASE + (uintptr_t)index * LA_MEM_SECTION_SIZE,
              "section bases are contiguous 1 MiB steps");
    }
    return 0;
}

static int test_fw_overlap(void)
{
    CHECK(la_mem_geometry_check(LA_MEM_NC_BASE + 1u) == LA_MEM_CHECK_ERR_FW_OVERLAP_NC,
          "firmware end inside the NC window detected");
    CHECK(la_mem_geometry_check(LA_MEM_NC_BASE) == LA_MEM_CHECK_OK,
          "firmware ending exactly at the window is legal");
    return 0;
}

static int test_full_branches(void)
{
    uintptr_t base = LA_MEM_NC_BASE;
    uintptr_t size = LA_MEM_NC_SIZE;

    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size,
                                     LA_MEM_CAPTURE_OFFSET, LA_MEM_CAPTURE_SIZE,
                                     LA_MEM_SG_OFFSET, LA_MEM_SG_SIZE) == LA_MEM_CHECK_OK,
          "full check passes with the nominal constants");

    /* Window outside DDR (fw_end=0 so the overlap check cannot preempt). */
    CHECK(la_mem_geometry_check_full(0u, LA_MEM_DDR_BASE - 1u, size,
                                     0u, 0u, 0u, 0u) == LA_MEM_CHECK_ERR_NC_OUT_OF_DDR,
          "window below DDR detected");
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, LA_MEM_NC_BASE, size * 2u,
                                     0u, 0u, 0u, 0u) == LA_MEM_CHECK_ERR_NC_OUT_OF_DDR,
          "window past DDR end detected");

    /* Window not section aligned (keep the far end inside DDR). */
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base + 0x1000u,
                                     size - 0x1000u, 0u, 0u, 0u, 0u) ==
              LA_MEM_CHECK_ERR_NC_MISALIGNED,
          "misaligned window base detected");
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size - 0x1000u,
                                     0u, 0u, 0u, 0u) == LA_MEM_CHECK_ERR_NC_MISALIGNED,
          "misaligned window size detected");

    /* Pools not 64-byte aligned or escaping the window. */
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size, 1u, 64u, 0u, 0u) ==
              LA_MEM_CHECK_ERR_POOL_MISALIGNED,
          "misaligned pool offset detected");
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size, 0u, 63u, 0u, 0u) ==
              LA_MEM_CHECK_ERR_POOL_MISALIGNED,
          "non-multiple pool size detected");
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size, 0u, 0u,
                                     size - 63u, 64u) == LA_MEM_CHECK_ERR_POOL_MISALIGNED,
          "sg pool not cache-line rounded detected");
    CHECK(la_mem_geometry_check_full(LA_MEM_DDR_BASE, base, size, 0u,
                                     size + 64u, 0u, 0u) == LA_MEM_CHECK_ERR_POOL_OUT_OF_NC,
          "capture pool escaping the window detected");
    return 0;
}

int main(void)
{
    if (test_constant_geometry_ok() != 0 || test_fw_overlap() != 0 ||
        test_full_branches() != 0) {
        return 1;
    }
    puts("PASS: la_native_mem");
    return 0;
}
