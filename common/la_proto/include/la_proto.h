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
#define LA_PROTO_FRAME_OVERHEAD ((size_t)7u)
#define LA_PROTO_DATA_CHUNK_MAX ((uint16_t)1016u)
#define LA_PROTO_CONFIG_WIRE_SIZE ((uint16_t)37u)
#define LA_PROTO_DECODER_CONFIG_WIRE_SIZE ((uint16_t)20u)

typedef enum la_proto_message_type {
    LA_PROTO_PING = 0x01,
    LA_PROTO_PONG = 0x02,
    LA_PROTO_CONFIG = 0x10,
    LA_PROTO_ARM = 0x11,
    LA_PROTO_ABORT = 0x12,
    LA_PROTO_GET_STATUS = 0x13,
    LA_PROTO_DECODER_CFG = 0x14,
    LA_PROTO_LIVE_START = 0x15,
    LA_PROTO_LIVE_STOP = 0x16,
    LA_PROTO_STATUS = 0x20,
    LA_PROTO_EVT_TRIGGERED = 0x21,
    LA_PROTO_EVT_DECODED = 0x22,
    LA_PROTO_EVT_OVERRUN = 0x23,
    LA_PROTO_DATA_BEGIN = 0x30,
    LA_PROTO_DATA_CHUNK = 0x31,
    LA_PROTO_DATA_END = 0x32,
    LA_PROTO_ACK = 0x40,
    LA_PROTO_NAK = 0x41,
    LA_PROTO_LOG = 0x7F
} la_proto_message_type_t;

typedef enum la_proto_error_code {
    LA_PROTO_ERROR_NONE = 0,
    LA_PROTO_ERROR_BAD_LENGTH = 1,
    LA_PROTO_ERROR_BAD_FIELD = 2,
    LA_PROTO_ERROR_BAD_STATE = 3,
    LA_PROTO_ERROR_UNSUPPORTED = 4,
    LA_PROTO_ERROR_BUSY = 5,
    LA_PROTO_ERROR_OVERFLOW = 6,
    LA_PROTO_ERROR_CRC = 7
} la_proto_error_code_t;

typedef enum la_proto_decoder_type {
    LA_PROTO_DECODER_OFF = 0,
    LA_PROTO_DECODER_UART = 1,
    LA_PROTO_DECODER_SPI = 2,
    LA_PROTO_DECODER_I2C = 3
} la_proto_decoder_type_t;

typedef struct la_proto_config {
    uint32_t ctrl_flags;
    uint32_t sample_div;
    uint32_t mode;
    uint8_t chan_en;
    uint8_t trig_mask;
    uint8_t trig_value;
    uint8_t trig_rise;
    uint8_t trig_fall;
    uint32_t pre_samples;
    uint32_t post_samples;
    uint32_t timeout;
    uint32_t txn_max_run;
    uint32_t live_flush_cycles;
} la_proto_config_t;

typedef struct la_proto_uart_config {
    uint8_t rx_channel;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
    uint8_t inverted;
    uint32_t baud_hz;
    uint32_t sample_rate_hz;
} la_proto_uart_config_t;

typedef struct la_proto_spi_config {
    int8_t sck_channel;
    int8_t mosi_channel;
    int8_t miso_channel;
    int8_t cs_channel;
    uint8_t cpol;
    uint8_t cpha;
    uint8_t word_size;
    uint8_t lsb_first;
    uint8_t cs_active;
} la_proto_spi_config_t;

typedef struct la_proto_i2c_config {
    int8_t scl_channel;
    int8_t sda_channel;
} la_proto_i2c_config_t;

typedef struct la_proto_decoder_config {
    uint8_t slot;
    uint8_t type;
    union {
        la_proto_uart_config_t uart;
        la_proto_spi_config_t spi;
        la_proto_i2c_config_t i2c;
    } params;
} la_proto_decoder_config_t;

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

/* Streaming CRC-32/IEEE for payloads delivered in chunks (protocol.md:
 * DATA_END carries the CRC32/IEEE of the concatenated data bytes, initial
 * 0xFFFFFFFF, reflected). Start the running value at LA_CRC32_IEEE_INIT,
 * feed every chunk with la_crc32_ieee_update and call
 * la_crc32_ieee_finish on the last state; the result equals
 * la_crc32_ieee(whole buffer) for any chunk split. */
#define LA_CRC32_IEEE_INIT UINT32_C(0xFFFFFFFF)
LA_PROTO_API uint32_t la_crc32_ieee_update(uint32_t crc,
                                           const uint8_t *data,
                                           size_t length);
LA_PROTO_API uint32_t la_crc32_ieee_finish(uint32_t crc);

LA_PROTO_API int la_proto_message_type_valid(uint8_t type);
LA_PROTO_API int la_proto_validate_config(const la_proto_config_t *config);
LA_PROTO_API size_t la_proto_pack_config(const la_proto_config_t *config,
                                         uint8_t *output,
                                         size_t output_capacity);
LA_PROTO_API int la_proto_unpack_config(la_proto_config_t *config,
                                        const uint8_t *input,
                                        size_t input_length);
LA_PROTO_API int la_proto_validate_decoder_config(const la_proto_decoder_config_t *config);
LA_PROTO_API size_t la_proto_pack_decoder_config(const la_proto_decoder_config_t *config,
                                                 uint8_t *output,
                                                 size_t output_capacity);
LA_PROTO_API int la_proto_unpack_decoder_config(la_proto_decoder_config_t *config,
                                                const uint8_t *input,
                                                size_t input_length);

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
