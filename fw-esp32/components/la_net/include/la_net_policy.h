#ifndef LA_NET_POLICY_H
#define LA_NET_POLICY_H

/*
 * P5.3 ESP32 network session policy core (pure logic, portable).
 *
 * Owns the gateway session lifecycle that la_net tasks implement: Wi-Fi
 * station association with exponential backoff, single-client TCP listen on
 * :5555, and the protocol rule that a lost client is answered by ABORT and
 * LIVE_STOP towards the Zynq link (docs/plan.md P5.3, docs/protocol.md).
 *
 * The module performs no ESP-IDF, socket, timer or FreeRTOS calls: la_net
 * adapters feed events and a monotonic millisecond clock through
 * la_net_policy_step() and execute the returned actions in order. One
 * policy instance is owned by the session task (NetServerTask); WifiMgr
 * forwards Wi-Fi events into it. Policy semantics and the single-client
 * rule are documented in docs/rtos-design.md "ESP32 network session policy
 * core (P5.3)".
 *
 * Backoff: the first attempt starts immediately after START; each failure
 * (WIFI_LOST) raises the wait by the multiplier up to the cap; a successful
 * association resets the attempt counter.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_NETPOL_API __declspec(dllexport)
#else
#define LA_NETPOL_API
#endif

#define LA_NET_POLICY_MAX_ACTIONS 3u

typedef enum la_net_policy_state {
    LA_NET_POLICY_IDLE = 0,        /* before START / after STOP */
    LA_NET_POLICY_CONNECTING,      /* Wi-Fi association in progress */
    LA_NET_POLICY_BACKOFF,         /* waiting until the next attempt */
    LA_NET_POLICY_CONNECTED,       /* associated, listening, no client */
    LA_NET_POLICY_CLIENT_ACTIVE    /* one TCP client relayed */
} la_net_policy_state_t;

typedef enum la_net_policy_event {
    LA_NET_POLICY_EVT_NONE = 0,
    LA_NET_POLICY_EVT_START,        /* begin the gateway session */
    LA_NET_POLICY_EVT_STOP,         /* end the session */
    LA_NET_POLICY_EVT_WIFI_UP,      /* association succeeded */
    LA_NET_POLICY_EVT_WIFI_LOST,    /* association failed or link lost */
    LA_NET_POLICY_EVT_CLIENT_CONNECTED,   /* a TCP client connected */
    LA_NET_POLICY_EVT_CLIENT_DISCONNECTED, /* the client went away */
    LA_NET_POLICY_EVT_BACKOFF_TICK  /* adapter timer expired; retry if due */
} la_net_policy_event_t;

typedef enum la_net_policy_action {
    LA_NET_POLICY_ACT_NONE = 0,
    LA_NET_POLICY_ACT_WIFI_CONNECT, /* start an association attempt */
    LA_NET_POLICY_ACT_WIFI_STOP,    /* stop Wi-Fi station operation */
    LA_NET_POLICY_ACT_LISTEN_START, /* start TCP listen on :5555 */
    LA_NET_POLICY_ACT_LISTEN_STOP,  /* stop accepting clients */
    LA_NET_POLICY_ACT_CLIENT_REJECT, /* refuse a client (single-client rule) */
    LA_NET_POLICY_ACT_LINK_ABORT,   /* send ABORT to the Zynq link */
    LA_NET_POLICY_ACT_LINK_LIVE_STOP /* send LIVE_STOP to the Zynq link */
} la_net_policy_action_t;

typedef struct la_net_policy_config {
    uint32_t backoff_start_ms;  /* first failure wait, default 1000 */
    uint32_t backoff_max_ms;    /* cap, default 60000 */
    uint8_t multiplier;         /* 2 for classic exponential doubling */
} la_net_policy_config_t;

typedef struct la_net_policy_stats {
    uint32_t wifi_attempts;
    uint32_t wifi_successes;
    uint32_t clients_accepted;
    uint32_t clients_rejected;
    uint32_t client_disconnects;
} la_net_policy_stats_t;

typedef struct la_net_policy {
    la_net_policy_state_t state;
    la_net_policy_config_t config;
    la_net_policy_stats_t stats;
    uint32_t consecutive_failures;
    uint32_t next_attempt_ms; /* valid while BACKOFF */
} la_net_policy_t;

/* Zero configuration is replaced by the documented defaults; the multiplier
 * is clamped to >= 1 and backoff_max_ms is raised to backoff_start_ms when
 * smaller. Returns 0, or -1 on a NULL policy. */
LA_NETPOL_API int la_net_policy_init(la_net_policy_t *policy,
                                     const la_net_policy_config_t *config);

/* Processes one event at clock time now_ms. Returns 0 and writes the
 * ordered actions into actions (count in *action_count), or -1 on invalid
 * arguments. Events that do not apply to the current state are ignored. */
LA_NETPOL_API int la_net_policy_step(la_net_policy_t *policy,
                                     la_net_policy_event_t event,
                                     uint32_t now_ms,
                                     la_net_policy_action_t *actions,
                                     size_t capacity,
                                     size_t *action_count);

LA_NETPOL_API la_net_policy_state_t la_net_policy_state(const la_net_policy_t *policy);

LA_NETPOL_API void la_net_policy_snapshot(const la_net_policy_t *policy,
                                          la_net_policy_stats_t *stats);

#endif /* LA_NET_POLICY_H */
