#include "la_decode.h"

#include <string.h>

static uint8_t logical_level(const la_uart_decoder_t *decoder)
{
    return (uint8_t)((decoder->current_level ^ decoder->inverted) & 1u);
}

static int emit_event_data(la_decode_event_callback callback,
                           void *context,
                           la_decode_event_kind_t kind,
                           uint64_t t_sample,
                           uint8_t channel,
                           uint8_t flags,
                           const uint8_t *data,
                           uint8_t length)
{
    la_decode_event_t event;

    if (callback == NULL || length > LA_DECODE_MAX_EVENT_DATA || (data == NULL && length != 0u)) {
        return 0;
    }
    memset(&event, 0, sizeof(event));
    event.kind = (uint8_t)kind;
    event.channel = channel;
    event.flags = flags;
    event.t_sample = t_sample;
    event.length = length;
    if (length != 0u) {
        memcpy(event.data, data, length);
    }
    callback(context, &event);
    return 1;
}

static int emit_marker(la_decode_event_callback callback,
                       void *context,
                       la_decode_event_kind_t kind,
                       uint64_t t_sample,
                       uint8_t channel,
                       uint8_t flags)
{
    return emit_event_data(callback, context, kind, t_sample, channel, flags, NULL, 0u);
}

static int emit_word(la_decode_event_callback callback,
                     void *context,
                     uint64_t t_sample,
                     uint8_t channel,
                     uint8_t flags,
                     uint16_t value,
                     uint8_t word_size)
{
    uint8_t data[2];

    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8);
    return emit_event_data(callback, context, LA_DECODE_EVENT_BYTE, t_sample, channel, flags, data,
                           word_size == 16u ? 2u : 1u);
}

static int emit_uart_event(const la_uart_decoder_t *decoder,
                           la_decode_event_callback callback,
                           void *context,
                           la_decode_event_kind_t kind,
                           uint64_t t_sample,
                           uint16_t value)
{
    uint8_t data[2];
    uint8_t length = 0u;

    if (kind == LA_DECODE_EVENT_BYTE) {
        data[0] = (uint8_t)(value & 0xFFu);
        data[1] = (uint8_t)(value >> 8);
        length = value > 0xFFu ? 2u : 1u;
    }
    return emit_event_data(callback, context, kind, t_sample, decoder->channel, 0u, data, length);
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
    decoder->timestamp_valid = 0u;
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
            events += emit_uart_event(decoder, callback, context, LA_DECODE_EVENT_FRAME_ERROR, t_sample, 0u);
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
            events += emit_uart_event(decoder, callback, context, LA_DECODE_EVENT_FRAME_ERROR, t_sample, 0u);
            reset_frame(decoder);
            break;
        }
        ++decoder->stop_index;
        if (decoder->stop_index < decoder->stop_bits) {
            decoder->next_sample += decoder->samples_per_bit;
            break;
        }
        if (decoder->parity_bad != 0u) {
            events += emit_uart_event(decoder, callback, context, LA_DECODE_EVENT_PARITY_ERROR,
                                      decoder->frame_start, 0u);
        }
        events += emit_uart_event(decoder, callback, context, LA_DECODE_EVENT_BYTE,
                                  decoder->frame_start, decoder->data_word);
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

    if (decoder == NULL || (decoder->timestamp_valid != 0u && t_sample < decoder->last_time)) {
        return -1;
    }
    if (decoder->have_time == 0u) {
        decoder->have_time = 1u;
        decoder->timestamp_valid = 1u;
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
    if (decoder->timestamp_valid != 0u && transition->t_sample < decoder->last_time) {
        return -1;
    }
    events = la_uart_decoder_advance(decoder, transition->t_sample, callback, context);
    if (events < 0) {
        return events;
    }
    old_level = decoder->current_level;
    decoder->current_level = (uint8_t)((transition->sample >> decoder->channel) & 1u);
    decoder->last_time = transition->t_sample;
    decoder->timestamp_valid = 1u;
    if (decoder->state == LA_UART_IDLE && old_level == decoder->idle_level &&
        decoder->current_level != decoder->idle_level) {
        decoder->state = LA_UART_START;
        decoder->frame_start = transition->t_sample;
        decoder->next_sample = transition->t_sample + decoder->samples_per_bit / 2u;
        decoder->stop_index = 0u;
    }
    return events;
}

