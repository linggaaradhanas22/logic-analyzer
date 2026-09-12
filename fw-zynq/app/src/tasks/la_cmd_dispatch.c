#include "la_cmd_dispatch.h"

#include "la_burst_plan.h"
#include "la_regs.h"

#include <string.h>

static void result_none(la_cmd_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->outcome = LA_CMD_OUT_NONE;
}

static void result_nak(la_cmd_result_t *result,
                       uint8_t type,
                       uint8_t sequence,
                       uint8_t error)
{
    result_none(result);
    result->outcome = LA_CMD_OUT_NAK;
    result->type = type;
    result->sequence = sequence;
    result->error = error;
}

static void result_ack(la_cmd_result_t *result, uint8_t type, uint8_t sequence)
{
    result_none(result);
    result->outcome = LA_CMD_OUT_ACK;
    result->type = type;
    result->sequence = sequence;
}

static void result_pong(la_cmd_result_t *result,
                        uint32_t fw_version,
                        uint32_t pl_id,
                        uint64_t uptime_ms)
{
    la_proto_pong_t pong;

    result_none(result);
    result->outcome = LA_CMD_OUT_PONG;
    pong.fw_version = fw_version;
    pong.pl_id = pl_id;
    pong.uptime_ms = uptime_ms;
    (void)la_proto_pack_pong(&pong, result->payload, sizeof(result->payload));
}

static void result_chunk_ack(la_cmd_result_t *result, uint8_t sequence)
{
    result_none(result);
    result->outcome = LA_CMD_OUT_CHUNK_ACK;
    result->type = LA_PROTO_DATA_CHUNK;
    result->sequence = sequence;
}

static void result_chunk_nak(la_cmd_result_t *result,
                             uint8_t sequence,
                             uint8_t error)
{
    result_none(result);
    result->outcome = LA_CMD_OUT_CHUNK_NAK;
    result->type = LA_PROTO_DATA_CHUNK;
    result->sequence = sequence;
    result->error = error;
}

static void result_burst_nak(la_cmd_result_t *result,
                             uint8_t type,
                             uint8_t sequence,
                             uint8_t error)
{
    result_none(result);
    result->outcome = LA_CMD_OUT_BURST_NAK;
    result->type = type;
    result->sequence = sequence;
    result->error = error;
}

void la_cmd_dispatch_init(la_cmd_dispatch_t *dispatch,
                          uint32_t fw_version,
                          uint32_t pl_id)
{
    if (dispatch == NULL) {
        return;
    }
    memset(dispatch, 0, sizeof(*dispatch));
    dispatch->view = LA_CMD_VIEW_IDLE;
    dispatch->fw_version = fw_version;
    dispatch->pl_id = pl_id;
}

