#include "la_proto.h"
#include "la_regs.h"

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

static void put_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)((value >> 8) & 0xFFu);
    output[2] = (uint8_t)((value >> 16) & 0xFFu);
    output[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32_le(const uint8_t *input)
{
    return (uint32_t)input[0] |
           ((uint32_t)input[1] << 8) |
           ((uint32_t)input[2] << 16) |
           ((uint32_t)input[3] << 24);
}

int la_proto_message_type_valid(uint8_t type)
{
    switch (type) {
    case LA_PROTO_PING:
    case LA_PROTO_PONG:
    case LA_PROTO_CONFIG:
    case LA_PROTO_ARM:
    case LA_PROTO_ABORT:
    case LA_PROTO_GET_STATUS:
    case LA_PROTO_DECODER_CFG:
    case LA_PROTO_LIVE_START:
    case LA_PROTO_LIVE_STOP:
    case LA_PROTO_STATUS:
    case LA_PROTO_EVT_TRIGGERED:
    case LA_PROTO_EVT_DECODED:
    case LA_PROTO_EVT_OVERRUN:
    case LA_PROTO_DATA_BEGIN:
    case LA_PROTO_DATA_CHUNK:
    case LA_PROTO_DATA_END:
    case LA_PROTO_ACK:
    case LA_PROTO_NAK:
    case LA_PROTO_LOG:
        return 1;
    default:
        return 0;
    }
}

int la_proto_validate_config(const la_proto_config_t *config)
{
    const uint32_t allowed_ctrl = LA_REG_CTRL_SELFTEST_EN_MASK | LA_REG_CTRL_STREAM_MASK;

    if (config == NULL || (config->ctrl_flags & ~allowed_ctrl) != 0u ||
        (config->mode & ~LA_REG_MODE_ALLOWED_MASK) != 0u ||
        config->pre_samples > LA_REG_PRE_SAMPLES_MAX ||
        config->txn_max_run < LA_REG_TXN_MAX_RUN_MIN ||
        config->txn_max_run > LA_REG_TXN_MAX_RUN_MAX ||
        ((config->ctrl_flags & LA_REG_CTRL_STREAM_MASK) != 0u && config->live_flush_cycles == 0u)) {
        return -1;
    }
    return 0;
}

size_t la_proto_pack_config(const la_proto_config_t *config,
                            uint8_t *output,
                            size_t output_capacity)
{
    if (config == NULL || output == NULL || output_capacity < LA_PROTO_CONFIG_WIRE_SIZE ||
        la_proto_validate_config(config) != 0) {
        return 0u;
    }
    put_u32_le(&output[0], config->ctrl_flags);
    put_u32_le(&output[4], config->sample_div);
    put_u32_le(&output[8], config->mode);
    output[12] = config->chan_en;
    output[13] = config->trig_mask;
    output[14] = config->trig_value;
    output[15] = config->trig_rise;
    output[16] = config->trig_fall;
    put_u32_le(&output[17], config->pre_samples);
    put_u32_le(&output[21], config->post_samples);
    put_u32_le(&output[25], config->timeout);
    put_u32_le(&output[29], config->txn_max_run);
    put_u32_le(&output[33], config->live_flush_cycles);
    return LA_PROTO_CONFIG_WIRE_SIZE;
}

int la_proto_unpack_config(la_proto_config_t *config,
                            const uint8_t *input,
                            size_t input_length)
{
    if (config == NULL || input == NULL || input_length != LA_PROTO_CONFIG_WIRE_SIZE) {
        return -1;
    }
    memset(config, 0, sizeof(*config));
    config->ctrl_flags = get_u32_le(&input[0]);
    config->sample_div = get_u32_le(&input[4]);
    config->mode = get_u32_le(&input[8]);
    config->chan_en = input[12];
    config->trig_mask = input[13];
    config->trig_value = input[14];
    config->trig_rise = input[15];
    config->trig_fall = input[16];
    config->pre_samples = get_u32_le(&input[17]);
    config->post_samples = get_u32_le(&input[21]);
    config->timeout = get_u32_le(&input[25]);
    config->txn_max_run = get_u32_le(&input[29]);
    config->live_flush_cycles = get_u32_le(&input[33]);
    return la_proto_validate_config(config);
}

static int valid_channel(int8_t channel, int required)
{
    return (channel >= 0 && channel <= 7) || (!required && channel == -1);
}

int la_proto_validate_decoder_config(const la_proto_decoder_config_t *config)
{
    const la_proto_spi_config_t *spi;
    const la_proto_i2c_config_t *i2c;
    const la_proto_uart_config_t *uart;

    if (config == NULL || config->slot > 2u) {
        return -1;
    }
    switch (config->type) {
    case LA_PROTO_DECODER_OFF:
        return 0;
    case LA_PROTO_DECODER_UART:
        uart = &config->params.uart;
        return uart->rx_channel <= 7u && uart->data_bits >= 5u && uart->data_bits <= 9u &&
                       uart->parity <= 2u && (uart->stop_bits == 1u || uart->stop_bits == 2u) &&
                       uart->inverted <= 1u && uart->baud_hz != 0u && uart->sample_rate_hz != 0u
                   ? 0
                   : -1;
    case LA_PROTO_DECODER_SPI:
        spi = &config->params.spi;
        return valid_channel(spi->sck_channel, 1) && valid_channel(spi->mosi_channel, 0) &&
                       valid_channel(spi->miso_channel, 0) && valid_channel(spi->cs_channel, 0) &&
                       (spi->mosi_channel != -1 || spi->miso_channel != -1) && spi->cpol <= 1u &&
                       spi->cpha <= 1u && (spi->word_size == 8u || spi->word_size == 16u) &&
                       spi->lsb_first <= 1u && spi->cs_active <= 1u
                   ? 0
                   : -1;
    case LA_PROTO_DECODER_I2C:
        i2c = &config->params.i2c;
        return valid_channel(i2c->scl_channel, 1) && valid_channel(i2c->sda_channel, 1) &&
                       i2c->scl_channel != i2c->sda_channel
                   ? 0
                   : -1;
    default:
        return -1;
    }
}

size_t la_proto_pack_decoder_config(const la_proto_decoder_config_t *config,
                                    uint8_t *output,
                                    size_t output_capacity)
{
    if (config == NULL || output == NULL || output_capacity < LA_PROTO_DECODER_CONFIG_WIRE_SIZE ||
        la_proto_validate_decoder_config(config) != 0) {
        return 0u;
    }
    memset(output, 0, LA_PROTO_DECODER_CONFIG_WIRE_SIZE);
    output[0] = config->slot;
    output[1] = config->type;
    switch (config->type) {
    case LA_PROTO_DECODER_UART:
        output[4] = config->params.uart.rx_channel;
        output[5] = config->params.uart.data_bits;
        output[6] = config->params.uart.parity;
        output[7] = config->params.uart.stop_bits;
        output[8] = config->params.uart.inverted;
        put_u32_le(&output[12], config->params.uart.baud_hz);
        put_u32_le(&output[16], config->params.uart.sample_rate_hz);
        break;
    case LA_PROTO_DECODER_SPI:
        output[4] = (uint8_t)config->params.spi.sck_channel;
        output[5] = (uint8_t)config->params.spi.mosi_channel;
        output[6] = (uint8_t)config->params.spi.miso_channel;
        output[7] = (uint8_t)config->params.spi.cs_channel;
        output[8] = config->params.spi.cpol;
        output[9] = config->params.spi.cpha;
        output[10] = config->params.spi.word_size;
        output[11] = config->params.spi.lsb_first;
        output[12] = config->params.spi.cs_active;
        break;
    case LA_PROTO_DECODER_I2C:
        output[4] = (uint8_t)config->params.i2c.scl_channel;
        output[5] = (uint8_t)config->params.i2c.sda_channel;
        break;
    default:
        break;
    }
    return LA_PROTO_DECODER_CONFIG_WIRE_SIZE;
}

int la_proto_unpack_decoder_config(la_proto_decoder_config_t *config,
                                   const uint8_t *input,
                                   size_t input_length)
{
    if (config == NULL || input == NULL || input_length != LA_PROTO_DECODER_CONFIG_WIRE_SIZE) {
        return -1;
    }
    memset(config, 0, sizeof(*config));
    config->slot = input[0];
    config->type = input[1];
    switch (config->type) {
    case LA_PROTO_DECODER_UART:
        config->params.uart.rx_channel = input[4];
        config->params.uart.data_bits = input[5];
        config->params.uart.parity = input[6];
        config->params.uart.stop_bits = input[7];
        config->params.uart.inverted = input[8];
        config->params.uart.baud_hz = get_u32_le(&input[12]);
        config->params.uart.sample_rate_hz = get_u32_le(&input[16]);
        break;
    case LA_PROTO_DECODER_SPI:
        config->params.spi.sck_channel = (int8_t)input[4];
        config->params.spi.mosi_channel = (int8_t)input[5];
        config->params.spi.miso_channel = (int8_t)input[6];
        config->params.spi.cs_channel = (int8_t)input[7];
        config->params.spi.cpol = input[8];
        config->params.spi.cpha = input[9];
        config->params.spi.word_size = input[10];
        config->params.spi.lsb_first = input[11];
        config->params.spi.cs_active = input[12];
        break;
    case LA_PROTO_DECODER_I2C:
        config->params.i2c.scl_channel = (int8_t)input[4];
        config->params.i2c.sda_channel = (int8_t)input[5];
        break;
    default:
        break;
    }
    return la_proto_validate_decoder_config(config);
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
