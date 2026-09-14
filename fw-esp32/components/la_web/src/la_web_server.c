/*
 * P10.1 live web view server.
 *
 * esp_http_server serves one embedded HTML page and a WebSocket at /ws.
 * LinkRxTask mirrors every EVT_DECODED frame into the 8 KiB live ring; the
 * LiveViewTask formats those frames into compact JSON (la_web_core) and
 * broadcasts them to the connected browsers. Sends are direct calls to
 * httpd_ws_send_frame_async (which transmits synchronously in the calling
 * task), so a dead client is pruned on the first failed send.
 */

#include "la_web.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "la_board.h"
#include "la_link.h"
#include "la_proto.h"
#include "la_web_core.h"

static const char *TAG = "la_web";

#define LA_WEB_JSON_BYTES 2560u
#define LA_WEB_STATS_PERIOD_MS 1000u

static httpd_handle_t s_httpd;
static int s_clients[LA_BOARD_WEB_MAX_CLIENTS];
static portMUX_TYPE s_client_lock = portMUX_INITIALIZER_UNLOCKED;
static la_web_counters_t s_counters;
static char s_json[LA_WEB_JSON_BYTES];
static StackType_t s_view_stack[LA_BOARD_WEB_TASK_STACK];
static StaticTask_t s_view_tcb;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static int client_add(int fd)
{
    int index;

    portENTER_CRITICAL(&s_client_lock);
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        if (s_clients[index] == fd) {
            portEXIT_CRITICAL(&s_client_lock);
            return 0;
        }
    }
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        if (s_clients[index] < 0) {
            s_clients[index] = fd;
            portEXIT_CRITICAL(&s_client_lock);
            return 0;
        }
    }
    portEXIT_CRITICAL(&s_client_lock);
    return -1;
}

static void client_remove(int fd)
{
    int index;

    portENTER_CRITICAL(&s_client_lock);
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        if (s_clients[index] == fd) {
            s_clients[index] = -1;
        }
    }
    portEXIT_CRITICAL(&s_client_lock);
}

static int client_count(void)
{
    int index;
    int count = 0;

    portENTER_CRITICAL(&s_client_lock);
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        if (s_clients[index] >= 0) {
            count += 1;
        }
    }
    portEXIT_CRITICAL(&s_client_lock);
    return count;
}

static int client_snapshot(int *out)
{
    int index;
    int count = 0;

    portENTER_CRITICAL(&s_client_lock);
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        if (s_clients[index] >= 0) {
            out[count] = s_clients[index];
            count += 1;
        }
    }
    portEXIT_CRITICAL(&s_client_lock);
    return count;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)index_html_start,
                           index_html_end - index_html_start);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        if (client_add(fd) != 0) {
            s_counters.ws_rejected += 1u;
            ESP_LOGW(TAG, "client table full, rejecting fd %d", fd);
            return ESP_FAIL;
        }
        s_counters.ws_connections += 1u;
        ESP_LOGI(TAG, "ws client %d connected", fd);
        return ESP_OK;
    }

    {
        uint8_t scratch[128];
        httpd_ws_frame_t frame = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = scratch,
            .len = sizeof(scratch),
        };
        esp_err_t result = httpd_ws_recv_frame(req, &frame, sizeof(scratch));

        if (result != ESP_OK) {
            client_remove(fd);
            return result;
        }
        if (frame.type == HTTPD_WS_TYPE_CLOSE) {
            ESP_LOGI(TAG, "ws client %d closed", fd);
            client_remove(fd);
        }
        /* Any other client payload is ignored: the page is read-only. */
    }
    return ESP_OK;
}

static uint64_t read_u64_le(const uint8_t *input)
{
    return (uint64_t)input[0] | ((uint64_t)input[1] << 8u) |
           ((uint64_t)input[2] << 16u) | ((uint64_t)input[3] << 24u) |
           ((uint64_t)input[4] << 32u) | ((uint64_t)input[5] << 40u) |
           ((uint64_t)input[6] << 48u) | ((uint64_t)input[7] << 56u);
}

typedef struct live_ctx {
    la_web_core_t *core;
    int ok;
} live_ctx_t;

static void on_live_frame(void *context, uint8_t type, uint8_t sequence,
                          const uint8_t *payload, uint16_t length)
{
    live_ctx_t *ctx = (live_ctx_t *)context;
    size_t offset = 0u;
    int begun = 0;

    (void)sequence;
    if (type != LA_PROTO_EVT_DECODED || payload == NULL || length == 0u) {
        return;
    }
    while (offset + 11u <= (size_t)length) {
        uint8_t slot = payload[offset];
        uint8_t kind = payload[offset + 1u];
        uint64_t t_sample = read_u64_le(&payload[offset + 2u]);
        uint8_t data_length = payload[offset + 10u];

        if (offset + 11u + (size_t)data_length > (size_t)length) {
            ctx->ok = 0;
            return;
        }
        if (begun == 0) {
            if (la_web_core_begin(ctx->core, slot) != 0) {
                ctx->ok = 0;
                return;
            }
            begun = 1;
        }
        if (la_web_core_event(ctx->core, kind, t_sample,
                              &payload[offset + 11u], data_length) != 0) {
            ctx->ok = 0;
            return;
        }
        offset += 11u + (size_t)data_length;
    }
    if (begun == 0 || offset != (size_t)length) {
        ctx->ok = 0;
    }
}

