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
 * P4 scope guards: DECODER_CFG, LIVE_START and LIVE_STOP are rejected with
 * NAK unsupported (P7.4 extends this table), and an ARM whose latched
 * CONFIG has the STREAM flag set is rejected with NAK unsupported - the
 * same guard la_acq_fsm applies until the P7.x live path exists. CONFIG
 * while the machine is not IDLE is rejected as busy. PATGEN_CFG (the
 * additive v1 extension for the pattern generator) is accepted in any
 * state and latched for CmdTask to write; an ARM whose worst-case capture
 * cannot fit the configured capture pool is rejected with NAK overflow.
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
    LA_CMD_ACQ_ABORTED
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
    LA_CMD_OUT_BURST_NAK
} la_cmd_outcome_t;

typedef struct la_cmd_result {
    la_cmd_outcome_t outcome;
    uint8_t type;     /* echoed type for ACK/NAK; NAKed type for OUT_BURST_NAK;
                       * DATA_CHUNK for the chunk outcomes */
    uint8_t sequence; /* echoed request sequence; chunk sequence for the
                       * ACK/NAK routing outcomes */
    uint8_t error;    /* LA_PROTO_ERROR_* code for NAK outcomes */
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
    uint64_t pool_bytes; /* burst capture pool capacity; 0 disables the check */
} la_cmd_dispatch_t;

LA_CMD_API void la_cmd_dispatch_init(la_cmd_dispatch_t *dispatch,
                                     uint32_t fw_version,
                                     uint32_t pl_id);

/* Installs the burst capture pool capacity used to reject an ARM whose
 * worst-case word count cannot fit (NAK overflow) before any register is
 * touched. */
LA_CMD_API void la_cmd_dispatch_set_pool(la_cmd_dispatch_t *dispatch,
                                         uint64_t pool_bytes);

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

#endif /* LA_CMD_DISPATCH_H */
