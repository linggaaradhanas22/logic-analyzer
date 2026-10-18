#include "la_decoder_core.h"
#include "la_event_batch.h"
#include "la_live_ring.h"
#include "la_segment_pool.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

#define LIVE_BASE UINT32_C(0x3F000000)
#define SEGMENT_BYTES LA_LIVE_SEGMENT_BYTES
#define TEST_SAMPLES 8192u

typedef struct sample_buf {
    uint8_t data[TEST_SAMPLES];
    uint32_t count;
} sample_buf_t;

typedef struct event_log {
    la_decode_event_t events[128];
    uint32_t count;
} event_log_t;

static void log_event(void *context, const la_decode_event_t *event)
{
    event_log_t *log = (event_log_t *)context;

    if (log->count < (uint32_t)(sizeof(log->events) / sizeof(log->events[0]))) {
        log->events[log->count] = *event;
    }
    log->count++;
}

static void push_run(sample_buf_t *buffer, uint8_t value, uint32_t count)
{
    uint32_t index;

    for (index = 0u; index < count; ++index) {
        if (buffer->count < (uint32_t)sizeof(buffer->data)) {
            buffer->data[buffer->count++] = value;
        }
    }
}

static uint32_t samples_to_words(const sample_buf_t *buffer, uint32_t *words,
                                 uint32_t max_words)
{
    uint32_t index = 0u;
    uint32_t count = 0u;

    while (index < buffer->count && count < max_words) {
        uint32_t run = 1u;

        while (index + run < buffer->count &&
               buffer->data[index + run] == buffer->data[index] &&
               run < 0x00FFFFFFu) {
            ++run;
        }
        words[count++] = (run << 8) | (uint32_t)buffer->data[index];
        index += run;
    }
    return count;
}

/* ---------------------------------------------------------------- pool/ring */

