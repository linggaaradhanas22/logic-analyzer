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

uint32_t la_crc32_ieee_update(uint32_t crc, const uint8_t *data, size_t length)
{
    size_t index;
    unsigned bit;

    if (data == NULL && length != 0u) {
        return crc;
    }
    for (index = 0; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0; bit < 8u; ++bit) {
            crc = (crc & 1u) != 0u ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return crc;
}

uint32_t la_crc32_ieee_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

uint32_t la_crc32_ieee(const uint8_t *data, size_t length)
{
    if (data == NULL && length != 0u) {
        return 0u;
    }
    return la_crc32_ieee_finish(la_crc32_ieee_update(LA_CRC32_IEEE_INIT, data, length));
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

static void put_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)(value >> 8);
}

static uint16_t get_u16_le(const uint8_t *input)
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static void put_u64_le(uint8_t *output, uint64_t value)
{
    unsigned index;

    for (index = 0u; index < 8u; ++index) {
        output[index] = (uint8_t)(value & 0xFFu);
        value >>= 8;
    }
}

static uint64_t get_u64_le(const uint8_t *input)
{
    uint64_t value = 0u;
    unsigned index;

    for (index = 0u; index < 8u; ++index) {
        value |= (uint64_t)input[index] << (8u * index);
    }
    return value;
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
    case LA_PROTO_PATGEN_CFG:
    case LA_PROTO_TRACE_CTRL:
    case LA_PROTO_EVT_CFG:
    case LA_PROTO_STATUS:
    case LA_PROTO_EVT_TRIGGERED:
    case LA_PROTO_EVT_DECODED:
    case LA_PROTO_EVT_OVERRUN:
    case LA_PROTO_EVT_BLOCK:
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

/* P7.7: the stop pattern must fit the type's reserved bytes and every unused
 * byte of that region must be zero. */
static int valid_stop_on(uint8_t limit, const la_proto_decoder_config_t *config)
{
    uint8_t index;

    if (config->stop_on_len > limit) {
        return 0;
    }
    for (index = config->stop_on_len; index < limit; ++index) {
        if (config->stop_on[index] != 0u) {
            return 0;
        }
    }
    return 1;
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
        return config->stop_on_len == 0u ? 0 : -1;
    case LA_PROTO_DECODER_UART:
        uart = &config->params.uart;
        return uart->rx_channel <= 7u && uart->data_bits >= 5u && uart->data_bits <= 9u &&
                       uart->parity <= 2u && (uart->stop_bits == 1u || uart->stop_bits == 2u) &&
                       uart->inverted <= 1u && uart->baud_hz != 0u && uart->sample_rate_hz != 0u &&
                       valid_stop_on(LA_PROTO_STOP_ON_MAX_UART, config)
                   ? 0
                   : -1;
    case LA_PROTO_DECODER_SPI:
        spi = &config->params.spi;
        return valid_channel(spi->sck_channel, 1) && valid_channel(spi->mosi_channel, 0) &&
                       valid_channel(spi->miso_channel, 0) && valid_channel(spi->cs_channel, 0) &&
                       (spi->mosi_channel != -1 || spi->miso_channel != -1) && spi->cpol <= 1u &&
                       spi->cpha <= 1u && (spi->word_size == 8u || spi->word_size == 16u) &&
                       spi->lsb_first <= 1u && spi->cs_active <= 1u &&
                       valid_stop_on(LA_PROTO_STOP_ON_MAX_SPI, config)
                   ? 0
                   : -1;
    case LA_PROTO_DECODER_I2C:
        i2c = &config->params.i2c;
        return valid_channel(i2c->scl_channel, 1) && valid_channel(i2c->sda_channel, 1) &&
                       i2c->scl_channel != i2c->sda_channel &&
                       valid_stop_on(LA_PROTO_STOP_ON_MAX_I2C, config)
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
    output[2] = config->stop_on_len;
    switch (config->type) {
    case LA_PROTO_DECODER_UART:
        output[4] = config->params.uart.rx_channel;
        output[5] = config->params.uart.data_bits;
        output[6] = config->params.uart.parity;
        output[7] = config->params.uart.stop_bits;
        output[8] = config->params.uart.inverted;
        memcpy(&output[9], config->stop_on, LA_PROTO_STOP_ON_MAX_UART);
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
        memcpy(&output[13], config->stop_on, LA_PROTO_STOP_ON_MAX_SPI);
        break;
    case LA_PROTO_DECODER_I2C:
        output[4] = (uint8_t)config->params.i2c.scl_channel;
        output[5] = (uint8_t)config->params.i2c.sda_channel;
        memcpy(&output[6], config->stop_on, LA_PROTO_STOP_ON_MAX_I2C);
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
    config->stop_on_len = input[2];
    switch (config->type) {
    case LA_PROTO_DECODER_UART:
        config->params.uart.rx_channel = input[4];
        config->params.uart.data_bits = input[5];
        config->params.uart.parity = input[6];
        config->params.uart.stop_bits = input[7];
        config->params.uart.inverted = input[8];
        memcpy(config->stop_on, &input[9], LA_PROTO_STOP_ON_MAX_UART);
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
        memcpy(config->stop_on, &input[13], LA_PROTO_STOP_ON_MAX_SPI);
        break;
    case LA_PROTO_DECODER_I2C:
        config->params.i2c.scl_channel = (int8_t)input[4];
        config->params.i2c.sda_channel = (int8_t)input[5];
        memcpy(config->stop_on, &input[6], LA_PROTO_STOP_ON_MAX_I2C);
        break;
    default:
        break;
    }
    return la_proto_validate_decoder_config(config);
}

size_t la_proto_pack_patgen_cfg(uint32_t patgen_ctrl,
                                uint8_t *output,
                                size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_PATGEN_CFG_WIRE_SIZE ||
        (patgen_ctrl & ~LA_REG_PATGEN_CTRL_ALLOWED_MASK) != 0u ||
        (patgen_ctrl & LA_REG_PATGEN_CTRL_MODE_MASK) >>
                LA_REG_PATGEN_CTRL_MODE_LSB >
            LA_REG_PATGEN_CTRL_MODE_VALUE_UART_SINGLE) {
        return 0u;
    }
    put_u32_le(&output[0], patgen_ctrl);
    return LA_PROTO_PATGEN_CFG_WIRE_SIZE;
}

int la_proto_unpack_patgen_cfg(uint32_t *patgen_ctrl,
                               const uint8_t *input,
                               size_t input_length)
{
    uint32_t value;

    if (patgen_ctrl == NULL || input == NULL ||
        input_length != LA_PROTO_PATGEN_CFG_WIRE_SIZE) {
        return -1;
    }
    value = get_u32_le(input);
    if ((value & ~LA_REG_PATGEN_CTRL_ALLOWED_MASK) != 0u ||
        ((value & LA_REG_PATGEN_CTRL_MODE_MASK) >> LA_REG_PATGEN_CTRL_MODE_LSB) >
            LA_REG_PATGEN_CTRL_MODE_VALUE_UART_SINGLE) {
        return -1;
    }
    *patgen_ctrl = value;
    return 0;
}

size_t la_proto_pack_trace_ctrl(uint8_t action,
                                uint8_t *output,
                                size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_TRACE_CTRL_WIRE_SIZE ||
        action < LA_PROTO_TRACE_STOP || action > LA_PROTO_TRACE_DUMP) {
        return 0u;
    }
    output[0] = action;
    output[1] = 0u;
    output[2] = 0u;
    output[3] = 0u;
    return LA_PROTO_TRACE_CTRL_WIRE_SIZE;
}

