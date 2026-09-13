#include "la_cmd_dispatch.h"
#include "la_regs.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

#define TEST_FW_VERSION (0x00000001u)
#define TEST_PL_ID (0x4C410002u)

static void make_config(la_proto_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->ctrl_flags = 0u;
    config->sample_div = 9u;
    config->mode = 0u;
    config->chan_en = 0xFFu;
    config->trig_mask = 0x03u;
    config->trig_value = 0x01u;
    config->trig_rise = 0x02u;
    config->trig_fall = 0x04u;
    config->pre_samples = 1024u;
    config->post_samples = 8192u;
    config->timeout = 500000u;
    config->txn_max_run = 0x00FFFFFFu;
    config->live_flush_cycles = 0u;
}

static int send_config(la_cmd_dispatch_t *dispatch, uint8_t sequence, la_cmd_result_t *result)
{
    la_proto_config_t config;
    uint8_t payload[LA_PROTO_CONFIG_WIRE_SIZE];

    make_config(&config);
    (void)la_proto_pack_config(&config, payload, sizeof(payload));
    return la_cmd_dispatch_step(dispatch, LA_PROTO_CONFIG, sequence, payload,
                                (uint16_t)sizeof(payload), 0u, result);
}

static int send_stream_config(la_cmd_dispatch_t *dispatch,
                              uint8_t sequence,
                              la_cmd_result_t *result)
{
    la_proto_config_t config;
    uint8_t payload[LA_PROTO_CONFIG_WIRE_SIZE];

    make_config(&config);
    config.ctrl_flags = LA_REG_CTRL_STREAM_MASK;
    config.live_flush_cycles = 200000u;
    (void)la_proto_pack_config(&config, payload, sizeof(payload));
    return la_cmd_dispatch_step(dispatch, LA_PROTO_CONFIG, sequence, payload,
                                (uint16_t)sizeof(payload), 0u, result);
}

static int test_init_clears(void)
{
    la_cmd_dispatch_t dispatch;

    memset(&dispatch, 0xFF, sizeof(dispatch));
    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "init resets to IDLE");
    CHECK(la_cmd_dispatch_config_valid(&dispatch) == 0, "init clears the config latch");
    return 0;
}

static int test_ping_pong(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    la_proto_pong_t pong;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PING, 0x42u, NULL, 0u,
                               UINT64_C(0x0102030405060708), &result) == 0,
          "PING accepted");
    CHECK(result.outcome == LA_CMD_OUT_PONG, "PING produces PONG");
    CHECK(la_proto_unpack_pong(&pong, result.payload, LA_PROTO_PONG_WIRE_SIZE) == 0 &&
              pong.fw_version == TEST_FW_VERSION && pong.pl_id == TEST_PL_ID &&
              pong.uptime_ms == UINT64_C(0x0102030405060708),
          "PONG carries firmware identity and caller uptime");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PING, 1u, (const uint8_t *)"x", 1u, 0u,
                               &result) == 0,
          "PING with payload rejected");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH &&
              result.sequence == 1u && result.type == LA_PROTO_PING,
          "PING bad length NAK");
    return 0;
}

static int test_unknown_and_inbound_rejected(void)
{
    static const uint8_t types[] = {
        LA_PROTO_PONG, LA_PROTO_STATUS, LA_PROTO_EVT_TRIGGERED, LA_PROTO_EVT_DECODED,
        LA_PROTO_EVT_OVERRUN, LA_PROTO_DATA_BEGIN, LA_PROTO_DATA_CHUNK, LA_PROTO_DATA_END,
        LA_PROTO_LOG};
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    size_t index;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, 0x03u, 7u, NULL, 0u, 0u, &result) == 0,
          "unknown type routed");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_UNSUPPORTED &&
              result.type == 0x03u && result.sequence == 7u,
          "unknown type NAK unsupported");
    for (index = 0u; index < sizeof(types); ++index) {
        CHECK(la_cmd_dispatch_step(&dispatch, types[index], (uint8_t)index, NULL, 0u, 0u,
                                   &result) == 0 &&
                  result.outcome == LA_CMD_OUT_NAK &&
                  result.error == LA_PROTO_ERROR_UNSUPPORTED,
              "Zynq -> host message type is not a request");
    }
    return 0;
}