static int test_live_ring(void)
{
    la_segment_pool_t pool;
    la_live_ring_t ring;
    uint32_t slot = 0u;
    uint32_t index;
    la_segment_state_t state = LA_SEGMENT_FREE;
    uint32_t references = 0u;
    uint32_t sequence = 0u;
    uint32_t length = 0u;

    CHECK(la_segment_pool_init(&pool, SEGMENT_BYTES) == 0, "pool init");
    CHECK(la_live_ring_init(NULL, &pool, LIVE_BASE, 16u, SEGMENT_BYTES) == -1,
          "ring init NULL rejected");
    CHECK(la_live_ring_init(&ring, &pool, LIVE_BASE, 16u, 0u) == -1,
          "ring init zero segment rejected");
    CHECK(la_live_ring_init(&ring, &pool, LIVE_BASE, 17u, SEGMENT_BYTES) == -1,
          "ring init over capacity rejected");
    CHECK(la_live_ring_init(&ring, &pool, LIVE_BASE, 16u, SEGMENT_BYTES) == 0,
          "ring init");
    CHECK(la_live_ring_slot_address(&ring, 0u) == LIVE_BASE, "slot 0 address");
    CHECK(la_live_ring_slot_address(&ring, 3u) == LIVE_BASE + 3u * SEGMENT_BYTES,
          "slot 3 address");
    CHECK(la_live_ring_slot_state(&ring, 5u) == LA_LIVE_SLOT_FREE, "slot starts free");
    CHECK(la_live_ring_complete(&ring, 0u, 100u, 0u) == -2,
          "complete on a free slot rejected");
    CHECK(la_live_ring_release(&ring, 0u) == -2, "release on a free slot rejected");

    CHECK(la_live_ring_start(&ring) == 16, "start posts every slot");
    CHECK(la_live_ring_dma_posted(&ring) == 16u, "posted count");
    for (index = 0u; index < 16u; ++index) {
        CHECK(la_live_ring_slot_state(&ring, index) == LA_LIVE_SLOT_DMA,
              "started slot is DMA-owned");
        CHECK(la_segment_pool_snapshot(&pool, ring.slots[index].handle, &state,
                                       &references, &sequence, &length) == 0 &&
                  state == LA_SEGMENT_DMA_OWNED && sequence == index + 1u,
              "pool mirrors the ring slots");
    }

    CHECK(la_live_ring_complete(&ring, 4u, 4096u, 0u) == 0, "zero-consumer completion");
    CHECK(la_live_ring_slot_state(&ring, 4u) == LA_LIVE_SLOT_FREE,
          "zero-consumer slot recycles immediately");
    CHECK(la_live_ring_dma_posted(&ring) == 15u, "posted count after completion");
    CHECK(la_live_ring_repost(&ring, &slot) == 4 && slot == 4u, "repost lowest free slot");
    CHECK(la_live_ring_dma_posted(&ring) == 16u, "posted count after repost");

    CHECK(la_live_ring_complete(&ring, 5u, 2048u, 2u) == 0, "two-consumer completion");
    CHECK(la_live_ring_slot_state(&ring, 5u) == LA_LIVE_SLOT_CPU, "published slot is CPU");
    CHECK(la_live_ring_release(&ring, 5u) == 0, "first consumer release");
    CHECK(la_live_ring_slot_state(&ring, 5u) == LA_LIVE_SLOT_CPU,
          "one reference keeps the slot CPU-owned");
    CHECK(la_live_ring_release(&ring, 5u) == 0, "second consumer release");
    CHECK(la_live_ring_slot_state(&ring, 5u) == LA_LIVE_SLOT_FREE,
          "last reference frees the slot");
    CHECK(la_live_ring_repost(&ring, &slot) == 5 && slot == 5u,
          "released slot reposts");

    /* Pool exhaustion: every slot published with one consumer cannot repost. */
    for (index = 0u; index < 16u; ++index) {
        if (la_live_ring_slot_state(&ring, index) == LA_LIVE_SLOT_DMA) {
            CHECK(la_live_ring_complete(&ring, index, 1024u, 1u) == 0,
                  "fill remaining slots");
        }
    }
    CHECK(la_live_ring_repost(&ring, &slot) == -1, "pool exhaustion detected");
    CHECK(la_live_ring_abort(&ring) == 16, "abort reports every CPU-owned slot");
    CHECK(la_live_ring_dma_posted(&ring) == 0u, "no DMA slots remain");
    for (index = 0u; index < 16u; ++index) {
        CHECK(la_live_ring_release(&ring, index) == 0, "drain published slots");
    }
    CHECK(la_live_ring_abort(&ring) == 0, "abort after drain is clean");
    CHECK(la_live_ring_start(&ring) == 16, "restart after drain");

    /* Abort frees DMA slots without touching consumers. */
    CHECK(la_live_ring_complete(&ring, 0u, 512u, 1u) == 0, "publish slot 0");
    CHECK(la_live_ring_abort(&ring) == 1, "abort keeps the CPU-owned slot");
    CHECK(la_live_ring_slot_state(&ring, 0u) == LA_LIVE_SLOT_CPU, "slot 0 still CPU");
    CHECK(la_live_ring_dma_posted(&ring) == 0u, "DMA slots aborted");
    CHECK(la_live_ring_release(&ring, 0u) == 0, "release slot 0");
    CHECK(la_live_ring_start(&ring) == 16, "restart after mixed abort");

    la_segment_pool_destroy(&pool);
    return 0;
}

/* ------------------------------------------------------------- decoder core */

static void build_uart_text(sample_buf_t *buffer, const char *text, uint32_t count)
{
    uint32_t index;

    for (index = 0u; index < count; ++index) {
        uint8_t value = (uint8_t)text[index];
        uint32_t bit;

        push_run(buffer, 0u, 4u); /* start bit */
        for (bit = 0u; bit < 8u; ++bit) {
            push_run(buffer, (uint8_t)((value >> bit) & 1u), 4u);
        }
        push_run(buffer, 1u, 8u); /* stop + idle */
    }
}

