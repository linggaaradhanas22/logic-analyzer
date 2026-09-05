#include "la_decode.h"
#include "la_proto.h"
#include "segment_pool.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#endif

#define CHECK(condition, message)             \
    do {                                      \
        if (!(condition)) {                   \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                         \
        }                                       \
    } while (0)

typedef struct frame_observer {
    unsigned count;
    int bad;
    uint8_t type;
    uint8_t sequence;
    uint16_t length;
    uint8_t payload[LA_PROTO_MAX_PAYLOAD];
} frame_observer_t;

static void on_frame(void *context,
                     uint8_t type,
                     uint8_t sequence,
                     const uint8_t *payload,
                     uint16_t length)
{
    frame_observer_t *observer = (frame_observer_t *)context;

    ++observer->count;
    observer->type = type;
    observer->sequence = sequence;
    observer->length = length;
    if (length != 0u) {
        memcpy(observer->payload, payload, length);
    }
}

static int test_protocol(void)
{
    static const uint8_t payload[] = {0x00u, 0xA5u, 0x7Fu, 0xFFu};
    uint8_t frame[LA_PROTO_MAX_PAYLOAD + 7u];
    uint8_t corrupted[LA_PROTO_MAX_PAYLOAD + 7u];
    la_proto_parser_t parser;
    frame_observer_t observer;
    size_t frame_length;
    size_t index;

    CHECK(la_crc16_ccitt((const uint8_t *)"123456789", 9u) == 0x29B1u,
          "CRC16-CCITT test vector");
    CHECK(la_crc32_ieee((const uint8_t *)"123456789", 9u) == 0xCBF43926u,
          "CRC32-IEEE test vector");
    frame_length = la_proto_encode(0x22u, 0x17u, payload, (uint16_t)sizeof(payload), frame,
                                   sizeof(frame));
    CHECK(frame_length == sizeof(payload) + 7u, "protocol frame length");

    memset(&observer, 0, sizeof(observer));
    la_proto_parser_init(&parser);
    for (index = 0u; index < frame_length; ++index) {
        (void)la_proto_parser_feed(&parser, &frame[index], 1u, on_frame, &observer);
    }
    CHECK(observer.count == 1u, "incremental parser frame callback");
    CHECK(observer.type == 0x22u && observer.sequence == 0x17u, "frame header round trip");
    CHECK(observer.length == sizeof(payload) && memcmp(observer.payload, payload, sizeof(payload)) == 0,
          "frame payload round trip");

    memcpy(corrupted, frame, frame_length);
    corrupted[frame_length - 1u] ^= 0x01u;
    memset(&observer, 0, sizeof(observer));
    la_proto_parser_init(&parser);
    (void)la_proto_parser_feed(&parser, (const uint8_t *)"garbage", 7u, on_frame, &observer);
    (void)la_proto_parser_feed(&parser, corrupted, frame_length, on_frame, &observer);
    (void)la_proto_parser_feed(&parser, frame, frame_length, on_frame, &observer);
    CHECK(observer.count == 1u, "parser resynchronizes after bad CRC");
    CHECK(parser.crc_errors == 1u && parser.dropped_bytes == 7u, "parser error counters");
    return 0;
}

typedef struct decode_observer {
    unsigned count;
    unsigned byte_count;
    unsigned error_count;
    uint8_t value;
    uint64_t timestamp;
} decode_observer_t;

static void on_decode_event(void *context, const la_decode_event_t *event)
{
    decode_observer_t *observer = (decode_observer_t *)context;

    ++observer->count;
    if (event->kind == LA_DECODE_EVENT_BYTE) {
        ++observer->byte_count;
        observer->value = event->data[0];
        observer->timestamp = event->t_sample;
    } else if (event->kind == LA_DECODE_EVENT_FRAME_ERROR ||
               event->kind == LA_DECODE_EVENT_PARITY_ERROR) {
        ++observer->error_count;
    }
}

