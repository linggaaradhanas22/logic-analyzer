#include "la_spi_core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                   \
    do {                                            \
        if (!(condition)) {                         \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                               \
        }                                           \
    } while (0)

static int test_init_and_guards(void)
{
    la_spi_core_t core;
    uint8_t buffer[8];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_spi_core_space(&core) == sizeof(buffer), "initial space");
    CHECK(la_spi_core_used(&core) == 0u, "initial used");
    CHECK(la_spi_core_ready(&core) == 0, "not ready when empty");
    CHECK(la_spi_core_push(&core, NULL, 1u) == -1, "push NULL frame");
    CHECK(la_spi_core_push(&core, buffer, 0u) == -1, "push zero length");
    CHECK(la_spi_core_feed(&core, NULL, 1u) == 0u, "feed NULL out");
    CHECK(la_spi_core_feed(&core, buffer, 0u) == 0u, "feed zero bytes");
    CHECK(la_spi_core_space(NULL) == 0u, "space guard");
    CHECK(la_spi_core_used(NULL) == 0u, "used guard");
    CHECK(la_spi_core_ready(NULL) == 0, "ready guard");
    la_spi_core_init(NULL, buffer, sizeof(buffer));
    la_spi_core_note_underrun(NULL);
    return 0;
}

static int test_push_feed_order(void)
{
    la_spi_core_t core;
    uint8_t buffer[8];
    uint8_t out[8];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_spi_core_push(&core, (const uint8_t *)"ABCD", 4u) == 0, "push 1");
    CHECK(la_spi_core_push(&core, (const uint8_t *)"EF", 2u) == 0, "push 2");
    CHECK(la_spi_core_used(&core) == 6u, "used after pushes");
    CHECK(la_spi_core_space(&core) == 2u, "space after pushes");
    CHECK(la_spi_core_ready(&core) == 1, "ready with data");
    CHECK(la_spi_core_feed(&core, out, 3u) == 3u && memcmp(out, "ABC", 3u) == 0,
          "feed partial order");
    CHECK(la_spi_core_used(&core) == 3u, "used after partial feed");
    CHECK(la_spi_core_feed(&core, out, sizeof(out)) == 3u &&
              memcmp(out, "DEF", 3u) == 0,
          "feed rest order");
    CHECK(la_spi_core_ready(&core) == 0, "not ready after drain");
    CHECK(la_spi_core_feed(&core, out, sizeof(out)) == 0u, "feed when empty");
    CHECK(core.frames_queued == 2u, "frames queued");
    CHECK(core.bytes_fed == 6u, "bytes fed");
    return 0;
}

static int test_wrap_and_compaction(void)
{
    la_spi_core_t core;
    uint8_t buffer[8];
    uint8_t out[8];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_spi_core_push(&core, (const uint8_t *)"ABCDEF", 6u) == 0, "push 6");
    CHECK(la_spi_core_feed(&core, out, 4u) == 4u && memcmp(out, "ABCD", 4u) == 0,
          "feed head");
    /* tail = 6 and 6 + 4 > 8: the push must compact the two buffered bytes. */
    CHECK(la_spi_core_push(&core, (const uint8_t *)"GHIJ", 4u) == 0,
          "push across wrap");
    CHECK(la_spi_core_used(&core) == 6u, "used after wrap push");
    CHECK(la_spi_core_feed(&core, out, sizeof(out)) == 6u &&
              memcmp(out, "EFGHIJ", 6u) == 0,
          "order after compaction");
    return 0;
}

static int test_feed_wrap(void)
{
    la_spi_core_t core;
    uint8_t buffer[8];
    uint8_t out[8];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_spi_core_push(&core, (const uint8_t *)"ABCDEFG", 7u) == 0, "push 7");
    CHECK(la_spi_core_feed(&core, out, 6u) == 6u, "feed 6");
    CHECK(la_spi_core_push(&core, (const uint8_t *)"H", 1u) == 0, "push wrap");
    CHECK(la_spi_core_feed(&core, out, sizeof(out)) == 2u &&
              memcmp(out, "GH", 2u) == 0,
          "order across feed wrap");
    return 0;
}

static int test_capacity_rejection(void)
{
    la_spi_core_t core;
    uint8_t buffer[4];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_spi_core_push(&core, (const uint8_t *)"ABCDE", 5u) == -1,
          "reject oversized frame");
    CHECK(core.frames_dropped == 1u, "drop counter");
    CHECK(la_spi_core_push(&core, (const uint8_t *)"ABCD", 4u) == 0, "fill ring");
    CHECK(la_spi_core_push(&core, (const uint8_t *)"E", 1u) == -1,
          "reject when full");
    CHECK(core.frames_dropped == 2u, "drop counter after full");
    return 0;
}

static int test_underrun_counter(void)
{
    la_spi_core_t core;
    uint8_t buffer[4];

    la_spi_core_init(&core, buffer, sizeof(buffer));
    la_spi_core_note_underrun(&core);
    la_spi_core_note_underrun(&core);
    CHECK(core.underruns == 2u, "underrun counter");
    return 0;
}

int main(void)
{
    if (test_init_and_guards() != 0 || test_push_feed_order() != 0 ||
        test_wrap_and_compaction() != 0 || test_feed_wrap() != 0 ||
        test_capacity_rejection() != 0 || test_underrun_counter() != 0) {
        return 1;
    }
    puts("PASS: la_native_spi_core");
    return 0;
}
