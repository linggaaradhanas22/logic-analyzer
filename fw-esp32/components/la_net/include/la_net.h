#ifndef LA_NET_H
#define LA_NET_H

/*
 * P5.3/P5.4 ESP32 network adapter: Wi-Fi station with backoff, single-client
 * TCP relay on :5555 and mDNS. The pure session policy lives in
 * la_net_policy.{h,c}; this layer owns the ESP-IDF drivers and the
 * NetServerTask that executes the policy actions.
 */

#include <stdint.h>

#include "esp_err.h"
#include "la_net_policy.h"

typedef struct la_net_counters {
    uint32_t client_rx_frames;   /* host -> Zynq frames accepted */
    uint32_t client_rx_bytes;
    uint32_t client_tx_frames;   /* Zynq -> host frames written */
    uint32_t client_tx_bytes;
    uint32_t host_frame_failures; /* frame could not be queued to the link */
    uint32_t to_host_discarded;   /* drained with no client attached */
    uint32_t wifi_event_drops;    /* Wi-Fi event queue overflow */
    uint32_t net_stack_hwm;
    int32_t rssi;                 /* last station RSSI, -127 when unknown */
    uint32_t sockets_open;        /* accepted client sockets currently open */
    uint32_t accept_failures;     /* accept() returned -1 */
    int32_t accept_errno;         /* errno of the last accept() failure */
    uint32_t close_failures;      /* close() returned -1 */
    int32_t close_errno;          /* errno of the last close() failure */
    uint32_t close_max_ms;        /* longest observed close() duration */
    uint32_t send_retries;        /* transient send errors left for retry */
    uint32_t accept_drains;       /* connections handled per accept call > 1 */
    uint32_t loops;               /* NetServerTask main-loop iterations */
    uint32_t select_ready;        /* probe select() reported readiness */
    uint32_t select_errors;       /* probe select() returned -1 */
    uint32_t listener_refreshes;  /* stale listening sockets rebuilt */
} la_net_counters_t;

/* Initializes Wi-Fi, mDNS and starts NetServerTask (which drives the session
 * policy and the single-client TCP relay). Call once from app_main after
 * la_link_start(). */
esp_err_t la_net_start(void);

void la_net_stats(la_net_policy_stats_t *policy_stats,
                  la_net_policy_state_t *state,
                  la_net_counters_t *counters);

/* P9.4 diagnosis: logs the listening netconn and the lwIP listen/active PCB
 * lists so a stale listener (connections accepted by a PCB whose netconn is
 * no longer the one accept() reads) is visible on the console. */
void la_net_diag_dump(void);

#endif /* LA_NET_H */