static int test_config_arm_lifecycle(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    const la_proto_config_t *config;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(send_config(&dispatch, 0x10u, &result) == 0, "CONFIG accepted");
    CHECK(result.outcome == LA_CMD_OUT_ACK && result.type == LA_PROTO_CONFIG &&
              result.sequence == 0x10u,
          "CONFIG acknowledged");
    CHECK(la_cmd_dispatch_config_valid(&dispatch) == 1, "CONFIG latch valid");
    config = la_cmd_dispatch_config(&dispatch);
    CHECK(config != NULL && config->sample_div == 9u && config->pre_samples == 1024u,
          "CONFIG latch contents");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 0x11u, NULL, 0u, 0u, &result) == 0,
          "ARM accepted");
    CHECK(result.outcome == LA_CMD_OUT_ARM && result.type == LA_PROTO_ARM &&
              result.sequence == 0x11u,
          "ARM outcome with echo");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_ARMED, "view ARMED after ARM");

    CHECK(send_config(&dispatch, 0x20u, &result) == 0, "CONFIG while ARMED routed");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "CONFIG while ARMED NAK busy");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 0x12u, NULL, 0u, 0u, &result) == 0,
          "double ARM routed");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "double ARM NAK busy");

    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_ARMED);
    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_TRIGGERED);
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_CAPT, "trigger notice moves to CAPT");
    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_DONE);
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "done notice returns to IDLE");
    CHECK(la_cmd_dispatch_config_valid(&dispatch) == 1,
          "config latch survives the capture lifecycle");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 0x13u, NULL, 0u, 0u, &result) == 0,
          "re-ARM after completion");
    CHECK(result.outcome == LA_CMD_OUT_ARM, "re-ARM outcome");
    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_TRIGGERED);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 0x14u, NULL, 0u, 0u, &result) == 0,
          "ARM during CAPT routed");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "ARM during CAPT NAK busy");
    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_ABORTED);
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "abort notice returns to IDLE");
    return 0;
}

static int test_config_errors(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    la_proto_config_t config;
    uint8_t payload[LA_PROTO_CONFIG_WIRE_SIZE];
    uint8_t bad_field[LA_PROTO_CONFIG_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 1u, payload,
                               LA_PROTO_CONFIG_WIRE_SIZE - 1u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "CONFIG wrong length NAK bad length");
    CHECK(la_cmd_dispatch_config_valid(&dispatch) == 0, "no latch after bad length");

    make_config(&config);
    (void)la_proto_pack_config(&config, payload, sizeof(payload));
    memcpy(bad_field, payload, sizeof(bad_field));
    bad_field[29] = 0u;
    bad_field[30] = 0u;
    bad_field[31] = 0u;
    bad_field[32] = 0u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 2u, bad_field, sizeof(bad_field), 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_FIELD,
          "CONFIG bad field NAK");
    CHECK(la_cmd_dispatch_config_valid(&dispatch) == 0, "no latch after bad field");
    return 0;
}

static int test_arm_errors(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 3u, NULL, 0u, 0u, &result) == 0,
          "ARM without CONFIG routed");
    CHECK(result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_STATE &&
              result.sequence == 3u,
          "ARM without CONFIG NAK bad state");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 4u, (const uint8_t *)"x", 1u, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "ARM with payload NAK bad length");

    CHECK(send_stream_config(&dispatch, 5u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "STREAM CONFIG latches");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 6u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_UNSUPPORTED,
          "STREAM ARM NAK unsupported (live uses LIVE_START)");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "rejected ARM stays IDLE");
    return 0;
}