static int test_uart_decoder(void)
{
    la_uart_decoder_t decoder;
    decode_observer_t observer;
    la_transition_t transition;
    uint8_t current = 1u;
    uint8_t value = 0xA5u;
    unsigned bit;
    uint64_t start = 100u;

    CHECK(la_uart_decoder_init(&decoder, 0u, 8u, 8u, LA_UART_PARITY_NONE, 1u, 0u) == 0,
          "UART decoder initialization");
    memset(&observer, 0, sizeof(observer));
    transition.t_sample = start;
    transition.sample = 0u;
    CHECK(la_uart_decoder_feed_transition(&decoder, &transition, on_decode_event, &observer) == 0,
          "UART start transition");
    current = 0u;
    for (bit = 0u; bit < 8u; ++bit) {
        uint8_t next = (uint8_t)((value >> bit) & 1u);
        transition.t_sample = start + 8u * (uint64_t)(bit + 1u);
        transition.sample = next;
        if (next != current) {
            CHECK(la_uart_decoder_feed_transition(&decoder, &transition, on_decode_event, &observer) >= 0,
                  "UART data transition");
            current = next;
        }
    }
    transition.t_sample = start + 8u * 9u;
    transition.sample = 1u;
    if (current != 1u) {
        CHECK(la_uart_decoder_feed_transition(&decoder, &transition, on_decode_event, &observer) >= 0,
              "UART stop transition");
    }
    CHECK(la_uart_decoder_advance(&decoder, start + 8u * 12u, on_decode_event, &observer) >= 0,
          "UART final advance");
    CHECK(observer.byte_count == 1u && observer.value == value && observer.timestamp == start,
          "UART byte decode");
    CHECK(observer.error_count == 0u, "UART clean frame");
    transition.t_sample = start - 1u;
    transition.sample = 1u;
    CHECK(la_uart_decoder_feed_transition(&decoder, &transition, on_decode_event, &observer) < 0,
          "UART rejects non-monotonic timestamp");
    return 0;
}

#if defined(_WIN32)
typedef struct release_context {
    la_segment_pool_t *pool;
    la_segment_handle_t handle;
    int result;
} release_context_t;

static unsigned __stdcall release_thread(void *argument)
{
    release_context_t *context = (release_context_t *)argument;
    context->result = la_segment_pool_release(context->pool, context->handle);
    return 0u;
}
#endif

static int test_segment_pool(void)
{
    la_segment_pool_t pool;
    la_segment_handle_t handles[LA_SEGMENT_POOL_CAPACITY];
    la_segment_handle_t published;
    la_segment_state_t state;
    uint32_t references;
    uint32_t sequence;
    uint32_t length;
    unsigned index;

    CHECK(la_segment_pool_init(&pool, 256u) == 0, "segment pool initialization");
    for (index = 0u; index < LA_SEGMENT_POOL_CAPACITY; ++index) {
        CHECK(la_segment_pool_acquire(&pool, &handles[index]) == 0, "segment acquisition");
    }
    CHECK(la_segment_pool_acquire(&pool, &published) == -2, "segment pool full policy");
    published = handles[0];
    CHECK(la_segment_pool_publish(&pool, published, 42u, 3u) == 0, "segment publication");
    CHECK(la_segment_pool_snapshot(&pool, published, &state, &references, &sequence, &length) == 0,
          "segment snapshot");
    CHECK(state == LA_SEGMENT_CPU_OWNED && references == 3u && length == 42u && sequence != 0u,
          "published segment ownership");
    CHECK(la_segment_pool_abort(&pool, published) == -3, "consumer ownership blocks abort");

#if defined(_WIN32)
    {
        HANDLE threads[3];
        release_context_t contexts[3];
        unsigned thread_index;

        for (thread_index = 0u; thread_index < 3u; ++thread_index) {
            contexts[thread_index].pool = &pool;
            contexts[thread_index].handle = published;
            contexts[thread_index].result = -99;
            threads[thread_index] = (HANDLE)_beginthreadex(NULL, 0u, release_thread,
                                                            &contexts[thread_index], 0u, NULL);
            CHECK(threads[thread_index] != NULL, "release thread creation");
        }
        CHECK(WaitForMultipleObjects(3u, threads, TRUE, 2000u) == WAIT_OBJECT_0,
              "release thread completion");
        for (thread_index = 0u; thread_index < 3u; ++thread_index) {
            CHECK(contexts[thread_index].result == 0, "concurrent reference release");
            CloseHandle(threads[thread_index]);
        }
    }
#else
    for (index = 0u; index < 3u; ++index) {
        CHECK(la_segment_pool_release(&pool, published) == 0, "reference release");
    }
#endif

    CHECK(la_segment_pool_snapshot(&pool, published, &state, &references, &sequence, &length) == -2,
          "released handle cannot be reused by consumers");
    for (index = 1u; index < LA_SEGMENT_POOL_CAPACITY; ++index) {
        CHECK(la_segment_pool_abort(&pool, handles[index]) == 0, "DMA abort release");
    }
    CHECK(la_segment_pool_acquire(&pool, &published) == 0, "released slot becomes available");
    CHECK(la_segment_pool_release(&pool, published) == -2, "DMA handle cannot be consumer released");
    CHECK(la_segment_pool_abort(&pool, published) == 0, "final segment abort");
    la_segment_pool_destroy(&pool);
    return 0;
}

int main(void)
{
    if (test_protocol() != 0 || test_uart_decoder() != 0 || test_segment_pool() != 0) {
        return 1;
    }
    puts("PASS: la_native_tests");
    return 0;
}
