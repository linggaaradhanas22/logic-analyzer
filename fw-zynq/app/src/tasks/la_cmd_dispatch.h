#ifndef LA_CMD_DISPATCH_H
#define LA_CMD_DISPATCH_H

/*
 * P4.9 CmdTask command dispatch core (pure logic).
 *
 * Receive-side routing for the host -> Zynq command set of the burst path:
 * PING, CONFIG, ARM, ABORT, GET_STATUS plus the inbound ACK/NAK responses
 * to the burst transfer. The framing layer (common/la_proto parser) has
 * already accepted each frame; this core decides the response and the
 * dispatch target. It performs no FreeRTOS, UART, parser or register calls;
 * CmdTask owns it as the single writer.
 *
 * Outcomes for CmdTask:
 *  - OUT_ACK / OUT_NAK: encode an ACK/NAK reply frame. The reply reuses the
 *    request sequence number; the payload carries the original type and
 *    sequence per protocol.md.
 *  - OUT_PONG: a PONG payload is built in result.payload (uptime taken from
 *    the caller so the wire value is fresh at transmit time).
 *  - OUT_ARM / OUT_ABORT: forward to AcqTask on ctrl_q.
 *  - OUT_REQUEST_STATUS: produce a STATUS snapshot frame (task asks the
 *    STATUS path for fresh counters). No ACK is sent for GET_STATUS; the
 *    STATUS frame is the response.
 *  - OUT_PATGEN: a validated PATGEN_CFG value was latched; CmdTask writes it
 *    to PATGEN_CTRL. The ACK fields are also set (the ACK is sent first).
 *  - OUT_CHUNK_ACK / OUT_CHUNK_NAK / OUT_BURST_NAK: routing of inbound
 *    ACK/NAK responses to the burst TX window (la_tx_win) and job control.
 *    result.sequence is the chunk sequence; for OUT_BURST_NAK result.type
 *    and result.error identify the NAKed frame. Stale, duplicate or
 *    malformed ACK/NAK responses are reported as OUT_NONE and ignored; the
 *    window tracker applies its own stale rules.
 *
 * Command legality mirrors la_acq_fsm from the host-visible side. The
 * acquisition view (IDLE/ARMED/CAPT) transitions optimistically when ARM or
 * ABORT is accepted and is reconciled with the firmware by
 * la_cmd_dispatch_acq_notice(), which CmdTask feeds from every AcqTask
 * lifecycle event. See docs/rtos-design.md "CmdTask command dispatch core
 * (P4.9)".
 *
 * P4/P7 scope guards: DECODER_CFG, LIVE_START and LIVE_STOP are accepted
 * (P7.4): DECODER_CFG latches a validated decoder slot configuration in
 * IDLE, LIVE_START requires a latched STREAM CONFIG (transition encoding
 * whenever a decoder is enabled) and moves the view to CAPT, LIVE_STOP is
 * accepted from CAPT and is an idempotent ACK in IDLE. An ARM whose latched
 * CONFIG has the STREAM flag set is still rejected with NAK unsupported -
 * live streaming uses LIVE_START, not ARM. CONFIG while the machine is not
 * IDLE is rejected as busy. PATGEN_CFG (the additive v1 extension for the
 * pattern generator) is accepted in any state and latched for CmdTask to
 * write; an ARM whose worst-case capture cannot fit the configured capture
 * pool is rejected with NAK overflow. TRACE_CTRL (0x18, P8.3) is a 4-byte
 * action (STOP/START/DUMP) accepted in any state: the dispatch validates and
 * routes it, and CmdTask acknowledges it before starting/stopping the trace
 * ring or running the dump.
 */

#include "la_proto.h"

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_CMD_API __declspec(dllexport)
#else
#define LA_CMD_API
#endif

typedef enum la_cmd_acq_view {
    LA_CMD_VIEW_IDLE = 0,
    LA_CMD_VIEW_ARMED,
    LA_CMD_VIEW_CAPT
} la_cmd_acq_view_t;

/* AcqTask lifecycle notices that reconcile the dispatch view. CmdTask calls
 * la_cmd_dispatch_acq_notice() on every event AcqTask reports (ARM accepted,
 * trigger, completion, any abort cause). */
typedef enum la_cmd_acq_notice {
    LA_CMD_ACQ_ARMED = 0,
    LA_CMD_ACQ_TRIGGERED,
    LA_CMD_ACQ_DONE,
    LA_CMD_ACQ_ABORTED,
    LA_CMD_ACQ_STREAMING
} la_cmd_acq_notice_t;

typedef enum la_cmd_outcome {
    LA_CMD_OUT_NONE = 0,
    LA_CMD_OUT_ACK,
    LA_CMD_OUT_NAK,
    LA_CMD_OUT_PONG,
    LA_CMD_OUT_ARM,
    LA_CMD_OUT_ABORT,
    LA_CMD_OUT_REQUEST_STATUS,
    LA_CMD_OUT_PATGEN,
    LA_CMD_OUT_CHUNK_ACK,
    LA_CMD_OUT_CHUNK_NAK,
    LA_CMD_OUT_BURST_NAK,
    LA_CMD_OUT_DECODER_CFG,
    LA_CMD_OUT_LIVE_START,
    LA_CMD_OUT_LIVE_STOP,
    LA_CMD_OUT_TRACE,
    LA_CMD_OUT_LINK_CFG,
    LA_CMD_OUT_WAVE_CFG
} la_cmd_outcome_t;

