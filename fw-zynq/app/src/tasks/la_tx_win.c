#include "la_tx_win.h"

#include <string.h>

static la_tx_win_slot_t *find_slot(la_tx_win_t *win, uint8_t sequence)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (win->slots[index].in_use != 0u && win->slots[index].sequence == sequence) {
            return &win->slots[index];
        }
    }
    return NULL;
}

static la_tx_win_slot_t *free_slot(la_tx_win_t *win)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (win->slots[index].in_use == 0u) {
            return &win->slots[index];
        }
    }
    return NULL;
}

int la_tx_win_push(la_tx_win_t *win, uint8_t sequence)
{
    la_tx_win_slot_t *slot = free_slot(win);

    if (slot == NULL) {
        return -1;
    }
    slot->sequence = sequence;
    slot->in_use = 1u;
    slot->retransmit = 0u;
    win->outstanding += 1u;
    return 0;
}

int la_tx_win_ack(la_tx_win_t *win, uint8_t sequence)
{
    la_tx_win_slot_t *slot = find_slot(win, sequence);

    if (slot == NULL) {
        return 1;
    }
    memset(slot, 0, sizeof(*slot));
    win->outstanding -= 1u;
    return 0;
}

int la_tx_win_nak(la_tx_win_t *win, uint8_t sequence)
{
    la_tx_win_slot_t *slot = find_slot(win, sequence);

    if (slot == NULL) {
        return -1;
    }
    slot->retransmit = 1u;
    return 0;
}

int la_tx_win_pop_retransmit(la_tx_win_t *win, uint8_t *sequence)
{
    size_t index;

    for (index = 0u; index < LA_TX_WIN_DEPTH; ++index) {
        if (win->slots[index].in_use != 0u && win->slots[index].retransmit != 0u) {
            win->slots[index].retransmit = 0u;
            *sequence = win->slots[index].sequence;
            return 0;
        }
    }
    return -1;
}

size_t la_tx_win_outstanding(const la_tx_win_t *win)
{
    return win->outstanding;
}
