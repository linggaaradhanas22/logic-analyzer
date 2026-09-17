#include "la_net.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "la_board.h"
#include "la_link.h"
#include "la_proto.h"
#include "la_usb.h"
#include "lwip/api.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/tcpip.h"
#include "net_mdns.h"
#include "net_wifi.h"

static const char *TAG = "la_net";

/* The relay drives its non-blocking sockets directly and sleeps for this
 * period between passes. The old 5 ms select() timeout both delayed idle
 * host-bound frames and, after a heavy relay flood, stopped waking the task
 * for the listening socket at all (P9.4 diagnosis). */
#define LA_NET_POLL_INTERVAL_MS 2
#define LA_NET_WIFI_QUEUE_LENGTH 8
#define LA_NET_HOST_FRAME_TIMEOUT_MS 2000

typedef struct la_net_server {
    la_net_policy_t policy;
    QueueHandle_t wifi_events;
    int listen_fd;
    int client_fd;
    int accept_idle; /* last accept() returned EAGAIN/EWOULDBLOCK */
    la_proto_parser_t host_parser;
    uint8_t staging[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    size_t staging_length;
    size_t staging_offset;
    la_net_counters_t counters;
} la_net_server_t;

static la_net_server_t s_server;
static StaticQueue_t s_wifi_queue_struct;
static uint8_t s_wifi_queue_storage[LA_NET_WIFI_QUEUE_LENGTH * sizeof(la_net_policy_event_t)];
static StackType_t s_net_stack[LA_BOARD_NET_TASK_STACK];
static StaticTask_t s_net_tcb;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static void close_client_socket(void)
{
    if (s_server.client_fd >= 0) {
        int fd = s_server.client_fd;
        int64_t started = esp_timer_get_time();
        int result;
        uint32_t elapsed_ms;

        s_server.client_fd = -1;
        result = close(fd);
        if (result != 0) {
            s_server.counters.close_failures += 1u;
            s_server.counters.close_errno = errno;
            ESP_LOGW(TAG, "close(%d) failed errno %d", fd, errno);
        } else if (s_server.counters.sockets_open > 0u) {
            s_server.counters.sockets_open -= 1u;
        }
        elapsed_ms = (uint32_t)((esp_timer_get_time() - started) / 1000);
        if (elapsed_ms > s_server.counters.close_max_ms) {
            s_server.counters.close_max_ms = elapsed_ms;
        }
        if (elapsed_ms >= 100u) {
            ESP_LOGW(TAG, "close(%d) took %u ms", fd, (unsigned)elapsed_ms);
        }
    }
    s_server.staging_length = 0u;
    s_server.staging_offset = 0u;
    /* A lost client may leave a partial frame; never stitch it to a new one. */
    la_proto_parser_init(&s_server.host_parser);
}

static void start_listen(void)
{
    struct sockaddr_in address;
    int fd;
    int enable = 1;

    if (s_server.listen_fd >= 0) {
        return;
    }
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket failed errno %d", errno);
        return;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(LA_BOARD_TCP_PORT);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(fd, 1) != 0) {
        ESP_LOGE(TAG, "listen on :%d failed errno %d", LA_BOARD_TCP_PORT, errno);
        close(fd);
        return;
    }
    set_nonblocking(fd);
    s_server.listen_fd = fd;
    ESP_LOGI(TAG, "listening on :%d", LA_BOARD_TCP_PORT);
}

static void stop_listen(void)
{
    if (s_server.listen_fd >= 0) {
        close(s_server.listen_fd);
        s_server.listen_fd = -1;
    }
    close_client_socket();
}

static void run_policy(la_net_policy_event_t event);

static void execute_action(la_net_policy_action_t action)
{
    switch (action) {
    case LA_NET_POLICY_ACT_WIFI_CONNECT:
        (void)la_net_wifi_connect();
        break;
    case LA_NET_POLICY_ACT_WIFI_STOP:
        (void)la_net_wifi_stop();
        break;
    case LA_NET_POLICY_ACT_LISTEN_START:
        start_listen();
        break;
    case LA_NET_POLICY_ACT_LISTEN_STOP:
        stop_listen();
        break;
    case LA_NET_POLICY_ACT_CLIENT_REJECT:
        /* Executed at accept time where the pending socket is known. */
        break;
    case LA_NET_POLICY_ACT_LINK_ABORT:
        if (la_link_send_control(LA_PROTO_ABORT) != ESP_OK) {
            s_server.counters.host_frame_failures += 1u;
        }
        break;
    case LA_NET_POLICY_ACT_LINK_LIVE_STOP:
        if (la_link_send_control(LA_PROTO_LIVE_STOP) != ESP_OK) {
            s_server.counters.host_frame_failures += 1u;
        }
        break;
    default:
        break;
    }
}

