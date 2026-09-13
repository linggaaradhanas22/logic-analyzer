#include "la_decoder_core.h"

#include <string.h>

typedef struct la_decoder_sink {
    la_decoder_core_t *core;
    la_decode_event_callback callback;
    void *context;
    int events;
} la_decoder_sink_t;

static void decoder_match_stop_on(la_decoder_core_t *core,
                                  const la_decode_event_t *event)
{
    uint8_t index;

    if (core->config.stop_on_len == 0u || core->stop_on_hit != 0u ||
        event->kind != LA_DECODE_EVENT_BYTE) {
        return;
    }
    for (index = 0u; index < event->length; ++index) {
        uint8_t value = event->data[index];

        if (value == core->config.stop_on[core->stop_on_pos]) {
            core->stop_on_pos++;
            if (core->stop_on_pos == core->config.stop_on_len) {
                core->stop_on_hit = 1u;
                core->stop_on_t_sample = event->t_sample;
                return;
            }
        } else {
            /* Restart at the first pattern byte when this byte matches it
             * (it may already start the next match). */
            core->stop_on_pos = (value == core->config.stop_on[0]) ? 1u : 0u;
        }
    }
}

static void decoder_emit(void *context, const la_decode_event_t *event)
{
    la_decoder_sink_t *sink = (la_decoder_sink_t *)context;

    sink->events++;
    sink->core->stats.events++;
    decoder_match_stop_on(sink->core, event);
    if (sink->callback != NULL) {
        sink->callback(sink->context, event);
    }
}

static int decoder_mark_discontinuity(la_decoder_core_t *core, la_decoder_sink_t *sink)
{
    int emitted;

    switch (core->config.type) {
    case LA_PROTO_DECODER_UART:
        emitted = la_uart_decoder_mark_discontinuity(&core->uart, core->t_sample,
                                                     decoder_emit, sink);
        break;
    case LA_PROTO_DECODER_SPI:
        emitted = la_spi_decoder_mark_discontinuity(&core->spi, core->t_sample,
                                                    decoder_emit, sink);
        break;
    case LA_PROTO_DECODER_I2C:
        emitted = la_i2c_decoder_mark_discontinuity(&core->i2c, core->t_sample,
                                                    decoder_emit, sink);
        break;
    default:
        emitted = 0;
        break;
    }
    return emitted < 0 ? -1 : emitted;
}

void la_decoder_core_init(la_decoder_core_t *core)
{
    if (core == NULL) {
        return;
    }
    memset(core, 0, sizeof(*core));
    core->config.type = LA_PROTO_DECODER_OFF;
}

int la_decoder_core_configure(la_decoder_core_t *core,
                              const la_proto_decoder_config_t *config)
{
    if (core == NULL || config == NULL ||
        la_proto_validate_decoder_config(config) != 0) {
        return -1;
    }
    core->config = *config;
    core->enabled = 0u;
    core->have_word = 0u;
    core->have_sequence = 0u;
    core->expected_sequence = 0u;
    core->t_sample = 0u;
    core->last_sample = 0u;
    core->stop_on_pos = 0u;
    core->stop_on_hit = 0u;
    core->stop_on_t_sample = 0u;
    memset(&core->stats, 0, sizeof(core->stats));

    switch (config->type) {
    case LA_PROTO_DECODER_UART: {
        uint32_t samples_per_bit = config->params.uart.sample_rate_hz /
                                   config->params.uart.baud_hz;

        if (samples_per_bit == 0u ||
            la_uart_decoder_init(&core->uart, config->params.uart.rx_channel,
                                 samples_per_bit, config->params.uart.data_bits,
                                 (la_uart_parity_t)config->params.uart.parity,
                                 config->params.uart.stop_bits,
                                 config->params.uart.inverted) != 0) {
            core->enabled = 0u;
            return -1;
        }
        break;
    }
    case LA_PROTO_DECODER_SPI:
        if (la_spi_decoder_init(&core->spi,
                                config->params.spi.sck_channel,
                                config->params.spi.mosi_channel,
                                config->params.spi.miso_channel,
                                config->params.spi.cs_channel,
                                config->params.spi.cpol,
                                config->params.spi.cpha,
                                config->params.spi.word_size,
                                config->params.spi.lsb_first,
                                config->params.spi.cs_active) != 0) {
            core->enabled = 0u;
            return -1;
        }
        break;
    case LA_PROTO_DECODER_I2C:
        if (la_i2c_decoder_init(&core->i2c, config->params.i2c.scl_channel,
                                config->params.i2c.sda_channel) != 0) {
            core->enabled = 0u;
            return -1;
        }
        break;
    case LA_PROTO_DECODER_OFF:
    default:
        core->enabled = 0u;
        return 0;
    }
    core->enabled = 1u;
    return 0;
}

