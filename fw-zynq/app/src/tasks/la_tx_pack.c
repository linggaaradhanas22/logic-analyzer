#include "la_tx_pack.h"

#include "la_proto.h"

#include <string.h>

#define LA_TX_PACK_CHUNK_MAX ((uint64_t)LA_PROTO_DATA_CHUNK_MAX)

static int build_begin_payload(la_tx_pack_t *pack, uint8_t *payload, size_t capacity)
{
    la_proto_data_begin_t begin;

    if (capacity < LA_PROTO_DATA_BEGIN_WIRE_SIZE) {
        return -1;
    }
    begin.total_words = pack->bytes_total / 4u;
    begin.encoding = pack->encoding;
    begin.sample_rate_hz = pack->sample_rate_hz;
    if (la_proto_pack_data_begin(&begin, payload, capacity) != LA_PROTO_DATA_BEGIN_WIRE_SIZE) {
        return -1;
    }
    return 0;
}

static int build_chunk_payload(la_tx_pack_t *pack,
                               uint64_t byte_offset,
                               size_t data_length,
                               la_tx_pack_read_fn read,
                               void *read_context,
                               uint8_t *payload,
                               size_t capacity)
{
    const uint8_t *data;

    if (byte_offset > UINT32_MAX ||
        capacity < (size_t)LA_PROTO_DATA_CHUNK_FIELD_SIZE + data_length) {
        return -1;
    }
    data = read(read_context, byte_offset);
    if (data == NULL) {
        return -1;
    }
    if (la_proto_pack_data_chunk((uint32_t)byte_offset, data, data_length, payload, capacity) ==
        0u) {
        return -1;
    }
    pack->crc = la_crc32_ieee_update(pack->crc, data, data_length);
    return 0;
}

int la_tx_pack_init(la_tx_pack_t *pack,
                    uint64_t total_words,
                    uint8_t encoding,
                    uint32_t sample_rate_hz)
{
    if (pack == NULL || total_words > (uint64_t)UINT32_MAX / 4u) {
        return -1;
    }
    memset(pack, 0, sizeof(*pack));
    pack->next_kind = LA_TX_PACK_BEGIN;
    pack->bytes_total = total_words * 4u;
    pack->crc = LA_CRC32_IEEE_INIT;
    pack->sample_rate_hz = sample_rate_hz;
    pack->encoding = encoding;
    return 0;
}

int la_tx_pack_next(la_tx_pack_t *pack,
                    la_tx_pack_read_fn read,
                    void *read_context,
                    uint8_t *payload,
                    size_t capacity,
                    la_tx_pack_out_t *out)
{
    size_t data_length;

    if (pack == NULL || read == NULL || payload == NULL || out == NULL || pack->finished) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    switch (pack->next_kind) {
    case LA_TX_PACK_BEGIN:
        if (build_begin_payload(pack, payload, capacity) != 0) {
            return -1;
        }
        out->kind = LA_TX_PACK_BEGIN;
        pack->next_kind = pack->bytes_total == 0u ? LA_TX_PACK_END : LA_TX_PACK_CHUNK;
        return 0;

    case LA_TX_PACK_CHUNK:
        data_length = (size_t)(pack->bytes_total - pack->next_offset);
        if (data_length > LA_TX_PACK_CHUNK_MAX) {
            data_length = (size_t)LA_TX_PACK_CHUNK_MAX;
        }
        if (build_chunk_payload(pack, pack->next_offset, data_length, read, read_context,
                                payload, capacity) != 0) {
            return -1;
        }
        out->kind = LA_TX_PACK_CHUNK;
        out->byte_offset = pack->next_offset;
        out->data_length = data_length;
        pack->next_offset += data_length;
        if (pack->next_offset == pack->bytes_total) {
            pack->next_kind = LA_TX_PACK_END;
        }
        return 0;

    case LA_TX_PACK_END:
        if (capacity < LA_PROTO_DATA_END_WIRE_SIZE ||
            la_proto_pack_data_end(la_crc32_ieee_finish(pack->crc), payload, capacity) !=
                LA_PROTO_DATA_END_WIRE_SIZE) {
            return -1;
        }
        out->kind = LA_TX_PACK_END;
        pack->finished = 1u;
        return 0;

    default:
        return -1;
    }
}

int la_tx_pack_chunk_at(const la_tx_pack_t *pack,
                        uint64_t chunk_index,
                        la_tx_pack_read_fn read,
                        void *read_context,
                        uint8_t *payload,
                        size_t capacity,
                        la_tx_pack_out_t *out)
{
    uint64_t byte_offset;
    size_t data_length;

    if (pack == NULL || read == NULL || payload == NULL || out == NULL ||
        pack->bytes_total == 0u || chunk_index > (pack->bytes_total - 1u) / LA_TX_PACK_CHUNK_MAX) {
        return -1;
    }
    byte_offset = chunk_index * LA_TX_PACK_CHUNK_MAX;
    data_length = (size_t)(pack->bytes_total - byte_offset);
    if (data_length > LA_TX_PACK_CHUNK_MAX) {
        data_length = (size_t)LA_TX_PACK_CHUNK_MAX;
    }
    if (byte_offset > UINT32_MAX ||
        capacity < (size_t)LA_PROTO_DATA_CHUNK_FIELD_SIZE + data_length) {
        return -1;
    }
    {
        const uint8_t *data = read(read_context, byte_offset);
        if (data == NULL) {
            return -1;
        }
        if (la_proto_pack_data_chunk((uint32_t)byte_offset, data, data_length, payload, capacity) ==
            0u) {
            return -1;
        }
    }
    memset(out, 0, sizeof(*out));
    out->kind = LA_TX_PACK_CHUNK;
    out->byte_offset = byte_offset;
    out->data_length = data_length;
    return 0;
}