static void run_policy(la_net_policy_event_t event)
{
    la_net_policy_action_t actions[LA_NET_POLICY_MAX_ACTIONS];
    size_t count = 0u;
    size_t index;

    if (la_net_policy_step(&s_server.policy, event, now_ms(), actions,
                           LA_NET_POLICY_MAX_ACTIONS, &count) != 0) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        execute_action(actions[index]);
    }
}

/* True when the connected client has already gone away (FIN or fatal error).
 * A non-blocking MSG_PEEK leaves any pending data in place. */
static int client_is_dead(void)
{
    uint8_t byte;
    int received;

    if (s_server.client_fd < 0) {
        return 0;
    }
    received = recv(s_server.client_fd, &byte, 1, MSG_PEEK);
    if (received == 0) {
        return 1;
    }
    if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return 1;
    }
    return 0;
}

static int policy_accept_connection(void)
{
    la_net_policy_action_t actions[LA_NET_POLICY_MAX_ACTIONS];
    size_t count = 0u;
    size_t index;

    if (la_net_policy_step(&s_server.policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED,
                           now_ms(), actions, LA_NET_POLICY_MAX_ACTIONS,
                           &count) != 0) {
        return 0;
    }
    for (index = 0u; index < count; ++index) {
        if (actions[index] == LA_NET_POLICY_ACT_CLIENT_REJECT) {
            return 0;
        }
        execute_action(actions[index]);
    }
    return 1;
}

static void adopt_client(int fd)
{
    s_server.client_fd = fd;
    s_server.counters.sockets_open += 1u;
    s_server.staging_length = 0u;
    s_server.staging_offset = 0u;
    la_proto_parser_init(&s_server.host_parser);
    ESP_LOGI(TAG, "client connected");
}

/* Wait briefly for a departing client's FIN to arrive before judging the
 * single-client rule, so a fast reconnect is not rejected while the old FIN
 * is still in flight. Returns 1 when the departed client was cleared. */
static int clear_departed_client(void)
{
    unsigned tries;

    for (tries = 0u; tries < 125u; ++tries) {
        if (client_is_dead()) {
            ESP_LOGI(TAG, "departed client cleared for a new connection");
            close_client_socket();
            run_policy(LA_NET_POLICY_EVT_CLIENT_DISCONNECTED);
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(LA_NET_POLL_INTERVAL_MS));
        esp_task_wdt_reset();
    }
    return 0;
}

static void handle_new_client(void)
{
    unsigned handled = 0u;

    for (;;) {
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);
        int fd;
        int nodelay = 1;

        fd = accept(s_server.listen_fd, (struct sockaddr *)&peer, &peer_length);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                s_server.accept_idle = 1;
            } else {
                s_server.accept_idle = 0;
                s_server.counters.accept_failures += 1u;
                s_server.counters.accept_errno = errno;
                ESP_LOGW(TAG, "accept failed errno %d (open=%u)", errno,
                         (unsigned)s_server.counters.sockets_open);
            }
            return;
        }
        s_server.accept_idle = 0;
        handled += 1u;
        set_nonblocking(fd);
        (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        /* P9.1: a USB CDC host is the analyzer endpoint while its port is
         * open. Close the TCP connection immediately so the host sees a clean
         * EOF instead of a session that never answers. */
        if (la_usb_host_active()) {
            s_server.counters.usb_rejections += 1u;
            ESP_LOGW(TAG, "USB host owns the analyzer stream: TCP client rejected");
            close(fd);
            continue;
        }
        /* A new connection may arrive before the previous client's FIN has
         * been read. Clear a departed client first so the fast reconnect is
         * not rejected by the single-client rule. */
        if (la_net_policy_state(&s_server.policy) == LA_NET_POLICY_CLIENT_ACTIVE) {
            (void)clear_departed_client();
        }
        if (!policy_accept_connection()) {
            ESP_LOGW(TAG, "single-client rule: second client rejected");
            close(fd);
            if (handled > 1u) {
                s_server.counters.accept_drains += 1u;
            }
            continue;
        }
        adopt_client(fd);
        if (handled > 1u) {
            s_server.counters.accept_drains += 1u;
        }
        return;
    }
}

