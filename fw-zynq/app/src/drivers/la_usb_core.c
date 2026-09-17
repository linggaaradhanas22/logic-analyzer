/*
 * U02 direct-USB portable core implementation. See la_usb_core.h for the
 * ownership and interrupt-deferral contract.
 */

#include "la_usb_core.h"

static uint32_t la_usb_actions_for_new_session(la_usb_session_t *session)
{
    session->generation++;
    session->address = 0u;
    session->configured = 0u;
    return LA_USB_ACT_INVALIDATE | LA_USB_ACT_CLEAR_PARSER |
           LA_USB_ACT_STOP_ACQUISITION | LA_USB_ACT_STOP_ENDPOINTS;
}

void la_usb_session_init(la_usb_session_t *session)
{
    if (session == NULL) {
        return;
    }
    session->state = LA_USB_STATE_DETACHED;
    session->resume_state = LA_USB_STATE_DETACHED;
    session->generation = LA_USB_GENERATION_INIT;
    session->address = 0u;
    session->configured = 0u;
    session->protocol_errors = 0u;
}

unsigned la_usb_session_event(la_usb_session_t *session, la_usb_event_t event,
                              uint8_t arg)
{
    if (session == NULL) {
        return LA_USB_ACT_NONE;
    }

    switch (event) {
    case LA_USB_EV_DISCONNECT:
        if (session->state == LA_USB_STATE_DETACHED) {
            return LA_USB_ACT_NONE;
        }
        session->state = LA_USB_STATE_DETACHED;
        session->resume_state = LA_USB_STATE_DETACHED;
        return la_usb_actions_for_new_session(session);

    case LA_USB_EV_BUS_RESET:
        session->state = LA_USB_STATE_DEFAULT;
        session->resume_state = LA_USB_STATE_DEFAULT;
        return la_usb_actions_for_new_session(session);

    case LA_USB_EV_SET_ADDRESS:
        if (session->state != LA_USB_STATE_DEFAULT &&
            session->state != LA_USB_STATE_ADDRESSED) {
            session->protocol_errors++;
            return LA_USB_ACT_NONE;
        }
        session->address = arg;
        session->state = LA_USB_STATE_ADDRESSED;
        return LA_USB_ACT_NONE;

    case LA_USB_EV_SET_CONFIGURATION:
        if (arg != 0u) {
            if (session->state != LA_USB_STATE_ADDRESSED &&
                session->state != LA_USB_STATE_CONFIGURED) {
                session->protocol_errors++;
                return LA_USB_ACT_NONE;
            }
            session->configured = 1u;
            session->state = LA_USB_STATE_CONFIGURED;
            return LA_USB_ACT_START_ENDPOINTS;
        }
        if (session->state != LA_USB_STATE_CONFIGURED) {
            return LA_USB_ACT_NONE;
        }
        session->configured = 0u;
        session->state = LA_USB_STATE_ADDRESSED;
        return LA_USB_ACT_STOP_ENDPOINTS;

    case LA_USB_EV_SUSPEND:
        if (session->state == LA_USB_STATE_DETACHED ||
            session->state == LA_USB_STATE_SUSPENDED) {
            return LA_USB_ACT_NONE;
        }
        session->resume_state = session->state;
        session->state = LA_USB_STATE_SUSPENDED;
        return LA_USB_ACT_GATE_SUBMISSIONS;

    case LA_USB_EV_RESUME:
        if (session->state != LA_USB_STATE_SUSPENDED) {
            session->protocol_errors++;
            return LA_USB_ACT_NONE;
        }
        session->state = session->resume_state;
        return LA_USB_ACT_NONE;

    default:
        session->protocol_errors++;
        return LA_USB_ACT_NONE;
    }
}

void la_usb_ring_init(la_usb_ring_t *ring)
{
    uint32_t index;

    if (ring == NULL) {
        return;
    }
    for (index = 0u; index < LA_USB_RING_CAPACITY; index++) {
        ring->entries[index].generation = 0u;
        ring->entries[index].handle = 0u;
        ring->entries[index].ptr = NULL;
        ring->entries[index].len = 0u;
        ring->entries[index].ep = 0u;
    }
    ring->head = 0u;
    ring->tail = 0u;
    ring->used = 0u;
    ring->pushed = 0u;
    ring->popped = 0u;
    ring->stale_drops = 0u;
    ring->overflows = 0u;
}

int la_usb_ring_push(la_usb_ring_t *ring, const la_usb_completion_t *completion)
{
    if (ring == NULL || completion == NULL) {
        return -1;
    }
    if (ring->used >= LA_USB_RING_CAPACITY) {
        ring->overflows++;
        return -1;
    }
    ring->entries[ring->head] = *completion;
    ring->head = (ring->head + 1u) % LA_USB_RING_CAPACITY;
    ring->used++;
    ring->pushed++;
    return 0;
}

int la_usb_ring_pop(la_usb_ring_t *ring, uint32_t generation,
                    la_usb_completion_t *out)
{
    if (ring == NULL || out == NULL) {
        return 0;
    }
    while (ring->used > 0u) {
        la_usb_completion_t *entry = &ring->entries[ring->tail];

        ring->tail = (ring->tail + 1u) % LA_USB_RING_CAPACITY;
        ring->used--;
        ring->popped++;
        if (entry->generation != generation) {
            ring->stale_drops++;
            continue;
        }
        *out = *entry;
        return 1;
    }
    return 0;
}

uint32_t la_usb_ring_used(const la_usb_ring_t *ring)
{
    return (ring == NULL) ? 0u : ring->used;
}
