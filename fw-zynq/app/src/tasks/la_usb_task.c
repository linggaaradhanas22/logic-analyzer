/*
 * U03 direct-USB CDC-ACM service task.
 *
 * UsbTask is the only task that talks to the XUsbPs device: it processes
 * standard and CDC class control requests, copies OUT payloads into the
 * CmdTask RX StreamBuffer, submits TxTask frames and applies session
 * actions. The interrupt callbacks in la_usb_dev.c only pop/copy/notify.
 *
 * EP0 control transfers are serialized by the host; class requests with an
 * OUT data stage (SET_LINE_CODING) latch the wait flag and complete when the
 * data completion arrives, with a bounded timeout that stalls EP0 instead of
 * hanging the control pipe.
 */

#include "la_app.h"

#if LA_TRANSPORT_USB

#include "la_usb_desc.h"
#include "la_usb_dev.h"

#include "xil_printf.h"

#include <string.h>

#define LA_USB_SERVICE_PERIOD_MS 1u
#define LA_USB_EP0_DATA_TIMEOUT_MS 500u

static uint8_t s_scratch[LA_USB_STRING_MAX];
static la_usb_completion_t s_pending;
static uint8_t s_rx_pending;
static uint32_t s_rx_offset;
static uint8_t s_ep0_wait_data;
static TickType_t s_ep0_wait_start;

static void usb_apply_session(la_app_t *app, la_usb_event_t event, uint8_t arg)
{
    la_usb_dev_t *dev = &app->usb;
    unsigned actions = la_usb_session_event(&dev->session, event, arg);

    xil_printf("USB session ev=%u arg=%u act=%02x st=%u\r\n", (unsigned)event,
               (unsigned)arg, actions, (unsigned)dev->session.state);
    if ((actions & LA_USB_ACT_INVALIDATE) != 0u) {
        /* Stale driver handles are never released: the vendor reset already
         * flushed its descriptors. Drop the held completion and queued TX. */
        s_rx_pending = 0u;
        s_ep0_wait_data = 0u;
        (void)xStreamBufferReset(dev->rx_sb);
        la_usb_tx_reset(dev);
    }
    if ((actions & LA_USB_ACT_STOP_ACQUISITION) != 0u) {
        la_ctrl_msg_t message;

        memset(&message, 0, sizeof(message));
        message.kind = LA_CTRL_ABORT;
        (void)xQueueSend(app->ctrl_q, &message, 0);
    }
    if ((actions & LA_USB_ACT_START_ENDPOINTS) != 0u) {
        (void)la_usb_reconfigure(dev);
    }
    dev->session_events++;
}

static void usb_handle_session(la_app_t *app)
{
    la_usb_dev_t *dev = &app->usb;
    uint32_t status = dev->session_status;

    dev->session_pending = 0u;
    if ((status & XUSBPS_IXR_UR_MASK) != 0u) {
        usb_apply_session(app, LA_USB_EV_BUS_RESET, 0u);
    }
    if ((status & XUSBPS_IXR_SLE_MASK) != 0u) {
        usb_apply_session(app, LA_USB_EV_SUSPEND, 0u);
    } else if ((status & XUSBPS_IXR_PC_MASK) != 0u &&
               dev->session.state == LA_USB_STATE_SUSPENDED) {
        usb_apply_session(app, LA_USB_EV_RESUME, 0u);
    }
}

static void usb_get_descriptor(la_usb_dev_t *dev, const XUsbPs_SetupData *setup)
{
    uint8_t type = (uint8_t)(setup->wValue >> 8);
    uint8_t index = (uint8_t)(setup->wValue & 0xFFu);
    const uint8_t *data = NULL;
    size_t length = 0u;

    switch (type) {
    case 0x01u:
        data = la_usb_desc_device();
        length = la_usb_desc_device_len();
        break;
    case 0x02u:
        data = la_usb_desc_config(&length, la_usb_high_speed(dev));
        break;
    case 0x03u:
        length = la_usb_desc_string(index, s_scratch, sizeof(s_scratch));
        data = (length != 0u) ? s_scratch : NULL;
        break;
    case 0x06u:
        data = la_usb_desc_qualifier(&length);
        break;
    case 0x07u:
        data = la_usb_desc_config(&length, !la_usb_high_speed(dev));
        break;
    default:
        break;
    }
    if (data == NULL || length == 0u) {
        la_usb_ep0_stall(dev, XUSBPS_EP_DIRECTION_IN);
        return;
    }
    if ((size_t)setup->wLength < length) {
        length = setup->wLength;
    }
    if (la_usb_ep0_send(dev, data, length) != 0) {
        xil_printf("USB ep0 send failed (desc %u)\r\n", (unsigned)type);
    }
}

