/*
 * U03 host tests for the portable USB event trace ring (la_usb_trace).
 *
 * The ring must bound memory, overwrite the oldest entry, expose how many
 * entries were lost, and keep a reader that starts from `head` consistent.
 */

#include "la_usb_trace.h"

#include <stdio.h>
#include <string.h>

static int s_failures;

#define CHECK(condition, message)                                              \
    do {                                                                       \
        if (!(condition)) {                                                    \
            printf("FAIL: %s\n", message);                                     \
            s_failures++;                                                      \
        }                                                                      \
    } while (0)

static void test_init_and_guards(void)
{
    la_usb_trace_t trace;

    memset(&trace, 0xA5, sizeof(trace));
    la_usb_trace_init(&trace);
    CHECK(trace.head == 0u, "init head");
    CHECK(la_usb_trace_count(&trace) == 0u, "init count");
    CHECK(la_usb_trace_lost(&trace) == 0u, "init lost");
    CHECK(trace.words[0][0] == 0u && trace.words[0][3] == 0u, "init zeroed");
    CHECK(trace.words[LA_USB_TRACE_CAPACITY - 1u][2] == 0u, "init last zeroed");

    la_usb_trace_push(NULL, LA_USB_TRACE_SETUP, 1u, 2u, 3u);
    la_usb_trace_init(NULL);
    CHECK(la_usb_trace_count(NULL) == 0u, "NULL count guard");
    CHECK(la_usb_trace_lost(NULL) == 0u, "NULL lost guard");
}

static void test_push_order(void)
{
    la_usb_trace_t trace;
    uint32_t index;

    la_usb_trace_init(&trace);
    for (index = 0u; index < 5u; index++) {
        la_usb_trace_push(&trace, LA_USB_TRACE_OUT_DTD, index, index + 10u,
                          index + 20u);
    }
    CHECK(trace.head == 5u, "head after pushes");
    CHECK(la_usb_trace_count(&trace) == 5u, "count after pushes");
    CHECK(la_usb_trace_lost(&trace) == 0u, "no loss below capacity");
    for (index = 0u; index < 5u; index++) {
        CHECK(trace.words[index][0] == LA_USB_TRACE_OUT_DTD, "tag order");
        CHECK(trace.words[index][1] == index, "a order");
        CHECK(trace.words[index][2] == index + 10u, "b order");
        CHECK(trace.words[index][3] == index + 20u, "c order");
    }
}

static void test_wrap_and_loss(void)
{
    la_usb_trace_t trace;
    uint32_t index;
    uint32_t first_slot;

    la_usb_trace_init(&trace);
    for (index = 0u; index < LA_USB_TRACE_CAPACITY + 7u; index++) {
        la_usb_trace_push(&trace, LA_USB_TRACE_FRAME, index, 0u, 0u);
    }
    CHECK(trace.head == LA_USB_TRACE_CAPACITY + 7u, "head after wrap");
    CHECK(la_usb_trace_count(&trace) == LA_USB_TRACE_CAPACITY, "count capped");
    CHECK(la_usb_trace_lost(&trace) == 7u, "lost after wrap");
    first_slot = 7u % LA_USB_TRACE_CAPACITY;
    CHECK(trace.words[first_slot][1] == 7u, "oldest surviving entry");
    CHECK(trace.words[(first_slot + LA_USB_TRACE_CAPACITY - 1u) %
                      LA_USB_TRACE_CAPACITY][1] ==
              LA_USB_TRACE_CAPACITY + 6u,
          "newest entry");
}

int main(void)
{
    test_init_and_guards();
    test_push_order();
    test_wrap_and_loss();
    if (s_failures != 0) {
        printf("usb trace host tests: %d failure(s)\n", s_failures);
        return 1;
    }
    printf("usb trace host tests: PASS\n");
    return 0;
}
