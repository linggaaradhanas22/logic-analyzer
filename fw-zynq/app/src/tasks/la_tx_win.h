#ifndef LA_TX_WIN_H
#define LA_TX_WIN_H

/*
 * P4.9 UART burst TX window tracker.
 *
 * Pure sender-side window logic for the protocol.md burst rules: at most
 * LA_TX_WIN_DEPTH unacknowledged DATA_CHUNK frames, ACK clears the matching
 * slot, NAK marks the slot retransmit-pending without adding a duplicate
 * (the resend keeps the same sequence, so duplicate sequences never coexist
 * in the window). Stale or duplicate ACKs are ignored.
 *
 * The tracker makes no FreeRTOS, UART or buffer calls; TxTask owns it as the
 * single writer and holds the retransmit payload buffer. Design note:
 * docs/rtos-design.md, "UART burst TX window (P4.9)".
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_TXWIN_API __declspec(dllexport)
#else
#define LA_TXWIN_API
#endif

#define LA_TX_WIN_DEPTH 16u

typedef struct la_tx_win_slot {
    uint8_t sequence;
    uint8_t in_use;
    uint8_t retransmit;
} la_tx_win_slot_t;

typedef struct la_tx_win {
    la_tx_win_slot_t slots[LA_TX_WIN_DEPTH];
    size_t outstanding;
} la_tx_win_t;

/* Returns 0 on success, -1 when the window is full (caller must pause). */
LA_TXWIN_API int la_tx_win_push(la_tx_win_t *win, uint8_t sequence);

/* Clears the outstanding slot with this sequence; returns 1 when the ACK was
 * stale or unknown (ignored), 0 when a slot was cleared. */
LA_TXWIN_API int la_tx_win_ack(la_tx_win_t *win, uint8_t sequence);

/* Marks the outstanding slot for retransmission; returns 0 when a slot was
 * flagged, -1 when the sequence is not outstanding. The window count is
 * unchanged. */
LA_TXWIN_API int la_tx_win_nak(la_tx_win_t *win, uint8_t sequence);

/* Pops the next retransmit-pending sequence (oldest first); returns 0 with
 * the sequence in *sequence, or -1 when nothing is pending. The slot stays
 * outstanding; ACK later clears it. */
LA_TXWIN_API int la_tx_win_pop_retransmit(la_tx_win_t *win, uint8_t *sequence);

LA_TXWIN_API size_t la_tx_win_outstanding(const la_tx_win_t *win);

#endif /* LA_TX_WIN_H */
