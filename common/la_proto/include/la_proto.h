#ifndef LA_PROTO_H
#define LA_PROTO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_PROTO_API __declspec(dllexport)
#else
#define LA_PROTO_API
#endif

#define LA_PROTO_SOF ((uint8_t)0xA5u)
#define LA_PROTO_MAX_PAYLOAD ((uint16_t)1024u)

typedef void (*la_proto_frame_callback)(void *context,
                                        uint8_t type,
                                        uint8_t sequence,
                                        const uint8_t *payload,
                                        uint16_t length);

typedef enum la_proto_parser_state {
    LA_PROTO_WAIT_SOF = 0,
    LA_PROTO_READ_TYPE,
    LA_PROTO_READ_SEQUENCE,
    LA_PROTO_READ_LENGTH_LOW,
    LA_PROTO_READ_LENGTH_HIGH,
    LA_PROTO_READ_PAYLOAD,
    LA_PROTO_READ_CRC_LOW,
    LA_PROTO_READ_CRC_HIGH
} la_proto_parser_state_t;

typedef struct la_proto_parser {
    la_proto_parser_state_t state;
    uint8_t type;
    uint8_t sequence;
    uint16_t length;
    uint16_t payload_index;
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint32_t frames;
    uint32_t crc_errors;
    uint32_t format_errors;
    uint32_t dropped_bytes;
    uint8_t payload[LA_PROTO_MAX_PAYLOAD];
} la_proto_parser_t;

LA_PROTO_API uint16_t la_crc16_ccitt(const uint8_t *data, size_t length);
LA_PROTO_API uint32_t la_crc32_ieee(const uint8_t *data, size_t length);

/* Returns the encoded frame size, or zero if output capacity/arguments fail. */
LA_PROTO_API size_t la_proto_encode(uint8_t type,
                                    uint8_t sequence,
                                    const uint8_t *payload,
                                    uint16_t length,
                                    uint8_t *output,
                                    size_t output_capacity);

LA_PROTO_API void la_proto_parser_init(la_proto_parser_t *parser);

/* Feed bytes incrementally. Returns the number of accepted frames. */
LA_PROTO_API size_t la_proto_parser_feed(la_proto_parser_t *parser,
                                          const uint8_t *data,
                                          size_t length,
                                          la_proto_frame_callback callback,
                                          void *context);

#endif /* LA_PROTO_H */
