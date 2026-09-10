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
#include "lwip/sockets.h"
#include "net_mdns.h"
#include "net_wifi.h"

static const char *TAG = "la_net";

#define LA_NET_SELECT_TIMEOUT_US 100000
#define LA_NET_WIFI_QUEUE_LENGTH 8
#define LA_NET_HOST_FRAME_TIMEOUT_MS 2000

typedef struct la_net_server {
    la_net_policy_t policy;
    QueueHandle_t wifi_events;
    int listen_fd;
    int client_fd;
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
        close(s_server.client_fd);
        s_server.client_fd = -1;
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

static void handle_new_client(void)
{
    struct sockaddr_in peer;
    socklen_t peer_length = sizeof(peer);
    la_net_policy_action_t actions[LA_NET_POLICY_MAX_ACTIONS];
    size_t count = 0u;
    size_t index;
    int fd;

    fd = accept(s_server.listen_fd, (struct sockaddr *)&peer, &peer_length);
    if (fd < 0) {
        return;
    }
    set_nonblocking(fd);
    if (la_net_policy_step(&s_server.policy, LA_NET_POLICY_EVT_CLIENT_CONNECTED,
                           now_ms(), actions, LA_NET_POLICY_MAX_ACTIONS,
                           &count) != 0) {
        close(fd);
        return;
    }
    for (index = 0u; index < count; ++index) {
        if (actions[index] == LA_NET_POLICY_ACT_CLIENT_REJECT) {
            ESP_LOGW(TAG, "single-client rule: second client rejected");
            close(fd);
            return;
        }
        execute_action(actions[index]);
    }
    s_server.client_fd = fd;
    s_server.staging_length = 0u;
    s_server.staging_offset = 0u;
    la_proto_parser_init(&s_server.host_parser);
    ESP_LOGI(TAG, "client connected");
}

static void on_host_frame(void *context,
                          uint8_t type,
                          uint8_t sequence,
                          const uint8_t *payload,
                          uint16_t length)
{
    uint8_t frame[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    size_t frame_bytes;

    (void)context;
    frame_bytes = la_proto_encode(type, sequence, payload, length, frame,
                                  sizeof(frame));
    if (frame_bytes == 0u) {
        return;
    }
    if (la_link_send_to_zynq(frame, frame_bytes,
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
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
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
    for (;;) {
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
        fd_set write_fds;
        struct timeval timeout;
        int max_fd = -1;
        int want_write;
        int ready;

        while (xQueueReceive(s_server.wifi_events, &event, 0) == pdTRUE) {
            run_policy(event);
        }
        run_policy(LA_NET_POLICY_EVT_BACKOFF_TICK);

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
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
        want_write = s_server.client_fd >= 0 &&
                     s_server.staging_offset < s_server.staging_length;
        if (want_write) {
            FD_SET(s_server.client_fd, &write_fds);
        }
        timeout.tv_sec = 0;
        timeout.tv_usec = LA_NET_SELECT_TIMEOUT_US;
        ready = select(max_fd + 1, &read_fds, want_write ? &write_fds : NULL,
                       NULL, &timeout);
        if (ready > 0) {
            if (s_server.listen_fd >= 0 && FD_ISSET(s_server.listen_fd, &read_fds)) {
                handle_new_client();
            }
            if (s_server.client_fd >= 0 && FD_ISSET(s_server.client_fd, &read_fds)) {
                read_client();
            }
            if (want_write && s_server.client_fd >= 0 &&
                FD_ISSET(s_server.client_fd, &write_fds)) {
                flush_client();
            }
        }
        pump_link_to_client();
        s_server.counters.net_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
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