static void broadcast(const char *text, size_t length)
{
    int fds[LA_BOARD_WEB_MAX_CLIENTS];
    int count = client_snapshot(fds);
    int index;

    for (index = 0; index < count; ++index) {
        httpd_ws_frame_t frame = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)text,
            .len = length,
        };
        esp_err_t result = httpd_ws_send_frame_async(s_httpd, fds[index], &frame);

        if (result == ESP_OK) {
            s_counters.bytes_sent += (uint32_t)length;
        } else {
            s_counters.send_failures += 1u;
            client_remove(fds[index]);
        }
    }
}

static void send_stats(void)
{
    la_link_counters_t link;
    la_relay_stats_t up;
    la_relay_stats_t down;
    char text[256];
    int written;

    la_link_stats(&up, &down, &link);
    written = snprintf(text, sizeof(text),
                       "{\"stats\":{\"clients\":%u,\"events\":%u,\"sent\":%u,"
                       "\"dropped\":%u,\"fwd\":%u,\"ffail\":%u,\"drop\":%u,"
                       "\"mirror_drop\":%u,\"heap\":%u}}",
                       (unsigned)s_counters.ws_clients,
                       (unsigned)s_counters.frames_sent,
                       (unsigned)s_counters.bytes_sent,
                       (unsigned)s_counters.frames_dropped,
                       (unsigned)up.frames_forwarded,
                       (unsigned)up.frames_forward_failed,
                       (unsigned)up.best_effort_dropped,
                       (unsigned)up.mirror_dropped,
                       (unsigned)esp_get_free_heap_size());
    if (written > 0 && (size_t)written < sizeof(text)) {
        broadcast(text, (size_t)written);
    }
}

static void live_view_task(void *argument)
{
    TickType_t last_stats = xTaskGetTickCount();

    (void)argument;
    esp_task_wdt_add(NULL);
    for (;;) {
        uint8_t *frame = NULL;
        size_t length = la_link_receive_live(&frame, pdMS_TO_TICKS(100));

        if (length != 0u) {
            if (client_count() > 0) {
                la_web_core_t core;
                live_ctx_t ctx = { &core, 1 };
                la_proto_parser_t parser;

                la_web_core_init(&core, s_json, sizeof(s_json));
                la_proto_parser_init(&parser);
                (void)la_proto_parser_feed(&parser, frame, length,
                                           on_live_frame, &ctx);
                if (ctx.ok != 0 && core.length > 0u &&
                    la_web_core_end(&core) == 0) {
                    s_counters.frames_sent += 1u;
                    broadcast(s_json, core.length);
                } else {
                    s_counters.frames_dropped += 1u;
                }
            } else {
                s_counters.frames_dropped += 1u;
            }
            la_link_release_live(frame);
        }
        if ((xTaskGetTickCount() - last_stats) >
            pdMS_TO_TICKS(LA_WEB_STATS_PERIOD_MS)) {
            last_stats = xTaskGetTickCount();
            s_counters.ws_clients = (uint32_t)client_count();
            send_stats();
        }
        s_counters.web_stack_hwm = uxTaskGetStackHighWaterMark(NULL);
        esp_task_wdt_reset();
    }
}

esp_err_t la_web_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
    };
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    int index;

    memset(&s_counters, 0, sizeof(s_counters));
    for (index = 0; index < LA_BOARD_WEB_MAX_CLIENTS; ++index) {
        s_clients[index] = -1;
    }
    config.server_port = LA_BOARD_WEB_PORT;
    config.max_uri_handlers = 4;
    config.max_open_sockets = LA_BOARD_WEB_MAX_CLIENTS + 2;
    config.lru_purge_enable = true;
    config.stack_size = 6144;

    if (httpd_start(&s_httpd, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return ESP_FAIL;
    }
    if (httpd_register_uri_handler(s_httpd, &index_uri) != ESP_OK ||
        httpd_register_uri_handler(s_httpd, &ws_uri) != ESP_OK) {
        ESP_LOGE(TAG, "uri registration failed");
        httpd_stop(s_httpd);
        s_httpd = NULL;
        return ESP_FAIL;
    }
    if (xTaskCreateStaticPinnedToCore(live_view_task, "LiveView",
                                      LA_BOARD_WEB_TASK_STACK, NULL,
                                      LA_BOARD_WEB_TASK_PRIO, s_view_stack,
                                      &s_view_tcb,
                                      LA_BOARD_WEB_TASK_CORE) == NULL) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "live view on http://%s.local/ (port %d)",
             LA_BOARD_MDNS_HOSTNAME, LA_BOARD_WEB_PORT);
    return ESP_OK;
}

void la_web_stats(la_web_counters_t *out)
{
    if (out != NULL) {
        memcpy(out, &s_counters, sizeof(*out));
    }
}
