#include "la_web_core.h"
#include "la_proto.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

typedef struct live_ctx {
    la_web_core_t *core;
    int ok;
    int events;
} live_ctx_t;

static void on_live_frame(void *context, uint8_t type, uint8_t sequence,
                          const uint8_t *payload, uint16_t length)
{
    live_ctx_t *ctx = (live_ctx_t *)context;
    size_t offset = 0u;
    int begun = 0;

    (void)sequence;
    if (type != LA_PROTO_EVT_DECODED || payload == NULL || length == 0u) {
        return;
    }
    while (offset + 11u <= (size_t)length) {
        uint8_t slot = payload[offset];
        uint8_t kind = payload[offset + 1u];
        uint64_t t_sample = (uint64_t)payload[offset + 2u] |
                            ((uint64_t)payload[offset + 3u] << 8u) |
                            ((uint64_t)payload[offset + 4u] << 16u) |
                            ((uint64_t)payload[offset + 5u] << 24u) |
                            ((uint64_t)payload[offset + 6u] << 32u) |
                            ((uint64_t)payload[offset + 7u] << 40u) |
                            ((uint64_t)payload[offset + 8u] << 48u) |
                            ((uint64_t)payload[offset + 9u] << 56u);
        uint8_t data_length = payload[offset + 10u];

        if (offset + 11u + (size_t)data_length > (size_t)length) {
            ctx->ok = 0;
            return;
        }
        if (begun == 0) {
            if (la_web_core_begin(ctx->core, slot) != 0) {
                ctx->ok = 0;
                return;
            }
            begun = 1;
        }
        if (la_web_core_event(ctx->core, kind, t_sample,
                              &payload[offset + 11u], data_length) != 0) {
            ctx->ok = 0;
            return;
        }
        ctx->events += 1;
        offset += 11u + (size_t)data_length;
    }
    if (begun == 0 || offset != (size_t)length) {
        ctx->ok = 0;
    }
}

static int test_frame_round_trip(void)
{
    uint8_t payload[64];
    uint8_t frame[128];
    size_t payload_length = 0u;
    size_t frame_length;
    char buffer[512];
    la_web_core_t core;
    live_ctx_t ctx;
    la_proto_parser_t parser;

    payload[payload_length++] = 0u;   /* slot */
    payload[payload_length++] = 1u;   /* kind BYTE */
    memset(&payload[payload_length], 0, 8u);
    payload[payload_length] = 0x40u;  /* t_sample = 0x40 */
    payload_length += 8u;
    payload[payload_length++] = 1u;   /* len */
    payload[payload_length++] = 'A';
    payload[payload_length++] = 0u;   /* slot */
    payload[payload_length++] = 8u;   /* kind ACK */
    memset(&payload[payload_length], 0, 8u);
    payload[payload_length] = 0x50u;
    payload_length += 8u;
    payload[payload_length++] = 0u;   /* len */

    frame_length = la_proto_encode(LA_PROTO_EVT_DECODED, 7u, payload,
                                   (uint16_t)payload_length, frame,
                                   sizeof(frame));
    CHECK(frame_length == payload_length + 7u, "frame encoded");

    la_web_core_init(&core, buffer, sizeof(buffer));
    ctx.core = &core;
    ctx.ok = 1;
    ctx.events = 0;
    la_proto_parser_init(&parser);
    CHECK(la_proto_parser_feed(&parser, frame, frame_length, on_live_frame,
                               &ctx) == 1u, "one frame parsed");
    CHECK(ctx.ok == 1, "callback accepted the records");
    CHECK(ctx.events == 2, "two records formatted");
    CHECK(la_web_core_end(&core) == 0, "message closed");
    CHECK(strcmp(buffer, "{\"s\":0,\"e\":[[64,1,\"41\"],[80,8,\"\"]]}") == 0,
          buffer);
    return 0;
}

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
        test_null_guards() != 0 ||
        test_frame_round_trip() != 0) {
        return 1;
    }
    printf("la_web_core: all tests passed\n");
    return 0;
}