int la_proto_unpack_trace_ctrl(uint8_t *action,
                               const uint8_t *input,
                               size_t input_length)
{
    if (action == NULL || input == NULL ||
        input_length != LA_PROTO_TRACE_CTRL_WIRE_SIZE ||
        input[0] < LA_PROTO_TRACE_STOP || input[0] > LA_PROTO_TRACE_DUMP) {
        return -1;
    }
    *action = input[0];
    return 0;
}

size_t la_proto_pack_evt_cfg(uint8_t format,
                             uint8_t *output,
                             size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_EVT_CFG_WIRE_SIZE ||
        format > LA_PROTO_EVT_FORMAT_BLOCK) {
        return 0u;
    }
    output[0] = format;
    output[1] = 0u;
    output[2] = 0u;
    output[3] = 0u;
    return LA_PROTO_EVT_CFG_WIRE_SIZE;
}

int la_proto_unpack_evt_cfg(uint8_t *format,
                            const uint8_t *input,
                            size_t input_length)
{
    if (format == NULL || input == NULL ||
        input_length != LA_PROTO_EVT_CFG_WIRE_SIZE ||
        input[0] > LA_PROTO_EVT_FORMAT_BLOCK) {
        return -1;
    }
    *format = input[0];
    return 0;
}

size_t la_proto_pack_link_cfg(uint8_t transport,
                              uint8_t *output,
                              size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_LINK_CFG_WIRE_SIZE ||
        transport > LA_PROTO_LINK_SPI) {
        return 0u;
    }
    output[0] = transport;
    output[1] = 0u;
    output[2] = 0u;
    output[3] = 0u;
    return LA_PROTO_LINK_CFG_WIRE_SIZE;
}