static int test_decoder_core_uart(void)
{
    la_decoder_core_t core;
    la_proto_decoder_config_t config;
    sample_buf_t buffer;
    sample_buf_t head;
    sample_buf_t tail;
    uint32_t words[TEST_SAMPLES];
    uint32_t head_words[TEST_SAMPLES];
    uint32_t tail_words[TEST_SAMPLES];
    uint32_t word_count;
    uint32_t head_count;
    uint32_t tail_count;
    event_log_t log;
    uint32_t index;
    const char *text = "LA-TEST\n";
    uint32_t split;

    memset(&buffer, 0, sizeof(buffer));
    build_uart_text(&buffer, text, 8u);
    word_count = samples_to_words(&buffer, words,
                                  (uint32_t)(sizeof(words) / sizeof(words[0])));

    memset(&head, 0, sizeof(head));
    memset(&tail, 0, sizeof(tail));
    build_uart_text(&head, text, 3u);
    build_uart_text(&tail, text + 3u, 5u);
    head_count = samples_to_words(&head, head_words,
                                  (uint32_t)(sizeof(head_words) / sizeof(head_words[0])));
    tail_count = samples_to_words(&tail, tail_words,
                                  (uint32_t)(sizeof(tail_words) / sizeof(tail_words[0])));

    la_decoder_core_init(&core);
    memset(&config, 0, sizeof(config));
    config.slot = 0u;
    config.type = LA_PROTO_DECODER_UART;
    config.params.uart.rx_channel = 0u;
    config.params.uart.data_bits = 8u;
    config.params.uart.parity = LA_UART_PARITY_NONE;
    config.params.uart.stop_bits = 1u;
    config.params.uart.inverted = 0u;
    config.params.uart.baud_hz = 100u;
    config.params.uart.sample_rate_hz = 400u;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "UART decoder configured");
    CHECK(la_decoder_core_enabled(&core) == 1, "UART decoder enabled");
    CHECK(la_decoder_core_config(&core)->params.uart.baud_hz == 100u,
          "decoder config latched");

    /* Split at a word boundary inside the stream: the sample clock and the
     * decoder state must continue across segments with consecutive
     * sequences. */
    split = word_count / 2u;
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, split, 1u, log_event, &log) >= 0,
          "feed first segment");
    CHECK(la_decoder_core_feed_segment(&core, words + split, word_count - split, 2u,
                                       log_event, &log) >= 0,
          "feed second segment");
    CHECK(log.count == 8u, "eight UART bytes decoded across segments");
    for (index = 0u; index < 8u; ++index) {
        CHECK(log.events[index].kind == LA_DECODE_EVENT_BYTE &&
                  log.events[index].length == 1u &&
                  log.events[index].data[0] == (uint8_t)text[index],
              "UART byte value and kind");
        /* Each frame is 4 samples/bit * 11 bits = 44 samples. */
        CHECK(log.events[index].t_sample == (uint64_t)index * 44u,
              "UART byte timestamp tracks the sample clock");
    }
    CHECK(la_decoder_core_stats(&core)->events == 8u, "UART event counter");
    CHECK(la_decoder_core_stats(&core)->dropped_segments == 0u,
          "no UART gaps on consecutive sequences");

    /* Sequence gap at a frame boundary: DISCONTINUITY, reset, then clean
     * decoding resumes. */
    la_decoder_core_init(&core);
    CHECK(la_decoder_core_configure(&core, &config) == 0, "reconfigure UART");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, head_words, head_count, 1u,
                                       log_event, &log) >= 0,
          "feed before the gap");
    CHECK(log.count == 3u, "three bytes before the gap");
    CHECK(la_decoder_core_feed_segment(&core, tail_words, tail_count, 3u,
                                       log_event, &log) >= 0,
          "feed across the gap");
    CHECK(log.count == 3u + 1u + 5u, "gap adds one discontinuity event");
    CHECK(log.events[3].kind == LA_DECODE_EVENT_DISCONTINUITY,
          "gap emits DISCONTINUITY after the last pre-gap byte");
    CHECK(log.events[4].data[0] == (uint8_t)text[3] &&
              log.events[8].data[0] == (uint8_t)text[7],
          "post-gap bytes decode in order");
    CHECK(la_decoder_core_stats(&core)->dropped_segments == 1u, "gap counter");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, word_count, 4u, log_event, &log) >= 0,
          "feed after the gap");
    CHECK(log.count == 8u, "decoding resumes after a gap");
    CHECK(la_decoder_core_configure(&core, &config) == 0, "configure mid-stream slot");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment_at(&core, words, word_count, 100u,
          UINT64_C(1000000000), log_event, &log) >= 0 && log.count == 8u,
          "mid-stream slot decodes at acquisition epoch");
    CHECK(log.events[0].t_sample == UINT64_C(1000000000) &&
          log.events[7].t_sample == UINT64_C(1000000000) + 7u * 44u,
          "decoded events align with absolute waveform samples");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment_at(&core, words, word_count, 102u,
          UINT64_C(2000000000), log_event, &log) >= 0 && log.count == 9u &&
          log.events[0].kind == LA_DECODE_EVENT_DISCONTINUITY &&
          log.events[1].t_sample == UINT64_C(2000000000),
          "queue gap is marked without losing waveform time alignment");
    return 0;
}