typedef struct la_cmd_result {
    la_cmd_outcome_t outcome;
    uint8_t type;     /* echoed type for ACK/NAK; NAKed type for OUT_BURST_NAK;
                       * DATA_CHUNK for the chunk outcomes */
    uint8_t sequence; /* echoed request sequence; chunk sequence for the
                       * ACK/NAK routing outcomes */
    uint8_t error;    /* LA_PROTO_ERROR_* code for NAK outcomes */
    uint8_t slot;     /* decoder slot for OUT_DECODER_CFG */
    uint8_t action;   /* LA_PROTO_TRACE_* action for OUT_TRACE */
    uint16_t trace_start; /* first record index for a chunked OUT_TRACE DUMP */
    uint8_t trace_count;  /* requested record count (0 = firmware maximum) */
    uint8_t transport; /* LA_PROTO_LINK_* transport for OUT_LINK_CFG */
    uint8_t payload[LA_PROTO_PONG_WIRE_SIZE]; /* valid for OUT_PONG */
} la_cmd_result_t;

typedef struct la_cmd_dispatch {
    la_proto_config_t config;
    uint8_t config_valid;
    la_cmd_acq_view_t view;
    uint32_t fw_version;
    uint32_t pl_id;
    uint32_t patgen_ctrl;
    uint8_t patgen_valid;
    uint8_t evt_format; /* LA_PROTO_EVT_FORMAT_* latched by EVT_CFG (P9.2) */
    uint8_t link_transport; /* LA_PROTO_LINK_* latched by LINK_CFG (P9.1) */
    uint8_t link_cfg_allowed; /* 0 in the direct-USB profile (U03) */
    uint64_t pool_bytes; /* burst capture pool capacity; 0 disables the check */
    la_proto_decoder_config_t decoders[LA_PROTO_DECODER_MAX_SLOTS];
    uint8_t decoder_valid[LA_PROTO_DECODER_MAX_SLOTS];
    uint8_t decoder_count;
} la_cmd_dispatch_t;

LA_CMD_API void la_cmd_dispatch_init(la_cmd_dispatch_t *dispatch,
                                     uint32_t fw_version,
                                     uint32_t pl_id);

/* Installs the burst capture pool capacity used to reject an ARM whose
 * worst-case word count cannot fit (NAK overflow) before any register is
 * touched. */
LA_CMD_API void la_cmd_dispatch_set_pool(la_cmd_dispatch_t *dispatch,
                                         uint64_t pool_bytes);

/* 0 rejects LINK_CFG with UNSUPPORTED before any state change (direct-USB
 * profile: the gateway UART/SPI routing does not exist there). */
LA_CMD_API void la_cmd_dispatch_set_link_cfg_allowed(
    la_cmd_dispatch_t *dispatch, int allowed);

/* Route one accepted frame. Returns 0 on success, -1 on invalid arguments.
 * result.outcome is OUT_NONE when there is nothing to do. */
LA_CMD_API int la_cmd_dispatch_step(la_cmd_dispatch_t *dispatch,
                                    uint8_t type,
                                    uint8_t sequence,
                                    const uint8_t *payload,
                                    uint16_t length,
                                    uint64_t uptime_ms,
                                    la_cmd_result_t *result);

LA_CMD_API void la_cmd_dispatch_acq_notice(la_cmd_dispatch_t *dispatch,
                                           la_cmd_acq_notice_t notice);

LA_CMD_API la_cmd_acq_view_t la_cmd_dispatch_view(const la_cmd_dispatch_t *dispatch);

/* Validated CONFIG latch. la_cmd_dispatch_config() is only meaningful when
 * la_cmd_dispatch_config_valid() is nonzero. */
LA_CMD_API int la_cmd_dispatch_config_valid(const la_cmd_dispatch_t *dispatch);
LA_CMD_API const la_proto_config_t *la_cmd_dispatch_config(const la_cmd_dispatch_t *dispatch);

/* Latest validated PATGEN_CTRL latch. Only meaningful when
 * la_cmd_dispatch_patgen_valid() is nonzero. */
LA_CMD_API int la_cmd_dispatch_patgen_valid(const la_cmd_dispatch_t *dispatch);
LA_CMD_API uint32_t la_cmd_dispatch_patgen_ctrl(const la_cmd_dispatch_t *dispatch);

/* Live event encoding selected by EVT_CFG (0x19, P9.2). Defaults to
 * LA_PROTO_EVT_FORMAT_RECORDS and is forwarded on the next LIVE_START. */
LA_CMD_API uint8_t la_cmd_dispatch_evt_format(const la_cmd_dispatch_t *dispatch);

/* Bulk transport selected by LINK_CFG (0x1A, P9.1). Defaults to
 * LA_PROTO_LINK_UART; TxTask routes burst DATA_* frames to the SPI slave when
 * it reads LA_PROTO_LINK_SPI. */
LA_CMD_API uint8_t la_cmd_dispatch_link_transport(const la_cmd_dispatch_t *dispatch);

/* Validated decoder slot latch (P7.4). Returns 0 and copies the latched
 * configuration when slot holds one, -1 otherwise. decoder_count is the
 * number of slots holding an enabled (non-OFF) configuration. */
LA_CMD_API int la_cmd_dispatch_decoder(const la_cmd_dispatch_t *dispatch,
                                       uint8_t slot,
                                       la_proto_decoder_config_t *config);
LA_CMD_API uint8_t la_cmd_dispatch_decoder_count(const la_cmd_dispatch_t *dispatch);

#endif /* LA_CMD_DISPATCH_H */
