/*
 * U02 host tests for the direct-USB portable core (la_usb_core).
 *
 * Covers the session lifecycle actions (generation bump, parser clear,
 * acquisition stop, endpoint gating) and the single-producer /
 * single-consumer completion ring with stale-generation dropping.
 */

#include "la_usb_core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                   \
    do {                                            \
        if (!(condition)) {                         \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                               \
        }                                           \
    } while (0)

static la_usb_completion_t completion(uint32_t generation, uint32_t handle,
                                      uint8_t ep, uint32_t len)
{
    la_usb_completion_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.generation = generation;
    entry.handle = handle;
    entry.ep = ep;
    entry.len = len;
    entry.ptr = (uint8_t *)(uintptr_t)handle;
    return entry;
}

static int test_session_init(void)
{
    la_usb_session_t session;

    la_usb_session_init(&session);
    CHECK(session.state == LA_USB_STATE_DETACHED, "initial state");
    CHECK(session.generation == LA_USB_GENERATION_INIT, "initial generation");
    CHECK(session.address == 0u, "initial address");
    CHECK(session.configured == 0u, "initial configured");
    CHECK(session.protocol_errors == 0u, "initial errors");
    la_usb_session_init(NULL);
    CHECK(la_usb_session_event(NULL, LA_USB_EV_BUS_RESET, 0u) ==
              LA_USB_ACT_NONE,
          "NULL session guard");
    return 0;
}

static int test_reset_and_disconnect(void)
{
    la_usb_session_t session;
    unsigned actions;

    la_usb_session_init(&session);
    actions = la_usb_session_event(&session, LA_USB_EV_BUS_RESET, 0u);
    CHECK(session.state == LA_USB_STATE_DEFAULT, "reset state");
    CHECK(session.generation == LA_USB_GENERATION_INIT + 1u,
          "reset bumps generation");
    CHECK((actions & LA_USB_ACT_INVALIDATE) != 0u, "reset invalidates");
    CHECK((actions & LA_USB_ACT_CLEAR_PARSER) != 0u, "reset clears parser");
    CHECK((actions & LA_USB_ACT_STOP_ACQUISITION) != 0u,
          "reset stops acquisition");
    CHECK((actions & LA_USB_ACT_STOP_ENDPOINTS) != 0u, "reset stops endpoints");

    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_ADDRESS, 7u) ==
              LA_USB_ACT_NONE,
          "address event");
    CHECK(session.state == LA_USB_STATE_ADDRESSED, "addressed state");
    CHECK(session.address == 7u, "address latched");
    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 1u) ==
              LA_USB_ACT_START_ENDPOINTS,
          "configure starts endpoints");
    CHECK(session.state == LA_USB_STATE_CONFIGURED, "configured state");

    actions = la_usb_session_event(&session, LA_USB_EV_DISCONNECT, 0u);
    CHECK(session.state == LA_USB_STATE_DETACHED, "disconnect state");
    CHECK(session.configured == 0u, "disconnect clears configured");
    CHECK(session.address == 0u, "disconnect clears address");
    CHECK((actions & LA_USB_ACT_INVALIDATE) != 0u, "disconnect invalidates");
    CHECK(session.generation == LA_USB_GENERATION_INIT + 2u,
          "disconnect bumps generation");

    CHECK(la_usb_session_event(&session, LA_USB_EV_DISCONNECT, 0u) ==
              LA_USB_ACT_NONE,
          "repeated disconnect is idle");
    CHECK(session.generation == LA_USB_GENERATION_INIT + 2u,
          "repeated disconnect does not bump generation");
    return 0;
}