static int test_decoder_core_spi(void)
{
    la_decoder_core_t core;
    la_proto_decoder_config_t config;
    sample_buf_t buffer;
    uint32_t words[TEST_SAMPLES];
    uint32_t word_count;
    event_log_t log;
    uint32_t index;
    uint8_t bytes[2];
    uint32_t byte_index;

    bytes[0] = 0x4Cu;
    bytes[1] = 0x41u;
    memset(&buffer, 0, sizeof(buffer));
    push_run(&buffer, 0x04u, 2u); /* idle: CS high */
    push_run(&buffer, 0x00u, 1u); /* CS assert */
    for (byte_index = 0u; byte_index < 2u; ++byte_index) {
        uint32_t bit;

        for (bit = 0u; bit < 8u; ++bit) {
            uint8_t mosi = (uint8_t)((bytes[byte_index] >> (7u - bit)) & 1u);

            push_run(&buffer, (uint8_t)(mosi << 1), 1u);
            push_run(&buffer, (uint8_t)(0x01u | (mosi << 1)), 1u);
            push_run(&buffer, (uint8_t)(mosi << 1), 1u);
        }
    }
    push_run(&buffer, 0x04u, 1u); /* CS release */
    push_run(&buffer, 0x04u, 2u);
    word_count = samples_to_words(&buffer, words,
                                  (uint32_t)(sizeof(words) / sizeof(words[0])));

    la_decoder_core_init(&core);
    memset(&config, 0, sizeof(config));
    config.slot = 1u;
    config.type = LA_PROTO_DECODER_SPI;
    config.params.spi.sck_channel = 0;
    config.params.spi.mosi_channel = 1;
    config.params.spi.miso_channel = -1;
    config.params.spi.cs_channel = 2;
    config.params.spi.cpol = 0u;
    config.params.spi.cpha = 0u;
    config.params.spi.word_size = 8u;
    config.params.spi.lsb_first = 0u;
    config.params.spi.cs_active = 0u;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "SPI decoder configured");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, word_count, 1u, log_event, &log) >= 0,
          "feed SPI segment");
    CHECK(log.count == 4u, "SPI emits CS assert, two bytes and CS release");
    CHECK(log.events[0].kind == LA_DECODE_EVENT_CS_ASSERT, "SPI CS assert first");
    CHECK(log.events[1].kind == LA_DECODE_EVENT_BYTE &&
              log.events[1].flags == LA_DECODE_EVENT_FLAG_MOSI &&
              log.events[1].data[0] == 0x4Cu,
          "SPI MOSI byte 0x4C");
    CHECK(log.events[2].kind == LA_DECODE_EVENT_BYTE && log.events[2].data[0] == 0x41u,
          "SPI MOSI byte 0x41");
    CHECK(log.events[3].kind == LA_DECODE_EVENT_CS_RELEASE, "SPI CS release last");
    for (index = 0u; index < log.count; ++index) {
        CHECK(log.events[index].channel != LA_DECODE_NO_CHANNEL ||
                  log.events[index].kind == LA_DECODE_EVENT_CS_RELEASE,
              "SPI event channels");
    }
    return 0;
}

