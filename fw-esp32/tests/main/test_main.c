/*
 * P5.2 target Unity tests ([HUMAN] `test-esp32-target`).
 *
 * Runs on the confirmed DOIT ESP32 DevKit V1 with the gateway components but
 * without Wi-Fi/TCP: the real FreeRTOS queues, task notifications, static
 * NOSPLIT ring buffers, the UART2 link driver (internal loopback, no jumpers
 * needed) and the task watchdog API are exercised on target. The relay core
 * is re-checked here as it runs in the gateway build.
 *
 * Build with `python tools/run.py test-esp32-target`; flash and capture with
 * `python tools/run.py test-esp32-target -- --port COM5`.
 */

#include <string.h>

#include "unity.h"
#include "unity_test_runner.h"

#include "driver/uart.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#include "la_board.h"
#include "la_link.h"
#include "la_link_uart.h"
#include "la_proto.h"
#include "la_relay.h"

static int relay_room(void *context, size_t frame_bytes)
{
    (void)context;
    (void)frame_bytes;
    return 1;
}

static int relay_full(void *context, size_t frame_bytes)
{
    (void)context;
    (void)frame_bytes;
    return 0;
}

TEST_CASE("la_relay forwards, drops best-effort and flags control pressure",
          "[la_relay]")
{
    static la_relay_t relay;
    static const uint8_t best_effort[] = {LA_PROTO_EVT_DECODED};
    static const uint8_t mirror[] = {LA_PROTO_EVT_DECODED};
    la_relay_decision_t decision = LA_RELAY_DROP_BEST_EFFORT;
    la_relay_stats_t stats;

    TEST_ASSERT_EQUAL_INT(0, la_relay_init(&relay, LA_RELAY_ROLE_UART_TO_NET,
                                           best_effort, 1, mirror, 1));
    TEST_ASSERT_EQUAL_INT(0, la_relay_accept(&relay, LA_PROTO_DATA_CHUNK, 64,
                                             relay_room, NULL, &decision));
    TEST_ASSERT_EQUAL_INT(LA_RELAY_FORWARD, decision);
    (void)la_relay_report(&relay, LA_PROTO_DATA_CHUNK, LA_RELAY_FWD_OK);
    TEST_ASSERT_EQUAL_INT(0, la_relay_accept(&relay, LA_PROTO_EVT_DECODED, 64,
                                             relay_full, NULL, &decision));
    TEST_ASSERT_EQUAL_INT(LA_RELAY_DROP_BEST_EFFORT, decision);
    TEST_ASSERT_EQUAL_INT(0, la_relay_accept(&relay, LA_PROTO_DATA_CHUNK, 64,
                                             relay_full, NULL, &decision));
    TEST_ASSERT_EQUAL_INT(LA_RELAY_PRESSURE, decision);
    la_relay_snapshot(&relay, &stats);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.frames_forwarded);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.best_effort_dropped);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.control_pressure_events);
}

static QueueHandle_t s_queue;
static StaticQueue_t s_queue_struct;
static uint32_t s_queue_storage[4];
static TaskHandle_t s_worker;
static StaticTask_t s_worker_tcb;
static StackType_t s_worker_stack[2048];
static volatile uint32_t s_worker_value;
static volatile uint32_t s_worker_notifications;