static int handle_config(la_cmd_dispatch_t *dispatch,
                         uint8_t sequence,
                         const uint8_t *payload,
                         uint16_t length,
                         la_cmd_result_t *result)
{
    la_proto_config_t config;

    if (length != LA_PROTO_CONFIG_WIRE_SIZE) {
        result_nak(result, LA_PROTO_CONFIG, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view != LA_CMD_VIEW_IDLE) {
        result_nak(result, LA_PROTO_CONFIG, sequence, LA_PROTO_ERROR_BUSY);
        return 0;
    }
    if (la_proto_unpack_config(&config, payload, length) != 0) {
        result_nak(result, LA_PROTO_CONFIG, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    dispatch->config = config;
    dispatch->config_valid = 1u;
    result_ack(result, LA_PROTO_CONFIG, sequence);
    return 0;
}

static int handle_arm(la_cmd_dispatch_t *dispatch,
                      uint8_t sequence,
                      const uint8_t *payload,
                      uint16_t length,
                      la_cmd_result_t *result)
{
    (void)payload;
    if (length != 0u) {
        result_nak(result, LA_PROTO_ARM, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view != LA_CMD_VIEW_IDLE) {
        result_nak(result, LA_PROTO_ARM, sequence, LA_PROTO_ERROR_BUSY);
        return 0;
    }
    if (dispatch->config_valid == 0u) {
        result_nak(result, LA_PROTO_ARM, sequence, LA_PROTO_ERROR_BAD_STATE);
        return 0;
    }
    if ((dispatch->config.ctrl_flags & LA_REG_CTRL_STREAM_MASK) != 0u) {
        result_nak(result, LA_PROTO_ARM, sequence, LA_PROTO_ERROR_UNSUPPORTED);
        return 0;
    }
    if (dispatch->pool_bytes != 0u) {
        uint64_t words;
        uint8_t encoding =
            (dispatch->config.mode & LA_REG_MODE_ENCODING_MASK) != 0u ? 1u : 0u;
        if (la_burst_plan_words(encoding, dispatch->config.pre_samples,
                                dispatch->config.post_samples, &words) != 0 ||
            words * 4u > dispatch->pool_bytes) {
            result_nak(result, LA_PROTO_ARM, sequence, LA_PROTO_ERROR_OVERFLOW);
            return 0;
        }
    }
    result_ack(result, LA_PROTO_ARM, sequence);
    result->outcome = LA_CMD_OUT_ARM;
    dispatch->view = LA_CMD_VIEW_ARMED;
    return 0;
}

static int handle_abort(la_cmd_dispatch_t *dispatch,
                        uint8_t sequence,
                        const uint8_t *payload,
                        uint16_t length,
                        la_cmd_result_t *result)
{
    (void)payload;
    if (length != 0u) {
        result_nak(result, LA_PROTO_ABORT, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view == LA_CMD_VIEW_IDLE) {
        /* Idempotent: an abort of an idle machine is trivially satisfied and
         * is acknowledged without forwarding. */
        result_ack(result, LA_PROTO_ABORT, sequence);
        return 0;
    }
    result_ack(result, LA_PROTO_ABORT, sequence);
    result->outcome = LA_CMD_OUT_ABORT;
    dispatch->view = LA_CMD_VIEW_IDLE;
    return 0;
}

static int handle_patgen(la_cmd_dispatch_t *dispatch,
                         uint8_t sequence,
                         const uint8_t *payload,
                         uint16_t length,
                         la_cmd_result_t *result)
{
    uint32_t ctrl;

    if (length != LA_PROTO_PATGEN_CFG_WIRE_SIZE) {
        result_nak(result, LA_PROTO_PATGEN_CFG, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (la_proto_unpack_patgen_cfg(&ctrl, payload, length) != 0) {
        result_nak(result, LA_PROTO_PATGEN_CFG, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    dispatch->patgen_ctrl = ctrl;
    dispatch->patgen_valid = 1u;
    result_ack(result, LA_PROTO_PATGEN_CFG, sequence);
    result->outcome = LA_CMD_OUT_PATGEN;
    return 0;
}

static int handle_trace(uint8_t sequence,
                        const uint8_t *payload,
                        uint16_t length,
                        la_cmd_result_t *result)
{
    uint8_t action;

    if (length != LA_PROTO_TRACE_CTRL_WIRE_SIZE) {
        result_nak(result, LA_PROTO_TRACE_CTRL, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (la_proto_unpack_trace_ctrl(&action, payload, length) != 0) {
        result_nak(result, LA_PROTO_TRACE_CTRL, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    result_ack(result, LA_PROTO_TRACE_CTRL, sequence);
    result->outcome = LA_CMD_OUT_TRACE;
    result->action = action;
    return 0;
}

static int handle_evt_cfg(la_cmd_dispatch_t *dispatch,
                          uint8_t sequence,
                          const uint8_t *payload,
                          uint16_t length,
                          la_cmd_result_t *result)
{
    uint8_t format;

    if (length != LA_PROTO_EVT_CFG_WIRE_SIZE) {
        result_nak(result, LA_PROTO_EVT_CFG, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view != LA_CMD_VIEW_IDLE) {
        result_nak(result, LA_PROTO_EVT_CFG, sequence, LA_PROTO_ERROR_BUSY);
        return 0;
    }
    if (la_proto_unpack_evt_cfg(&format, payload, length) != 0) {
        result_nak(result, LA_PROTO_EVT_CFG, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    dispatch->evt_format = format;
    result_ack(result, LA_PROTO_EVT_CFG, sequence);
    return 0;
}

static uint8_t decoder_enabled_count(const la_cmd_dispatch_t *dispatch)
{
    uint8_t count = 0u;
    uint8_t slot;

    for (slot = 0u; slot < LA_PROTO_DECODER_MAX_SLOTS; ++slot) {
        if (dispatch->decoder_valid[slot] != 0u &&
            dispatch->decoders[slot].type != LA_PROTO_DECODER_OFF) {
            ++count;
        }
    }
    return count;
}

static int handle_decoder_cfg(la_cmd_dispatch_t *dispatch,
                              uint8_t sequence,
                              const uint8_t *payload,
                              uint16_t length,
                              la_cmd_result_t *result)
{
    la_proto_decoder_config_t config;

    if (length != LA_PROTO_DECODER_CONFIG_WIRE_SIZE) {
        result_nak(result, LA_PROTO_DECODER_CFG, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view != LA_CMD_VIEW_IDLE) {
        result_nak(result, LA_PROTO_DECODER_CFG, sequence, LA_PROTO_ERROR_BUSY);
        return 0;
    }
    if (la_proto_unpack_decoder_config(&config, payload, length) != 0) {
        result_nak(result, LA_PROTO_DECODER_CFG, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    dispatch->decoders[config.slot] = config;
    dispatch->decoder_valid[config.slot] = 1u;
    dispatch->decoder_count = decoder_enabled_count(dispatch);
    result_ack(result, LA_PROTO_DECODER_CFG, sequence);
    result->outcome = LA_CMD_OUT_DECODER_CFG;
    result->slot = config.slot;
    return 0;
}

static int handle_live_start(la_cmd_dispatch_t *dispatch,
                             uint8_t sequence,
                             const uint8_t *payload,
                             uint16_t length,
                             la_cmd_result_t *result)
{
    (void)payload;
    if (length != 0u) {
        result_nak(result, LA_PROTO_LIVE_START, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view != LA_CMD_VIEW_IDLE) {
        result_nak(result, LA_PROTO_LIVE_START, sequence, LA_PROTO_ERROR_BUSY);
        return 0;
    }
    if (dispatch->config_valid == 0u) {
        result_nak(result, LA_PROTO_LIVE_START, sequence, LA_PROTO_ERROR_BAD_STATE);
        return 0;
    }
    if ((dispatch->config.ctrl_flags & LA_REG_CTRL_STREAM_MASK) == 0u ||
        dispatch->config.live_flush_cycles == 0u) {
        result_nak(result, LA_PROTO_LIVE_START, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    if (dispatch->decoder_count != 0u &&
        (dispatch->config.mode & LA_REG_MODE_ENCODING_MASK) == 0u) {
        /* Live decode consumes transition words; raw packing is only legal
         * when no decoder is enabled. */
        result_nak(result, LA_PROTO_LIVE_START, sequence, LA_PROTO_ERROR_BAD_FIELD);
        return 0;
    }
    result_ack(result, LA_PROTO_LIVE_START, sequence);
    result->outcome = LA_CMD_OUT_LIVE_START;
    dispatch->view = LA_CMD_VIEW_CAPT;
    return 0;
}

static int handle_live_stop(la_cmd_dispatch_t *dispatch,
                            uint8_t sequence,
                            const uint8_t *payload,
                            uint16_t length,
                            la_cmd_result_t *result)
{
    (void)payload;
    if (length != 0u) {
        result_nak(result, LA_PROTO_LIVE_STOP, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        return 0;
    }
    if (dispatch->view == LA_CMD_VIEW_IDLE) {
        /* Idempotent: stopping an idle machine is trivially satisfied. */
        result_ack(result, LA_PROTO_LIVE_STOP, sequence);
        return 0;
    }
    result_ack(result, LA_PROTO_LIVE_STOP, sequence);
    result->outcome = LA_CMD_OUT_LIVE_STOP;
    dispatch->view = LA_CMD_VIEW_IDLE;
    return 0;
}

static int handle_ack(uint8_t sequence,                      const uint8_t *payload,
                      uint16_t length,
                      la_cmd_result_t *result)
{
    uint8_t acknowledged_type;
    uint8_t acknowledged_sequence;

    if (length != LA_PROTO_ACK_WIRE_SIZE ||
        la_proto_unpack_ack(&acknowledged_type, &acknowledged_sequence, payload, length) != 0) {
        result_none(result);
        return 0;
    }
    if (acknowledged_type != LA_PROTO_DATA_CHUNK) {
        result_none(result);
        return 0;
    }
    result_chunk_ack(result, acknowledged_sequence);
    (void)sequence;
    return 0;
}

static int handle_nak(uint8_t sequence,
                      const uint8_t *payload,
                      uint16_t length,
                      la_cmd_result_t *result)
{
    uint8_t nak_type;
    uint8_t nak_sequence;
    uint8_t error;

    if (length != LA_PROTO_NAK_WIRE_SIZE ||
        la_proto_unpack_nak(&nak_type, &nak_sequence, &error, payload, length) != 0) {
        result_none(result);
        return 0;
    }
    if (nak_type == LA_PROTO_DATA_CHUNK) {
        result_chunk_nak(result, nak_sequence, error);
    } else {
        result_burst_nak(result, nak_type, nak_sequence, error);
    }
    (void)sequence;
    return 0;
}

int la_cmd_dispatch_step(la_cmd_dispatch_t *dispatch,
                         uint8_t type,
                         uint8_t sequence,
                         const uint8_t *payload,
                         uint16_t length,
                         uint64_t uptime_ms,
                         la_cmd_result_t *result)
{
    if (dispatch == NULL || result == NULL || (payload == NULL && length != 0u)) {
        return -1;
    }
    result_none(result);
    switch (type) {
    case LA_PROTO_PING:
        if (length != 0u) {
            result_nak(result, type, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        } else {
            result_pong(result, dispatch->fw_version, dispatch->pl_id, uptime_ms);
        }
        return 0;

    case LA_PROTO_CONFIG:
        return handle_config(dispatch, sequence, payload, length, result);

    case LA_PROTO_ARM:
        return handle_arm(dispatch, sequence, payload, length, result);

    case LA_PROTO_ABORT:
        return handle_abort(dispatch, sequence, payload, length, result);

    case LA_PROTO_PATGEN_CFG:
        return handle_patgen(dispatch, sequence, payload, length, result);

    case LA_PROTO_TRACE_CTRL:
        return handle_trace(sequence, payload, length, result);

    case LA_PROTO_EVT_CFG:
        return handle_evt_cfg(dispatch, sequence, payload, length, result);

    case LA_PROTO_GET_STATUS:
        if (length != 0u) {
            result_nak(result, type, sequence, LA_PROTO_ERROR_BAD_LENGTH);
        } else {
            result_none(result);
            result->outcome = LA_CMD_OUT_REQUEST_STATUS;
            result->type = type;
            result->sequence = sequence;
        }
        return 0;

    case LA_PROTO_ACK:
        return handle_ack(sequence, payload, length, result);

    case LA_PROTO_NAK:
        return handle_nak(sequence, payload, length, result);

    case LA_PROTO_DECODER_CFG:
        return handle_decoder_cfg(dispatch, sequence, payload, length, result);

    case LA_PROTO_LIVE_START:
        return handle_live_start(dispatch, sequence, payload, length, result);

    case LA_PROTO_LIVE_STOP:
        return handle_live_stop(dispatch, sequence, payload, length, result);

    case LA_PROTO_PONG:
    case LA_PROTO_STATUS:
    case LA_PROTO_EVT_TRIGGERED:
    case LA_PROTO_EVT_DECODED:
    case LA_PROTO_EVT_OVERRUN:
    case LA_PROTO_EVT_BLOCK:
    case LA_PROTO_DATA_BEGIN:
    case LA_PROTO_DATA_CHUNK:
    case LA_PROTO_DATA_END:
    case LA_PROTO_LOG:
        /* Zynq -> host messages are not requests; a peer sending them here
         * is out of contract. */
        result_nak(result, type, sequence, LA_PROTO_ERROR_UNSUPPORTED);
        return 0;

    default:
        result_nak(result, type, sequence, LA_PROTO_ERROR_UNSUPPORTED);
        return 0;
    }
}

void la_cmd_dispatch_acq_notice(la_cmd_dispatch_t *dispatch,
                                la_cmd_acq_notice_t notice)
{
    if (dispatch == NULL) {
        return;
    }
    switch (notice) {
    case LA_CMD_ACQ_ARMED:
        dispatch->view = LA_CMD_VIEW_ARMED;
        break;
    case LA_CMD_ACQ_STREAMING:
        dispatch->view = LA_CMD_VIEW_CAPT;
        break;
    case LA_CMD_ACQ_TRIGGERED:
        dispatch->view = LA_CMD_VIEW_CAPT;
        break;
    case LA_CMD_ACQ_DONE:
    case LA_CMD_ACQ_ABORTED:
        dispatch->view = LA_CMD_VIEW_IDLE;
        break;
    default:
        break;
    }
}

la_cmd_acq_view_t la_cmd_dispatch_view(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return LA_CMD_VIEW_IDLE;
    }
    return dispatch->view;
}

int la_cmd_dispatch_config_valid(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return 0;
    }
    return dispatch->config_valid != 0u ? 1 : 0;
}

const la_proto_config_t *la_cmd_dispatch_config(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return NULL;
    }
    return &dispatch->config;
}

void la_cmd_dispatch_set_pool(la_cmd_dispatch_t *dispatch, uint64_t pool_bytes)
{
    if (dispatch == NULL) {
        return;
    }
    dispatch->pool_bytes = pool_bytes;
}

int la_cmd_dispatch_patgen_valid(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return 0;
    }
    return dispatch->patgen_valid != 0u ? 1 : 0;
}

uint32_t la_cmd_dispatch_patgen_ctrl(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return 0u;
    }
    return dispatch->patgen_ctrl;
}

uint8_t la_cmd_dispatch_evt_format(const la_cmd_dispatch_t *dispatch)
{
    if (dispatch == NULL) {
        return LA_PROTO_EVT_FORMAT_RECORDS;
    }
    return dispatch->evt_format;
}

int la_cmd_dispatch_decoder(const la_cmd_dispatch_t *dispatch,
                            uint8_t slot,
                            la_proto_decoder_config_t *config)
{
    if (dispatch == NULL || config == NULL || slot >= LA_PROTO_DECODER_MAX_SLOTS ||
        dispatch->decoder_valid[slot] == 0u) {
        return -1;
    }
    *config = dispatch->decoders[slot];
    return 0;
}

uint8_t la_cmd_dispatch_decoder_count(const la_cmd_dispatch_t *dispatch)
{
    return dispatch != NULL ? dispatch->decoder_count : 0u;
}
