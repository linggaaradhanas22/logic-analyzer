#include "la_relay.h"

#include <string.h>

static int type_in_table(const uint8_t *types, size_t count, uint8_t type)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (types[index] == type) {
            return 1;
        }
    }
    return 0;
}

int la_relay_init(la_relay_t *relay,
                  la_relay_role_t role,
                  const uint8_t *best_effort_types,
                  size_t best_effort_count,
                  const uint8_t *mirror_types,
                  size_t mirror_count)
{
    if (relay == NULL ||
        (role != LA_RELAY_ROLE_UART_TO_NET && role != LA_RELAY_ROLE_NET_TO_UART) ||
        best_effort_count > LA_RELAY_MAX_TYPES ||
        (best_effort_types == NULL && best_effort_count != 0u) ||
        mirror_count > LA_RELAY_MAX_TYPES ||
        (mirror_types == NULL && mirror_count != 0u)) {
        return -1;
    }
    memset(relay, 0, sizeof(*relay));
    relay->role = role;
    if (best_effort_count != 0u) {
        memcpy(relay->best_effort_types, best_effort_types, best_effort_count);
    }
    relay->best_effort_count = (uint8_t)best_effort_count;
    if (mirror_count != 0u) {
        memcpy(relay->mirror_types, mirror_types, mirror_count);
    }
    relay->mirror_count = (uint8_t)mirror_count;
    return 0;
}

int la_relay_is_best_effort(const la_relay_t *relay, uint8_t type)
{
    if (relay == NULL) {
        return 0;
    }
    return type_in_table(relay->best_effort_types, relay->best_effort_count, type);
}

int la_relay_is_mirror(const la_relay_t *relay, uint8_t type)
{
    if (relay == NULL) {
        return 0;
    }
    return type_in_table(relay->mirror_types, relay->mirror_count, type);
}

int la_relay_accept(la_relay_t *relay,
                    uint8_t type,
                    size_t frame_bytes,
                    la_relay_capacity_fn capacity,
                    void *context,
                    la_relay_decision_t *decision)
{
    int room;

    if (relay == NULL || decision == NULL || capacity == NULL || frame_bytes == 0u) {
        return -1;
    }
    room = capacity(context, frame_bytes);
    if (la_relay_is_best_effort(relay, type)) {
        if (room != 0) {
            *decision = LA_RELAY_FORWARD;
            return 0;
        }
        *decision = LA_RELAY_DROP_BEST_EFFORT;
        relay->stats.best_effort_dropped += 1u;
        return 0;
    }
    if (room != 0) {
        *decision = LA_RELAY_FORWARD;
        return 0;
    }
    *decision = LA_RELAY_PRESSURE;
    relay->stats.control_pressure_events += 1u;
    return 0;
}

int la_relay_report(la_relay_t *relay,
                    uint8_t type,
                    la_relay_result_t result)
{
    if (relay == NULL) {
        return -1;
    }
    switch (result) {
    case LA_RELAY_FWD_OK:
        relay->stats.frames_forwarded += 1u;
        return 0;
    case LA_RELAY_FWD_FAIL:
        relay->stats.frames_forward_failed += 1u;
        return 0;
    case LA_RELAY_MIRROR_OK:
        if (!la_relay_is_mirror(relay, type)) {
            return -1;
        }
        relay->stats.frames_mirrored += 1u;
        return 0;
    case LA_RELAY_MIRROR_FAIL:
        if (!la_relay_is_mirror(relay, type)) {
            return -1;
        }
        relay->stats.mirror_dropped += 1u;
        return 0;
    default:
        return -1;
    }
}

void la_relay_snapshot(const la_relay_t *relay, la_relay_stats_t *stats)
{
    if (relay == NULL || stats == NULL) {
        return;
    }
    memcpy(stats, &relay->stats, sizeof(*stats));
}

void la_relay_reset_counters(la_relay_t *relay)
{
    if (relay == NULL) {
        return;
    }
    memset(&relay->stats, 0, sizeof(relay->stats));
}
