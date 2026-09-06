#ifndef LA_ACQ_FSM_H
#define LA_ACQ_FSM_H

/*
 * P4.7 AcqTask burst-path state machine (pure logic).
 *
 * Tracks the acquisition lifecycle for a burst capture: IDLE -> ARMED ->
 * CAPT -> IDLE, driven by the events AcqTask receives (PL/DMA IRQ
 * notifications and the control queue). The machine performs no register,
 * DMA, FreeRTOS or queue calls; AcqTask owns it as the single writer,
 * applies configuration to la_top through the la_regs driver, and forwards
 * the produced events (EVT_TRIGGERED, a finished TxJob, an abort reason)
 * to TxTask or the STATUS path.
 *
 * Burst-only by design in P4.7: configuration with the STREAM bit set is
 * rejected (-2); the live/stream path is P7.x work. A timeout while ARMED
 * aborts without a job. A completion while CAPT hands one job whose status
 * word is the raw la_top STATUS snapshot (DONE/FIFO_OVF/... decoded by the
 * task). Abort during CAPT discards the partial job - the DMA-driver layer
 * (P4.6) owns descriptor drain, this machine only reports the abort.
 */

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_ACQ_API __declspec(dllexport)
#else
#define LA_ACQ_API
#endif

typedef enum la_acq_state {
    LA_ACQ_IDLE = 0,
    LA_ACQ_ARMED = 1,
    LA_ACQ_CAPT = 2
} la_acq_state_t;

typedef enum la_acq_event_kind {
    LA_ACQ_EVT_NONE = 0,
    LA_ACQ_EVT_TRIGGERED = 1,
    LA_ACQ_EVT_JOB = 2,
    LA_ACQ_EVT_ABORTED = 3
} la_acq_event_kind_t;

typedef enum la_acq_abort_cause {
    LA_ACQ_ABORT_CMD = 0,
    LA_ACQ_ABORT_TIMEOUT = 1
} la_acq_abort_cause_t;

typedef struct la_acq_event {
    la_acq_event_kind_t kind;
    la_acq_abort_cause_t abort_cause; /* valid when kind == LA_ACQ_EVT_ABORTED */
    uint64_t trig_index;              /* EVT_TRIGGERED payload */
    uint64_t word_count;              /* JOB: words captured through the closing word */
    uint32_t status;                  /* JOB: raw la_top STATUS snapshot */
} la_acq_event_t;

typedef struct la_acq_fsm {
    la_acq_state_t state;
    int stream_cfg;      /* burst-only guard: nonzero STREAM rejected at arm */
    uint64_t last_trig_index;
} la_acq_fsm_t;

LA_ACQ_API void la_acq_fsm_init(la_acq_fsm_t *fsm);

/* CONFIG step: latch the burst/stream decision. Returns 0 when accepted,
 * -1 when not IDLE, -2 when the STREAM bit is set (P7.x scope). */
LA_ACQ_API int la_acq_fsm_configure(la_acq_fsm_t *fsm, int stream_enabled);

/* ARM step: IDLE -> ARMED. Returns 0, or -1 when not IDLE (incl. busy
 * double-arm), -2 when the latched configuration is stream mode. */
LA_ACQ_API int la_acq_fsm_arm(la_acq_fsm_t *fsm);

/* Trigger notification: ARMED -> CAPT and reports LA_ACQ_EVT_TRIGGERED
 * with the trigger index. Returns 0, or -1 when not ARMED. */
LA_ACQ_API int la_acq_fsm_trigger(la_acq_fsm_t *fsm, uint64_t trig_index,
                                  la_acq_event_t *event);

/* Timeout (ARMED without a trigger): aborts with LA_ACQ_ABORT_TIMEOUT and
 * returns to IDLE. Returns 0, or -1 when not ARMED. */
LA_ACQ_API int la_acq_fsm_timeout(la_acq_fsm_t *fsm, la_acq_event_t *event);

/* Abort command: ARMED or CAPT -> IDLE with LA_ACQ_ABORT_CMD. Returns 0,
 * or -1 when already IDLE. */
LA_ACQ_API int la_acq_fsm_abort(la_acq_fsm_t *fsm, la_acq_event_t *event);

/* Capture completion (DMA burst done): CAPT -> IDLE and reports the JOB
 * event carrying the raw STATUS snapshot (DONE sticky; FIFO_OVF sticky
 * when the FIFO dropped data) and the word count. Returns 0, or -1 when
 * not CAPT. */
LA_ACQ_API int la_acq_fsm_complete(la_acq_fsm_t *fsm, uint64_t word_count,
                                   uint32_t status, la_acq_event_t *event);

LA_ACQ_API la_acq_state_t la_acq_fsm_state(const la_acq_fsm_t *fsm);

#endif /* LA_ACQ_FSM_H */