static int test_abort(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(send_config(&dispatch, 1u, &result) == 0, "CONFIG accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 2u, NULL, 0u, 0u, &result) == 0,
          "ARM accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ABORT, 3u, NULL, 0u, 0u, &result) == 0,
          "ABORT while ARMED routed");
    CHECK(result.outcome == LA_CMD_OUT_ABORT && result.type == LA_PROTO_ABORT &&
              result.sequence == 3u,
          "ABORT outcome with echo");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "ABORT returns to IDLE");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ABORT, 4u, NULL, 0u, 0u, &result) == 0,
          "ABORT while IDLE routed");
    CHECK(result.outcome == LA_CMD_OUT_ACK, "ABORT while IDLE is an idempotent ACK");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ABORT, 5u, (const uint8_t *)"x", 1u, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "ABORT with payload NAK bad length");

    CHECK(send_config(&dispatch, 6u, &result) == 0, "CONFIG accepted again");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 7u, NULL, 0u, 0u, &result) == 0,
          "ARM accepted again");
    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_TRIGGERED);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ABORT, 8u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ABORT,
          "ABORT during CAPT forwards");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE, "ABORT during CAPT returns IDLE");
    return 0;
}

static int test_get_status(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_GET_STATUS, 9u, NULL, 0u, 0u, &result) == 0,
          "GET_STATUS accepted");
    CHECK(result.outcome == LA_CMD_OUT_REQUEST_STATUS && result.type == LA_PROTO_GET_STATUS &&
              result.sequence == 9u,
          "GET_STATUS requests a STATUS snapshot without ACK");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_GET_STATUS, 10u, (const uint8_t *)"x", 1u, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "GET_STATUS with payload NAK bad length");
    return 0;
}

static void make_uart_decoder(la_proto_decoder_config_t *config, uint8_t slot)
{
    memset(config, 0, sizeof(*config));
    config->slot = slot;
    config->type = LA_PROTO_DECODER_UART;
    config->params.uart.rx_channel = 0u;
    config->params.uart.data_bits = 8u;
    config->params.uart.parity = 0u;
    config->params.uart.stop_bits = 1u;
    config->params.uart.inverted = 0u;
    config->params.uart.baud_hz = 115200u;
    config->params.uart.sample_rate_hz = 100000000u;
}

static int test_decoder_cfg(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    la_proto_decoder_config_t config;
    la_proto_decoder_config_t latched;
    uint8_t payload[LA_PROTO_DECODER_CONFIG_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_decoder_count(&dispatch) == 0u, "no decoder latches initially");
    CHECK(la_cmd_dispatch_decoder(&dispatch, 0u, &latched) == -1,
          "empty decoder slot not readable");

    make_uart_decoder(&config, 0u);
    CHECK(la_proto_pack_decoder_config(&config, payload, sizeof(payload)) ==
              LA_PROTO_DECODER_CONFIG_WIRE_SIZE,
          "decoder config packs");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x21u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0,
          "DECODER_CFG routed");
    CHECK(result.outcome == LA_CMD_OUT_DECODER_CFG && result.type == LA_PROTO_DECODER_CFG &&
              result.sequence == 0x21u && result.slot == 0u,
          "DECODER_CFG acknowledged with the slot");
    CHECK(la_cmd_dispatch_decoder_count(&dispatch) == 1u, "one decoder latched");
    CHECK(la_cmd_dispatch_decoder(&dispatch, 0u, &latched) == 0 &&
              latched.type == LA_PROTO_DECODER_UART &&
              latched.params.uart.baud_hz == 115200u &&
              latched.params.uart.sample_rate_hz == 100000000u,
          "decoder latch contents");

    make_uart_decoder(&config, 1u);
    (void)la_proto_pack_decoder_config(&config, payload, sizeof(payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x22u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.slot == 1u,
          "second decoder slot accepted");
    CHECK(la_cmd_dispatch_decoder_count(&dispatch) == 2u, "two decoders latched");

    make_uart_decoder(&config, 0u);
    config.type = LA_PROTO_DECODER_OFF;
    (void)la_proto_pack_decoder_config(&config, payload, sizeof(payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x23u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.slot == 0u,
          "decoder slot reconfiguration accepted");
    CHECK(la_cmd_dispatch_decoder_count(&dispatch) == 1u,
          "an OFF slot is not counted as enabled");
    CHECK(la_cmd_dispatch_decoder(&dispatch, 0u, &latched) == 0 &&
              latched.type == LA_PROTO_DECODER_OFF,
          "reconfigured slot latches OFF");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x24u, payload,
                                (uint16_t)(sizeof(payload) - 1u), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "DECODER_CFG bad length NAK");

    memset(payload, 0, sizeof(payload));
    payload[0] = 0u;
    payload[1] = LA_PROTO_DECODER_SPI;
    payload[4] = 0xFFu; /* SCK 0xFF: not a channel, and no data channel */
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x25u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_FIELD,
          "DECODER_CFG bad field NAK");

    /* P7.7: the stop pattern rides in DECODER_CFG byte 2 plus the type's
     * reserved bytes and latches with the slot. */
    make_uart_decoder(&config, 0u);
    config.stop_on_len = 2u;
    config.stop_on[0] = 0x4Cu;
    config.stop_on[1] = 0x41u;
    CHECK(la_proto_pack_decoder_config(&config, payload, sizeof(payload)) ==
              LA_PROTO_DECODER_CONFIG_WIRE_SIZE,
          "stop-on decoder config packs");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x29u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_DECODER_CFG && result.slot == 0u,
          "stop-on decoder config accepted");
    CHECK(la_cmd_dispatch_decoder(&dispatch, 0u, &latched) == 0 &&
              latched.stop_on_len == 2u && latched.stop_on[0] == 0x4Cu &&
              latched.stop_on[1] == 0x41u,
          "stop-on pattern latched");

    /* An oversized pattern is a bad-field NAK and leaves the latch alone. */
    payload[2] = LA_PROTO_STOP_ON_MAX_UART + 1u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x2Au, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_FIELD,
          "stop-on oversized pattern NAK");
    CHECK(la_cmd_dispatch_decoder(&dispatch, 0u, &latched) == 0 &&
              latched.stop_on_len == 2u,
          "rejected stop-on leaves the latch unchanged");

    CHECK(send_config(&dispatch, 0x26u, &result) == 0, "burst CONFIG accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 0x27u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ARM,
          "ARM accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 0x28u, payload,
                                (uint16_t)sizeof(payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "DECODER_CFG while busy NAK");
    return 0;
}

