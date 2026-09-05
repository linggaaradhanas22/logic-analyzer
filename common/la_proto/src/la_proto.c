#include "la_proto.h"

#include <string.h>

static uint16_t crc16_update(uint16_t crc, uint8_t value)
{
    uint8_t bit;

    crc ^= (uint16_t)value << 8;
    for (bit = 0; bit < 8u; ++bit) {
        if ((crc & 0x8000u) != 0u) {
            crc = (uint16_t)((crc << 1) ^ 0x1021u);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t la_crc16_ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    size_t index;

    if (data == NULL && length != 0u) {
        return 0u;
    }
    for (index = 0; index < length; ++index) {
        crc = crc16_update(crc, data[index]);
    }
    return crc;
}

uint32_t la_crc32_ieee(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t index;
    unsigned bit;

    if (data == NULL && length != 0u) {
        return 0u;
    }
    for (index = 0; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0; bit < 8u; ++bit) {
            crc = (crc & 1u) != 0u ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

size_t la_proto_encode(uint8_t type,
                       uint8_t sequence,
                       const uint8_t *payload,
                       uint16_t length,
                       uint8_t *output,
                       size_t output_capacity)
{
    uint16_t crc = 0xFFFFu;
    size_t index;
    size_t frame_length = (size_t)length + 7u;

    if (length > LA_PROTO_MAX_PAYLOAD || output == NULL || output_capacity < frame_length ||
        (payload == NULL && length != 0u)) {
        return 0u;
    }
    output[0] = LA_PROTO_SOF;
    output[1] = type;
    output[2] = sequence;
    output[3] = (uint8_t)(length & 0xFFu);
    output[4] = (uint8_t)(length >> 8);
    crc = crc16_update(crc, type);
    crc = crc16_update(crc, sequence);
    crc = crc16_update(crc, output[3]);
    crc = crc16_update(crc, output[4]);
    for (index = 0; index < length; ++index) {
        output[5u + index] = payload[index];
        crc = crc16_update(crc, payload[index]);
    }
    output[5u + length] = (uint8_t)(crc & 0xFFu);
    output[6u + length] = (uint8_t)(crc >> 8);
    return frame_length;
}

void la_proto_parser_init(la_proto_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser->state = LA_PROTO_WAIT_SOF;
}

static void parser_restart(la_proto_parser_t *parser, uint8_t value)
{
    parser->state = value == LA_PROTO_SOF ? LA_PROTO_READ_TYPE : LA_PROTO_WAIT_SOF;
    parser->payload_index = 0u;
    parser->length = 0u;
    parser->received_crc = 0u;
    parser->calculated_crc = 0xFFFFu;
}

static size_t parser_byte(la_proto_parser_t *parser,
                          uint8_t value,
                          la_proto_frame_callback callback,
                          void *context)
{
    switch (parser->state) {
    case LA_PROTO_WAIT_SOF:
        if (value == LA_PROTO_SOF) {
            parser->state = LA_PROTO_READ_TYPE;
            parser->calculated_crc = 0xFFFFu;
        } else {
            ++parser->dropped_bytes;
        }
        return 0u;

    case LA_PROTO_READ_TYPE:
        parser->type = value;
        parser->calculated_crc = crc16_update(parser->calculated_crc, value);
        parser->state = LA_PROTO_READ_SEQUENCE;
        return 0u;

    case LA_PROTO_READ_SEQUENCE:
        parser->sequence = value;
        parser->calculated_crc = crc16_update(parser->calculated_crc, value);
        parser->state = LA_PROTO_READ_LENGTH_LOW;
        return 0u;

    case LA_PROTO_READ_LENGTH_LOW:
        parser->length = value;
        parser->calculated_crc = crc16_update(parser->calculated_crc, value);
        parser->state = LA_PROTO_READ_LENGTH_HIGH;
        return 0u;

    case LA_PROTO_READ_LENGTH_HIGH:
        parser->length |= (uint16_t)value << 8;
        parser->calculated_crc = crc16_update(parser->calculated_crc, value);
        if (parser->length > LA_PROTO_MAX_PAYLOAD) {
            ++parser->format_errors;
            parser_restart(parser, value);
        } else if (parser->length == 0u) {
            parser->state = LA_PROTO_READ_CRC_LOW;
        } else {
            parser->state = LA_PROTO_READ_PAYLOAD;
        }
        return 0u;

    case LA_PROTO_READ_PAYLOAD:
        parser->payload[parser->payload_index++] = value;
        parser->calculated_crc = crc16_update(parser->calculated_crc, value);
        if (parser->payload_index == parser->length) {
            parser->state = LA_PROTO_READ_CRC_LOW;
        }
        return 0u;

    case LA_PROTO_READ_CRC_LOW:
        parser->received_crc = value;
        parser->state = LA_PROTO_READ_CRC_HIGH;
        return 0u;

    case LA_PROTO_READ_CRC_HIGH:
        parser->received_crc |= (uint16_t)value << 8;
        if (parser->received_crc == parser->calculated_crc) {
            ++parser->frames;
            if (callback != NULL) {
                callback(context, parser->type, parser->sequence, parser->payload, parser->length);
            }
            parser_restart(parser, 0u);
            return 1u;
        }
        ++parser->crc_errors;
        parser_restart(parser, value);
        return 0u;
    }

    parser_restart(parser, value);
    return 0u;
}

size_t la_proto_parser_feed(la_proto_parser_t *parser,
                            const uint8_t *data,
                            size_t length,
                            la_proto_frame_callback callback,
                            void *context)
{
    size_t frames = 0u;
    size_t index;

    if (parser == NULL || (data == NULL && length != 0u)) {
        return 0u;
    }
    for (index = 0; index < length; ++index) {
        frames += parser_byte(parser, data[index], callback, context);
    }
    return frames;
}