static void usb_handle_standard(la_app_t *app, const XUsbPs_SetupData *setup,
                                uint8_t direction)
{
    la_usb_dev_t *dev = &app->usb;

    switch (setup->bRequest) {
    case 0x06u: /* GET_DESCRIPTOR */
        usb_get_descriptor(dev, setup);
        break;
    case 0x05u: /* SET_ADDRESS */
        (void)XUsbPs_SetDeviceAddress(&dev->usb, (u8)(setup->wValue & 0x7Fu));
        usb_apply_session(app, LA_USB_EV_SET_ADDRESS,
                          (uint8_t)(setup->wValue & 0x7Fu));
        (void)la_usb_ep0_send(dev, NULL, 0u);
        break;
    case 0x09u: /* SET_CONFIGURATION */
        usb_apply_session(app, LA_USB_EV_SET_CONFIGURATION,
                          (setup->wValue != 0u) ? 1u : 0u);
        (void)la_usb_ep0_send(dev, NULL, 0u);
        break;
    case 0x08u: { /* GET_CONFIGURATION */
        static uint8_t value;
        value = (dev->session.configured != 0u) ? 1u : 0u;
        (void)la_usb_ep0_send(dev, &value, 1u);
        break;
    }
    case 0x00u: { /* GET_STATUS */
        static const uint8_t zeros[2] = {0u, 0u};
        (void)la_usb_ep0_send(dev, zeros, 2u);
        break;
    }
    case 0x01u: /* CLEAR_FEATURE */
    case 0x03u: /* SET_FEATURE */
        if (setup->wValue == 0u && (setup->wIndex & 0xFFu) != 0u) {
            uint8_t endpoint = (uint8_t)(setup->wIndex & 0xFFu);
            uint8_t epdir = ((endpoint & 0x80u) != 0u)
                                ? XUSBPS_EP_DIRECTION_IN
                                : XUSBPS_EP_DIRECTION_OUT;
            uint8_t number = (uint8_t)(endpoint & 0x0Fu);

            if (setup->bRequest == 0x03u) {
                XUsbPs_EpStall(&dev->usb, number, epdir);
            } else {
                XUsbPs_EpUnStall(&dev->usb, number, epdir);
            }
        }
        (void)la_usb_ep0_send(dev, NULL, 0u);
        break;
    case 0x0Bu: /* SET_INTERFACE */
        (void)la_usb_ep0_send(dev, NULL, 0u);
        break;
    default:
        la_usb_ep0_stall(dev, direction);
        break;
    }
}

static void usb_handle_setup(la_app_t *app)
{
    la_usb_dev_t *dev = &app->usb;
    XUsbPs_SetupData setup;
    uint8_t direction;

    if (dev->setup_pending == 0u) {
        return;
    }
    setup = dev->setup;
    dev->setup_pending = 0u;
    direction = ((setup.bmRequestType & 0x80u) != 0u)
                    ? XUSBPS_EP_DIRECTION_IN
                    : XUSBPS_EP_DIRECTION_OUT;
    xil_printf("USB setup %02x %02x v=%04x i=%04x l=%u\r\n",
               (unsigned)setup.bmRequestType, (unsigned)setup.bRequest,
               (unsigned)setup.wValue, (unsigned)setup.wIndex,
               (unsigned)setup.wLength);

    if ((setup.bmRequestType & 0x60u) == 0x00u) {
        usb_handle_standard(app, &setup, direction);
    } else if ((setup.bmRequestType & 0x60u) == 0x20u) {
        switch (setup.bRequest) {
        case 0x20u: /* SET_LINE_CODING: wait for the 7-byte OUT stage */
            s_ep0_wait_data = 1u;
            s_ep0_wait_start = xTaskGetTickCount();
            break;
        case 0x21u: /* GET_LINE_CODING */
            (void)la_usb_ep0_send(dev, (const uint8_t *)dev->line_coding, 7u);
            break;
        case 0x22u: /* SET_CONTROL_LINE_STATE: advisory DTR/RTS */
            dev->control_line_state = setup.wValue;
            (void)la_usb_ep0_send(dev, NULL, 0u);
            break;
        case 0x23u: /* SEND_BREAK: accepted, no action */
            (void)la_usb_ep0_send(dev, NULL, 0u);
            break;
        default:
            la_usb_ep0_stall(dev, direction);
            break;
        }
    } else {
        la_usb_ep0_stall(dev, direction);
    }
}