static unsigned count_port_5555_pcbs(void)
{
    const struct tcp_pcb *pcb;
    unsigned count = 0u;

    for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
        if (pcb->local_port == LA_BOARD_TCP_PORT) {
            count += 1u;
        }
    }
    return count;
}

/* P9.4: after a relay flood the app observed accept() returning EAGAIN while
 * the TCP stack kept completing handshakes for :5555 (data buffered, no
 * response). If established connections for the listen port exist while the
 * accept path has been idle, the listening socket is rebuilt so a fresh
 * listen PCB (added at the head of the lwIP list) serves connections again. */
static void check_stale_listener(void)
{
    static unsigned strikes;
    unsigned count;

    if (s_server.listen_fd < 0 || s_server.client_fd >= 0 ||
        !s_server.accept_idle) {
        strikes = 0u;
        return;
    }
    LOCK_TCPIP_CORE();
    count = count_port_5555_pcbs();
    UNLOCK_TCPIP_CORE();
    if (count == 0u) {
        strikes = 0u;
        return;
    }
    strikes += 1u;
    if (strikes < 3u) {
        return;
    }
    strikes = 0u;
    ESP_LOGW(TAG, "stale listener: %u connection(s) on :%d while accept() is idle; refreshing",
             count, LA_BOARD_TCP_PORT);
    close(s_server.listen_fd);
    s_server.listen_fd = -1;
    s_server.counters.listener_refreshes += 1u;
    start_listen();
}

void la_net_diag_dump(void)
{
    struct tcp_pcb_listen *lpcb;
    const struct tcp_pcb *pcb;
    struct lwip_sock *lsock = NULL;
    unsigned listen_count = 0u;
    unsigned active_count = 0u;
    unsigned tw_count = 0u;

    LOCK_TCPIP_CORE();
    if (s_server.listen_fd >= 0) {
        lsock = lwip_socket_dbg_get_socket(s_server.listen_fd);
    }
    ESP_LOGI(TAG, "diag listen_fd=%d client_fd=%d listen_conn=%p",
             s_server.listen_fd, s_server.client_fd,
             lsock != NULL ? (void *)lsock->conn : NULL);
    if (lsock != NULL) {
        ESP_LOGI(TAG, "diag listen sock rcvevent=%d sendevent=%d errevent=%d waiting=%d err=%d",
                 (int)lsock->rcvevent, (int)lsock->sendevent,
                 (int)lsock->errevent, (int)lsock->select_waiting,
                 lsock->conn != NULL ? (int)netconn_err(lsock->conn) : -99);
    }
    for (lpcb = tcp_listen_pcbs.listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
        ESP_LOGI(TAG, "diag listen pcb port=%u accept=%p arg=%p",
                 (unsigned)lpcb->local_port, (void *)lpcb->accept,
                 lpcb->callback_arg);
        listen_count += 1u;
    }
    for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
        if (pcb->local_port == LA_BOARD_TCP_PORT) {
            ESP_LOGI(TAG, "diag active pcb remote=%u state=%d listener=%p",
                     (unsigned)pcb->remote_port, (int)pcb->state,
                     (void *)pcb->listener);
        }
        active_count += 1u;
    }
    for (pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
        tw_count += 1u;
    }
    UNLOCK_TCPIP_CORE();
    ESP_LOGI(TAG, "diag pcbs listen=%u active=%u tw=%u", listen_count,
             active_count, tw_count);