static int test_live_lifecycle(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    la_proto_config_t config;
    la_proto_decoder_config_t decoder;
    uint8_t config_payload[LA_PROTO_CONFIG_WIRE_SIZE];
    uint8_t decoder_payload[LA_PROTO_DECODER_CONFIG_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 1u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_STATE,
          "LIVE_START without CONFIG NAK bad state");

    CHECK(send_config(&dispatch, 2u, &result) == 0, "burst CONFIG accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 3u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "LIVE_START without STREAM NAK bad field");

    make_config(&config);
    config.ctrl_flags = LA_REG_CTRL_STREAM_MASK;
    config.live_flush_cycles = 200000u;
    (void)la_proto_pack_config(&config, config_payload, sizeof(config_payload));
    config_payload[33] = 0u;
    config_payload[34] = 0u;
    config_payload[35] = 0u;
    config_payload[36] = 0u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 4u, config_payload,
                                (uint16_t)sizeof(config_payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "STREAM CONFIG with zero flush cycles NAK bad field");

    config.live_flush_cycles = 200000u;
    config.mode = 0u;
    (void)la_proto_pack_config(&config, config_payload, sizeof(config_payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 5u, config_payload,
                                (uint16_t)sizeof(config_payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "STREAM CONFIG accepted");
    make_uart_decoder(&decoder, 0u);
    (void)la_proto_pack_decoder_config(&decoder, decoder_payload,
                                       sizeof(decoder_payload));
    (void)la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 6u, decoder_payload,
                               (uint16_t)sizeof(decoder_payload), 0u, &result);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 7u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "LIVE_START with a decoder in raw mode NAK bad field");

    config.mode = LA_REG_MODE_ENCODING_MASK;
    (void)la_proto_pack_config(&config, config_payload, sizeof(config_payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 8u, config_payload,
                                (uint16_t)sizeof(config_payload), 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "transition-encoding STREAM CONFIG accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 9u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_LIVE_START && result.type == LA_PROTO_LIVE_START &&
              result.sequence == 9u,
          "LIVE_START accepted with ACK");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_CAPT,
          "LIVE_START moves the view to CAPT");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 10u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "double LIVE_START NAK busy");
    CHECK(send_config(&dispatch, 11u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BUSY,
          "CONFIG while streaming NAK busy");

    la_cmd_dispatch_acq_notice(&dispatch, LA_CMD_ACQ_STREAMING);
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_CAPT,
          "streaming notice keeps the view CAPT");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_STOP, 12u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_LIVE_STOP && result.type == LA_PROTO_LIVE_STOP &&
              result.sequence == 12u,
          "LIVE_STOP accepted with ACK");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE,
          "LIVE_STOP returns the view to IDLE");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_STOP, 13u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "LIVE_STOP while IDLE is an idempotent ACK");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_STOP, 14u, (const uint8_t *)"x", 1u, 0u,
                                &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "LIVE_STOP with payload NAK bad length");

    /* Decoder-free raw streaming stays legal (P7.2 decoders-off HIL). */
    config.mode = 0u;
    config.live_flush_cycles = 200000u;
    (void)la_proto_pack_config(&config, config_payload, sizeof(config_payload));
    (void)la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 15u, config_payload,
                               (uint16_t)sizeof(config_payload), 0u, &result);
    make_uart_decoder(&decoder, 0u);
    decoder.type = LA_PROTO_DECODER_OFF;
    (void)la_proto_pack_decoder_config(&decoder, decoder_payload,
                                       sizeof(decoder_payload));
    (void)la_cmd_dispatch_step(&dispatch, LA_PROTO_DECODER_CFG, 16u, decoder_payload,
                               (uint16_t)sizeof(decoder_payload), 0u, &result);
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LIVE_START, 17u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_LIVE_START,
          "LIVE_START accepted with all decoders off");
    return 0;
}