static void usb_handle_ep0_data(la_usb_dev_t *dev)
{
    xil_printf("USB ep0 data len=%u wait=%u\r\n", (unsigned)s_pending.len,
               (unsigned)s_ep0_wait_data);
    if (s_ep0_wait_data != 0u) {
        if (s_pending.len >= 7u) {
            memcpy((void *)dev->line_coding, s_pending.ptr, 7u);
            s_ep0_wait_data = 0u;
            (void)la_usb_ep0_send(dev, NULL, 0u);
        } else {
            s_ep0_wait_data = 0u;
            la_usb_ep0_stall(dev, XUSBPS_EP_DIRECTION_OUT);
            dev->ep0_errors++;
        }
    }
    la_usb_rx_release(dev, &s_pending);
    s_rx_pending = 0u;
}

static void usb_pump_rx(la_app_t *app)
{
    la_usb_dev_t *dev = &app->usb;

    if (s_rx_pending == 0u) {
        int popped;

        taskENTER_CRITICAL();
        popped = la_usb_ring_pop(&dev->rx_ring, dev->session.generation,
                                 &s_pending);
        taskEXIT_CRITICAL();
        if (popped == 0) {
            return;
        }
        s_rx_pending = 1u;
        s_rx_offset = 0u;
    }
    if (s_pending.ep == 0u) {
        usb_handle_ep0_data(dev);
        return;
    }
    while (s_rx_offset < s_pending.len) {
        size_t sent = xStreamBufferSend(dev->rx_sb, s_pending.ptr + s_rx_offset,
                                        s_pending.len - s_rx_offset, 0);
        if (sent == 0u) {
            /* rx_sb full: keep the driver buffer (no re-prime) so the
             * controller NAKs instead of dropping bytes. */
            return;
        }
        s_rx_offset += (uint32_t)sent;
    }
    dev->rx_bytes += s_pending.len;
    la_usb_rx_release(dev, &s_pending);
    s_rx_pending = 0u;
}

void la_usb_task(void *parameters)
{
    la_app_t *app = (la_app_t *)parameters;
    la_usb_dev_t *dev = &app->usb;
    uint32_t bits;

    for (;;) {
        bits = 0u;
        (void)xTaskNotifyWait(0u, LA_USB_NOTIFY_ALL, &bits,
                              pdMS_TO_TICKS(LA_USB_SERVICE_PERIOD_MS));
        if ((bits & LA_USB_NOTIFY_SESSION) != 0u) {
            usb_handle_session(app);
        }
        if ((bits & LA_USB_NOTIFY_EP0_SETUP) != 0u) {
            usb_handle_setup(app);
        }
        usb_pump_rx(app);
        la_usb_tx_pump(dev);
        if (s_ep0_wait_data != 0u &&
            (xTaskGetTickCount() - s_ep0_wait_start) >
                pdMS_TO_TICKS(LA_USB_EP0_DATA_TIMEOUT_MS)) {
            s_ep0_wait_data = 0u;
            la_usb_ep0_stall(dev, XUSBPS_EP_DIRECTION_OUT);
            dev->ep0_errors++;
        }
    }
}

#endif /* LA_TRANSPORT_USB */
