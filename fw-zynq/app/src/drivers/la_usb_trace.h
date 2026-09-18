#ifndef LA_USB_TRACE_H
#define LA_USB_TRACE_H

/*
 * U03 bounded in-memory USB event trace.
 *
 * The trace ring records setups, descriptor completions, staging, parser
 * delivery and TX submissions/completions with four 32-bit words per entry.
 * It is CPU-only memory (never DMA) and is retrieved over JTAG after a burst;
 * the USB profile prints nothing long during the experiment, so the trace
 * cannot perturb the link or hide behind the unresolved print-path fault.
 *
 * The ring overwrites its oldest entry when full. `head` counts every push,
 * so the reader detects loss with la_usb_trace_lost(): entries older than
 * (head - capacity) were overwritten. Push ordering publishes the tag and
 * payload before `head`, so a reader that starts from `head` never observes a
 * half-written entry.
 */

#include <stdint.h>

#define LA_USB_TRACE_CAPACITY 512u
#define LA_USB_TRACE_WORDS 4u

typedef enum la_usb_trace_tag {
    LA_USB_TRACE_NONE = 0,
    LA_USB_TRACE_SETUP = 1,      /* a=bmReqType|bRequest|wValue b=wIndex|wLength c=setup count */
    LA_USB_TRACE_OUT_DTD = 2,    /* a=ep|dtd b=length c=first 4 payload bytes LE */
    LA_USB_TRACE_STAGE_PUSH = 3, /* a=ep b=length c=stage head index */
    LA_USB_TRACE_STAGE_POP = 4,  /* a=ep b=length c=stage tail index */
    LA_USB_TRACE_RX_SB = 5,      /* a=ep b=length c=accepted bytes */
    LA_USB_TRACE_TX_SUBMIT = 6,  /* a=slot b=length c=echo flag */
    LA_USB_TRACE_TX_DONE = 7,    /* a=slot b=EPCOMPL c=tx bytes */
    LA_USB_TRACE_RESET = 8,      /* a=source b=EPCR1 c=EPRDY */
    LA_USB_TRACE_SESSION = 9,    /* a=event b=actions c=state */
    LA_USB_TRACE_EP_STATE = 10,  /* a=EPRDY b=EPCOMPL c=EPCTRL1 */
    LA_USB_TRACE_SWEEP = 11,     /* a=ep b=serviced c=prime result */
    LA_USB_TRACE_FRAME = 12,     /* a=type|sequence b=length c=first 4 payload bytes LE */
    LA_USB_TRACE_ECHO = 13       /* reserved: isolated test echo (unused) */
} la_usb_trace_tag_t;

typedef struct la_usb_trace {
    volatile uint32_t head;
    uint32_t words[LA_USB_TRACE_CAPACITY][LA_USB_TRACE_WORDS];
} la_usb_trace_t;

void la_usb_trace_init(la_usb_trace_t *trace);
void la_usb_trace_push(la_usb_trace_t *trace, uint32_t tag, uint32_t a,
                       uint32_t b, uint32_t c);

/* Entries still readable (min(head, capacity)). */
uint32_t la_usb_trace_count(const la_usb_trace_t *trace);

/* Entries overwritten before retrieval (head > capacity). */
uint32_t la_usb_trace_lost(const la_usb_trace_t *trace);

#endif /* LA_USB_TRACE_H */
