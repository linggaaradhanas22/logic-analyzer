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
/* U06 design amendment (2026-09-19): the maximum frame payload and DATA_CHUNK
 * data size were raised from 1024/1016 to cut the per-frame overhead on the
 * direct-USB drain path. Chunk data still starts at 4-byte offsets and the
 * frame layout is unchanged; only the bounds grew. */
#define LA_PROTO_MAX_PAYLOAD ((uint16_t)4096u)
#define LA_PROTO_FRAME_OVERHEAD ((size_t)7u)
#define LA_PROTO_DATA_CHUNK_MAX ((uint16_t)4092u)
#define LA_PROTO_CONFIG_WIRE_SIZE ((uint16_t)37u)
#define LA_PROTO_DECODER_CONFIG_WIRE_SIZE ((uint16_t)20u)
#define LA_PROTO_PATGEN_CFG_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_TRACE_CTRL_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_EVT_CFG_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_LINK_CFG_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_TEST_RECORD_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_EVT_BLOCK_HEADER_SIZE ((uint16_t)12u)
#define LA_PROTO_EVT_BLOCK_VERSION ((uint8_t)1u)
#define LA_PROTO_EVT_BLOCK_DELTA_ESCAPE ((uint16_t)0xFFFFu)
#define LA_PROTO_PONG_WIRE_SIZE ((uint16_t)16u)
#define LA_PROTO_ACK_WIRE_SIZE ((uint16_t)2u)
#define LA_PROTO_NAK_WIRE_SIZE ((uint16_t)3u)
#define LA_PROTO_STATUS_WIRE_SIZE ((uint16_t)52u)
#define LA_PROTO_DATA_BEGIN_WIRE_SIZE ((uint16_t)16u)
#define LA_PROTO_DATA_END_WIRE_SIZE ((uint16_t)4u)
#define LA_PROTO_EVT_OVERRUN_WIRE_SIZE ((uint16_t)8u)
#define LA_PROTO_DATA_CHUNK_FIELD_SIZE ((uint16_t)4u)

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
    LA_PROTO_PATGEN_CFG = 0x17,
    LA_PROTO_TRACE_CTRL = 0x18,
    LA_PROTO_EVT_CFG = 0x19,
    LA_PROTO_LINK_CFG = 0x1A,
    LA_PROTO_TEST_RECORD = 0x1B,
    LA_PROTO_STATUS = 0x20,
    LA_PROTO_EVT_TRIGGERED = 0x21,
    LA_PROTO_EVT_DECODED = 0x22,
    LA_PROTO_EVT_OVERRUN = 0x23,
    LA_PROTO_EVT_BLOCK = 0x24,
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

#define LA_PROTO_DECODER_MAX_SLOTS 3u

/* P7.7 protocol-aware stop. DECODER_CFG byte 2 is `stop_on_len` (0 disables)
 * and the decoder type's reserved bytes carry the pattern in decode order:
 * UART bytes 9..11, SPI bytes 13..19, I2C bytes 6..19. A decoded BYTE record
 * matching the whole pattern stops the live acquisition. */
#define LA_PROTO_STOP_ON_MAX_UART ((uint8_t)3u)
#define LA_PROTO_STOP_ON_MAX_SPI ((uint8_t)7u)
#define LA_PROTO_STOP_ON_MAX_I2C ((uint8_t)14u)
#define LA_PROTO_STOP_ON_MAX LA_PROTO_STOP_ON_MAX_I2C

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
    uint8_t stop_on_len; /* P7.7: pattern length, 0 = disabled */
    uint8_t stop_on[LA_PROTO_STOP_ON_MAX];
    union {
        la_proto_uart_config_t uart;
        la_proto_spi_config_t spi;
        la_proto_i2c_config_t i2c;
    } params;
} la_proto_decoder_config_t;

/* PONG payload (protocol.md): fw_version, pl_id and uptime in milliseconds. */
typedef struct la_proto_pong {
    uint32_t fw_version;
    uint32_t pl_id;
    uint64_t uptime_ms;
} la_proto_pong_t;

