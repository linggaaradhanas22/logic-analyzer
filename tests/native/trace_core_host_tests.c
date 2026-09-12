#include "la_trace_core.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static la_trace_core_t trace;

static int test_enable_and_record(void)
{
    la_trace_core_init(&trace);
    CHECK(la_trace_core_enabled(&trace) == 0, "init leaves tracing disabled");
    CHECK(la_trace_core_count(&trace) == 0u, "init count");
    CHECK(la_trace_core_record(&trace, LA_TRACE_EVENT_TASK_IN, 0x1000u, 1u, 0u, 10u) == -1,
          "record while disabled rejected");

    la_trace_core_start(&trace);
    CHECK(la_trace_core_enabled(&trace) == 1, "start enables");
    CHECK(la_trace_core_record(&trace, LA_TRACE_EVENT_TASK_IN, 0x1000u, 1u, 0u, 10u) == 0,
          "record while enabled accepted");
    CHECK(la_trace_core_record(&trace, LA_TRACE_EVENT_QUEUE_SEND, 0x2000u, 0u, 0u, 20u) == 0,
          "second record");
    CHECK(la_trace_core_count(&trace) == 2u, "count two");
    CHECK(la_trace_core_total(&trace) == 2u, "total two");
    CHECK(la_trace_core_dropped(&trace) == 0u, "no drops");

    la_trace_core_stop(&trace);
    CHECK(la_trace_core_enabled(&trace) == 0, "stop disables");
    CHECK(la_trace_core_record(&trace, LA_TRACE_EVENT_TASK_OUT, 0x1000u, 1u, 0u, 30u) == -1,
          "record after stop rejected");
    CHECK(la_trace_core_count(&trace) == 2u, "stop keeps records");
    return 0;
}

static int test_order_and_fields(void)
{
    const la_trace_record_t *record;

    la_trace_core_init(&trace);
    la_trace_core_start(&trace);
    (void)la_trace_core_record(&trace, LA_TRACE_EVENT_TASK_IN, 0xAAAAu, 3u, 0u, 100u);
    (void)la_trace_core_record(&trace, LA_TRACE_EVENT_MARK_TX_FRAME, 0xBBBBu, 0u, 0x31u, 200u);
    record = la_trace_core_record_at(&trace, 0u);
    CHECK(record != NULL && record->event == LA_TRACE_EVENT_TASK_IN &&
              record->id == 0xAAAAu && record->object == 3u && record->time == 100u,
          "oldest record fields");
    record = la_trace_core_record_at(&trace, 1u);
    CHECK(record != NULL && record->event == LA_TRACE_EVENT_MARK_TX_FRAME &&
              record->value == 0x31u && record->time == 200u,
          "newest record fields");
    CHECK(la_trace_core_record_at(&trace, 2u) == NULL, "out of range record");
    return 0;
}

static int test_wrap_overwrites_oldest(void)
{
    uint32_t index;
    uint32_t extra = 7u;
    const la_trace_record_t *record;

    la_trace_core_init(&trace);
    la_trace_core_start(&trace);
    for (index = 0u; index < LA_TRACE_CAPACITY + extra; ++index) {
        CHECK(la_trace_core_record(&trace, LA_TRACE_EVENT_QUEUE_SEND,
                                   index, 0u, (uint16_t)index, (uint64_t)index) == 0,
              "wrap record accepted");
    }
    CHECK(la_trace_core_count(&trace) == LA_TRACE_CAPACITY, "count saturates");
    CHECK(la_trace_core_total(&trace) == LA_TRACE_CAPACITY + extra, "total keeps counting");
    CHECK(la_trace_core_dropped(&trace) == extra, "dropped equals overwritten");
    record = la_trace_core_record_at(&trace, 0u);
    CHECK(record != NULL && record->id == extra, "oldest surviving record");
    record = la_trace_core_record_at(&trace, LA_TRACE_CAPACITY - 1u);
    CHECK(record != NULL && record->id == LA_TRACE_CAPACITY + extra - 1u,
          "newest record after wrap");
    CHECK(la_trace_core_record_at(&trace, LA_TRACE_CAPACITY) == NULL,
          "index at capacity is out of range");
    return 0;
}

