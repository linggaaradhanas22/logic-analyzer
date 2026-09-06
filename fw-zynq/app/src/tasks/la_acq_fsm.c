#include "la_acq_fsm.h"

void la_acq_fsm_init(la_acq_fsm_t *fsm)
{
    fsm->state = LA_ACQ_IDLE;
    fsm->stream_cfg = 0;
    fsm->last_trig_index = 0u;
}

int la_acq_fsm_configure(la_acq_fsm_t *fsm, int stream_enabled)
{
    if (fsm->state != LA_ACQ_IDLE) {
        return -1;
    }
    fsm->stream_cfg = stream_enabled != 0;
    return 0;
}

int la_acq_fsm_arm(la_acq_fsm_t *fsm)
{
    if (fsm->state != LA_ACQ_IDLE) {
        return -1;
    }
    if (fsm->stream_cfg != 0) {
        return -2;
    }
    fsm->state = LA_ACQ_ARMED;
    return 0;
}

int la_acq_fsm_trigger(la_acq_fsm_t *fsm, uint64_t trig_index,
                       la_acq_event_t *event)
{
    if (fsm->state != LA_ACQ_ARMED) {
        return -1;
    }
    fsm->state = LA_ACQ_CAPT;
    fsm->last_trig_index = trig_index;
    event->kind = LA_ACQ_EVT_TRIGGERED;
    event->abort_cause = LA_ACQ_ABORT_CMD;
    event->trig_index = trig_index;
    event->word_count = 0u;
    event->status = 0u;
    return 0;
}

int la_acq_fsm_timeout(la_acq_fsm_t *fsm, la_acq_event_t *event)
{
    if (fsm->state != LA_ACQ_ARMED) {
        return -1;
    }
    fsm->state = LA_ACQ_IDLE;
    event->kind = LA_ACQ_EVT_ABORTED;
    event->abort_cause = LA_ACQ_ABORT_TIMEOUT;
    event->trig_index = 0u;
    event->word_count = 0u;
    event->status = 0u;
    return 0;
}

int la_acq_fsm_abort(la_acq_fsm_t *fsm, la_acq_event_t *event)
{
    if (fsm->state == LA_ACQ_IDLE) {
        return -1;
    }
    fsm->state = LA_ACQ_IDLE;
    event->kind = LA_ACQ_EVT_ABORTED;
    event->abort_cause = LA_ACQ_ABORT_CMD;
    event->trig_index = fsm->last_trig_index;
    event->word_count = 0u;
    event->status = 0u;
    return 0;
}

int la_acq_fsm_complete(la_acq_fsm_t *fsm, uint64_t word_count,
                        uint32_t status, la_acq_event_t *event)
{
    if (fsm->state != LA_ACQ_CAPT) {
        return -1;
    }
    fsm->state = LA_ACQ_IDLE;
    event->kind = LA_ACQ_EVT_JOB;
    event->abort_cause = LA_ACQ_ABORT_CMD;
    event->trig_index = fsm->last_trig_index;
    event->word_count = word_count;
    event->status = status;
    return 0;
}

la_acq_state_t la_acq_fsm_state(const la_acq_fsm_t *fsm)
{
    return fsm->state;
}
