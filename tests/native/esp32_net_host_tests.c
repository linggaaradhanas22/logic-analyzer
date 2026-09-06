#include "la_net_policy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

#define ACTION_CAP 8u

typedef struct step_result {
    la_net_policy_action_t actions[ACTION_CAP];
    size_t count;
} step_result_t;

static int step(la_net_policy_t *policy, la_net_policy_event_t event, uint32_t now,
                step_result_t *result)
{
    return la_net_policy_step(policy, event, now, result->actions, ACTION_CAP,
                              &result->count);
}

static int contains(const step_result_t *result, la_net_policy_action_t action)
{
    size_t index;

    for (index = 0u; index < result->count; ++index) {
        if (result->actions[index] == action) {
            return 1;
        }
    }
    return 0;
}

static int test_init_defaults(void)
{
    la_net_policy_t policy;

    memset(&policy, 0xFF, sizeof(policy));
    CHECK(la_net_policy_init(&policy, NULL) == 0, "init with defaults");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_IDLE, "init state IDLE");
    CHECK(policy.config.backoff_start_ms == 1000u &&
              policy.config.backoff_max_ms == 60000u && policy.config.multiplier == 2u,
          "default backoff configuration");
    return 0;
}

static int test_init_clamps(void)
{
    la_net_policy_t policy;
    la_net_policy_config_t config;

    memset(&config, 0, sizeof(config));
    config.backoff_start_ms = 3000u;
    config.backoff_max_ms = 500u;
    config.multiplier = 0u;
    CHECK(la_net_policy_init(&policy, &config) == 0, "init with clamped config");
    CHECK(policy.config.backoff_start_ms == 3000u && policy.config.backoff_max_ms == 3000u &&
              policy.config.multiplier == 1u,
          "max raised to start and multiplier clamped to one");
    return 0;
}

static int test_happy_session(void)
{
    la_net_policy_t policy;
    step_result_t result;
    la_net_policy_stats_t stats;

    CHECK(la_net_policy_init(&policy, NULL) == 0, "init");
    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 0u, &result) == 0, "start");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_WIFI_CONNECT),
          "start connects immediately");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_CONNECTING, "state CONNECTING");

    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 250u, &result) == 0, "wifi up");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_LISTEN_START),
          "association starts the listen");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_CONNECTED, "state CONNECTED");

    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 300u, &result) == 0, "client in");
    CHECK(result.count == 0u, "client accepted silently");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_CLIENT_ACTIVE, "state CLIENT_ACTIVE");

    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_DISCONNECTED, 900u, &result) == 0,
          "client gone");
    CHECK(result.count == 2u && result.actions[0] == LA_NET_POLICY_ACT_LINK_ABORT &&
              result.actions[1] == LA_NET_POLICY_ACT_LINK_LIVE_STOP,
          "disconnect answers with ABORT then LIVE_STOP in order");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_CONNECTED, "back to CONNECTED");

    la_net_policy_snapshot(&policy, &stats);
    CHECK(stats.wifi_attempts == 1u && stats.wifi_successes == 1u &&
              stats.clients_accepted == 1u && stats.client_disconnects == 1u,
          "session counters");
    return 0;
}

static int test_backoff_sequence(void)
{
    la_net_policy_t policy;
    la_net_policy_config_t config;
    step_result_t result;
    la_net_policy_stats_t stats;
    uint32_t now;

    memset(&config, 0, sizeof(config));
    config.backoff_start_ms = 100u;
    config.backoff_max_ms = 600u;
    config.multiplier = 2u;
    CHECK(la_net_policy_init(&policy, &config) == 0, "init fast backoff");
    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 0u, &result) == 0, "start");

    now = 10u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, now, &result) == 0, "first failure");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_BACKOFF, "state BACKOFF");
    CHECK(result.count == 0u, "no actions on entering backoff");

    now = 100u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0, "early tick");
    CHECK(result.count == 0u && la_net_policy_state(&policy) == LA_NET_POLICY_BACKOFF,
          "tick before the deadline is ignored");

    now = 110u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0, "deadline tick");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_WIFI_CONNECT) &&
              la_net_policy_state(&policy) == LA_NET_POLICY_CONNECTING,
          "retry fires at the deadline");

    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 150u, &result) == 0, "second failure");
    now = 110u + 200u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 0u && la_net_policy_state(&policy) == LA_NET_POLICY_BACKOFF,
          "second wait doubles");
    now += 100u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 1u, "second retry fires after 200 ms");

    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 500u, &result) == 0, "third failure");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 500u, &result) == 0, "duplicate failure");
    CHECK(result.count == 0u && la_net_policy_state(&policy) == LA_NET_POLICY_BACKOFF,
          "duplicate WIFI_LOST while waiting is ignored");
    now = 500u + 400u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 1u, "third retry fires after 400 ms");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 950u, &result) == 0, "fourth failure");
    now = 950u + 600u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 1u, "retry after cap wait (600 ms)");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 1600u, &result) == 0, "fifth failure");
    now = 1600u + 600u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 1u, "cap holds at 600 ms");

    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 2300u, &result) == 0, "eventual success");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_CONNECTED, "connected after backoff");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 2500u, &result) == 0, "failure after success");
    now = 2500u + 100u;
    CHECK(step(&policy, LA_NET_POLICY_EVT_BACKOFF_TICK, now, &result) == 0 &&
              result.count == 1u,
          "consecutive-failure count reset after success (100 ms wait)");

    la_net_policy_snapshot(&policy, &stats);
    CHECK(stats.wifi_attempts == 7u && stats.wifi_successes == 1u,
          "backoff attempt counters");
    return 0;
}

