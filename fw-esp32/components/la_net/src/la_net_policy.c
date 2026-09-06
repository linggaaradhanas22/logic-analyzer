#include "la_net_policy.h"

#include <string.h>

#define LA_NET_POLICY_DEFAULT_START_MS ((uint32_t)1000u)
#define LA_NET_POLICY_DEFAULT_MAX_MS ((uint32_t)60000u)
#define LA_NET_POLICY_DEFAULT_MULTIPLIER ((uint8_t)2u)

static void no_actions(size_t *action_count)
{
    if (action_count != NULL) {
        *action_count = 0u;
    }
}

static void emit(la_net_policy_action_t *actions,
                 size_t capacity,
                 size_t *action_count,
                 la_net_policy_action_t action)
{
    if (action_count != NULL && *action_count < capacity) {
        actions[*action_count] = action;
        *action_count += 1u;
    }
}

static uint32_t backoff_delay(const la_net_policy_t *policy)
{
    uint32_t delay = policy->config.backoff_start_ms;
    uint32_t failures = policy->consecutive_failures;
    uint32_t factor;
    uint64_t product;

    /* failures starts at 1 for the wait after the first failure. */
    for (factor = 1u; factor < failures; ++factor) {
        product = (uint64_t)delay * policy->config.multiplier;
        if (product > policy->config.backoff_max_ms) {
            delay = policy->config.backoff_max_ms;
            break;
        }
        delay = (uint32_t)product;
    }
    if (delay > policy->config.backoff_max_ms) {
        delay = policy->config.backoff_max_ms;
    }
    return delay;
}

static void enter_backoff(la_net_policy_t *policy, uint32_t now_ms)
{
    uint32_t delay;

    policy->consecutive_failures += 1u;
    delay = backoff_delay(policy);
    policy->next_attempt_ms = now_ms + delay;
    policy->state = LA_NET_POLICY_BACKOFF;
}

int la_net_policy_init(la_net_policy_t *policy,
                       const la_net_policy_config_t *config)
{
    if (policy == NULL) {
        return -1;
    }
    memset(policy, 0, sizeof(*policy));
    policy->state = LA_NET_POLICY_IDLE;
    if (config == NULL) {
        policy->config.backoff_start_ms = LA_NET_POLICY_DEFAULT_START_MS;
        policy->config.backoff_max_ms = LA_NET_POLICY_DEFAULT_MAX_MS;
        policy->config.multiplier = LA_NET_POLICY_DEFAULT_MULTIPLIER;
        return 0;
    }
    policy->config = *config;
    if (policy->config.backoff_start_ms == 0u) {
        policy->config.backoff_start_ms = LA_NET_POLICY_DEFAULT_START_MS;
    }
    if (policy->config.backoff_max_ms == 0u) {
        policy->config.backoff_max_ms = LA_NET_POLICY_DEFAULT_MAX_MS;
    }
    if (policy->config.multiplier < 1u) {
        policy->config.multiplier = 1u;
    }
    if (policy->config.backoff_max_ms < policy->config.backoff_start_ms) {
        policy->config.backoff_max_ms = policy->config.backoff_start_ms;
    }
    return 0;
}

static int step_start(la_net_policy_t *policy,
                      uint32_t now_ms,
                      la_net_policy_action_t *actions,
                      size_t capacity,
                      size_t *action_count)
{
    (void)now_ms;
    if (policy->state != LA_NET_POLICY_IDLE) {
        return 0; /* double start ignored; STOP first */
    }
    policy->state = LA_NET_POLICY_CONNECTING;
    policy->stats.wifi_attempts += 1u;
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_WIFI_CONNECT);
    return 0;
}

static int step_stop(la_net_policy_t *policy,
                     uint32_t now_ms,
                     la_net_policy_action_t *actions,
                     size_t capacity,
                     size_t *action_count)
{
    (void)now_ms;
    if (policy->state == LA_NET_POLICY_IDLE) {
        return 0;
    }
    if (policy->state == LA_NET_POLICY_CLIENT_ACTIVE ||
        policy->state == LA_NET_POLICY_CONNECTED) {
        emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LISTEN_STOP);
    }
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_WIFI_STOP);
    policy->state = LA_NET_POLICY_IDLE;
    policy->consecutive_failures = 0u;
    return 0;
}

static int step_wifi_up(la_net_policy_t *policy,
                        uint32_t now_ms,
                        la_net_policy_action_t *actions,
                        size_t capacity,
                        size_t *action_count)
{
    (void)now_ms;
    if (policy->state != LA_NET_POLICY_CONNECTING) {
        return 0;
    }
    policy->stats.wifi_successes += 1u;
    policy->consecutive_failures = 0u;
    policy->state = LA_NET_POLICY_CONNECTED;
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LISTEN_START);
    return 0;
}

