#include "la_tx_pack.h"

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

#define TEST_CAPTURE_BYTES (32768u)
#define MAX_EMISSIONS (40u)

static uint8_t capture[TEST_CAPTURE_BYTES];
static uint64_t capture_len;

static const uint8_t *mem_read(void *context, uint64_t byte_offset)
{
    (void)context;
    if (byte_offset >= capture_len) {
        return NULL;
    }
    return &capture[byte_offset];
}

static const uint8_t *never_read(void *context, uint64_t byte_offset)
{
    (void)context;
    (void)byte_offset;
    return NULL;
}

typedef struct emission {
    la_tx_pack_kind_t kind;
    uint64_t byte_offset;
    size_t data_length;
    size_t payload_length;
    uint8_t payload[LA_PROTO_MAX_PAYLOAD];
} emission_t;

/* Drives a whole job over the capture buffer and verifies the frame
 * structure: BEGIN first, CHUNKs covering 0..bytes_total once in ascending
 * contiguous order with word-aligned 1016-byte chunks, END with the
 * one-shot CRC. Emissions are recorded for retransmission checks. */
static int run_job(uint64_t total_words,
                   uint8_t encoding,
                   uint32_t sample_rate,
                   emission_t *emissions,
                   size_t *chunk_count)
{
    la_tx_pack_t pack;
    la_tx_pack_out_t out;
    uint64_t bytes_total = total_words * 4u;
    uint8_t scratch[TEST_CAPTURE_BYTES];
    uint8_t payload[LA_PROTO_MAX_PAYLOAD];
    uint32_t expected_crc = la_crc32_ieee(capture, (size_t)bytes_total);
    size_t emission_count = 0u;
    size_t chunks = 0u;
    size_t scratch_offset = 0u;
    la_proto_data_begin_t begin;

    CHECK(la_tx_pack_init(&pack, total_words, encoding, sample_rate) == 0, "job init");
    for (;;) {
        if (la_tx_pack_next(&pack, mem_read, NULL, payload, sizeof(payload), &out) != 0) {
            break;
        }
        CHECK(emission_count < MAX_EMISSIONS, "emission capacity");
        emissions[emission_count].kind = out.kind;
        emissions[emission_count].byte_offset = out.byte_offset;
        emissions[emission_count].data_length = out.data_length;
        emissions[emission_count].payload_length = 0u;
        switch (out.kind) {
        case LA_TX_PACK_BEGIN:
            CHECK(emission_count == 0u, "DATA_BEGIN is first");
            CHECK(out.data_length == 0u, "BEGIN has no data field");
            CHECK(la_proto_unpack_data_begin(&begin, payload,
                                             LA_PROTO_DATA_BEGIN_WIRE_SIZE) == 0 &&
                      begin.total_words == total_words && begin.encoding == encoding &&
                      begin.sample_rate_hz == sample_rate,
                  "BEGIN payload metadata");
            emissions[emission_count].payload_length = LA_PROTO_DATA_BEGIN_WIRE_SIZE;
            break;
        case LA_TX_PACK_CHUNK:
            CHECK(out.byte_offset == chunks * 1016u, "chunk offsets are contiguous");
            CHECK(out.data_length == (bytes_total - out.byte_offset > 1016u
                                          ? 1016u
                                          : (size_t)(bytes_total - out.byte_offset)),
                  "chunk data length");
            CHECK(out.data_length != 0u, "never an empty chunk");
            CHECK(out.byte_offset + out.data_length <= bytes_total, "chunk stays in range");
            memcpy(scratch + scratch_offset, &payload[4], out.data_length);
            scratch_offset += out.data_length;
            emissions[emission_count].payload_length = 4u + out.data_length;
            ++chunks;
            break;
        case LA_TX_PACK_END:
            CHECK(emission_count == chunks + 1u, "END follows the last chunk");
            CHECK(out.data_length == 0u, "END has no data field");
            emissions[emission_count].payload_length = LA_PROTO_DATA_END_WIRE_SIZE;
            break;
        default:
            CHECK(0, "unknown emission kind");
        }
        memcpy(emissions[emission_count].payload, payload,
               emissions[emission_count].payload_length);
        ++emission_count;
    }
    CHECK(emission_count == chunks + 2u, "BEGIN + chunks + END emitted");
    CHECK(scratch_offset == bytes_total, "chunks cover the capture exactly once");
    if (bytes_total != 0u) {
        CHECK(memcmp(scratch, capture, (size_t)bytes_total) == 0,
              "reconstructed chunk data equals capture bytes");
    }
    {
        uint8_t end_payload[LA_PROTO_DATA_END_WIRE_SIZE];
        uint32_t end_crc = 0u;

        CHECK(la_tx_pack_next(&pack, mem_read, NULL, end_payload, sizeof(end_payload), &out) ==
                  -1,
              "next() after END stays finished");
        memcpy(end_payload, emissions[emission_count - 1u].payload,
               LA_PROTO_DATA_END_WIRE_SIZE);
        CHECK(la_proto_unpack_data_end(&end_crc, end_payload, sizeof(end_payload)) == 0 &&
                  end_crc == expected_crc,
              "END CRC equals the one-shot CRC of the capture bytes");
    }
    *chunk_count = chunks;
    return 0;
}