static void worker_task(void *argument)
{
    uint32_t value = 0u;

    (void)argument;
    if (xQueueReceive(s_queue, &value, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_worker_value = value;
    }
    s_worker_notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

TEST_CASE("static queue and task notification deliver", "[rtos]")
{
    uint32_t value = 0x12345678u;

    s_worker_value = 0u;
    s_worker_notifications = 0u;
    s_queue = xQueueCreateStatic(4, sizeof(uint32_t), (uint8_t *)s_queue_storage,
                                 &s_queue_struct);
    TEST_ASSERT_NOT_NULL(s_queue);
    s_worker = xTaskCreateStatic(worker_task, "la_test_worker",
                                 sizeof(s_worker_stack) / sizeof(s_worker_stack[0]),
                                 NULL, 5, s_worker_stack, &s_worker_tcb);
    TEST_ASSERT_NOT_NULL(s_worker);
    TEST_ASSERT_EQUAL_INT(pdTRUE, xQueueSend(s_queue, &value, pdMS_TO_TICKS(100)));
    xTaskNotifyGive(s_worker);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, s_worker_value);
    TEST_ASSERT_EQUAL_UINT32(1u, s_worker_notifications);
    vTaskDelete(s_worker);
    vQueueDelete(s_queue);
}

TEST_CASE("static NOSPLIT ring buffer round-trips a frame", "[rtos]")
{
    static uint8_t storage[2048];
    static StaticRingbuffer_t rb_struct;
    static uint8_t tx[1000];
    RingbufHandle_t rb;
    size_t length = 0u;
    uint8_t *rx;
    size_t index;

    for (index = 0u; index < sizeof(tx); ++index) {
        tx[index] = (uint8_t)(index * 7u + 3u);
    }
    rb = xRingbufferCreateStatic(sizeof(storage), RINGBUF_TYPE_NOSPLIT,
                                 storage, &rb_struct);
    TEST_ASSERT_NOT_NULL(rb);
    TEST_ASSERT_EQUAL_INT(pdTRUE,
                          xRingbufferSend(rb, tx, sizeof(tx), pdMS_TO_TICKS(100)));
    rx = (uint8_t *)xRingbufferReceive(rb, &length, pdMS_TO_TICKS(100));
    TEST_ASSERT_NOT_NULL(rx);
    TEST_ASSERT_EQUAL_UINT32(sizeof(tx), length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
    vRingbufferReturnItem(rb, rx);
    vRingbufferDelete(rb);
}

TEST_CASE("UART2 loopback round-trips a maximum frame", "[uart]")
{
    static uint8_t tx[LA_PROTO_MAX_PAYLOAD + LA_PROTO_FRAME_OVERHEAD];
    static uint8_t rx[sizeof(tx)];
    QueueHandle_t events = NULL;
    size_t index;
    size_t received = 0u;

    TEST_ASSERT_EQUAL_INT(ESP_OK, la_link_uart_init(&events));
    TEST_ASSERT_NOT_NULL(events);
    TEST_ASSERT_EQUAL_INT(ESP_OK,
                          uart_set_loop_back((uart_port_t)LA_BOARD_LINK_UART, true));
    for (index = 0u; index < sizeof(tx); ++index) {
        tx[index] = (uint8_t)(index ^ 0xA5u);
    }
    TEST_ASSERT_EQUAL_INT(ESP_OK, la_link_uart_write_frame(tx, sizeof(tx)));
    while (received < sizeof(tx)) {
        int count = la_link_uart_read(rx + received, sizeof(tx) - received,
                                      pdMS_TO_TICKS(200));
        if (count <= 0) {
            break;
        }
        received += (size_t)count;
    }
    TEST_ASSERT_EQUAL_UINT32(sizeof(tx), received);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
    TEST_ASSERT_EQUAL_INT(ESP_OK,
                          uart_set_loop_back((uart_port_t)LA_BOARD_LINK_UART, false));
    TEST_ASSERT_EQUAL_INT(ESP_OK,
                          uart_driver_delete((uart_port_t)LA_BOARD_LINK_UART));
}

TEST_CASE("task watchdog subscribe/reset/delete", "[wdt]")
{
    const esp_task_wdt_config_t config = {
        .timeout_ms = 2000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    esp_err_t result = esp_task_wdt_init(&config);

    TEST_ASSERT_TRUE(result == ESP_OK || result == ESP_ERR_INVALID_STATE);
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_task_wdt_add(NULL));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_task_wdt_status(NULL));
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_task_wdt_reset());
    TEST_ASSERT_EQUAL_INT(ESP_OK, esp_task_wdt_delete(NULL));
}

void app_main(void)
{
    printf("LA_TARGET_TESTS_BEGIN\n");
    fflush(stdout);
    unity_run_all_tests();
    printf("LA_TARGET_TESTS_DONE\n");
    fflush(stdout);
}
