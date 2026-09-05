#include "la_decode.h"
#include "la_proto.h"
#include "la_regs.h"
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
    la_proto_config_t config;
    la_proto_config_t unpacked_config;
    la_proto_decoder_config_t decoder_config;
    la_proto_decoder_config_t unpacked_decoder;
    uint8_t typed_payload[LA_PROTO_CONFIG_WIRE_SIZE];
    uint8_t decoder_payload[LA_PROTO_DECODER_CONFIG_WIRE_SIZE];

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

    frame_length = la_proto_encode(0x01u, 0x18u, NULL, 0u, frame, sizeof(frame));
    CHECK(frame_length == LA_PROTO_FRAME_OVERHEAD, "empty protocol frame");
    memset(&observer, 0, sizeof(observer));
    la_proto_parser_init(&parser);
    CHECK(la_proto_parser_feed(&parser, frame, frame_length, on_frame, &observer) == 1u &&
              observer.length == 0u,
          "empty frame parser");
    for (index = 0u; index < LA_PROTO_MAX_PAYLOAD; ++index) {
        corrupted[index] = (uint8_t)(index ^ 0xA5u);
    }
    frame_length = la_proto_encode(0x31u, 0x19u, corrupted, LA_PROTO_MAX_PAYLOAD, frame,
                                   sizeof(frame));
    CHECK(frame_length == LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD,
          "maximum protocol frame");
    memset(&observer, 0, sizeof(observer));
    la_proto_parser_init(&parser);
    CHECK(la_proto_parser_feed(&parser, frame, frame_length, on_frame, &observer) == 1u &&
              observer.length == LA_PROTO_MAX_PAYLOAD && observer.payload[0] == 0xA5u &&
              observer.payload[LA_PROTO_MAX_PAYLOAD - 1u] == corrupted[LA_PROTO_MAX_PAYLOAD - 1u],
          "maximum frame parser");

    {
        static const uint8_t malformed_length[] = {LA_PROTO_SOF, 0x10u, 0x01u, 0x01u, 0x04u};

        la_proto_parser_init(&parser);
        memset(&observer, 0, sizeof(observer));
        (void)la_proto_parser_feed(&parser, malformed_length, sizeof(malformed_length), on_frame, &observer);
        (void)la_proto_parser_feed(&parser, frame, frame_length, on_frame, &observer);
        CHECK(observer.count == 1u && parser.format_errors == 1u,
              "parser rejects invalid payload length");
    }

    memcpy(corrupted, frame, frame_length);
    corrupted[frame_length - 1u] ^= 0x01u;
    memset(&observer, 0, sizeof(observer));
    la_proto_parser_init(&parser);
    (void)la_proto_parser_feed(&parser, (const uint8_t *)"garbage", 7u, on_frame, &observer);
    (void)la_proto_parser_feed(&parser, corrupted, frame_length, on_frame, &observer);
    (void)la_proto_parser_feed(&parser, frame, frame_length, on_frame, &observer);
    CHECK(observer.count == 1u, "parser resynchronizes after bad CRC");
    CHECK(parser.crc_errors == 1u && parser.dropped_bytes == 7u, "parser error counters");

    CHECK(la_proto_message_type_valid(LA_PROTO_DATA_CHUNK) != 0 &&
              la_proto_message_type_valid(0x03u) == 0,
          "protocol message type validation");
    memset(&config, 0, sizeof(config));
    config.ctrl_flags = LA_REG_CTRL_STREAM_MASK;
    config.sample_div = 9u;
    config.mode = LA_REG_MODE_ENCODING_MASK;
    config.chan_en = 0xFFu;
    config.trig_mask = 0x03u;
    config.trig_value = 0x01u;
    config.trig_rise = 0x02u;
    config.trig_fall = 0x04u;
    config.pre_samples = 1024u;
    config.post_samples = 8192u;
    config.timeout = 500000u;
    config.txn_max_run = 0x00FFFFFFu;
    config.live_flush_cycles = 200000u;
    CHECK(la_proto_pack_config(&config, typed_payload, sizeof(typed_payload)) ==
              LA_PROTO_CONFIG_WIRE_SIZE,
          "CONFIG payload pack");
    CHECK(la_proto_unpack_config(&unpacked_config, typed_payload, sizeof(typed_payload)) == 0,
          "CONFIG payload unpack");
    CHECK(unpacked_config.ctrl_flags == config.ctrl_flags &&
              unpacked_config.sample_div == config.sample_div &&
              unpacked_config.mode == config.mode && unpacked_config.pre_samples == config.pre_samples &&
              unpacked_config.txn_max_run == config.txn_max_run &&
              unpacked_config.live_flush_cycles == config.live_flush_cycles,
          "CONFIG payload round trip");
    config.live_flush_cycles = 0u;
    CHECK(la_proto_validate_config(&config) != 0 &&
              la_proto_pack_config(&config, typed_payload, sizeof(typed_payload)) == 0u,
          "CONFIG rejects zero stream flush");

    memset(&decoder_config, 0, sizeof(decoder_config));
    decoder_config.slot = 1u;
    decoder_config.type = LA_PROTO_DECODER_SPI;
    decoder_config.params.spi.sck_channel = 0;
    decoder_config.params.spi.mosi_channel = 1;
    decoder_config.params.spi.miso_channel = -1;
    decoder_config.params.spi.cs_channel = 2;
    decoder_config.params.spi.cpol = 1u;
    decoder_config.params.spi.cpha = 0u;
    decoder_config.params.spi.word_size = 16u;
    decoder_config.params.spi.lsb_first = 0u;
    decoder_config.params.spi.cs_active = 0u;
    CHECK(la_proto_pack_decoder_config(&decoder_config, decoder_payload, sizeof(decoder_payload)) ==
              LA_PROTO_DECODER_CONFIG_WIRE_SIZE,
          "DECODER_CFG payload pack");
    CHECK(la_proto_unpack_decoder_config(&unpacked_decoder, decoder_payload, sizeof(decoder_payload)) == 0,
          "DECODER_CFG payload unpack");
    CHECK(unpacked_decoder.params.spi.sck_channel == 0 &&
              unpacked_decoder.params.spi.mosi_channel == 1 &&
              unpacked_decoder.params.spi.miso_channel == -1 &&
              unpacked_decoder.params.spi.word_size == 16u,
          "DECODER_CFG payload round trip");
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

