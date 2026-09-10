#include "la_link_uart.h"

#include "xinterrupt_wrap.h"
#include "xuartps_hw.h"

#include "la_irq.h"

static void la_link_uart_rx_event(la_link_uart_t *link, u32 count, int error)
{
    BaseType_t woken = pdFALSE;
    size_t sent;

    if (count > sizeof(link->rx_stage)) {
        count = (u32)sizeof(link->rx_stage);
    }
    if (count != 0u) {
        sent = xStreamBufferSendFromISR(link->rx_sb, link->rx_stage, count,
                                        &woken);
        if (sent < (size_t)count) {
            link->rx_overflow++;
        }
    }
    if (error) {
        link->rx_errors++;
    }
    (void)XUartPs_Recv(&link->uart, link->rx_stage,
                       (u32)sizeof(link->rx_stage));
    portYIELD_FROM_ISR(woken);
}

static void la_link_uart_tx_pump(la_link_uart_t *link, BaseType_t *woken)
{
    size_t count;

    link->tx_busy = 0u;
    if (xStreamBufferIsEmpty(link->tx_sb)) {
        return;
    }
    count = xStreamBufferReceiveFromISR(link->tx_sb, link->tx_stage,
                                        sizeof(link->tx_stage), woken);
    if (count != 0u) {
        link->tx_busy = 1u;
        link->tx_events++;
        (void)XUartPs_Send(&link->uart, link->tx_stage, (u32)count);
    }
}

static void la_link_uart_event(void *reference, u32 event, u32 data)
{
    la_link_uart_t *link = (la_link_uart_t *)reference;

    switch (event) {
    case XUARTPS_EVENT_RECV_DATA:
    case XUARTPS_EVENT_RECV_TOUT:
        la_link_uart_rx_event(link, data, 0);
        break;
    case XUARTPS_EVENT_RECV_ERROR:
    case XUARTPS_EVENT_PARE_FRAME_BRKE:
    case XUARTPS_EVENT_RECV_ORERR:
        la_link_uart_rx_event(link, data, 1);
        break;
    case XUARTPS_EVENT_SENT_DATA: {
        BaseType_t woken = pdFALSE;

        la_link_uart_tx_pump(link, &woken);
        portYIELD_FROM_ISR(woken);
        break;
    }
    default:
        break;
    }
}

int la_link_uart_init(la_link_uart_t *link, uintptr_t base, uint32_t baud,
                      StreamBufferHandle_t rx_sb, StreamBufferHandle_t tx_sb)
{
    XUartPs_Config *config;
    XUartPsFormat format;
    uint32_t mask;

    if (link == NULL || rx_sb == NULL || tx_sb == NULL) {
        return -1;
    }
    config = XUartPs_LookupConfig(base);
    if (config == NULL) {
        return -1;
    }
    if (XUartPs_CfgInitialize(&link->uart, config, config->BaseAddress) !=
        XST_SUCCESS) {
        return -2;
    }
    if (XUartPs_SelfTest(&link->uart) != XST_SUCCESS) {
        return -3;
    }
    link->rx_sb = rx_sb;
    link->tx_sb = tx_sb;
    link->tx_busy = 0u;
    link->rx_overflow = 0u;
    link->rx_errors = 0u;
    link->tx_events = 0u;

    format.BaudRate = baud;
    format.DataBits = XUARTPS_FORMAT_8_BITS;
    format.Parity = XUARTPS_FORMAT_NO_PARITY;
    format.StopBits = XUARTPS_FORMAT_1_STOP_BIT;
    if (XUartPs_SetDataFormat(&link->uart, &format) != XST_SUCCESS) {
        return -4;
    }
    XUartPs_SetFifoThreshold(&link->uart, 1u);
    XUartPs_SetRecvTimeout(&link->uart, 8u);
    XUartPs_SetOperMode(&link->uart, XUARTPS_OPER_MODE_NORMAL);

    /* Automatic RTS/CTS hardware flow control. */
    XUartPs_WriteReg(config->BaseAddress, XUARTPS_MODEMCR_OFFSET,
                     XUARTPS_MODEMCR_FCM);

    if (xPortInstallInterruptHandler(
            config->IntrId, (XInterruptHandler)XUartPs_InterruptHandler,
            &link->uart) != pdPASS) {
        return -5;
    }
    XSetPriorityTriggerType(config->IntrId, XINTERRUPT_DEFAULT_PRIORITY,
                            config->IntrParent);
    XUartPs_SetHandler(&link->uart, la_link_uart_event, link);

    mask = XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING |
           XUARTPS_IXR_OVER | XUARTPS_IXR_TXEMPTY | XUARTPS_IXR_RXFULL |
           XUARTPS_IXR_RXOVR;
    XUartPs_SetInterruptMask(&link->uart, mask);
    vPortEnableInterrupt(config->IntrId);
    (void)XUartPs_Recv(&link->uart, link->rx_stage,
                       (u32)sizeof(link->rx_stage));
    return 0;
}

void la_link_uart_tx_kick(la_link_uart_t *link)
{
    if (link == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    if (link->tx_busy == 0u && !xStreamBufferIsEmpty(link->tx_sb)) {
        size_t count = xStreamBufferReceive(link->tx_sb, link->tx_stage,
                                            sizeof(link->tx_stage), 0);
        if (count != 0u) {
            link->tx_busy = 1u;
            link->tx_events++;
            (void)XUartPs_Send(&link->uart, link->tx_stage, (u32)count);
        }
    }
    taskEXIT_CRITICAL();
}