int la_decoder_core_enabled(const la_decoder_core_t *core)
{
    return core != NULL && core->enabled != 0u;
}

int la_decoder_core_feed_segment(la_decoder_core_t *core,
                                 const uint32_t *words,
                                 uint32_t word_count,
                                 uint32_t sequence,
                                 la_decode_event_callback callback,
                                 void *context)
{
    la_decoder_sink_t sink;
    uint32_t index;

    if (core == NULL || !core->enabled || (words == NULL && word_count != 0u)) {
        return -1;
    }
    sink.core = core;
    sink.callback = callback;
    sink.context = context;
    sink.events = 0;

    if (sequence != 0u) {
        if (core->have_sequence != 0u && sequence != core->expected_sequence) {
            if (decoder_mark_discontinuity(core, &sink) < 0) {
                return -1;
            }
            core->stats.dropped_segments++;
            core->have_word = 0u;
        }
        core->expected_sequence = sequence + 1u;
        core->have_sequence = 1u;
    }

    for (index = 0u; index < word_count; ++index) {
        uint32_t run = words[index] >> 8;
        uint8_t sample = (uint8_t)(words[index] & 0xFFu);
        int emitted = 0;

        if (run == 0u) {
            continue;
        }
        if (core->have_word == 0u || sample != core->last_sample) {
            la_transition_t transition;

            transition.t_sample = core->t_sample;
            transition.sample = sample;
            switch (core->config.type) {
            case LA_PROTO_DECODER_UART:
                emitted = la_uart_decoder_feed_transition(&core->uart, &transition,
                                                          decoder_emit, &sink);
                break;
            case LA_PROTO_DECODER_SPI:
                emitted = la_spi_decoder_feed_transition(&core->spi, &transition,
                                                         decoder_emit, &sink);
                break;
            case LA_PROTO_DECODER_I2C:
                emitted = la_i2c_decoder_feed_transition(&core->i2c, &transition,
                                                         decoder_emit, &sink);
                break;
            default:
                break;
            }
            if (emitted < 0) {
                return -1;
            }
            core->have_word = 1u;
        }
        if (core->config.type == LA_PROTO_DECODER_UART) {
            /* Advance over the run so UART bit sampling inside a long run and
             * end-of-frame events do not wait for the next transition. */
            if (la_uart_decoder_advance(&core->uart, core->t_sample + run - 1u,
                                        decoder_emit, &sink) < 0) {
                return -1;
            }
        }
        core->t_sample += run;
        core->last_sample = sample;
    }
    return sink.events;
}

const la_decoder_stats_t *la_decoder_core_stats(const la_decoder_core_t *core)
{
    return core != NULL ? &core->stats : NULL;
}

const la_proto_decoder_config_t *la_decoder_core_config(const la_decoder_core_t *core)
{
    return core != NULL ? &core->config : NULL;
}

int la_decoder_core_stop_on_hit(const la_decoder_core_t *core)
{
    return core != NULL && core->stop_on_hit != 0u;
}

int la_decoder_core_stop_on_sample(const la_decoder_core_t *core,
                                   uint64_t *t_sample)
{
    if (core == NULL || t_sample == NULL || core->stop_on_hit == 0u) {
        return -1;
    }
    *t_sample = core->stop_on_t_sample;
    return 0;
}