int la_proto_unpack_link_cfg(uint8_t *transport,
                             const uint8_t *input,
                             size_t input_length)
{
    if (transport == NULL || input == NULL ||
        input_length != LA_PROTO_LINK_CFG_WIRE_SIZE ||
        input[0] > LA_PROTO_LINK_SPI) {
        return -1;
    }
    *transport = input[0];
    return 0;
}

size_t la_proto_pack_test_record(uint32_t record_id,
                                 uint8_t *output,
                                 size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_TEST_RECORD_WIRE_SIZE) {
        return 0u;
    }
    put_u32_le(&output[0], record_id);
    return LA_PROTO_TEST_RECORD_WIRE_SIZE;
}

int la_proto_unpack_test_record(uint32_t *record_id,
                                const uint8_t *input,
                                size_t input_length)
{
    if (record_id == NULL || input == NULL ||
        input_length != LA_PROTO_TEST_RECORD_WIRE_SIZE) {
        return -1;
    }
    *record_id = get_u32_le(input);
    return 0;
}

size_t la_proto_pack_pong(const la_proto_pong_t *pong,
                          uint8_t *output,
                          size_t output_capacity)
{
    if (pong == NULL || output == NULL || output_capacity < LA_PROTO_PONG_WIRE_SIZE) {
        return 0u;
    }
    put_u32_le(&output[0], pong->fw_version);
    put_u32_le(&output[4], pong->pl_id);
    put_u64_le(&output[8], pong->uptime_ms);
    return LA_PROTO_PONG_WIRE_SIZE;
}

int la_proto_unpack_pong(la_proto_pong_t *pong,
                         const uint8_t *input,
                         size_t input_length)
{
    if (pong == NULL || input == NULL || input_length != LA_PROTO_PONG_WIRE_SIZE) {
        return -1;
    }
    memset(pong, 0, sizeof(*pong));
    pong->fw_version = get_u32_le(&input[0]);
    pong->pl_id = get_u32_le(&input[4]);
    pong->uptime_ms = get_u64_le(&input[8]);
    return 0;
}

size_t la_proto_pack_ack(uint8_t acknowledged_type,
                         uint8_t sequence,
                         uint8_t *output,
                         size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_ACK_WIRE_SIZE) {
        return 0u;
    }
    output[0] = acknowledged_type;
    output[1] = sequence;
    return LA_PROTO_ACK_WIRE_SIZE;
}

int la_proto_unpack_ack(uint8_t *acknowledged_type,
                        uint8_t *sequence,
                        const uint8_t *input,
                        size_t input_length)
{
    if (acknowledged_type == NULL || sequence == NULL || input == NULL ||
        input_length != LA_PROTO_ACK_WIRE_SIZE) {
        return -1;
    }
    *acknowledged_type = input[0];
    *sequence = input[1];
    return 0;
}

size_t la_proto_pack_nak(uint8_t nak_type,
                         uint8_t sequence,
                         uint8_t error,
                         uint8_t *output,
                         size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_NAK_WIRE_SIZE) {
        return 0u;
    }
    output[0] = nak_type;
    output[1] = sequence;
    output[2] = error;
    return LA_PROTO_NAK_WIRE_SIZE;
}

int la_proto_unpack_nak(uint8_t *nak_type,
                        uint8_t *sequence,
                        uint8_t *error,
                        const uint8_t *input,
                        size_t input_length)
{
    if (nak_type == NULL || sequence == NULL || error == NULL || input == NULL ||
        input_length != LA_PROTO_NAK_WIRE_SIZE) {
        return -1;
    }
    *nak_type = input[0];
    *sequence = input[1];
    *error = input[2];
    return 0;
}

size_t la_proto_pack_evt_overrun(uint32_t dropped,
                                 uint8_t stage,
                                 uint8_t *output,
                                 size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_EVT_OVERRUN_WIRE_SIZE) {
        return 0u;
    }
    memset(output, 0, LA_PROTO_EVT_OVERRUN_WIRE_SIZE);
    put_u32_le(&output[0], dropped);
    output[4] = stage;
    return LA_PROTO_EVT_OVERRUN_WIRE_SIZE;
}

int la_proto_unpack_evt_overrun(uint32_t *dropped,
                                uint8_t *stage,
                                const uint8_t *input,
                                size_t input_length)
{
    if (dropped == NULL || stage == NULL || input == NULL ||
        input_length != LA_PROTO_EVT_OVERRUN_WIRE_SIZE) {
        return -1;
    }
    *dropped = get_u32_le(&input[0]);
    *stage = input[4];
    return 0;
}

