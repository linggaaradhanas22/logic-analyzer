#include "la_web_core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static int test_message_layout(void)
{
    char buffer[256];
    la_web_core_t core;
    const uint8_t bytes[2] = { 'O', 'K' };

    la_web_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_web_core_begin(&core, 1) == 0, "begin slot 1");
    CHECK(la_web_core_event(&core, 1u, 1234u, bytes, 2u) == 0, "byte event");
    CHECK(la_web_core_event(&core, 8u, 2000u, NULL, 0u) == 0, "ack event");
    CHECK(la_web_core_end(&core) == 0, "end fits");
    CHECK(strcmp(buffer,
                 "{\"s\":1,\"e\":[[1234,1,\"4f4b\"],[2000,8,\"\"]]}") == 0,
          "exact json layout");
    CHECK(core.events == 2u, "event counter");
    CHECK(core.slots[1] == 1u && core.slots[0] == 0u, "slot counter");
    return 0;
}

static int test_hex_encoding(void)
{
    char buffer[128];
    la_web_core_t core;
    const uint8_t bytes[4] = { 0x00u, 0x0au, 0xffu, 0x41u };

    la_web_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_web_core_begin(&core, 2) == 0, "begin slot 2");
    CHECK(la_web_core_event(&core, 1u, 7u, bytes, 4u) == 0, "binary event");
    CHECK(la_web_core_end(&core) == 0, "end fits");
    CHECK(strstr(buffer, "\"000aff41\"") != NULL, "hex payload");
    return 0;
}

static int test_truncation(void)
{
    char buffer[40];
    la_web_core_t core;
    const uint8_t bytes[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    int index;

    la_web_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_web_core_begin(&core, 0) == 0, "begin");
    for (index = 0; index < 8; ++index) {
        if (la_web_core_event(&core, 1u, (uint64_t)index, bytes, 8u) != 0) {
            break;
        }
    }
    CHECK(index < 8, "small buffer truncates");
    CHECK(core.truncated != 0u, "truncation flagged");
    CHECK(la_web_core_end(&core) == -1, "end reports truncation");
    return 0;
}

static int test_slot_guards(void)
{
    char buffer[64];
    la_web_core_t core;

    la_web_core_init(&core, buffer, sizeof(buffer));
    CHECK(la_web_core_begin(&core, LA_WEB_CORE_SLOTS) == -1, "slot out of range");
    CHECK(la_web_core_begin(&core, 0) == 0, "slot 0 accepted");
    CHECK(la_web_core_end(&core) == 0, "empty message closes");
    CHECK(strcmp(buffer, "{\"s\":0,\"e\":[]}") == 0, "empty message layout");
    return 0;
}

static int test_null_guards(void)
{
    la_web_core_t core;

    memset(&core, 0, sizeof(core));
    CHECK(la_web_core_begin(&core, 0) == -1, "unbound buffer rejected");
    CHECK(la_web_core_event(&core, 1u, 0u, NULL, 0u) == -1, "unbound event rejected");
    CHECK(la_web_core_end(&core) == -1, "unbound end rejected");
    la_web_core_init(NULL, NULL, 0u);
    return 0;
}

int main(void)
{
    if (test_message_layout() != 0 ||
        test_hex_encoding() != 0 ||
        test_truncation() != 0 ||
        test_slot_guards() != 0 ||
        test_null_guards() != 0) {
        return 1;
    }
    printf("la_web_core: all tests passed\n");
    return 0;
}