/* Per-decoder counters inside STATUS, in UART, SPI, I2C order. */
typedef struct la_proto_decoder_stats {
    uint32_t events;
    uint32_t dropped_segments;
} la_proto_decoder_stats_t;

/* STATUS payload (protocol.md v1, 52 bytes). state is the host-visible
 * lifecycle state reported by the firmware; status_reg is the raw la_top
 * STATUS register snapshot. */
typedef struct la_proto_status {
    uint8_t state;
    uint32_t status_reg;
    uint64_t word_count;
    uint64_t trig_index;
    uint16_t cpu_load_permille;
    uint16_t min_free_stack;
    la_proto_decoder_stats_t decoders[3];
} la_proto_status_t;

/* DATA_BEGIN payload (protocol.md): burst metadata. encoding is 0 for raw
 * packed words and 1 for transition words. */
typedef struct la_proto_data_begin {
    uint64_t total_words;
    uint8_t encoding;
    uint32_t sample_rate_hz;
} la_proto_data_begin_t;

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

/* Fixed payload helpers for the P4.9 Zynq sender and its host peers. All
 * packs return the wire size or zero on invalid arguments/insufficient
 * capacity; all unpacks return 0 or -1. Reserved bytes are transmitted as
 * zero and ignored on unpack. */
/* PATGEN_CFG payload: the raw la_top PATGEN_CTRL register value. This is an
 * additive v1 extension (type 0x17) used by the serial HIL and E2E self-test
 * flows to select the pattern-generator mode; the burst-path CONFIG payload
 * has no patgen field. Validation: reserved bits zero and a defined MODE. */
LA_PROTO_API size_t la_proto_pack_patgen_cfg(uint32_t patgen_ctrl,
                                             uint8_t *output,
                                             size_t output_capacity);
LA_PROTO_API int la_proto_unpack_patgen_cfg(uint32_t *patgen_ctrl,
                                            const uint8_t *input,
                                            size_t input_length);

/* TRACE_CTRL payload: action:u8, reserved[3] zero (P8.3). START clears the
 * trace ring and enables recording; STOP disables recording; DUMP disables
 * recording and the Zynq answers with LOG frames carrying the ring dump. */
typedef enum la_proto_trace_action {
    LA_PROTO_TRACE_STOP = 1,
    LA_PROTO_TRACE_START = 2,
    LA_PROTO_TRACE_DUMP = 3
} la_proto_trace_action_t;

/* TRACE_CTRL payload: action:u8, start:u16 LE, count:u8. For DUMP, start is
 * the first record index and count the requested record count (0 = the
 * firmware chunk maximum); STOP/START ignore start/count. The chunking keeps
 * each dump response bounded on slow transports (direct USB, 2026-09-19). */
LA_PROTO_API size_t la_proto_pack_trace_ctrl(uint8_t action,
                                             uint16_t start,
                                             uint8_t count,
                                             uint8_t *output,
                                             size_t output_capacity);
LA_PROTO_API int la_proto_unpack_trace_ctrl(uint8_t *action,
                                            uint16_t *start,
                                            uint8_t *count,
                                            const uint8_t *input,
                                            size_t input_length);

/* EVT_CFG payload: format:u8, reserved[3] zero (P9.2). Format 0 keeps the v1
 * EVT_DECODED records; format 1 selects the compact EVT_BLOCK encoding. */
typedef enum la_proto_evt_format {
    LA_PROTO_EVT_FORMAT_RECORDS = 0,
    LA_PROTO_EVT_FORMAT_BLOCK = 1
} la_proto_evt_format_t;

LA_PROTO_API size_t la_proto_pack_evt_cfg(uint8_t format,
                                          uint8_t *output,
                                          size_t output_capacity);
LA_PROTO_API int la_proto_unpack_evt_cfg(uint8_t *format,
                                         const uint8_t *input,
                                         size_t input_length);

/* LINK_CFG payload: transport:u8, reserved[3] zero (P9.1). Transport 0 keeps
 * the burst DATA_* frames on the UART; transport 1 routes them over the PL
 * SPI slave (commands and ACKs always stay on the UART). */