static int test_invalid_transitions(void)
{
    la_usb_session_t session;

    la_usb_session_init(&session);
    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_ADDRESS, 3u) ==
              LA_USB_ACT_NONE,
          "address while detached");
    CHECK(session.protocol_errors == 1u, "address error counted");
    CHECK(session.state == LA_USB_STATE_DETACHED, "state unchanged");

    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 1u) ==
              LA_USB_ACT_NONE,
          "configure while detached");
    CHECK(session.protocol_errors == 2u, "configure error counted");

    CHECK(la_usb_session_event(&session, LA_USB_EV_RESUME, 0u) ==
              LA_USB_ACT_NONE,
          "resume while detached");
    CHECK(session.protocol_errors == 3u, "resume error counted");

    CHECK(la_usb_session_event(&session, (la_usb_event_t)99, 0u) ==
              LA_USB_ACT_NONE,
          "unknown event");
    CHECK(session.protocol_errors == 4u, "unknown event counted");
    return 0;
}

static int test_unconfigure_and_suspend(void)
{
    la_usb_session_t session;

    la_usb_session_init(&session);
    (void)la_usb_session_event(&session, LA_USB_EV_BUS_RESET, 0u);
    (void)la_usb_session_event(&session, LA_USB_EV_SET_ADDRESS, 1u);
    (void)la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 1u);
    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 0u) ==
              LA_USB_ACT_STOP_ENDPOINTS,
          "unconfigure stops endpoints");
    CHECK(session.state == LA_USB_STATE_ADDRESSED, "unconfigured state");
    CHECK(session.configured == 0u, "unconfigured flag");
    CHECK(la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 0u) ==
              LA_USB_ACT_NONE,
          "repeated unconfigure is idle");

    (void)la_usb_session_event(&session, LA_USB_EV_SET_CONFIGURATION, 1u);
    CHECK(la_usb_session_event(&session, LA_USB_EV_SUSPEND, 0u) ==
              LA_USB_ACT_GATE_SUBMISSIONS,
          "suspend gates submissions");
    CHECK(session.state == LA_USB_STATE_SUSPENDED, "suspended state");
    CHECK(session.resume_state == LA_USB_STATE_CONFIGURED,
          "resume state retained");
    CHECK(la_usb_session_event(&session, LA_USB_EV_SUSPEND, 0u) ==
              LA_USB_ACT_NONE,
          "repeated suspend is idle");
    CHECK(la_usb_session_event(&session, LA_USB_EV_RESUME, 0u) ==
              LA_USB_ACT_NONE,
          "resume has no actions");
    CHECK(session.state == LA_USB_STATE_CONFIGURED, "resumed state");

    (void)la_usb_session_event(&session, LA_USB_EV_SUSPEND, 0u);
    (void)la_usb_session_event(&session, LA_USB_EV_DISCONNECT, 0u);
    CHECK(session.state == LA_USB_STATE_DETACHED, "disconnect from suspend");
    CHECK(session.resume_state == LA_USB_STATE_DETACHED,
          "disconnect clears resume state");
    return 0;
}

static int test_ring_order_and_guards(void)
{
    la_usb_ring_t ring;
    la_usb_completion_t out;
    uint32_t index;

    la_usb_ring_init(&ring);
    CHECK(la_usb_ring_used(&ring) == 0u, "empty used");
    CHECK(la_usb_ring_pop(&ring, 1u, &out) == 0, "empty pop");
    CHECK(la_usb_ring_push(NULL, &out) == -1, "push NULL ring");
    CHECK(la_usb_ring_push(&ring, NULL) == -1, "push NULL completion");
    CHECK(la_usb_ring_pop(NULL, 1u, &out) == 0, "pop NULL ring");
    CHECK(la_usb_ring_pop(&ring, 1u, NULL) == 0, "pop NULL out");
    CHECK(la_usb_ring_used(NULL) == 0u, "used NULL guard");

    for (index = 0u; index < LA_USB_RING_CAPACITY; index++) {
        la_usb_completion_t entry = completion(1u, index + 1u,
                                               (uint8_t)(index % 2u),
                                               index);
        CHECK(la_usb_ring_push(&ring, &entry) == 0, "fill ring");
    }
    CHECK(la_usb_ring_used(&ring) == LA_USB_RING_CAPACITY, "full used");
    {
        la_usb_completion_t overflow = completion(1u, 0u, 0u, 0u);
        CHECK(la_usb_ring_push(&ring, &overflow) == -1, "overflow rejected");
    }
    CHECK(ring.overflows == 1u, "overflow counted");

    for (index = 0u; index < LA_USB_RING_CAPACITY; index++) {
        CHECK(la_usb_ring_pop(&ring, 1u, &out) == 1, "pop filled entry");
        CHECK(out.handle == index + 1u, "FIFO order");
        CHECK(out.len == index, "length preserved");
    }
    CHECK(la_usb_ring_pop(&ring, 1u, &out) == 0, "drained ring");
    CHECK(ring.pushed == LA_USB_RING_CAPACITY, "pushed counter");
    CHECK(ring.popped == LA_USB_RING_CAPACITY, "popped counter");
    return 0;
}