static int test_decoder_core_i2c(void)
{
    la_decoder_core_t core;
    la_proto_decoder_config_t config;
    sample_buf_t buffer;
    uint32_t words[TEST_SAMPLES];
    uint32_t word_count;
    event_log_t log;
    uint8_t address = 0xA0u;
    uint32_t bit;

    memset(&buffer, 0, sizeof(buffer));
    push_run(&buffer, 0x18u, 2u); /* idle: SCL and SDA high */
    push_run(&buffer, 0x08u, 1u); /* START: SDA falls with SCL high */
    for (bit = 0u; bit < 8u; ++bit) {
        uint8_t sda = (uint8_t)((address >> (7u - bit)) & 1u);

        push_run(&buffer, (uint8_t)(sda << 4), 1u);
        push_run(&buffer, (uint8_t)(0x08u | (sda << 4)), 1u);
        push_run(&buffer, (uint8_t)(sda << 4), 1u);
    }
    push_run(&buffer, 0x00u, 1u); /* ACK: SDA low */
    push_run(&buffer, 0x08u, 1u); /* ACK clock high */
    push_run(&buffer, 0x00u, 1u);
    push_run(&buffer, 0x08u, 1u); /* SCL high, SDA low */
    push_run(&buffer, 0x18u, 1u); /* STOP: SDA rises with SCL high */
    word_count = samples_to_words(&buffer, words,
                                  (uint32_t)(sizeof(words) / sizeof(words[0])));

    la_decoder_core_init(&core);
    memset(&config, 0, sizeof(config));
    config.slot = 2u;
    config.type = LA_PROTO_DECODER_I2C;
    config.params.i2c.scl_channel = 3;
    config.params.i2c.sda_channel = 4;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "I2C decoder configured");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, word_count, 1u, log_event, &log) >= 0,
          "feed I2C segment");
    CHECK(log.count == 4u, "I2C emits START, ADDRESS, ACK and STOP");
    CHECK(log.events[0].kind == LA_DECODE_EVENT_START, "I2C START");
    CHECK(log.events[1].kind == LA_DECODE_EVENT_ADDRESS &&
              log.events[1].data[0] == 0x50u &&
              (log.events[1].flags & LA_DECODE_EVENT_FLAG_READ) == 0u,
          "I2C address 0x50 write");
    CHECK(log.events[2].kind == LA_DECODE_EVENT_ACK, "I2C ACK");
    CHECK(log.events[3].kind == LA_DECODE_EVENT_STOP, "I2C STOP");
    return 0;
}

static int test_decoder_core_guards(void)
{
    la_decoder_core_t core;
    la_proto_decoder_config_t config;
    uint32_t word = 0x00040001u;

    la_decoder_core_init(&core);
    CHECK(la_decoder_core_enabled(&core) == 0, "decoder starts disabled");
    CHECK(la_decoder_core_feed_segment(&core, &word, 1u, 1u, NULL, NULL) == -1,
          "disabled decoder rejects feeds");
    CHECK(la_decoder_core_configure(NULL, &config) == -1, "NULL core rejected");
    CHECK(la_decoder_core_configure(&core, NULL) == -1, "NULL config rejected");

    memset(&config, 0, sizeof(config));
    config.type = LA_PROTO_DECODER_UART;
    config.params.uart.data_bits = 8u;
    config.params.uart.stop_bits = 1u;
    config.params.uart.baud_hz = 100000000u;
    config.params.uart.sample_rate_hz = 100u;
    CHECK(la_decoder_core_configure(&core, &config) == -1,
          "UART sample rate below baud rejected");

    memset(&config, 0, sizeof(config));
    config.type = LA_PROTO_DECODER_SPI;
    config.params.spi.sck_channel = 0;
    config.params.spi.mosi_channel = -1;
    config.params.spi.miso_channel = -1;
    config.params.spi.cs_channel = -1;
    config.params.spi.word_size = 8u;
    CHECK(la_decoder_core_configure(&core, &config) == -1,
          "SPI without a data channel rejected");

    memset(&config, 0, sizeof(config));
    config.type = LA_PROTO_DECODER_I2C;
    config.params.i2c.scl_channel = 2;
    config.params.i2c.sda_channel = 2;
    CHECK(la_decoder_core_configure(&core, &config) == -1,
          "I2C with equal channels rejected");

    memset(&config, 0, sizeof(config));
    config.type = LA_PROTO_DECODER_OFF;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "OFF config accepted");
    CHECK(la_decoder_core_enabled(&core) == 0, "OFF leaves the decoder disabled");
    return 0;
}