static int test_multi_chunk_job(void)
{
    emission_t emissions[MAX_EMISSIONS];
    size_t chunk_count = 0u;
    uint64_t total_words = 7777u;
    size_t index;

    capture_len = total_words * 4u;
    CHECK(run_job(total_words, 1u, 100000000u, emissions, &chunk_count) == 0,
          "multi-chunk job");
    CHECK(chunk_count == 31u, "7777 words make 30 full chunks plus a 628-byte tail");
    for (index = 0u; index < chunk_count; ++index) {
        la_tx_pack_t pack;
        la_tx_pack_out_t out;
        uint8_t payload[LA_PROTO_MAX_PAYLOAD];
        uint8_t rebuilt[LA_PROTO_MAX_PAYLOAD];

        CHECK(la_tx_pack_init(&pack, total_words, 1u, 100000000u) == 0, "chunk_at init");
        CHECK(la_tx_pack_chunk_at(&pack, index, mem_read, NULL, payload, sizeof(payload), &out) ==
                  0,
              "chunk_at rebuild");
        memcpy(rebuilt, payload, out.data_length + 4u);
        CHECK(out.byte_offset == emissions[index + 1u].byte_offset &&
                  out.data_length == emissions[index + 1u].data_length &&
                  memcmp(rebuilt, emissions[index + 1u].payload,
                         emissions[index + 1u].payload_length) == 0,
              "chunk_at reproduces the first-emission payload");
        CHECK(la_tx_pack_chunk_at(&pack, chunk_count, mem_read, NULL, payload, sizeof(payload),
                                  &out) == -1,
              "chunk_at out of range");
    }
    return 0;
}

static int test_boundary_jobs(void)
{
    emission_t emissions[MAX_EMISSIONS];
    size_t chunk_count = 0u;

    memset(capture, 0, TEST_CAPTURE_BYTES);
    capture_len = 0u;
    CHECK(run_job(0u, 0u, 115200u, emissions, &chunk_count) == 0, "empty job");
    CHECK(chunk_count == 0u, "empty job has no chunks");
    CHECK(emissions[1].kind == LA_TX_PACK_END, "empty job is BEGIN then END");

    capture_len = 4u;
    capture[0] = 0x12u;
    capture[1] = 0x34u;
    capture[2] = 0x56u;
    capture[3] = 0x78u;
    CHECK(run_job(1u, 0u, 1000000u, emissions, &chunk_count) == 0, "single-word job");
    CHECK(chunk_count == 1u && emissions[1].data_length == 4u &&
              emissions[1].byte_offset == 0u,
          "single word fits one chunk");

    capture_len = 1016u;
    CHECK(run_job(254u, 0u, 1000000u, emissions, &chunk_count) == 0, "exact chunk job");
    CHECK(chunk_count == 1u && emissions[1].data_length == 1016u,
          "254 words fill exactly one chunk");

    capture_len = 1020u;
    CHECK(run_job(255u, 0u, 1000000u, emissions, &chunk_count) == 0, "chunk-tail job");
    CHECK(chunk_count == 2u && emissions[1].data_length == 1016u &&
              emissions[2].data_length == 4u && emissions[2].byte_offset == 1016u,
          "255 words split across two chunks");

    capture_len = 2032u;
    CHECK(run_job(508u, 0u, 1000000u, emissions, &chunk_count) == 0, "two full chunks job");
    CHECK(chunk_count == 2u && emissions[2].byte_offset == 1016u,
          "two full chunks at word-aligned boundaries");
    return 0;
}