size_t la_proto_pack_status(const la_proto_status_t *status,
                            uint8_t *output,
                            size_t output_capacity)
{
    unsigned index;

    if (status == NULL || output == NULL || output_capacity < LA_PROTO_STATUS_WIRE_SIZE) {
        return 0u;
    }
    memset(output, 0, LA_PROTO_STATUS_WIRE_SIZE);
    output[0] = status->state;
    put_u32_le(&output[4], status->status_reg);
    put_u64_le(&output[8], status->word_count);
    put_u64_le(&output[16], status->trig_index);
    put_u16_le(&output[24], status->cpu_load_permille);
    put_u16_le(&output[26], status->min_free_stack);
    for (index = 0u; index < 3u; ++index) {
        put_u32_le(&output[28u + 8u * index], status->decoders[index].events);
        put_u32_le(&output[32u + 8u * index], status->decoders[index].dropped_segments);
    }
    return LA_PROTO_STATUS_WIRE_SIZE;
}

int la_proto_unpack_status(la_proto_status_t *status,
                           const uint8_t *input,
                           size_t input_length)
{
    unsigned index;

    if (status == NULL || input == NULL || input_length != LA_PROTO_STATUS_WIRE_SIZE) {
        return -1;
    }
    memset(status, 0, sizeof(*status));
    status->state = input[0];
    status->status_reg = get_u32_le(&input[4]);
    status->word_count = get_u64_le(&input[8]);
    status->trig_index = get_u64_le(&input[16]);
    status->cpu_load_permille = get_u16_le(&input[24]);
    status->min_free_stack = get_u16_le(&input[26]);
    for (index = 0u; index < 3u; ++index) {
        status->decoders[index].events = get_u32_le(&input[28u + 8u * index]);
        status->decoders[index].dropped_segments = get_u32_le(&input[32u + 8u * index]);
    }
    return 0;
}

size_t la_proto_pack_data_begin(const la_proto_data_begin_t *begin,
                                uint8_t *output,
                                size_t output_capacity)
{
    if (begin == NULL || output == NULL || output_capacity < LA_PROTO_DATA_BEGIN_WIRE_SIZE) {
        return 0u;
    }
    put_u64_le(&output[0], begin->total_words);
    output[8] = begin->encoding;
    memset(&output[9], 0, 3u);
    put_u32_le(&output[12], begin->sample_rate_hz);
    return LA_PROTO_DATA_BEGIN_WIRE_SIZE;
}

int la_proto_unpack_data_begin(la_proto_data_begin_t *begin,
                               const uint8_t *input,
                               size_t input_length)
{
    if (begin == NULL || input == NULL || input_length != LA_PROTO_DATA_BEGIN_WIRE_SIZE) {
        return -1;
    }
    memset(begin, 0, sizeof(*begin));
    begin->total_words = get_u64_le(&input[0]);
    begin->encoding = input[8];
    begin->sample_rate_hz = get_u32_le(&input[12]);
    return 0;
}

size_t la_proto_pack_data_end(uint32_t crc32,
                              uint8_t *output,
                              size_t output_capacity)
{
    if (output == NULL || output_capacity < LA_PROTO_DATA_END_WIRE_SIZE) {
        return 0u;
    }
    put_u32_le(output, crc32);
    return LA_PROTO_DATA_END_WIRE_SIZE;
}

int la_proto_unpack_data_end(uint32_t *crc32,
                             const uint8_t *input,
                             size_t input_length)
{
    if (crc32 == NULL || input == NULL || input_length != LA_PROTO_DATA_END_WIRE_SIZE) {
        return -1;
    }
    *crc32 = get_u32_le(input);
    return 0;
}

size_t la_proto_pack_data_chunk(uint32_t byte_offset,
                                const uint8_t *data,
                                size_t data_length,
                                uint8_t *output,
                                size_t output_capacity)
{
    if (output == NULL || (data == NULL && data_length != 0u) ||
        data_length > LA_PROTO_DATA_CHUNK_MAX ||
        data_length > (size_t)(UINT32_MAX - byte_offset) ||
        output_capacity < LA_PROTO_DATA_CHUNK_FIELD_SIZE + data_length) {
        return 0u;
    }
    put_u32_le(output, byte_offset);
    if (data_length != 0u) {
        memcpy(&output[LA_PROTO_DATA_CHUNK_FIELD_SIZE], data, data_length);
    }
    return (size_t)LA_PROTO_DATA_CHUNK_FIELD_SIZE + data_length;
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