int la_uart_decoder_mark_discontinuity(la_uart_decoder_t *decoder,
                                       uint64_t t_sample,
                                       la_decode_event_callback callback,
                                       void *context)
{
    if (decoder == NULL) {
        return -1;
    }
    la_uart_decoder_reset(decoder);
    decoder->timestamp_valid = 1u;
    decoder->last_time = t_sample;
    return emit_marker(callback, context, LA_DECODE_EVENT_DISCONTINUITY, t_sample,
                       LA_DECODE_NO_CHANNEL, 0u);
}

static int channel_level(uint8_t sample, int8_t channel)
{
    return (sample >> (uint8_t)channel) & 1u;
}

static void spi_clear_word(la_spi_decoder_t *decoder)
{
    decoder->bit_count = 0u;
    decoder->mosi_word = 0u;
    decoder->miso_word = 0u;
}

static int spi_valid_channel(int8_t channel, int required)
{
    return (channel >= 0 && channel <= 7) || (!required && channel == -1);
}

int la_spi_decoder_init(la_spi_decoder_t *decoder,
                        int8_t sck_channel,
                        int8_t mosi_channel,
                        int8_t miso_channel,
                        int8_t cs_channel,
                        uint8_t cpol,
                        uint8_t cpha,
                        uint8_t word_size,
                        uint8_t lsb_first,
                        uint8_t cs_active)
{
    if (decoder == NULL || !spi_valid_channel(sck_channel, 1) ||
        !spi_valid_channel(mosi_channel, 0) || !spi_valid_channel(miso_channel, 0) ||
        !spi_valid_channel(cs_channel, 0) || (mosi_channel == -1 && miso_channel == -1) ||
        cpol > 1u || cpha > 1u || (word_size != 8u && word_size != 16u) || lsb_first > 1u ||
        cs_active > 1u) {
        return -1;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->sck_channel = sck_channel;
    decoder->mosi_channel = mosi_channel;
    decoder->miso_channel = miso_channel;
    decoder->cs_channel = cs_channel;
    decoder->cpol = cpol;
    decoder->cpha = cpha;
    decoder->word_size = word_size;
    decoder->lsb_first = lsb_first;
    decoder->cs_active = cs_active;
    la_spi_decoder_reset(decoder);
    return 0;
}

void la_spi_decoder_reset(la_spi_decoder_t *decoder)
{
    if (decoder == NULL) {
        return;
    }
    decoder->current_sample = 0u;
    decoder->have_time = 0u;
    decoder->timestamp_valid = 0u;
    decoder->selected = decoder->cs_channel == -1 ? 1u : 0u;
    decoder->last_time = 0u;
    spi_clear_word(decoder);
}

int la_spi_decoder_feed_transition(la_spi_decoder_t *decoder,
                                   const la_transition_t *transition,
                                   la_decode_event_callback callback,
                                   void *context)
{
    uint8_t old_sample;
    uint8_t new_sample;
    int events = 0;
    int cs_changed = 0;
    int leading;
    int active_edge;

    if (decoder == NULL || transition == NULL ||
        (decoder->timestamp_valid != 0u && transition->t_sample < decoder->last_time)) {
        return -1;
    }
    if (decoder->have_time == 0u) {
        decoder->current_sample = transition->sample;
        decoder->have_time = 1u;
        decoder->timestamp_valid = 1u;
        decoder->last_time = transition->t_sample;
        if (decoder->cs_channel != -1 &&
            channel_level(transition->sample, decoder->cs_channel) == decoder->cs_active) {
            decoder->selected = 1u;
            events += emit_marker(callback, context, LA_DECODE_EVENT_CS_ASSERT, transition->t_sample,
                                  (uint8_t)decoder->cs_channel, 0u);
        }
        return events;
    }

    old_sample = decoder->current_sample;
    new_sample = transition->sample;
    if (decoder->cs_channel != -1 &&
        channel_level(old_sample, decoder->cs_channel) != channel_level(new_sample, decoder->cs_channel)) {
        cs_changed = 1;
        if (channel_level(new_sample, decoder->cs_channel) == decoder->cs_active) {
            decoder->selected = 1u;
            spi_clear_word(decoder);
            events += emit_marker(callback, context, LA_DECODE_EVENT_CS_ASSERT, transition->t_sample,
                                  (uint8_t)decoder->cs_channel, 0u);
        } else {
            if (decoder->selected && decoder->bit_count != 0u) {
                events += emit_marker(callback, context, LA_DECODE_EVENT_FRAME_ERROR,
                                      transition->t_sample, LA_DECODE_NO_CHANNEL, 0u);
            }
            decoder->selected = 0u;
            spi_clear_word(decoder);
            events += emit_marker(callback, context, LA_DECODE_EVENT_CS_RELEASE, transition->t_sample,
                                  (uint8_t)decoder->cs_channel, 0u);
        }
    }

    leading = channel_level(old_sample, decoder->sck_channel) == decoder->cpol &&
              channel_level(new_sample, decoder->sck_channel) != decoder->cpol;
    active_edge = decoder->cpha == 0u ? leading : !leading;
    if (decoder->selected && !cs_changed &&
        channel_level(old_sample, decoder->sck_channel) != channel_level(new_sample, decoder->sck_channel) &&
        active_edge) {
        uint8_t bit;

        if (decoder->mosi_channel != -1) {
            bit = (uint8_t)channel_level(new_sample, decoder->mosi_channel);
            if (decoder->lsb_first != 0u) {
                decoder->mosi_word |= (uint16_t)bit << decoder->bit_count;
            } else {
                decoder->mosi_word = (uint16_t)((decoder->mosi_word << 1) | bit);
            }
        }
        if (decoder->miso_channel != -1) {
            bit = (uint8_t)channel_level(new_sample, decoder->miso_channel);
            if (decoder->lsb_first != 0u) {
                decoder->miso_word |= (uint16_t)bit << decoder->bit_count;
            } else {
                decoder->miso_word = (uint16_t)((decoder->miso_word << 1) | bit);
            }
        }
        ++decoder->bit_count;
        if (decoder->bit_count == decoder->word_size) {
            if (decoder->mosi_channel != -1) {
                events += emit_word(callback, context, transition->t_sample,
                                    (uint8_t)decoder->mosi_channel, LA_DECODE_EVENT_FLAG_MOSI,
                                    decoder->mosi_word, decoder->word_size);
            }
            if (decoder->miso_channel != -1) {
                events += emit_word(callback, context, transition->t_sample,
                                    (uint8_t)decoder->miso_channel, LA_DECODE_EVENT_FLAG_MISO,
                                    decoder->miso_word, decoder->word_size);
            }
            spi_clear_word(decoder);
        }
    }
    decoder->current_sample = new_sample;
    decoder->last_time = transition->t_sample;
    return events;
}

int la_spi_decoder_mark_discontinuity(la_spi_decoder_t *decoder,
                                      uint64_t t_sample,
                                      la_decode_event_callback callback,
                                      void *context)
{
    if (decoder == NULL) {
        return -1;
    }
    la_spi_decoder_reset(decoder);
    decoder->timestamp_valid = 1u;
    decoder->last_time = t_sample;
    return emit_marker(callback, context, LA_DECODE_EVENT_DISCONTINUITY, t_sample,
                       LA_DECODE_NO_CHANNEL, 0u);
}

static int i2c_valid_channel(int8_t channel)
{
    return channel >= 0 && channel <= 7;
}

int la_i2c_decoder_init(la_i2c_decoder_t *decoder, int8_t scl_channel, int8_t sda_channel)
{
    if (decoder == NULL || !i2c_valid_channel(scl_channel) || !i2c_valid_channel(sda_channel) ||
        scl_channel == sda_channel) {
        return -1;
    }
    memset(decoder, 0, sizeof(*decoder));
    decoder->scl_channel = scl_channel;
    decoder->sda_channel = sda_channel;
    la_i2c_decoder_reset(decoder);
    return 0;
}

void la_i2c_decoder_reset(la_i2c_decoder_t *decoder)
{
    if (decoder == NULL) {
        return;
    }
    decoder->current_sample = 0u;
    decoder->have_time = 0u;
    decoder->timestamp_valid = 0u;
    decoder->active = 0u;
    decoder->bit_count = 0u;
    decoder->byte_value = 0u;
    decoder->first_byte = 1u;
    decoder->last_time = 0u;
    decoder->byte_end_time = 0u;
}

int la_i2c_decoder_feed_transition(la_i2c_decoder_t *decoder,
                                   const la_transition_t *transition,
                                   la_decode_event_callback callback,
                                   void *context)
{
    uint8_t old_sample;
    uint8_t new_sample;
    int old_scl;
    int new_scl;
    int old_sda;
    int new_sda;
    int events = 0;

    if (decoder == NULL || transition == NULL ||
        (decoder->timestamp_valid != 0u && transition->t_sample < decoder->last_time)) {
        return -1;
    }
    if (decoder->have_time == 0u) {
        decoder->current_sample = transition->sample;
        decoder->have_time = 1u;
        decoder->timestamp_valid = 1u;
        decoder->last_time = transition->t_sample;
        return 0;
    }

    old_sample = decoder->current_sample;
    new_sample = transition->sample;
    old_scl = channel_level(old_sample, decoder->scl_channel);
    new_scl = channel_level(new_sample, decoder->scl_channel);
    old_sda = channel_level(old_sample, decoder->sda_channel);
    new_sda = channel_level(new_sample, decoder->sda_channel);

    if (old_scl != 0 && new_scl != 0 && old_sda != new_sda) {
        if (old_sda != 0 && new_sda == 0) {
            decoder->active = 1u;
            decoder->bit_count = 0u;
            decoder->byte_value = 0u;
            decoder->first_byte = 1u;
            events += emit_marker(callback, context, LA_DECODE_EVENT_START, transition->t_sample,
                                  (uint8_t)decoder->sda_channel, 0u);
        } else if (old_sda == 0 && new_sda != 0) {
            if (decoder->active) {
                events += emit_marker(callback, context, LA_DECODE_EVENT_STOP, transition->t_sample,
                                      (uint8_t)decoder->sda_channel, 0u);
            }
            decoder->active = 0u;
            decoder->bit_count = 0u;
            decoder->byte_value = 0u;
        }
    }

    if (decoder->active && old_scl == 0 && new_scl != 0) {
        if (decoder->bit_count < 8u) {
            decoder->byte_value = (uint8_t)((decoder->byte_value << 1) | (uint8_t)new_sda);
            ++decoder->bit_count;
            decoder->byte_end_time = transition->t_sample;
            if (decoder->bit_count == 8u) {
                uint8_t data = decoder->byte_value;

                if (decoder->first_byte != 0u) {
                    uint8_t address = (uint8_t)(data >> 1);

                    events += emit_event_data(callback, context, LA_DECODE_EVENT_ADDRESS,
                                              decoder->byte_end_time, (uint8_t)decoder->sda_channel,
                                              (data & 1u) != 0u ? LA_DECODE_EVENT_FLAG_READ : 0u,
                                              &address, 1u);
                    decoder->first_byte = 0u;
                } else {
                    events += emit_event_data(callback, context, LA_DECODE_EVENT_BYTE,
                                              decoder->byte_end_time, (uint8_t)decoder->sda_channel, 0u,
                                              &data, 1u);
                }
            }
        } else {
            events += emit_marker(callback, context,
                                  new_sda == 0 ? LA_DECODE_EVENT_ACK : LA_DECODE_EVENT_NACK,
                                  transition->t_sample, (uint8_t)decoder->sda_channel, 0u);
            decoder->bit_count = 0u;
            decoder->byte_value = 0u;
        }
    }

    decoder->current_sample = new_sample;
    decoder->last_time = transition->t_sample;
    return events;
}

int la_i2c_decoder_mark_discontinuity(la_i2c_decoder_t *decoder,
                                      uint64_t t_sample,
                                      la_decode_event_callback callback,
                                      void *context)
{
    if (decoder == NULL) {
        return -1;
    }
    la_i2c_decoder_reset(decoder);
    decoder->timestamp_valid = 1u;
    decoder->last_time = t_sample;
    return emit_marker(callback, context, LA_DECODE_EVENT_DISCONTINUITY, t_sample,
                       LA_DECODE_NO_CHANNEL, 0u);
}