static int test_decoder_core_stop_on(void)
{
    la_decoder_core_t core;
    la_proto_decoder_config_t config;
    sample_buf_t buffer;
    uint32_t words[TEST_SAMPLES];
    uint32_t word_count;
    event_log_t log;
    uint64_t match = 0u;
    uint32_t index;
    int hit_at = -1;

    memset(&buffer, 0, sizeof(buffer));
    build_uart_text(&buffer, "LA-TEST\n", 8u);
    word_count = samples_to_words(&buffer, words,
                                  (uint32_t)(sizeof(words) / sizeof(words[0])));

    la_decoder_core_init(&core);
    memset(&config, 0, sizeof(config));
    config.slot = 0u;
    config.type = LA_PROTO_DECODER_UART;
    config.params.uart.rx_channel = 0u;
    config.params.uart.data_bits = 8u;
    config.params.uart.parity = LA_UART_PARITY_NONE;
    config.params.uart.stop_bits = 1u;
    config.params.uart.inverted = 0u;
    config.params.uart.baud_hz = 100u;
    config.params.uart.sample_rate_hz = 400u;
    config.stop_on_len = 2u;
    config.stop_on[0] = 0x4Cu; /* 'L' */
    config.stop_on[1] = 0x41u; /* 'A' */
    CHECK(la_decoder_core_configure(&core, &config) == 0, "stop-on UART configured");
    CHECK(la_decoder_core_stop_on_hit(&core) == 0, "no hit before any feed");
    CHECK(la_decoder_core_stop_on_sample(&core, &match) == -1,
          "no match sample before any feed");

    /* Feed one transition word per segment: the rolling match must survive
     * segment boundaries (the pattern completes on the second byte). */
    memset(&log, 0, sizeof(log));
    for (index = 0u; index < word_count; ++index) {
        CHECK(la_decoder_core_feed_segment(&core, &words[index], 1u,
                                           (uint32_t)(index + 1u), log_event,
                                           &log) >= 0,
              "feed stop-on word");
        if (la_decoder_core_stop_on_hit(&core)) {
            hit_at = (int)index;
            break;
        }
    }
    CHECK(hit_at >= 0 && (uint32_t)hit_at < word_count, "stop-on pattern matched");
    CHECK(la_decoder_core_stop_on_sample(&core, &match) == 0 && match == 44u,
          "match sample is the second byte's timestamp");
    CHECK(log.count == 2u && log.events[0].data[0] == 0x4Cu &&
              log.events[1].kind == LA_DECODE_EVENT_BYTE &&
              log.events[1].data[0] == 0x41u,
          "matching bytes are still emitted in order");

    /* A pattern that never occurs leaves the hit clear. */
    la_decoder_core_init(&core);
    config.stop_on[0] = 0xFFu;
    config.stop_on[1] = 0xFEu;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "nonmatching pattern configured");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, word_count, 1u, log_event, &log) >= 0,
          "feed nonmatching stream");
    CHECK(la_decoder_core_stop_on_hit(&core) == 0, "no false stop-on hit");

    /* Reconfiguring clears a previous hit. */
    config.stop_on[0] = 0x4Cu;
    config.stop_on[1] = 0x41u;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "stop-on reconfigured");
    CHECK(la_decoder_core_stop_on_hit(&core) == 0, "reconfigure clears the hit");

    /* A single-byte pattern matches immediately (length 1 is valid). */
    config.stop_on_len = 1u;
    config.stop_on[0] = 0x54u; /* 'T' */
    config.stop_on[1] = 0u;
    CHECK(la_decoder_core_configure(&core, &config) == 0, "single-byte pattern configured");
    memset(&log, 0, sizeof(log));
    CHECK(la_decoder_core_feed_segment(&core, words, word_count, 1u, log_event, &log) >= 0,
          "feed single-byte pattern stream");
    CHECK(la_decoder_core_stop_on_hit(&core) == 1 &&
              la_decoder_core_stop_on_sample(&core, &match) == 0 && match == 132u,
          "single-byte pattern matches the fourth byte");
    return 0;
}