static int test_argument_and_failure_guards(void)
{
    la_tx_pack_t pack;
    la_tx_pack_out_t out;
    uint8_t payload[LA_PROTO_MAX_PAYLOAD];

    capture_len = 4096u;
    CHECK(la_tx_pack_init(NULL, 1u, 0u, 0u) == -1, "NULL init rejected");
    CHECK(la_tx_pack_init(&pack, (uint64_t)UINT32_MAX / 4u + 1u, 0u, 0u) == -1,
          "unaddressable byte range rejected");
    CHECK(la_tx_pack_init(&pack, 1u, 0u, 0u) == 0, "init");
    CHECK(la_tx_pack_next(&pack, NULL, NULL, payload, sizeof(payload), &out) == -1,
          "NULL read callback rejected");
    CHECK(la_tx_pack_next(&pack, mem_read, NULL, NULL, sizeof(payload), &out) == -1,
          "NULL payload rejected");
    CHECK(la_tx_pack_next(&pack, mem_read, NULL, payload, sizeof(payload), NULL) == -1,
          "NULL output rejected");

    CHECK(la_tx_pack_next(&pack, mem_read, NULL, payload, 15u, &out) == -1,
          "BEGIN undersized capacity rejected without commit");
    CHECK(la_tx_pack_next(&pack, never_read, NULL, payload, sizeof(payload), &out) == 0 &&
              out.kind == LA_TX_PACK_BEGIN,
          "failed BEGIN attempt did not advance the job");
    CHECK(la_tx_pack_next(&pack, never_read, NULL, payload, sizeof(payload), &out) == -1,
          "failing chunk read rejected without commit");
    CHECK(la_tx_pack_next(&pack, mem_read, NULL, payload, 7u, &out) == -1,
          "chunk undersized capacity rejected without commit");
    CHECK(la_tx_pack_next(&pack, mem_read, NULL, payload, sizeof(payload), &out) == 0 &&
              out.kind == LA_TX_PACK_CHUNK && out.byte_offset == 0u && out.data_length == 4u,
          "failed chunk attempt did not advance the job");
    CHECK(la_tx_pack_chunk_at(&pack, 0u, never_read, NULL, payload, sizeof(payload), &out) == -1,
          "chunk_at read failure rejected");
    CHECK(la_tx_pack_chunk_at(&pack, 0u, NULL, NULL, payload, sizeof(payload), &out) == -1,
          "chunk_at NULL read rejected");
    CHECK(la_tx_pack_chunk_at(&pack, 0u, mem_read, NULL, payload, 7u, &out) == -1,
          "chunk_at undersized capacity rejected");
    return 0;
}

int main(void)
{
    size_t index;

    for (index = 0u; index < sizeof(capture); ++index) {
        capture[index] = (uint8_t)((index * 131u) + (index >> 3));
    }
    capture_len = sizeof(capture);
    if (test_multi_chunk_job() != 0 || test_boundary_jobs() != 0 ||
        test_argument_and_failure_guards() != 0) {
        return 1;
    }
    puts("PASS: la_native_txpack");
    return 0;
}
