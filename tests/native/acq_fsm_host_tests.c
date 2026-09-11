#include "la_acq_fsm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static int test_happy_burst(void)
{
    la_acq_fsm_t fsm;
    la_acq_event_t event;

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_configure(&fsm, 0) == 0, "configure burst");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "configure stays IDLE");
    CHECK(la_acq_fsm_arm(&fsm) == 0, "arm");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_ARMED, "state ARMED");
    CHECK(la_acq_fsm_trigger(&fsm, UINT64_C(0x100), &event) == 0, "trigger");
    CHECK(event.kind == LA_ACQ_EVT_TRIGGERED, "trigger event kind");
    CHECK(event.trig_index == UINT64_C(0x100), "trigger index payload");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_CAPT, "state CAPT");
    CHECK(la_acq_fsm_complete(&fsm, UINT64_C(0x1234), 0x00000008u, &event) == 0,
          "complete");
    CHECK(event.kind == LA_ACQ_EVT_JOB, "job event kind");
    CHECK(event.word_count == UINT64_C(0x1234), "job word count");
    CHECK(event.status == 0x00000008u, "job raw status (DONE)");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "back to IDLE");
    return 0;
}

static int test_stream_lifecycle(void)
{
    la_acq_fsm_t fsm;
    la_acq_event_t event;

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_configure(&fsm, 1) == 0, "stream config latches");
    CHECK(la_acq_fsm_stream(&fsm) == 1, "stream flag latched");
    CHECK(la_acq_fsm_arm(&fsm) == 0, "stream arm accepted");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_CAPT, "stream arms straight to CAPT");
    CHECK(la_acq_fsm_trigger(&fsm, 7u, &event) == -1, "no trigger transition in CAPT");
    CHECK(la_acq_fsm_timeout(&fsm, &event) == -1, "no timeout in CAPT");
    CHECK(la_acq_fsm_complete(&fsm, 0u, 0x10u, &event) == 0, "stream completion");
    CHECK(event.kind == LA_ACQ_EVT_JOB, "stream completion is a job event");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "IDLE after stream completion");

    CHECK(la_acq_fsm_configure(&fsm, 1) == 0, "reconfigure stream");
    CHECK(la_acq_fsm_arm(&fsm) == 0, "stream arm again");
    CHECK(la_acq_fsm_abort(&fsm, &event) == 0, "abort streaming");
    CHECK(event.kind == LA_ACQ_EVT_ABORTED, "stream abort event kind");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "IDLE after stream abort");

    CHECK(la_acq_fsm_configure(&fsm, 0) == 0, "reconfigure burst");
    CHECK(la_acq_fsm_stream(&fsm) == 0, "stream flag cleared");
    CHECK(la_acq_fsm_arm(&fsm) == 0, "burst arm after stream");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_ARMED, "burst arm is ARMED");
    return 0;
}

static int test_illegal_transitions(void)
{
    la_acq_fsm_t fsm;
    la_acq_event_t event;

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_trigger(&fsm, 0u, &event) == -1, "trigger from IDLE rejected");
    CHECK(la_acq_fsm_timeout(&fsm, &event) == -1, "timeout from IDLE rejected");
    CHECK(la_acq_fsm_complete(&fsm, 0u, 0u, &event) == -1, "complete from IDLE rejected");
    CHECK(la_acq_fsm_abort(&fsm, &event) == -1, "abort from IDLE rejected");
    CHECK(la_acq_fsm_arm(&fsm) == 0, "arm");
    CHECK(la_acq_fsm_arm(&fsm) == -1, "double arm rejected");
    CHECK(la_acq_fsm_configure(&fsm, 0) == -1, "configure while ARMED rejected");
    CHECK(la_acq_fsm_trigger(&fsm, 5u, &event) == 0, "trigger");
    CHECK(la_acq_fsm_arm(&fsm) == -1, "arm while CAPT rejected");
    CHECK(la_acq_fsm_trigger(&fsm, 6u, &event) == -1, "double trigger rejected");
    return 0;
}

static int test_abort_paths(void)
{
    la_acq_fsm_t fsm;
    la_acq_event_t event;

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_arm(&fsm) == 0, "arm");
    CHECK(la_acq_fsm_abort(&fsm, &event) == 0, "abort while ARMED");
    CHECK(event.kind == LA_ACQ_EVT_ABORTED, "aborted event kind");
    CHECK(event.abort_cause == LA_ACQ_ABORT_CMD, "abort cause command");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "IDLE after abort");

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_arm(&fsm) == 0, "arm again");
    CHECK(la_acq_fsm_timeout(&fsm, &event) == 0, "timeout while ARMED");
    CHECK(event.kind == LA_ACQ_EVT_ABORTED, "timeout aborted kind");
    CHECK(event.abort_cause == LA_ACQ_ABORT_TIMEOUT, "timeout cause");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "IDLE after timeout");

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_arm(&fsm) == 0 && la_acq_fsm_trigger(&fsm, 9u, &event) == 0,
          "arm+trigger");
    CHECK(la_acq_fsm_timeout(&fsm, &event) == -1, "timeout during CAPT rejected");
    CHECK(la_acq_fsm_abort(&fsm, &event) == 0, "abort during CAPT");
    CHECK(event.kind == LA_ACQ_EVT_ABORTED && event.abort_cause == LA_ACQ_ABORT_CMD,
          "CAPT abort reports command abort");
    CHECK(event.trig_index == 9u, "abort keeps last trigger index");
    return 0;
}

static int test_overflow_flag_passthrough(void)
{
    la_acq_fsm_t fsm;
    la_acq_event_t event;

    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_arm(&fsm) == 0, "arm");
    CHECK(la_acq_fsm_trigger(&fsm, 0u, &event) == 0, "trigger");
    /* la_top STATUS.FIFO_OVF = 0x10; the drain still completes with DONE. */
    CHECK(la_acq_fsm_complete(&fsm, UINT64_C(42), 0x00000018u, &event) == 0, "complete");
    CHECK(event.kind == LA_ACQ_EVT_JOB, "job kind with overflow");
    CHECK(event.status == 0x00000018u, "raw status carries DONE|FIFO_OVF");
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "IDLE after overflow completion");
    return 0;
}

static int test_init_clears(void)
{
    la_acq_fsm_t fsm;

    memset(&fsm, 0xFF, sizeof(fsm));
    la_acq_fsm_init(&fsm);
    CHECK(la_acq_fsm_state(&fsm) == LA_ACQ_IDLE, "init resets to IDLE");
    return 0;
}

int main(void)
{
    if (test_happy_burst() != 0 || test_stream_lifecycle() != 0 ||
        test_illegal_transitions() != 0 || test_abort_paths() != 0 ||
        test_overflow_flag_passthrough() != 0 || test_init_clears() != 0) {
        return 1;
    }
    puts("PASS: la_native_acq");
    return 0;
}