/* -------------------------------------------------------------- event batch */

static int test_event_batch(void)
{
    la_event_batch_t batch;
    la_decode_event_t event;
    uint16_t index;

    la_event_batch_init(&batch);
    CHECK(la_event_batch_empty(&batch) == 1, "batch starts empty");
    CHECK(la_event_batch_add(&batch, 0u, NULL) == -1, "NULL event rejected");

    memset(&event, 0, sizeof(event));
    event.kind = LA_DECODE_EVENT_BYTE;
    event.t_sample = UINT64_C(0x0102030405060708);
    event.length = 1u;
    event.data[0] = 0x4Cu;
    CHECK(la_event_batch_add(&batch, 2u, &event) == 1, "first record added");
    CHECK(la_event_batch_length(&batch) == 12u, "record length 11 + 1 data");
    CHECK(la_event_batch_records(&batch) == 1u, "record count");
    CHECK(batch.data[0] == 2u && batch.data[1] == LA_DECODE_EVENT_BYTE,
          "record slot and kind");
    CHECK(batch.data[2] == 0x08u && batch.data[9] == 0x01u,
          "record timestamp is little-endian");
    CHECK(batch.data[10] == 1u && batch.data[11] == 0x4Cu, "record data");

    event.length = 16u;
    for (index = 0u; index < 16u; ++index) {
        event.data[index] = (uint8_t)index;
    }
    while (la_event_batch_add(&batch, 1u, &event) == 1) {
        /* fill */
    }
    CHECK(la_event_batch_length(&batch) <= LA_EVENT_BATCH_CAPACITY,
          "batch never exceeds the payload cap");
    CHECK(la_event_batch_add(&batch, 1u, &event) == 0, "full batch drops whole records");
    CHECK(la_event_batch_length(&batch) <= LA_EVENT_BATCH_CAPACITY,
          "drop-newest leaves the batch unchanged");
    CHECK(la_event_batch_records(&batch) > 1u, "batch holds many records");

    event.length = LA_DECODE_MAX_EVENT_DATA + 1u;
    CHECK(la_event_batch_add(&batch, 0u, &event) == -1, "oversized event rejected");
    return 0;
}