typedef enum la_proto_link_transport {
    LA_PROTO_LINK_UART = 0,
    LA_PROTO_LINK_SPI = 1
} la_proto_link_transport_t;

LA_PROTO_API size_t la_proto_pack_link_cfg(uint8_t transport,
                                           uint8_t *output,
                                           size_t output_capacity);
LA_PROTO_API int la_proto_unpack_link_cfg(uint8_t *transport,
                                          const uint8_t *input,
                                          size_t input_length);

/* TEST_RECORD payload (U03 bench extension): record_id:u32 LE. A test-only
 * numbered record used by the bulk-OUT loss detector; the receiver validates
 * the length and acknowledges without touching acquisition state. The device
 * trace logs the record id so the JTAG dump can be compared with the host's
 * sent list. */
LA_PROTO_API size_t la_proto_pack_test_record(uint32_t record_id,
                                              uint8_t *output,
                                              size_t output_capacity);
LA_PROTO_API int la_proto_unpack_test_record(uint32_t *record_id,
                                             const uint8_t *input,
                                             size_t input_length);

LA_PROTO_API size_t la_proto_pack_pong(const la_proto_pong_t *pong,
                                       uint8_t *output,
                                       size_t output_capacity);
LA_PROTO_API int la_proto_unpack_pong(la_proto_pong_t *pong,
                                      const uint8_t *input,
                                      size_t input_length);

LA_PROTO_API size_t la_proto_pack_ack(uint8_t acknowledged_type,
                                      uint8_t sequence,
                                      uint8_t *output,
                                      size_t output_capacity);
LA_PROTO_API int la_proto_unpack_ack(uint8_t *acknowledged_type,
                                     uint8_t *sequence,
                                     const uint8_t *input,
                                     size_t input_length);

LA_PROTO_API size_t la_proto_pack_nak(uint8_t nak_type,
                                      uint8_t sequence,
                                      uint8_t error,
                                      uint8_t *output,
                                      size_t output_capacity);
LA_PROTO_API int la_proto_unpack_nak(uint8_t *nak_type,
                                     uint8_t *sequence,
                                     uint8_t *error,
                                     const uint8_t *input,
                                     size_t input_length);

/* EVT_OVERRUN payload (protocol.md): dropped:u32 LE, stage:u8, reserved[3].
 * stage is 0 for DMA, 1 for decoder and 2 for link. */
LA_PROTO_API size_t la_proto_pack_evt_overrun(uint32_t dropped,
                                              uint8_t stage,
                                              uint8_t *output,
                                              size_t output_capacity);
LA_PROTO_API int la_proto_unpack_evt_overrun(uint32_t *dropped,
                                             uint8_t *stage,
                                             const uint8_t *input,
                                             size_t input_length);

LA_PROTO_API size_t la_proto_pack_status(const la_proto_status_t *status,
                                         uint8_t *output,
                                         size_t output_capacity);
LA_PROTO_API int la_proto_unpack_status(la_proto_status_t *status,
                                        const uint8_t *input,
                                        size_t input_length);

LA_PROTO_API size_t la_proto_pack_data_begin(const la_proto_data_begin_t *begin,
                                             uint8_t *output,
                                             size_t output_capacity);
LA_PROTO_API int la_proto_unpack_data_begin(la_proto_data_begin_t *begin,
                                            const uint8_t *input,
                                            size_t input_length);

/* DATA_END payload: the CRC32/IEEE of the concatenated DATA_CHUNK bytes. */
LA_PROTO_API size_t la_proto_pack_data_end(uint32_t crc32,
                                           uint8_t *output,
                                           size_t output_capacity);
LA_PROTO_API int la_proto_unpack_data_end(uint32_t *crc32,
                                          const uint8_t *input,
                                          size_t input_length);

/* DATA_CHUNK payload: u32 little-endian byte offset followed by up to
 * LA_PROTO_DATA_CHUNK_MAX data bytes. Returns 4 + data_length. */
LA_PROTO_API size_t la_proto_pack_data_chunk(uint32_t byte_offset,
                                             const uint8_t *data,
                                             size_t data_length,
                                             uint8_t *output,
                                             size_t output_capacity);

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