static int test_stale_completions(void)
{
    la_usb_ring_t ring;
    la_usb_completion_t out;
    la_usb_completion_t entry;

    la_usb_ring_init(&ring);
    entry = completion(1u, 0x11u, 1u, 8u);
    CHECK(la_usb_ring_push(&ring, &entry) == 0, "push stale");
    entry = completion(2u, 0x22u, 1u, 9u);
    CHECK(la_usb_ring_push(&ring, &entry) == 0, "push current");
    entry = completion(1u, 0x33u, 0u, 2u);
    CHECK(la_usb_ring_push(&ring, &entry) == 0, "push second stale");

    CHECK(la_usb_ring_pop(&ring, 2u, &out) == 1, "pop skips stale head");
    CHECK(out.handle == 0x22u, "current generation delivered");
    CHECK(ring.stale_drops == 1u, "stale drop counted");
    CHECK(la_usb_ring_pop(&ring, 2u, &out) == 0, "trailing stale dropped");
    CHECK(ring.stale_drops == 2u, "second stale counted");

    entry = completion(3u, 0x44u, 1u, 4u);
    CHECK(la_usb_ring_push(&ring, &entry) == 0, "push new generation");
    CHECK(la_usb_ring_pop(&ring, 2u, &out) == 0, "old generation drops bump");
    CHECK(ring.stale_drops == 3u, "stale drop after bump");
    entry = completion(3u, 0x55u, 1u, 5u);
    CHECK(la_usb_ring_push(&ring, &entry) == 0, "push second new generation");
    CHECK(la_usb_ring_pop(&ring, 3u, &out) == 1, "new generation pops");
    CHECK(out.handle == 0x55u, "new generation handle");
    return 0;
}

static int test_ring_wrap(void)
{
    la_usb_ring_t ring;
    la_usb_completion_t out;
    la_usb_completion_t entry;
    uint32_t index;

    la_usb_ring_init(&ring);
    for (index = 0u; index < 40u; index++) {
        entry = completion(1u, index + 1u, 1u, index);
        CHECK(la_usb_ring_push(&ring, &entry) == 0, "wrap push");
        CHECK(la_usb_ring_pop(&ring, 1u, &out) == 1, "wrap pop");
        CHECK(out.handle == index + 1u, "wrap order");
    }
    CHECK(la_usb_ring_used(&ring) == 0u, "wrap drained");
    CHECK(ring.overflows == 0u, "no overflow while draining");
    return 0;
}

int main(void)
{
    if (test_session_init() != 0 || test_reset_and_disconnect() != 0 ||
        test_invalid_transitions() != 0 || test_unconfigure_and_suspend() != 0 ||
        test_ring_order_and_guards() != 0 || test_stale_completions() != 0 ||
        test_ring_wrap() != 0) {
        return 1;
    }
    puts("PASS: la_native_usb_core");
    return 0;
}