static int step_wifi_lost(la_net_policy_t *policy,
                          uint32_t now_ms,
                          la_net_policy_action_t *actions,
                          size_t capacity,
                          size_t *action_count)
{
    if (policy->state != LA_NET_POLICY_CONNECTING &&
        policy->state != LA_NET_POLICY_CONNECTED &&
        policy->state != LA_NET_POLICY_CLIENT_ACTIVE) {
        return 0;
    }
    if (policy->state == LA_NET_POLICY_CLIENT_ACTIVE) {
        /* The host connection is gone with the link; the Zynq session is
         * answered like a client disconnect so a mid-burst capture stops. */
        emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LINK_ABORT);
        emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LINK_LIVE_STOP);
    }
    if (policy->state == LA_NET_POLICY_CONNECTED ||
        policy->state == LA_NET_POLICY_CLIENT_ACTIVE) {
        emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LISTEN_STOP);
    }
    enter_backoff(policy, now_ms);
    return 0;
}

static int step_backoff_tick(la_net_policy_t *policy,
                             uint32_t now_ms,
                             la_net_policy_action_t *actions,
                             size_t capacity,
                             size_t *action_count)
{
    if (policy->state != LA_NET_POLICY_BACKOFF) {
        return 0;
    }
    if (now_ms < policy->next_attempt_ms) {
        return 0;
    }
    policy->state = LA_NET_POLICY_CONNECTING;
    policy->stats.wifi_attempts += 1u;
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_WIFI_CONNECT);
    return 0;
}

static int step_client_connected(la_net_policy_t *policy,
                                 uint32_t now_ms,
                                 la_net_policy_action_t *actions,
                                 size_t capacity,
                                 size_t *action_count)
{
    (void)now_ms;
    if (policy->state != LA_NET_POLICY_CONNECTED) {
        /* No listening session or a client already active: refuse. */
        emit(actions, capacity, action_count, LA_NET_POLICY_ACT_CLIENT_REJECT);
        policy->stats.clients_rejected += 1u;
        return 0;
    }
    policy->state = LA_NET_POLICY_CLIENT_ACTIVE;
    policy->stats.clients_accepted += 1u;
    return 0;
}

static int step_client_disconnected(la_net_policy_t *policy,
                                    uint32_t now_ms,
                                    la_net_policy_action_t *actions,
                                    size_t capacity,
                                    size_t *action_count)
{
    (void)now_ms;
    if (policy->state != LA_NET_POLICY_CLIENT_ACTIVE) {
        return 0;
    }
    policy->state = LA_NET_POLICY_CONNECTED;
    policy->stats.client_disconnects += 1u;
    /* protocol.md: the relay answers a lost client with ABORT then
     * LIVE_STOP so a mid-burst Zynq capture stops. */
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LINK_ABORT);
    emit(actions, capacity, action_count, LA_NET_POLICY_ACT_LINK_LIVE_STOP);
    return 0;
}

int la_net_policy_step(la_net_policy_t *policy,
                       la_net_policy_event_t event,
                       uint32_t now_ms,
                       la_net_policy_action_t *actions,
                       size_t capacity,
                       size_t *action_count)
{
    if (policy == NULL || actions == NULL || action_count == NULL ||
        event == LA_NET_POLICY_EVT_NONE) {
        return -1;
    }
    *action_count = 0u;
    switch (event) {
    case LA_NET_POLICY_EVT_START:
        return step_start(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_STOP:
        return step_stop(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_WIFI_UP:
        return step_wifi_up(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_WIFI_LOST:
        return step_wifi_lost(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_CLIENT_CONNECTED:
        return step_client_connected(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_CLIENT_DISCONNECTED:
        return step_client_disconnected(policy, now_ms, actions, capacity, action_count);
    case LA_NET_POLICY_EVT_BACKOFF_TICK:
        return step_backoff_tick(policy, now_ms, actions, capacity, action_count);
    default:
        no_actions(action_count);
        return 0;
    }
}

la_net_policy_state_t la_net_policy_state(const la_net_policy_t *policy)
{
    if (policy == NULL) {
        return LA_NET_POLICY_IDLE;
    }
    return policy->state;
}

void la_net_policy_snapshot(const la_net_policy_t *policy,
                            la_net_policy_stats_t *stats)
{
    if (policy == NULL || stats == NULL) {
        return;
    }
    memcpy(stats, &policy->stats, sizeof(*stats));
}
