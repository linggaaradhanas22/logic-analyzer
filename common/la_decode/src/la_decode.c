#include "la_decode.h"

#include <string.h>

static uint8_t logical_level(const la_uart_decoder_t *decoder)
{
    return (uint8_t)((decoder->current_level ^ decoder->inverted) & 1u);
}

static int emit_event(la_decode_event_callback callback,
                      void *context,
                      la_decode_event_kind_t kind,
                      uint64_t t_sample,
                      uint16_t value)
{
    la_decode_event_t event;

    if (callback == NULL) {
        return 0;
    }
    memset(&event, 0, sizeof(event));
    event.kind = kind;
    event.t_sample = t_sample;
    if (kind == LA_DECODE_EVENT_BYTE) {
        event.length = value > 0xFFu ? 2u : 1u;
        event.data[0] = (uint8_t)(value & 0xFFu);
        if (event.length == 2u) {
            event.data[1] = (uint8_t)(value >> 8);
        }
    }
    callback(context, &event);
    return 1;
}

static void reset_frame(la_uart_decoder_t *decoder)
{
    decoder->state = LA_UART_IDLE;
    decoder->bit_index = 0u;
    decoder->stop_index = 0u;
    decoder->parity_bad = 0u;
    decoder->data_word = 0u;
    decoder->parity_accum = 0u;
    decoder->next_sample = 0u;
}

int la_uart_decoder_init(la_uart_decoder_t *decoder,
                         uint8_t channel,
                         uint32_t samples_per_bit,
                         uint8_t data_bits,
                         la_uart_parity_t parity,
                         uint8_t stop_bits,
                         uint8_t inverted)
{
    if (decoder == NULL || channel > 7u || samples_per_bit == 0u || data_bits < 5u || data_bits > 9u ||
        (parity != LA_UART_PARITY_NONE && parity != LA_UART_PARITY_EVEN &&
         parity != LA_UART_PARITY_ODD) ||
        (stop_bits != 1u && stop_bits != 2u) || inverted > 1u) {
        return -1;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->channel = channel;
    decoder->samples_per_bit = samples_per_bit;
    decoder->data_bits = data_bits;
    decoder->parity = parity;
    decoder->stop_bits = stop_bits;
    decoder->inverted = inverted;
    decoder->idle_level = inverted != 0u ? 0u : 1u;
    decoder->current_level = decoder->idle_level;
    reset_frame(decoder);
    return 0;
}

void la_uart_decoder_reset(la_uart_decoder_t *decoder)
{
    if (decoder == NULL) {
        return;
    }
    decoder->current_level = decoder->idle_level;
    decoder->have_time = 0u;
    decoder->last_time = 0u;
    reset_frame(decoder);
}

static int process_sample(la_uart_decoder_t *decoder,
                          uint64_t t_sample,
                          la_decode_event_callback callback,
                          void *context)
{
    uint8_t level;
    int events = 0;

    switch (decoder->state) {
    case LA_UART_START:
        if (decoder->current_level != (uint8_t)(decoder->idle_level ^ 1u)) {
            events += emit_event(callback, context, LA_DECODE_EVENT_FRAME_ERROR, t_sample, 0u);
            reset_frame(decoder);
            break;
        }
        decoder->state = LA_UART_DATA;
        decoder->bit_index = 0u;
        decoder->data_word = 0u;
        decoder->parity_accum = 0u;
        decoder->next_sample = decoder->frame_start + decoder->samples_per_bit +
                               decoder->samples_per_bit / 2u;
        break;

    case LA_UART_DATA:
        level = logical_level(decoder);
        if (level != 0u) {
            decoder->data_word |= (uint16_t)1u << decoder->bit_index;
            decoder->parity_accum ^= 1u;
        }
        ++decoder->bit_index;
        decoder->next_sample += decoder->samples_per_bit;
        if (decoder->bit_index == decoder->data_bits) {
            decoder->state = decoder->parity == LA_UART_PARITY_NONE ? LA_UART_STOP : LA_UART_PARITY;
        }
        break;

    case LA_UART_PARITY:
        level = logical_level(decoder);
        if (decoder->parity == LA_UART_PARITY_EVEN) {
            decoder->parity_bad = (uint8_t)(level != decoder->parity_accum);
        } else {
            decoder->parity_bad = (uint8_t)(level == decoder->parity_accum);
        }
        decoder->next_sample += decoder->samples_per_bit;
        decoder->state = LA_UART_STOP;
        break;

    case LA_UART_STOP:
        if (decoder->current_level != decoder->idle_level) {
            events += emit_event(callback, context, LA_DECODE_EVENT_FRAME_ERROR, t_sample, 0u);
            reset_frame(decoder);
            break;
        }
        ++decoder->stop_index;
        if (decoder->stop_index < decoder->stop_bits) {
            decoder->next_sample += decoder->samples_per_bit;
            break;
        }
        if (decoder->parity_bad != 0u) {
            events += emit_event(callback, context, LA_DECODE_EVENT_PARITY_ERROR, decoder->frame_start, 0u);
        }
        events += emit_event(callback, context, LA_DECODE_EVENT_BYTE, decoder->frame_start,
                             decoder->data_word);
        reset_frame(decoder);
        break;

    case LA_UART_IDLE:
    default:
        decoder->next_sample = 0u;
        break;
    }
    return events;
}

int la_uart_decoder_advance(la_uart_decoder_t *decoder,
                            uint64_t t_sample,
                            la_decode_event_callback callback,
                            void *context)
{
    int events = 0;

    if (decoder == NULL || (decoder->have_time != 0u && t_sample < decoder->last_time)) {
        return -1;
    }
    if (decoder->have_time == 0u) {
        decoder->have_time = 1u;
    }
    while (decoder->state != LA_UART_IDLE && decoder->next_sample <= t_sample) {
        events += process_sample(decoder, decoder->next_sample, callback, context);
    }
    decoder->last_time = t_sample;
    return events;
}

int la_uart_decoder_feed_transition(la_uart_decoder_t *decoder,
                                    const la_transition_t *transition,
                                    la_decode_event_callback callback,
                                    void *context)
{
    uint8_t old_level;
    int events;

    if (decoder == NULL || transition == NULL || decoder->channel > 7u) {
        return -1;
    }
    if (decoder->have_time != 0u && transition->t_sample < decoder->last_time) {
        return -1;
    }
    events = la_uart_decoder_advance(decoder, transition->t_sample, callback, context);
    if (events < 0) {
        return events;
    }
    old_level = decoder->current_level;
    decoder->current_level = (uint8_t)((transition->sample >> decoder->channel) & 1u);
    decoder->last_time = transition->t_sample;
    if (decoder->state == LA_UART_IDLE && old_level == decoder->idle_level &&
        decoder->current_level != decoder->idle_level) {
        decoder->state = LA_UART_START;
        decoder->frame_start = transition->t_sample;
        decoder->next_sample = transition->t_sample + decoder->samples_per_bit / 2u;
        decoder->stop_index = 0u;
    }
    return events;
}