typedef struct event_log {
    unsigned count;
    la_decode_event_t events[32];
} event_log_t;

static void on_event_log(void *context, const la_decode_event_t *event)
{
    event_log_t *log = (event_log_t *)context;

    if (log->count < (unsigned)(sizeof(log->events) / sizeof(log->events[0]))) {
        log->events[log->count] = *event;
    }
    ++log->count;
}

static int test_spi_decoder(void)
{
    la_spi_decoder_t decoder;
    event_log_t log;
    la_transition_t transition;
    uint8_t sample = 4u;
    uint8_t value = 0xA5u;
    unsigned bit;
    uint64_t time = 0u;

    CHECK(la_spi_decoder_init(&decoder, 0, 1, -1, 2, 0u, 0u, 8u, 0u, 0u) == 0,
          "SPI decoder initialization");
    memset(&log, 0, sizeof(log));
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
          "SPI initial levels");
    sample = 0u;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 1,
          "SPI chip-select assertion");
    for (bit = 0u; bit < 8u; ++bit) {
        uint8_t data_bit = (uint8_t)((value >> (7u - bit)) & 1u);

        sample = (uint8_t)((sample & 0xFDu) | (uint8_t)(data_bit << 1));
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
              "SPI data setup");
        sample |= 1u;
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) ==
                  (bit == 7u ? 1 : 0),
              "SPI sampling edge");
        sample &= 0xFEu;
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
              "SPI trailing edge");
    }
    sample |= 4u;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_spi_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 1,
          "SPI chip-select release");
    CHECK(log.count == 3u && log.events[0].kind == LA_DECODE_EVENT_CS_ASSERT &&
              log.events[1].kind == LA_DECODE_EVENT_BYTE && log.events[1].length == 1u &&
              log.events[1].data[0] == value && (log.events[1].flags & LA_DECODE_EVENT_FLAG_MOSI) != 0u &&
              log.events[2].kind == LA_DECODE_EVENT_CS_RELEASE,
          "SPI event sequence");
    return 0;
}

static int test_i2c_decoder(void)
{
    la_i2c_decoder_t decoder;
    event_log_t log;
    la_transition_t transition;
    uint8_t sample = 3u;
    uint8_t values[] = {0x54u, 0xA5u};
    unsigned value_index;
    unsigned bit;
    uint64_t time = 0u;

    CHECK(la_i2c_decoder_init(&decoder, 0, 1) == 0, "I2C decoder initialization");
    memset(&log, 0, sizeof(log));
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
          "I2C initial levels");
    sample = 1u;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 1,
          "I2C start");
    for (value_index = 0u; value_index < 2u; ++value_index) {
        for (bit = 0u; bit < 8u; ++bit) {
            uint8_t data_bit = (uint8_t)((values[value_index] >> (7u - bit)) & 1u);

            sample &= 0xFEu;
            transition.t_sample = time++;
            transition.sample = sample;
            CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
                  "I2C clock low");
            sample = (uint8_t)((sample & 0xFDu) | (uint8_t)(data_bit << 1));
            transition.t_sample = time++;
            transition.sample = sample;
            CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
                  "I2C data setup");
            sample |= 1u;
            transition.t_sample = time++;
            transition.sample = sample;
            CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) ==
                      (bit == 7u ? 1 : 0),
                  "I2C data edge");
        }
        sample &= 0xFEu;
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
              "I2C acknowledge setup");
        sample = (uint8_t)(sample & 0xFDu);
        if (value_index == 1u) {
            sample |= 2u;
        }
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
              "I2C acknowledge data");
        sample |= 1u;
        transition.t_sample = time++;
        transition.sample = sample;
        CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 1,
              "I2C acknowledge edge");
    }
    sample &= 0xFEu;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
          "I2C stop setup");
    sample &= 0xFDu;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
          "I2C stop data");
    sample |= 1u;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 0,
          "I2C stop clock");
    sample |= 2u;
    transition.t_sample = time++;
    transition.sample = sample;
    CHECK(la_i2c_decoder_feed_transition(&decoder, &transition, on_event_log, &log) == 1,
          "I2C stop");
    CHECK(log.count == 6u && log.events[0].kind == LA_DECODE_EVENT_START &&
              log.events[1].kind == LA_DECODE_EVENT_ADDRESS && log.events[1].data[0] == 0x2Au &&
              log.events[2].kind == LA_DECODE_EVENT_ACK && log.events[3].kind == LA_DECODE_EVENT_BYTE &&
              log.events[3].data[0] == 0xA5u && log.events[4].kind == LA_DECODE_EVENT_NACK &&
              log.events[5].kind == LA_DECODE_EVENT_STOP,
          "I2C event sequence");
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
    if (test_protocol() != 0 || test_uart_decoder() != 0 || test_spi_decoder() != 0 ||
        test_i2c_decoder() != 0 || test_segment_pool() != 0) {
        return 1;
    }
    puts("PASS: la_native_tests");
    return 0;
}