#if LWIP_STATS
    ESP_LOGI(TAG, "diag tcp xmit=%u recv=%u drop=%u chkerr=%u memerr=%u err=%u",
             (unsigned)lwip_stats.tcp.xmit, (unsigned)lwip_stats.tcp.recv,
             (unsigned)lwip_stats.tcp.drop, (unsigned)lwip_stats.tcp.chkerr,
             (unsigned)lwip_stats.tcp.memerr, (unsigned)lwip_stats.tcp.err);
    ESP_LOGI(TAG, "diag ip recv=%u drop=%u chkerr=%u memerr=%u",
             (unsigned)lwip_stats.ip.recv, (unsigned)lwip_stats.ip.drop,
             (unsigned)lwip_stats.ip.chkerr, (unsigned)lwip_stats.ip.memerr);
#endif
}

static void on_host_frame(void *context,
                          uint8_t type,
                          uint8_t sequence,
                          const uint8_t *payload,
                          uint16_t length)
{
    (void)context;
    if (la_link_forward_host_frame(type, sequence, payload, length,
                                   pdMS_TO_TICKS(LA_NET_HOST_FRAME_TIMEOUT_MS)) != ESP_OK) {
        s_server.counters.host_frame_failures += 1u;
    } else {
        s_server.counters.client_rx_frames += 1u;
    }
}

static void read_client(void)
{
    uint8_t buffer[1024];
    int received = recv(s_server.client_fd, buffer, sizeof(buffer), 0);

    if (received > 0) {
        s_server.counters.client_rx_bytes += (uint32_t)received;
        (void)la_proto_parser_feed(&s_server.host_parser, buffer,
                                   (size_t)received, on_host_frame, NULL);
        return;
    }
    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }
    if (received == 0) {
        ESP_LOGI(TAG, "client closed the connection");
    } else {
        ESP_LOGW(TAG, "recv failed errno %d", errno);
    }
    close_client_socket();
    run_policy(LA_NET_POLICY_EVT_CLIENT_DISCONNECTED);
}

static void flush_client(void)
{
    while (s_server.staging_offset < s_server.staging_length) {
        int sent = send(s_server.client_fd,
                        s_server.staging + s_server.staging_offset,
                        s_server.staging_length - s_server.staging_offset, 0);
        if (sent > 0) {
            s_server.staging_offset += (size_t)sent;
            s_server.counters.client_tx_bytes += (uint32_t)sent;
            continue;
        }
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == ENOBUFS || errno == ENOMEM || errno == ENOSPC) {
                /* Transient lwIP pool shortage under relay pressure: keep the
                 * session and retry after the next select wake-up. Closing here
                 * used to strand a half-full send buffer in lwIP. */
                s_server.counters.send_retries += 1u;
                return;
            }
        }
        ESP_LOGW(TAG, "send failed errno %d", errno);
        close_client_socket();
        run_policy(LA_NET_POLICY_EVT_CLIENT_DISCONNECTED);
        return;
    }
    if (s_server.staging_offset != 0u) {
        s_server.counters.client_tx_frames += 1u;
    }
    s_server.staging_length = 0u;
    s_server.staging_offset = 0u;
}

static void pump_link_to_client(void)
{
    /* Bounded per pass: a saturated Zynq link must never starve the accept
     * and host-read paths above (the select loop runs every 5 ms). */
    unsigned budget = 64u;

    /* P9.1: while the USB CDC host owns the analyzer stream the ring must not
     * be drained (or discarded) here, or the two legs would split frames. */
    if (la_usb_host_active()) {
        return;
    }
    while (budget-- > 0u) {
        uint8_t *frame = NULL;
        size_t length;

        if (s_server.client_fd < 0) {
            /* No client: keep the Zynq->host ring drained so a later session
             * never inherits stale frames, and count the discarded traffic. */
            length = la_link_receive_to_host(&frame, 0);
            if (length == 0u) {
                return;
            }
            la_link_release_to_host(frame);
            s_server.counters.to_host_discarded += 1u;
            continue;
        }
        if (s_server.staging_offset >= s_server.staging_length) {
            length = la_link_receive_to_host(&frame, 0);
            if (length == 0u) {
                return;
            }
            if (length > sizeof(s_server.staging)) {
                la_link_release_to_host(frame);
                s_server.counters.to_host_discarded += 1u;
                continue;
            }
            memcpy(s_server.staging, frame, length);
            s_server.staging_length = length;
            s_server.staging_offset = 0u;
            la_link_release_to_host(frame);
        }
        flush_client();
        if (s_server.client_fd < 0 ||
            s_server.staging_offset < s_server.staging_length) {
            return;
        }
    }
}