static int test_ack_nak_routing(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    uint8_t payload[LA_PROTO_NAK_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    payload[0] = LA_PROTO_DATA_CHUNK;
    payload[1] = 0x77u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ACK, 1u, payload, LA_PROTO_ACK_WIRE_SIZE, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_CHUNK_ACK && result.type == LA_PROTO_DATA_CHUNK &&
              result.sequence == 0x77u,
          "chunk ACK routed to the window");
    payload[0] = LA_PROTO_DATA_END;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ACK, 2u, payload, LA_PROTO_ACK_WIRE_SIZE, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_NONE,
          "non-chunk ACK ignored");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ACK, 3u, payload, 3u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NONE,
          "malformed ACK ignored");

    payload[0] = LA_PROTO_DATA_CHUNK;
    payload[1] = 0x79u;
    payload[2] = LA_PROTO_ERROR_CRC;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_NAK, 4u, payload, LA_PROTO_NAK_WIRE_SIZE, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_CHUNK_NAK && result.type == LA_PROTO_DATA_CHUNK &&
              result.sequence == 0x79u && result.error == LA_PROTO_ERROR_CRC,
          "chunk NAK routed for retransmission");
    payload[0] = LA_PROTO_DATA_END;
    payload[1] = 0x7Au;
    payload[2] = LA_PROTO_ERROR_OVERFLOW;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_NAK, 5u, payload, LA_PROTO_NAK_WIRE_SIZE, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_BURST_NAK && result.type == LA_PROTO_DATA_END &&
              result.sequence == 0x7Au && result.error == LA_PROTO_ERROR_OVERFLOW,
          "non-chunk NAK routed as burst failure");
    payload[0] = LA_PROTO_ARM;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_NAK, 6u, payload, LA_PROTO_NAK_WIRE_SIZE, 0u,
                               &result) == 0 &&
              result.outcome == LA_CMD_OUT_BURST_NAK && result.type == LA_PROTO_ARM,
          "command NAK routed as burst failure");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_NAK, 7u, payload, 2u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NONE,
          "malformed NAK ignored");
    return 0;
}

