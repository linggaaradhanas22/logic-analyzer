#ifndef LA_DECODE_H
#define LA_DECODE_H

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_DECODE_API __declspec(dllexport)
#else
#define LA_DECODE_API
#endif

#define LA_DECODE_MAX_EVENT_DATA 16u
#define LA_DECODE_NO_CHANNEL 0xFFu
#define LA_DECODE_EVENT_FLAG_MOSI 0x01u
#define LA_DECODE_EVENT_FLAG_MISO 0x02u
#define LA_DECODE_EVENT_FLAG_READ 0x04u

typedef struct la_transition {
    uint64_t t_sample;
    uint8_t sample;
} la_transition_t;

typedef enum la_decode_event_kind {
    LA_DECODE_EVENT_BYTE = 1,
    LA_DECODE_EVENT_FRAME_ERROR = 2,
    LA_DECODE_EVENT_PARITY_ERROR = 3,
    LA_DECODE_EVENT_START = 4,
    LA_DECODE_EVENT_STOP = 5,
    LA_DECODE_EVENT_DISCONTINUITY = 6,
    LA_DECODE_EVENT_ADDRESS = 7,
    LA_DECODE_EVENT_ACK = 8,
    LA_DECODE_EVENT_NACK = 9,
    LA_DECODE_EVENT_CS_ASSERT = 10,
    LA_DECODE_EVENT_CS_RELEASE = 11
} la_decode_event_kind_t;

typedef struct la_decode_event {
    uint8_t kind;
    uint8_t channel;
    uint8_t flags;
    uint8_t length;
    uint64_t t_sample;
    uint8_t data[LA_DECODE_MAX_EVENT_DATA];
} la_decode_event_t;

typedef void (*la_decode_event_callback)(void *context, const la_decode_event_t *event);

typedef enum la_uart_parity {
    LA_UART_PARITY_NONE = 0,
    LA_UART_PARITY_EVEN = 1,
    LA_UART_PARITY_ODD = 2
} la_uart_parity_t;

typedef enum la_uart_state {
    LA_UART_IDLE = 0,
    LA_UART_START,
    LA_UART_DATA,
    LA_UART_PARITY,
    LA_UART_STOP
} la_uart_state_t;

typedef struct la_uart_decoder {
    uint8_t channel;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t inverted;
    la_uart_parity_t parity;
    uint32_t samples_per_bit;
    uint8_t idle_level;
    uint8_t current_level;
    uint8_t bit_index;
    uint8_t stop_index;
    uint8_t parity_bad;
    uint8_t have_time;
    uint8_t timestamp_valid;
    la_uart_state_t state;
    uint16_t data_word;
    uint8_t parity_accum;
    uint64_t frame_start;
    uint64_t next_sample;
    uint64_t last_time;
} la_uart_decoder_t;

LA_DECODE_API int la_uart_decoder_init(la_uart_decoder_t *decoder,
                                       uint8_t channel,
                                       uint32_t samples_per_bit,
                                       uint8_t data_bits,
                                       la_uart_parity_t parity,
                                       uint8_t stop_bits,
                                       uint8_t inverted);
LA_DECODE_API void la_uart_decoder_reset(la_uart_decoder_t *decoder);

/* Advance through unchanged input up to time, returning emitted event count. */
LA_DECODE_API int la_uart_decoder_advance(la_uart_decoder_t *decoder,
                                          uint64_t t_sample,
                                          la_decode_event_callback callback,
                                          void *context);

/* Feed a transition; timestamps must be monotonic. Returns -1 on bad input. */
LA_DECODE_API int la_uart_decoder_feed_transition(la_uart_decoder_t *decoder,
                                                   const la_transition_t *transition,
                                                   la_decode_event_callback callback,
                                                   void *context);
LA_DECODE_API int la_uart_decoder_mark_discontinuity(la_uart_decoder_t *decoder,
                                                      uint64_t t_sample,
                                                      la_decode_event_callback callback,
                                                      void *context);

typedef struct la_spi_decoder {
    int8_t sck_channel;
    int8_t mosi_channel;
    int8_t miso_channel;
    int8_t cs_channel;
    uint8_t cpol;
    uint8_t cpha;
    uint8_t word_size;
    uint8_t lsb_first;
    uint8_t cs_active;
    uint8_t current_sample;
    uint8_t have_time;
    uint8_t timestamp_valid;
    uint8_t selected;
    uint8_t bit_count;
    uint16_t mosi_word;
    uint16_t miso_word;
    uint64_t last_time;
} la_spi_decoder_t;

LA_DECODE_API int la_spi_decoder_init(la_spi_decoder_t *decoder,
                                      int8_t sck_channel,
                                      int8_t mosi_channel,
                                      int8_t miso_channel,
                                      int8_t cs_channel,
                                      uint8_t cpol,
                                      uint8_t cpha,
                                      uint8_t word_size,
                                      uint8_t lsb_first,
                                      uint8_t cs_active);
LA_DECODE_API void la_spi_decoder_reset(la_spi_decoder_t *decoder);
LA_DECODE_API int la_spi_decoder_feed_transition(la_spi_decoder_t *decoder,
                                                  const la_transition_t *transition,
                                                  la_decode_event_callback callback,
                                                  void *context);
LA_DECODE_API int la_spi_decoder_mark_discontinuity(la_spi_decoder_t *decoder,
                                                     uint64_t t_sample,
                                                     la_decode_event_callback callback,
                                                     void *context);

typedef struct la_i2c_decoder {
    int8_t scl_channel;
    int8_t sda_channel;
    uint8_t current_sample;
    uint8_t have_time;
    uint8_t timestamp_valid;
    uint8_t active;
    uint8_t bit_count;
    uint8_t byte_value;
    uint8_t first_byte;
    uint64_t last_time;
    uint64_t byte_end_time;
} la_i2c_decoder_t;

LA_DECODE_API int la_i2c_decoder_init(la_i2c_decoder_t *decoder,
                                      int8_t scl_channel,
                                      int8_t sda_channel);
LA_DECODE_API void la_i2c_decoder_reset(la_i2c_decoder_t *decoder);
LA_DECODE_API int la_i2c_decoder_feed_transition(la_i2c_decoder_t *decoder,
                                                  const la_transition_t *transition,
                                                  la_decode_event_callback callback,
                                                  void *context);
LA_DECODE_API int la_i2c_decoder_mark_discontinuity(la_i2c_decoder_t *decoder,
                                                     uint64_t t_sample,
                                                     la_decode_event_callback callback,
                                                     void *context);

#endif /* LA_DECODE_H */
