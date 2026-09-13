#ifndef LA_DECODER_CORE_H
#define LA_DECODER_CORE_H

/*
 * P7.3 live decoder core (pure logic around common/la_decode).
 *
 * One instance per decoder slot (0..2). The core owns the validated
 * la_proto_decoder_config_t, the concrete UART/SPI/I2C decoder state and the
 * per-slot counters. A decoder task feeds it transition-encoded live words
 * ({run[23:0], sample[7:0]}, the P2.5 la_txn_encoder format) one segment at a
 * time and forwards every emitted la_decode_event_t to a caller callback,
 * which the task turns into EVT_DECODED batch records.
 *
 * Feed semantics: the run of a word holds its sample for `run` samples, so
 * the transition to that sample is at the current sample index and the next
 * word starts at index + run. A value change calls feed_transition; UART
 * additionally advances time to the last sample of every run so bit samples
 * and end-of-frame events inside a long run are not missed. SPI and I2C are
 * edge-driven and only need the transitions. Words split by a packet flush
 * carry the same sample and are absorbed into the run.
 *
 * Gap handling: a segment sequence that is not the expected successor (a
 * dropped segment for this decoder) emits DISCONTINUITY, resets the decoder
 * state machine and increments dropped_segments, so events after the gap are
 * never interpreted as contiguous samples. The sample clock continues from
 * the accumulated run length; per protocol.md, timestamps after a gap are not
 * claimed to be sample-accurate.
 *
 * P7.7 protocol-aware stop: the config carries an optional stop pattern
 * (DECODER_CFG byte 2 plus the type's reserved bytes). The core matches
 * decoded BYTE record data in order with a rolling window; a full match
 * latches stop_on_hit and the matching record's t_sample so the decoder task
 * can ask AcqTask to quiesce the live stream ("trigger on decoded value").
 */

#include "la_decode.h"
#include "la_proto.h"

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_DECODER_API __declspec(dllexport)
#else
#define LA_DECODER_API
#endif

#define LA_DECODER_MAX_SLOTS 3u

typedef struct la_decoder_stats {
    uint32_t events;           /* events emitted (incl. discontinuity) */
    uint32_t dropped_segments; /* sequence gaps observed by this decoder */
} la_decoder_stats_t;

typedef struct la_decoder_core {
    la_proto_decoder_config_t config;
    uint8_t enabled;
    uint8_t have_word;
    uint8_t have_sequence;
    uint32_t expected_sequence;
    uint64_t t_sample;  /* next sample index (end of the last run) */
    uint8_t last_sample;
    uint8_t stop_on_pos;    /* rolling pattern match position */
    uint8_t stop_on_hit;    /* set once the pattern completed */
    uint64_t stop_on_t_sample; /* t_sample of the matching BYTE record */
    la_uart_decoder_t uart;
    la_spi_decoder_t spi;
    la_i2c_decoder_t i2c;
    la_decoder_stats_t stats;
} la_decoder_core_t;

LA_DECODER_API void la_decoder_core_init(la_decoder_core_t *core);

/* Validates config (la_proto_validate_decoder_config), initializes the
 * concrete decoder and resets the sample clock, sequence tracking and
 * counters. A type of OFF disables the slot. Returns 0, or -1 on invalid
 * arguments/config or a decoder init failure. */
LA_DECODER_API int la_decoder_core_configure(la_decoder_core_t *core,
                                             const la_proto_decoder_config_t *config);

LA_DECODER_API int la_decoder_core_enabled(const la_decoder_core_t *core);

/* Feeds one segment of word_count transition words. sequence is the pool
 * segment sequence (gap detection), callback receives every event in order
 * and may be NULL. Returns the number of events emitted, or -1 on invalid
 * arguments / a non-enabled core. */
LA_DECODER_API int la_decoder_core_feed_segment(la_decoder_core_t *core,
                                                const uint32_t *words,
                                                uint32_t word_count,
                                                uint32_t sequence,
                                                la_decode_event_callback callback,
                                                void *context);

LA_DECODER_API const la_decoder_stats_t *la_decoder_core_stats(const la_decoder_core_t *core);
LA_DECODER_API const la_proto_decoder_config_t *la_decoder_core_config(const la_decoder_core_t *core);

/* P7.7: 1 once a decoded BYTE record completed the configured stop pattern. */
LA_DECODER_API int la_decoder_core_stop_on_hit(const la_decoder_core_t *core);

/* P7.7: t_sample of the BYTE record that completed the stop pattern.
 * Returns 0 and stores the timestamp, or -1 when no match has occurred. */
LA_DECODER_API int la_decoder_core_stop_on_sample(const la_decoder_core_t *core,
                                                  uint64_t *t_sample);

#endif /* LA_DECODER_CORE_H */