static int test_patgen_cfg(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    uint8_t payload[LA_PROTO_PATGEN_CFG_WIRE_SIZE];
    uint32_t ctrl = LA_REG_PATGEN_CTRL_EN_MASK |
                    ((uint32_t)LA_REG_PATGEN_CTRL_MODE_VALUE_UART_115200
                     << LA_REG_PATGEN_CTRL_MODE_LSB) |
                    (UINT32_C(7) << LA_REG_PATGEN_CTRL_DIV_LSB);

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_patgen_valid(&dispatch) == 0, "patgen latch starts empty");
    CHECK(la_proto_pack_patgen_cfg(ctrl, payload, sizeof(payload)) ==
              LA_PROTO_PATGEN_CFG_WIRE_SIZE,
          "patgen payload packs");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PATGEN_CFG, 0x11u, payload,
                                LA_PROTO_PATGEN_CFG_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_PATGEN && result.type == LA_PROTO_PATGEN_CFG &&
              result.sequence == 0x11u,
          "PATGEN_CFG acknowledged and flagged");
    CHECK(la_cmd_dispatch_patgen_valid(&dispatch) == 1 &&
              la_cmd_dispatch_patgen_ctrl(&dispatch) == ctrl,
          "patgen latch carries the validated value");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PATGEN_CFG, 0x12u, payload, 3u, 0u,
                                &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "PATGEN_CFG bad length NAK");
    payload[0] = 0x02u; /* reserved mode bit: mode 0x20 is undefined */
    payload[1] = 0x00u;
    payload[2] = 0x00u;
    payload[3] = 0x00u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PATGEN_CFG, 0x13u, payload, 4u, 0u,
                                &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_BAD_FIELD,
          "PATGEN_CFG undefined mode NAK bad field");
    return 0;
}

static int test_trace_ctrl(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    uint8_t payload[LA_PROTO_TRACE_CTRL_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_proto_pack_trace_ctrl(LA_PROTO_TRACE_START, payload,
                                   sizeof(payload)) == LA_PROTO_TRACE_CTRL_WIRE_SIZE,
          "trace payload packs");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_TRACE_CTRL, 0x21u, payload,
                                LA_PROTO_TRACE_CTRL_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_TRACE &&
              result.type == LA_PROTO_TRACE_CTRL && result.sequence == 0x21u &&
              result.action == LA_PROTO_TRACE_START,
          "TRACE_CTRL start acknowledged and routed");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE,
          "TRACE_CTRL does not change the acquisition view");

    payload[0] = LA_PROTO_TRACE_DUMP;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_TRACE_CTRL, 0x22u, payload,
                                LA_PROTO_TRACE_CTRL_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_TRACE &&
              result.action == LA_PROTO_TRACE_DUMP,
          "TRACE_CTRL dump routed");

    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_TRACE_CTRL, 0x23u, payload, 3u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "TRACE_CTRL bad length NAK");
    payload[0] = 9u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_TRACE_CTRL, 0x24u, payload, 4u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "TRACE_CTRL undefined action NAK bad field");
    return 0;
}

static int test_evt_cfg(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    uint8_t payload[LA_PROTO_EVT_CFG_WIRE_SIZE];

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_evt_format(&dispatch) == LA_PROTO_EVT_FORMAT_RECORDS,
          "event format defaults to records");
    CHECK(la_proto_pack_evt_cfg(LA_PROTO_EVT_FORMAT_BLOCK, payload,
                                sizeof(payload)) == LA_PROTO_EVT_CFG_WIRE_SIZE,
          "EVT_CFG packs");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_EVT_CFG, 0x31u, payload,
                                LA_PROTO_EVT_CFG_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK &&
              result.type == LA_PROTO_EVT_CFG && result.sequence == 0x31u,
          "EVT_CFG block format acknowledged");
    CHECK(la_cmd_dispatch_evt_format(&dispatch) == LA_PROTO_EVT_FORMAT_BLOCK,
          "event format latched");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_EVT_CFG, 0x32u, payload, 3u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "EVT_CFG bad length NAK");
    payload[0] = 9u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_EVT_CFG, 0x33u, payload, 4u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "EVT_CFG undefined format NAK bad field");
    return 0;
}

