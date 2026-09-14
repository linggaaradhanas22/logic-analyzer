#ifndef LA_WEB_H
#define LA_WEB_H

/*
 * P10.1 live web view: HTTP server (port 80) plus a WebSocket at /ws streaming
 * the decoded events mirrored by LinkRxTask into the live ring. The page is a
 * single embedded HTML file; no external assets.
 */

#include <stdint.h>

#include "esp_err.h"

typedef struct la_web_counters {
    uint32_t ws_clients;      /* WebSocket clients connected right now */
    uint32_t ws_connections;  /* total accepted WebSocket connections */
    uint32_t ws_rejected;     /* connections rejected (client table full) */
    uint32_t frames_sent;     /* EVT_DECODED frames formatted and sent */
    uint32_t frames_dropped;  /* no client, empty ring or format overflow */
    uint32_t bytes_sent;      /* WebSocket payload bytes */
    uint32_t send_failures;   /* send errors (client pruned) */
    uint32_t web_stack_hwm;   /* LiveViewTask stack high-water mark */
} la_web_counters_t;

/* Starts the HTTP server and the LiveViewTask. Call once from app_main after
 * la_link_start() (and la_net_start() so the interface has an IP). */
esp_err_t la_web_start(void);

void la_web_stats(la_web_counters_t *out);

#endif /* LA_WEB_H */
