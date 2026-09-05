#ifndef LA_DECODE_H
#define LA_DECODE_H

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_DECODE_API __declspec(dllexport)
#else
#define LA_DECODE_API
#endif

#define LA_DECODE_MAX_EVENT_DATA 16u

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
    LA_DECODE_EVENT_DISCONTINUITY = 6
} la_decode_event_kind_t;

typedef struct la_decode_event {
    la_decode_event_kind_t kind;
    uint64_t t_sample;
    uint8_t length;
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

#endif /* LA_DECODE_H */