static int test_event_block(void)
{
    la_event_batch_t batch;
    la_decode_event_t event;
    uint64_t last = 0u;
    uint16_t offset;

    la_event_batch_init_block(&batch, 2u);
    CHECK(batch.type == LA_PROTO_EVT_BLOCK, "block type");
    CHECK(batch.format == LA_PROTO_EVT_FORMAT_BLOCK, "block format");
    CHECK(la_event_batch_empty(&batch) == 1, "block starts empty");
    CHECK(la_event_batch_last_sample(&batch, &last) == -1, "empty block has no sample");

    memset(&event, 0, sizeof(event));
    event.kind = LA_DECODE_EVENT_BYTE;
    event.t_sample = 1000u;
    event.length = 1u;
    event.data[0] = 0x4Cu;
    CHECK(la_event_batch_add(&batch, 9u, &event) == 1, "first block record");
    CHECK(la_event_batch_length(&batch) ==
              LA_PROTO_EVT_BLOCK_HEADER_SIZE + 5u,
          "block header plus 5-byte record");
    CHECK(batch.data[0] == LA_PROTO_EVT_BLOCK_VERSION && batch.data[1] == 2u,
          "block version and header slot");
    CHECK(batch.data[2] == 1u && batch.data[3] == 0u, "block count 1");
    CHECK(batch.data[4] == 0xE8u && batch.data[5] == 0x03u, "t_base little-endian");
    CHECK(batch.data[12] == 0u && batch.data[13] == 0u, "first delta is zero");
    CHECK(batch.data[14] == LA_DECODE_EVENT_BYTE && batch.data[15] == 1u &&
              batch.data[16] == 0x4Cu,
          "first block record fields");

    event.t_sample = 1100u;
    CHECK(la_event_batch_add(&batch, 0u, &event) == 1, "second block record");
    CHECK(batch.data[2] == 2u, "block count 2");
    CHECK(batch.data[17] == 100u && batch.data[18] == 0u, "u16 delta 100");
    CHECK(la_event_batch_last_sample(&batch, &last) == 0 && last == 1100u,
          "block last sample");

    event.t_sample = 1100u + LA_PROTO_EVT_BLOCK_DELTA_ESCAPE;
    CHECK(la_event_batch_add(&batch, 0u, &event) == 1, "escape record added");
    offset = (uint16_t)(LA_PROTO_EVT_BLOCK_HEADER_SIZE + 5u + 5u);
    CHECK(batch.data[offset] == 0xFFu && batch.data[offset + 1u] == 0xFFu,
          "escape marker");
    CHECK(batch.data[offset + 2u] == (uint8_t)(event.t_sample & 0xFFu) &&
              batch.data[offset + 9u] == 0u,
          "escape absolute timestamp little-endian");
    CHECK(batch.data[offset + 10u] == LA_DECODE_EVENT_BYTE &&
              batch.data[offset + 11u] == 1u && batch.data[offset + 12u] == 0x4Cu,
          "escape record fields");
    CHECK(la_event_batch_last_sample(&batch, &last) == 0 && last == event.t_sample,
          "escape last sample");

    la_event_batch_init(&batch);
    event.t_sample = 42u;
    (void)la_event_batch_add(&batch, 1u, &event);
    event.t_sample = 77u;
    (void)la_event_batch_add(&batch, 1u, &event);
    CHECK(la_event_batch_last_sample(&batch, &last) == 0 && last == 77u,
          "records last sample");

    la_event_batch_init_block(&batch, 0u);
    event.length = 16u;
    while (la_event_batch_add(&batch, 0u, &event) == 1) {
        /* fill */
    }
    CHECK(la_event_batch_length(&batch) <= LA_EVENT_BATCH_CAPACITY,
          "block never exceeds the payload cap");
    CHECK(la_event_batch_records(&batch) > 1u, "block holds many records");
    CHECK(batch.data[2] == (uint8_t)(batch.records & 0xFFu) &&
              batch.data[3] == (uint8_t)(batch.records >> 8),
          "header count tracks records");
    return 0;
}

static int test_evt_overrun_payload(void)
{
    uint8_t payload[LA_PROTO_EVT_OVERRUN_WIRE_SIZE];
    uint32_t dropped = 0u;
    uint8_t stage = 0u;

    CHECK(la_proto_pack_evt_overrun(0x11223344u, 1u, payload, sizeof(payload)) ==
              LA_PROTO_EVT_OVERRUN_WIRE_SIZE,
          "overrun payload packs");
    CHECK(payload[0] == 0x44u && payload[1] == 0x33u && payload[2] == 0x22u &&
              payload[3] == 0x11u && payload[4] == 1u &&
              payload[5] == 0u && payload[6] == 0u && payload[7] == 0u,
          "overrun byte layout");
    CHECK(la_proto_unpack_evt_overrun(&dropped, &stage, payload, sizeof(payload)) == 0 &&
              dropped == 0x11223344u && stage == 1u,
          "overrun payload round trip");
    CHECK(la_proto_pack_evt_overrun(0u, 0u, payload, sizeof(payload) - 1u) == 0u,
          "short buffer rejected");
    CHECK(la_proto_unpack_evt_overrun(&dropped, &stage, payload, 4u) == -1,
          "short payload rejected");
    return 0;
}

int main(void)
{
    if (test_live_ring() != 0 || test_decoder_core_uart() != 0 ||
        test_decoder_core_spi() != 0 || test_decoder_core_i2c() != 0 ||
        test_decoder_core_guards() != 0 || test_decoder_core_stop_on() != 0 ||
        test_event_batch() != 0 || test_event_block() != 0 ||
        test_evt_overrun_payload() != 0) {
        return 1;
    }
    puts("PASS: la_native_live");
    return 0;
}