static int test_wifi_loss_with_client(void)
{
    la_net_policy_t policy;
    step_result_t result;

    CHECK(la_net_policy_init(&policy, NULL) == 0, "init");
    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 0u, &result) == 0, "start");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 100u, &result) == 0, "wifi up");
    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 200u, &result) == 0, "client in");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 400u, &result) == 0, "wifi lost mid-session");
    CHECK(result.count == 3u && contains(&result, LA_NET_POLICY_ACT_LINK_ABORT) &&
              contains(&result, LA_NET_POLICY_ACT_LINK_LIVE_STOP) &&
              contains(&result, LA_NET_POLICY_ACT_LISTEN_STOP),
          "wifi loss with a client aborts the Zynq session and stops the listen");
    CHECK(la_net_policy_state(&policy) == LA_NET_POLICY_BACKOFF, "backoff after wifi loss");
    return 0;
}

static int test_single_client_rejection(void)
{
    la_net_policy_t policy;
    step_result_t result;
    la_net_policy_stats_t stats;

    CHECK(la_net_policy_init(&policy, NULL) == 0, "init");
    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 0u, &result) == 0, "start");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 100u, &result) == 0, "wifi up");
    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 200u, &result) == 0, "first client");
    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 210u, &result) == 0,
          "second client while active");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_CLIENT_REJECT),
          "second client rejected");
    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 220u, &result) == 0 &&
              contains(&result, LA_NET_POLICY_ACT_CLIENT_REJECT),
          "client while not listening rejected");
    la_net_policy_snapshot(&policy, &stats);
    CHECK(stats.clients_accepted == 1u && stats.clients_rejected == 2u,
          "single-client counters");
    return 0;
}

static int test_stop_from_every_state(void)
{
    la_net_policy_t policy;
    step_result_t result;

    CHECK(la_net_policy_init(&policy, NULL) == 0, "init");
    CHECK(step(&policy, LA_NET_POLICY_EVT_STOP, 0u, &result) == 0 &&
              result.count == 0u, "stop while IDLE is a no-op");

    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 0u, &result) == 0, "start");
    CHECK(step(&policy, LA_NET_POLICY_EVT_STOP, 5u, &result) == 0, "stop while CONNECTING");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_WIFI_STOP) &&
              la_net_policy_state(&policy) == LA_NET_POLICY_IDLE,
          "connecting stop emits wifi stop");

    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 10u, &result) == 0, "start again");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_LOST, 20u, &result) == 0, "failure");
    CHECK(step(&policy, LA_NET_POLICY_EVT_STOP, 30u, &result) == 0, "stop while BACKOFF");
    CHECK(result.count == 1u && contains(&result, LA_NET_POLICY_ACT_WIFI_STOP) &&
              la_net_policy_state(&policy) == LA_NET_POLICY_IDLE,
          "backoff stop emits wifi stop");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 40u, &result) == 0 &&
              result.count == 0u, "late wifi-up after stop is ignored");

    CHECK(step(&policy, LA_NET_POLICY_EVT_START, 50u, &result) == 0, "start again");
    CHECK(step(&policy, LA_NET_POLICY_EVT_WIFI_UP, 60u, &result) == 0, "wifi up");
    CHECK(step(&policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED, 70u, &result) == 0, "client in");
    CHECK(step(&policy, LA_NET_POLICY_EVT_STOP, 80u, &result) == 0, "stop while active");
    CHECK(result.count == 2u && contains(&result, LA_NET_POLICY_ACT_LISTEN_STOP) &&
              contains(&result, LA_NET_POLICY_ACT_WIFI_STOP) &&
              la_net_policy_state(&policy) == LA_NET_POLICY_IDLE,
          "active stop emits listen stop and wifi stop");
    return 0;
}

static int test_argument_guards(void)
{
    la_net_policy_t policy;
    step_result_t result;

    CHECK(la_net_policy_init(NULL, NULL) == -1, "NULL policy rejected");
    CHECK(la_net_policy_init(&policy, NULL) == 0, "init");
    CHECK(la_net_policy_state(NULL) == LA_NET_POLICY_IDLE, "state on NULL is IDLE");
    CHECK(la_net_policy_step(NULL, LA_NET_POLICY_EVT_START, 0u, result.actions, ACTION_CAP,
                             &result.count) == -1,
          "NULL policy step rejected");
    CHECK(la_net_policy_step(&policy, LA_NET_POLICY_EVT_START, 0u, NULL, ACTION_CAP,
                             &result.count) == -1,
          "NULL actions rejected");
    CHECK(la_net_policy_step(&policy, LA_NET_POLICY_EVT_START, 0u, result.actions, ACTION_CAP,
                             NULL) == -1,
          "NULL count rejected");
    CHECK(la_net_policy_step(&policy, LA_NET_POLICY_EVT_NONE, 0u, result.actions, ACTION_CAP,
                             &result.count) == -1,
          "NONE event rejected");
    la_net_policy_snapshot(NULL, NULL);
    return 0;
}

int main(void)
{
    if (test_init_defaults() != 0 || test_init_clamps() != 0 || test_happy_session() != 0 ||
        test_backoff_sequence() != 0 || test_wifi_loss_with_client() != 0 ||
        test_single_client_rejection() != 0 || test_stop_from_every_state() != 0 ||
        test_argument_guards() != 0) {
        return 1;
    }
    puts("PASS: la_esp32_mocks_net");
    return 0;
}