static void net_server_task(void *argument)
{
    (void)argument;
    esp_task_wdt_add(NULL);
    run_policy(LA_NET_POLICY_EVT_START);

    for (;;) {
        la_net_policy_event_t event;
        fd_set read_fds;
        struct timeval timeout;
        int max_fd = -1;
        int ready;

        while (xQueueReceive(s_server.wifi_events, &event, 0) == pdTRUE) {
            run_policy(event);
        }
        run_policy(LA_NET_POLICY_EVT_BACKOFF_TICK);

        /* Non-blocking readiness probe only: after a heavy relay flood the
         * lwIP select() wake-up for the listening socket was observed to stop
         * firing while the task stayed alive (accept() was never reached and
         * no error was reported). The relay therefore drives the non-blocking
         * accept/recv/send paths directly and uses select() just to record
         * readiness for the P9.4 diagnosis. */
        FD_ZERO(&read_fds);
        if (s_server.listen_fd >= 0) {
            FD_SET(s_server.listen_fd, &read_fds);
            max_fd = s_server.listen_fd;
        }
        if (s_server.client_fd >= 0) {
            FD_SET(s_server.client_fd, &read_fds);
            if (s_server.client_fd > max_fd) {
                max_fd = s_server.client_fd;
            }
        }
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        s_server.counters.loops += 1u;
        if (ready > 0) {
            s_server.counters.select_ready += 1u;
        } else if (ready < 0) {
            s_server.counters.select_errors += 1u;
        }

        /* P9.1: if the USB host opened its port while a TCP analyzer session
         * was active, hand the stream over. The USB attach flushed the relay
         * ring; close the TCP client and let the policy stop the Zynq. */
        if (s_server.client_fd >= 0 && la_usb_host_active()) {
            ESP_LOGW(TAG, "USB host attached: closing the TCP analyzer client");
            close_client_socket();
            run_policy(LA_NET_POLICY_EVT_CLIENT_DISCONNECTED);
        }
        /* Read the current client first: a disconnect observed in this cycle
         * must clear the single-client state before a connection that arrived
         * in the same cycle is accepted or rejected. */
        if (s_server.client_fd >= 0) {
            read_client();
        }
        if (s_server.listen_fd >= 0) {
            handle_new_client();
        }
        flush_client();
        pump_link_to_client();
        if ((s_server.counters.loops % 500u) == 0u) {
            check_stale_listener();
        }
        s_server.counters.net_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(LA_NET_POLL_INTERVAL_MS));
    }
}

esp_err_t la_net_start(void)
{
    esp_err_t result;

    memset(&s_server, 0, sizeof(s_server));
    s_server.listen_fd = -1;
    s_server.client_fd = -1;
    if (la_net_policy_init(&s_server.policy, NULL) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    la_proto_parser_init(&s_server.host_parser);
    s_server.wifi_events = xQueueCreateStatic(LA_NET_WIFI_QUEUE_LENGTH,
                                              sizeof(la_net_policy_event_t),
                                              s_wifi_queue_storage,
                                              &s_wifi_queue_struct);
    if (s_server.wifi_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    result = la_net_wifi_init(s_server.wifi_events);
    if (result != ESP_OK) {
        return result;
    }
    result = la_net_mdns_start();
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "mDNS start failed: %s", esp_err_to_name(result));
    }
    if (xTaskCreateStaticPinnedToCore(net_server_task, "NetServer",
                                      LA_BOARD_NET_TASK_STACK, NULL,
                                      LA_BOARD_NET_TASK_PRIO,
                                      s_net_stack, &s_net_tcb,
                                      LA_BOARD_NET_TASK_CORE) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void la_net_stats(la_net_policy_stats_t *policy_stats,
                  la_net_policy_state_t *state,
                  la_net_counters_t *counters)
{
    if (policy_stats != NULL) {
        la_net_policy_snapshot(&s_server.policy, policy_stats);
    }
    if (state != NULL) {
        *state = la_net_policy_state(&s_server.policy);
    }
    if (counters != NULL) {
        memcpy(counters, &s_server.counters, sizeof(*counters));
        counters->wifi_event_drops = la_net_wifi_event_drops();
        counters->rssi = la_net_wifi_rssi();
    }
}