static int test_link_cfg(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    uint8_t payload[LA_PROTO_LINK_CFG_WIRE_SIZE];
    uint8_t transport;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_link_transport(&dispatch) == LA_PROTO_LINK_UART,
          "link transport defaults to UART");
    CHECK(la_proto_pack_link_cfg(LA_PROTO_LINK_SPI, payload, sizeof(payload)) ==
              LA_PROTO_LINK_CFG_WIRE_SIZE,
          "LINK_CFG packs");
    CHECK(la_proto_unpack_link_cfg(&transport, payload, sizeof(payload)) == 0 &&
              transport == LA_PROTO_LINK_SPI,
          "LINK_CFG unpacks");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LINK_CFG, 0x41u, payload,
                               LA_PROTO_LINK_CFG_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_LINK_CFG &&
              result.transport == LA_PROTO_LINK_SPI && result.sequence == 0x41u,
          "LINK_CFG SPI accepted");
    CHECK(la_cmd_dispatch_link_transport(&dispatch) == LA_PROTO_LINK_SPI,
          "link transport latched");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LINK_CFG, 0x42u, payload, 3u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_LENGTH,
          "LINK_CFG bad length NAK");
    payload[0] = 9u;
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_LINK_CFG, 0x43u, payload, 4u,
                                0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK &&
              result.error == LA_PROTO_ERROR_BAD_FIELD,
          "LINK_CFG undefined transport NAK bad field");
    return 0;
}

static int test_arm_pool_capacity(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;
    la_proto_config_t config;
    uint8_t payload[LA_PROTO_CONFIG_WIRE_SIZE];

    /* Raw mode: 16384 samples fit in 16384 bytes; the pool holds 8192. */
    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    la_cmd_dispatch_set_pool(&dispatch, 8192u);
    make_config(&config);
    config.pre_samples = 0u;
    config.post_samples = 16384u;
    (void)la_proto_pack_config(&config, payload, sizeof(payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 0u, payload,
                                LA_PROTO_CONFIG_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "oversized config still latches");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 1u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_NAK && result.error == LA_PROTO_ERROR_OVERFLOW,
          "oversized ARM NAK overflow before any register access");
    CHECK(la_cmd_dispatch_view(&dispatch) == LA_CMD_VIEW_IDLE,
          "rejected ARM leaves the view IDLE");

    /* Transition mode worst case is one word per sample: 2048 samples fit
     * exactly in the 8192-byte pool. */
    config.mode = LA_REG_MODE_ENCODING_MASK;
    config.pre_samples = 0u;
    config.post_samples = 2048u;
    (void)la_proto_pack_config(&config, payload, sizeof(payload));
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 2u, payload,
                                LA_PROTO_CONFIG_WIRE_SIZE, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ACK,
          "fitting config accepted");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_ARM, 3u, NULL, 0u, 0u, &result) == 0 &&
              result.outcome == LA_CMD_OUT_ARM,
          "fitting ARM accepted");
    return 0;
}

static int test_argument_guards(void)
{
    la_cmd_dispatch_t dispatch;
    la_cmd_result_t result;

    la_cmd_dispatch_init(&dispatch, TEST_FW_VERSION, TEST_PL_ID);
    CHECK(la_cmd_dispatch_step(NULL, LA_PROTO_PING, 0u, NULL, 0u, 0u, &result) == -1,
          "NULL dispatch rejected");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_PING, 0u, NULL, 0u, 0u, NULL) == -1,
          "NULL result rejected");
    CHECK(la_cmd_dispatch_step(&dispatch, LA_PROTO_CONFIG, 0u, NULL, 1u, 0u, &result) == -1,
          "NULL payload with length rejected");
    return 0;
}

int main(void)
{
    if (test_init_clears() != 0 || test_ping_pong() != 0 ||
        test_unknown_and_inbound_rejected() != 0 || test_config_arm_lifecycle() != 0 ||
        test_config_errors() != 0 || test_arm_errors() != 0 || test_abort() != 0 ||
        test_get_status() != 0 || test_decoder_cfg() != 0 ||
        test_live_lifecycle() != 0 ||
        test_patgen_cfg() != 0 || test_trace_ctrl() != 0 ||
        test_evt_cfg() != 0 || test_link_cfg() != 0 ||
        test_arm_pool_capacity() != 0 ||
        test_ack_nak_routing() != 0 || test_argument_guards() != 0) {
        return 1;
    }
    puts("PASS: la_native_cmd");
    return 0;
}
