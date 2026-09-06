#include "la_tx_win.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

static int test_window_limit(void)
{
    la_tx_win_t win;
    size_t index;

    memset(&win, 0, sizeof(win));
    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        CHECK(la_tx_win_push(&win, (uint8_t)index) == 0, "push within window");
    }
    CHECK(la_tx_win_outstanding(&win) == LA_TX_WIN_DEPTH, "window full count");
    CHECK(la_tx_win_push(&win, 0x10u) == -1, "window full rejects another chunk");
    CHECK(la_tx_win_ack(&win, 0x05u) == 0, "ack frees a slot");
    CHECK(la_tx_win_outstanding(&win) == LA_TX_WIN_DEPTH - 1u, "count after ack");
    CHECK(la_tx_win_push(&win, 0x10u) == 0, "push after ack succeeds");
    return 0;
}

static int test_ack_semantics(void)
{
    la_tx_win_t win;

    memset(&win, 0, sizeof(win));
    CHECK(la_tx_win_ack(&win, 0x00u) == 1, "stale ack ignored on empty window");
    CHECK(la_tx_win_push(&win, 0x07u) == 0, "push one");
    CHECK(la_tx_win_ack(&win, 0x07u) == 0, "ack clears matching slot");
    CHECK(la_tx_win_outstanding(&win) == 0u, "empty after ack");
    CHECK(la_tx_win_ack(&win, 0x07u) == 1, "duplicate ack ignored");
    CHECK(la_tx_win_push(&win, 0x07u) == 0, "sequence reusable after clear");
    CHECK(la_tx_win_push(&win, 0x08u) == 0, "second chunk");
    CHECK(la_tx_win_ack(&win, 0x08u) == 0, "out-of-order ack clears own slot");
    CHECK(la_tx_win_outstanding(&win) == 1u, "first slot still outstanding");
    CHECK(la_tx_win_ack(&win, 0x07u) == 0, "final ack");
    CHECK(la_tx_win_outstanding(&win) == 0u, "all cleared");
    return 0;
}

static int test_nak_retransmit(void)
{
    la_tx_win_t win;
    uint8_t sequence;

    memset(&win, 0, sizeof(win));
    CHECK(la_tx_win_nak(&win, 0x03u) == -1, "nak on unknown sequence rejected");
    CHECK(la_tx_win_push(&win, 0x03u) == 0, "push chunk");
    CHECK(la_tx_win_push(&win, 0x04u) == 0, "push second chunk");
    CHECK(la_tx_win_nak(&win, 0x03u) == 0, "nak flags slot");
    CHECK(la_tx_win_outstanding(&win) == 2u, "nak leaves count unchanged");
    CHECK(la_tx_win_pop_retransmit(&win, &sequence) == 0 && sequence == 0x03u,
          "retransmit pops flagged sequence (same seq)");
    CHECK(la_tx_win_outstanding(&win) == 2u, "resend does not add a slot");
    CHECK(la_tx_win_pop_retransmit(&win, &sequence) == -1, "no duplicate retransmit");
    CHECK(la_tx_win_ack(&win, 0x03u) == 0, "ack after resend clears the slot");
    CHECK(la_tx_win_outstanding(&win) == 1u, "count after resend-ack");
    return 0;
}

static int test_sequence_wraparound(void)
{
    la_tx_win_t win;

    memset(&win, 0, sizeof(win));
    CHECK(la_tx_win_push(&win, 0xFFu) == 0, "push seq 255");
    CHECK(la_tx_win_push(&win, 0x00u) == 0, "push seq 0 after wrap");
    CHECK(la_tx_win_ack(&win, 0x00u) == 0, "ack clears wrapped seq 0");
    CHECK(la_tx_win_outstanding(&win) == 1u, "only seq 255 remains");
    CHECK(la_tx_win_ack(&win, 0xFFu) == 0, "ack clears seq 255");
    return 0;
}

static int test_mixed_nak_order(void)
{
    la_tx_win_t win;
    uint8_t sequence;

    memset(&win, 0, sizeof(win));
    CHECK(la_tx_win_push(&win, 0x01u) == 0, "push a");
    CHECK(la_tx_win_push(&win, 0x02u) == 0, "push b");
    CHECK(la_tx_win_push(&win, 0x03u) == 0, "push c");
    CHECK(la_tx_win_nak(&win, 0x02u) == 0, "nak b");
    CHECK(la_tx_win_nak(&win, 0x01u) == 0, "nak a");
    CHECK(la_tx_win_ack(&win, 0x03u) == 0, "ack c while a/b pending");
    CHECK(la_tx_win_pop_retransmit(&win, &sequence) == 0 && sequence == 0x01u,
          "retransmit order is oldest first");
    CHECK(la_tx_win_pop_retransmit(&win, &sequence) == 0 && sequence == 0x02u,
          "second retransmit follows");
    CHECK(la_tx_win_pop_retransmit(&win, &sequence) == -1, "retransmit queue empty");
    CHECK(la_tx_win_outstanding(&win) == 2u, "a and b still outstanding");
    CHECK(la_tx_win_ack(&win, 0x01u) == 0 && la_tx_win_ack(&win, 0x02u) == 0,
          "final acks clear both");
    CHECK(la_tx_win_outstanding(&win) == 0u, "window empty");
    return 0;
}

int main(void)
{
    if (test_window_limit() != 0 || test_ack_semantics() != 0 ||
        test_nak_retransmit() != 0 || test_sequence_wraparound() != 0 ||
        test_mixed_nak_order() != 0) {
        return 1;
    }
    puts("PASS: la_native_txwin");
    return 0;
}
