#ifndef LA_LINK_UART_H
#define LA_LINK_UART_H

/*
 * P4.8 UART0 link driver (PS UART0 through EMIO on JD, RTS/CTS).
 *
 * RX: the XUartPs driver fills a 64-byte staging buffer and its interrupt
 * handler reports RECV_DATA / RECV_TOUT / error events; this driver copies
 * the received bytes into the single-producer RX StreamBuffer with
 * xStreamBufferSendFromISR and immediately re-arms the receive. The RX
 * StreamBuffer is CmdTask's input.
 *
 * TX: TxTask writes complete frames into the single-producer TX
 * StreamBuffer and calls la_link_uart_tx_kick(); the driver moves bytes
 * from the StreamBuffer into its staging buffer and hands them to
 * XUartPs_Send, which transmits through the TX-empty interrupt. Only one
 * XUartPs_Send is in flight at a time (tx_busy), so the staging buffer is
 * never overwritten while the UART still reads it.
 *
 * Flow control: the EMIO UART0 modem signals are wired to JD with RTS/CTS
 * crossed to the peer; the MODEMCR FCM bit enables automatic hardware flow
 * control (RTS deasserts at the RX trigger, CTS gates the transmitter).
 * Both StreamBuffer operations are ISR-safe and never block in the ISR.
 */

#include "FreeRTOS.h"
#include "stream_buffer.h"

#include "xuartps.h"

#include <stdint.h>

#define LA_UART_RX_STAGE 64u
#define LA_UART_TX_STAGE 256u

typedef struct la_link_uart {
    XUartPs uart;
    StreamBufferHandle_t rx_sb;
    StreamBufferHandle_t tx_sb;
    uint8_t rx_stage[LA_UART_RX_STAGE];
    uint8_t tx_stage[LA_UART_TX_STAGE];
    volatile uint8_t tx_busy;
    volatile uint32_t rx_overflow;
    volatile uint32_t rx_errors;
    volatile uint32_t tx_events;
} la_link_uart_t;

/* base is XPAR_XUARTPS_0_BASEADDR; the UART interrupt is installed through
 * the FreeRTOS port. Returns 0 or a negative error. */
int la_link_uart_init(la_link_uart_t *link, uintptr_t base, uint32_t baud,
                      StreamBufferHandle_t rx_sb, StreamBufferHandle_t tx_sb);

/* Pumps the TX StreamBuffer into the UART when no send is in flight. Call
 * from task context after writing a frame. */
void la_link_uart_tx_kick(la_link_uart_t *link);

#endif /* LA_LINK_UART_H */