static int test_restart_clears(void)
{
    la_trace_core_init(&trace);
    la_trace_core_start(&trace);
    (void)la_trace_core_record(&trace, LA_TRACE_EVENT_TASK_IN, 1u, 1u, 0u, 1u);
    la_trace_core_start(&trace);
    CHECK(la_trace_core_count(&trace) == 0u, "restart clears the ring");
    CHECK(la_trace_core_total(&trace) == 0u, "restart clears total");
    return 0;
}

static int test_registration(void)
{
    const la_trace_object_t *object;

    la_trace_core_init(&trace);
    CHECK(la_trace_core_register(&trace, 0x1000u, LA_TRACE_OBJECT_TASK, 1u, "acq") == 0,
          "register task");
    CHECK(la_trace_core_register(&trace, 0x2000u, LA_TRACE_OBJECT_QUEUE, 0u, "seg0") == 0,
          "register queue");
    CHECK(la_trace_core_register(&trace, 0x1000u, LA_TRACE_OBJECT_TASK, 5u, "acq2") == 0,
          "re-register replaces");
    CHECK(trace.object_count == 2u, "re-register does not grow the table");
    CHECK(la_trace_core_task_number(&trace, 0x1000u) == 5u, "task number lookup");
    CHECK(la_trace_core_task_number(&trace, 0x2000u) == 0u,
          "queue is not a task");
    CHECK(la_trace_core_task_number(&trace, 0x9999u) == 0u, "unknown id");
    object = la_trace_core_object_at(&trace, 0u);
    CHECK(object != NULL && strcmp(object->name, "acq2") == 0 &&
              object->kind == LA_TRACE_OBJECT_TASK && object->number == 5u,
          "object entry fields");
    CHECK(la_trace_core_object_at(&trace, 2u) == NULL, "object out of range");
    CHECK(la_trace_core_register(&trace, 0u, LA_TRACE_OBJECT_TASK, 1u, "bad") == -1,
          "zero id rejected");
    CHECK(la_trace_core_register(&trace, 0x3000u, LA_TRACE_OBJECT_TASK, 1u, NULL) == -1,
          "null name rejected");
    return 0;
}

static int test_name_truncation_and_full_table(void)
{
    const la_trace_object_t *object;
    uint32_t index;

    la_trace_core_init(&trace);
    CHECK(la_trace_core_register(&trace, 1u, LA_TRACE_OBJECT_QUEUE, 0u, "0123456789") == 0,
          "long name accepted");
    object = la_trace_core_object_at(&trace, 0u);
    CHECK(object != NULL && strlen(object->name) == LA_TRACE_NAME_MAX - 1u &&
              strncmp(object->name, "0123456", LA_TRACE_NAME_MAX - 1u) == 0,
          "name truncated");

    la_trace_core_init(&trace);
    for (index = 0u; index < LA_TRACE_OBJECT_MAX; ++index) {
        CHECK(la_trace_core_register(&trace, index + 1u, LA_TRACE_OBJECT_QUEUE, 0u, "q") == 0,
              "table fill");
    }
    CHECK(la_trace_core_register(&trace, LA_TRACE_OBJECT_MAX + 1u,
                                 LA_TRACE_OBJECT_QUEUE, 0u, "x") == -1,
          "full table rejected");
    return 0;
}

int main(void)
{
    if (test_enable_and_record() != 0 || test_order_and_fields() != 0 ||
        test_wrap_overwrites_oldest() != 0 || test_restart_clears() != 0 ||
        test_registration() != 0 || test_name_truncation_and_full_table() != 0) {
        return 1;
    }
    puts("PASS: la_native_trace");
    return 0;
}
