/*
 * U03 direct-USB CDC-ACM device driver implementation.
 *
 * See la_usb_dev.h for the interrupt and ownership contract. This file is
 * compiled only for the direct-USB profile so the gateway image links no
 * XUsbPs code.
 */

#include "la_usb_dev.h"

#if LA_TRANSPORT_USB

#include "la_irq.h"
#include "la_mem_geom.h"

#include "xil_printf.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"
#include "xusbps_endpoint.h"

#include <string.h>

static void la_usb_isr_notify(la_usb_dev_t *dev, uint32_t bits)
{
    BaseType_t woken = pdFALSE;

    if (dev->task != NULL) {
        xTaskNotifyFromISR(dev->task, bits, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

int la_usb_poll_setup(la_usb_dev_t *dev, XUsbPs_SetupData *setup)
{
    u32 epstat;

    if (dev == NULL || setup == NULL) {
        return 0;
    }
    epstat = XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                            XUSBPS_EPSTAT_OFFSET);
    if ((epstat & 0x1u) == 0u) {
        return 0;
    }
    if (XUsbPs_EpGetSetupData(&dev->usb, 0, setup) != XST_SUCCESS) {
        return 0;
    }
    /* The vendor ISR used to re-prime EP0 OUT after the setup; with UI
     * masked the poll does it. */
    (void)XUsbPs_EpPrime(&dev->usb, 0u, XUSBPS_EP_DIRECTION_OUT);
    dev->setup_count++;
    return 1;
}

int la_usb_poll_rx(la_usb_dev_t *dev, la_usb_completion_t *completion)
{
    uint8_t *ptr = NULL;
    uint32_t length = 0u;
    uint32_t handle = 0u;
    uint8_t ep;

    if (dev == NULL || completion == NULL) {
        return 0;
    }
    for (ep = 0u; ep < 2u; ep++) {
        XUsbPs_EpOut *out;

        if (XUsbPs_EpBufferReceive(&dev->usb, ep, &ptr, &length, &handle) !=
            XST_SUCCESS) {
            continue;
        }
        /* The vendor RX interrupt loop advances the software cursor after
         * each pop; the polled service must do the same or the ring stalls
         * on the re-activated descriptor. */
        out = &dev->usb.DeviceConfig.Ep[ep].Out;
        out->dTDCurr = XUsbPs_dTDGetNLP(out->dTDCurr);
        XUsbPs_dTDInvalidateCache(out->dTDCurr);
        completion->generation = dev->session.generation;
        completion->handle = handle;
        completion->ptr = ptr;
        completion->len = length;
        completion->ep = ep;
        return 1;
    }
    return 0;
}

void la_usb_poll_tx(la_usb_dev_t *dev)
{
    u32 complete;

    if (dev == NULL) {
        return;
    }
    complete = XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                              XUSBPS_EPCOMPL_OFFSET);
    if (complete == 0u) {
        return;
    }
    XUsbPs_WriteReg(dev->usb.Config.BaseAddress, XUSBPS_EPCOMPL_OFFSET,
                    complete);
    /* EP1 IN completion bit (EP0 IN is one bit below it). */
    if ((complete & (0x00010000u << 1)) != 0u && dev->tx_inflight >= 0) {
        int32_t slot = dev->tx_inflight;

        if (slot < (int32_t)LA_USB_TX_FRAME_COUNT) {
            dev->tx_slot_state[slot] = 0u;
        }
        dev->tx_inflight = -1;
        (void)xSemaphoreGive(dev->tx_free_sem);
    }
}

static void la_usb_general_handler(void *reference, u32 status)
{
    la_usb_dev_t *dev = (la_usb_dev_t *)reference;

    /* UI and SLE are never enabled in the polled bring-up path: the UI
     * summary re-asserts from the level-held suspend bit (storm, bench
     * 2026-09-18) and the vendor RX loop requires pop-and-release inside the
     * endpoint callback. Only reset/error/port-change reach this handler. */
    dev->session_status = status;
    dev->session_pending = 1u;
    dev->isr_events++;
    la_usb_isr_notify(dev, LA_USB_NOTIFY_SESSION);
}

static int la_usb_pool_check(const la_usb_dev_t *dev, const uint8_t *pool)
{
    const uint8_t *end = pool + LA_USB_DMA_POOL_BYTES;
    int index;

    for (index = 0; index < dev->usb.DeviceConfig.NumEndpoints; index++) {
        const XUsbPs_Endpoint *ep = &dev->usb.DeviceConfig.Ep[index];
        const XUsbPs_EpConfig *cfg = &dev->usb.DeviceConfig.EpCfg[index];

        if (ep->Out.dTDBufs != NULL &&
            ep->Out.dTDBufs + ((size_t)cfg->Out.NumBufs * cfg->Out.BufSize) >
                end) {
            return -1;
        }
        if (ep->Out.dTDs != NULL &&
            (const uint8_t *)(ep->Out.dTDs + cfg->Out.NumBufs) > end) {
            return -1;
        }
        if (ep->In.dTDs != NULL &&
            (const uint8_t *)(ep->In.dTDs + cfg->In.NumBufs) > end) {
            return -1;
        }
    }
    return 0;
}

int la_usb_dev_init(la_usb_dev_t *dev, StreamBufferHandle_t rx_sb,
                    TaskHandle_t task)
{
    XUsbPs_Config *config;
    uint8_t *pool;

    if (dev == NULL || rx_sb == NULL) {
        return -1;
    }
    memset(dev, 0, sizeof(*dev));
    dev->rx_sb = rx_sb;
    dev->task = task;
    dev->tx_inflight = -1;
    la_usb_session_init(&dev->session);

    dev->tx_free_sem = xSemaphoreCreateCountingStatic(
        LA_USB_TX_FRAME_COUNT, LA_USB_TX_FRAME_COUNT, &dev->tx_sem_struct);
    if (dev->tx_free_sem == NULL) {
        return -2;
    }

    /* SDT flow: the lookup key is the base address (0xE0002000), not a
     * truncated device id. */
    config = XUsbPs_LookupConfig((u32)XPAR_XUSBPS_0_BASEADDR);
    if (config == NULL) {
        xil_printf("LA USB fatal: XUsbPs config missing\r\n");
        return -3;
    }
    if (XUsbPs_CfgInitialize(&dev->usb, config, config->BaseAddress) !=
        XST_SUCCESS) {
        xil_printf("LA USB fatal: CfgInitialize failed\r\n");
        return -4;
    }

    memset(&dev->device_config, 0, sizeof(dev->device_config));
    dev->device_config.EpCfg[0].Out.Type = XUSBPS_EP_TYPE_CONTROL;
    dev->device_config.EpCfg[0].Out.NumBufs = LA_USB_EP0_BUF_COUNT;
    dev->device_config.EpCfg[0].Out.BufSize = LA_USB_EP0_BUF_SIZE;
    dev->device_config.EpCfg[0].Out.MaxPacketSize = LA_USB_EP0_BUF_SIZE;
    dev->device_config.EpCfg[0].In.Type = XUSBPS_EP_TYPE_CONTROL;
    dev->device_config.EpCfg[0].In.NumBufs = LA_USB_EP0_BUF_COUNT;
    dev->device_config.EpCfg[0].In.MaxPacketSize = LA_USB_EP0_BUF_SIZE;
    dev->device_config.EpCfg[1].Out.Type = XUSBPS_EP_TYPE_BULK;
    dev->device_config.EpCfg[1].Out.NumBufs = LA_USB_BULK_BUF_COUNT;
    dev->device_config.EpCfg[1].Out.BufSize = LA_USB_BULK_BUF_SIZE;
    dev->device_config.EpCfg[1].Out.MaxPacketSize = LA_USB_BULK_BUF_SIZE;
    dev->device_config.EpCfg[1].In.Type = XUSBPS_EP_TYPE_BULK;
    dev->device_config.EpCfg[1].In.NumBufs = LA_USB_BULK_BUF_COUNT;
    dev->device_config.EpCfg[1].In.MaxPacketSize = LA_USB_BULK_BUF_SIZE;
    dev->device_config.EpCfg[2].In.Type = XUSBPS_EP_TYPE_INTERRUPT;
    dev->device_config.EpCfg[2].In.NumBufs = LA_USB_NOTIFY_BUF_COUNT;
    dev->device_config.EpCfg[2].In.MaxPacketSize = LA_USB_NOTIFY_BUF_SIZE;
    dev->device_config.NumEndpoints = 3u;

    pool = (uint8_t *)(uintptr_t)(LA_MEM_NC_BASE + LA_MEM_USB_OFFSET);
    /* The driver initializes dQH/dTD fields with read-modify-write and
     * assumes zeroed DMA memory (the vendor example memsets its pool). DDR
     * powers up with garbage, which left the first IN descriptor marked
     * active and made the controller DMA to wild addresses on the bench
     * (2026-09-18). Zero the pool before ConfigureDevice. */
    memset(pool, 0, LA_USB_DMA_POOL_BYTES);
    dev->device_config.DMAMemPhys = (u32)(uintptr_t)pool;
    if (XUsbPs_ConfigureDevice(&dev->usb, &dev->device_config) != XST_SUCCESS) {
        xil_printf("LA USB fatal: ConfigureDevice failed\r\n");
        return -5;
    }
    if (la_usb_pool_check(dev, pool) != 0) {
        xil_printf("LA USB fatal: DMA pool too small\r\n");
        return -6;
    }

    if (XUsbPs_IntrSetHandler(&dev->usb, la_usb_general_handler, dev,
                              XUSBPS_IXR_UE_MASK | XUSBPS_IXR_PC_MASK) !=
        XST_SUCCESS) {
        xil_printf("LA USB fatal: IntrSetHandler failed\r\n");
        return -7;
    }
    /* Endpoint handlers are intentionally not registered: the vendor RX
     * loop requires pop-and-release inside the callback, and the polled
     * service (la_usb_poll_*) replaces it. See la_usb_dev.h. */

    if (la_irq_connect(LA_USB_GIC_ENCODED, XUsbPs_IntrHandler, &dev->usb,
                       (uintptr_t)XPAR_XUSBPS_0_INTERRUPT_PARENT,
                       XINTERRUPT_DEFAULT_PRIORITY) != 0) {
        xil_printf("LA USB fatal: interrupt connect failed\r\n");
        return -8;
    }
    XSetPriorityTriggerType(LA_USB_GIC_ENCODED, XINTERRUPT_DEFAULT_PRIORITY,
                            (uintptr_t)XPAR_XUSBPS_0_INTERRUPT_PARENT);
    /* Clear any stale status from configuration before the line is enabled,
     * connect the device, and only then unmask the interrupt sources. */
    {
        u32 pending = XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                                     XUSBPS_ISR_OFFSET);

        XUsbPs_WriteReg(dev->usb.Config.BaseAddress, XUSBPS_ISR_OFFSET,
                        pending);
    }
    XUsbPs_Start(&dev->usb);
    (void)XUsbPs_EpPrime(&dev->usb, 1u, XUSBPS_EP_DIRECTION_OUT);
    /* UI stays masked (polled endpoint service); SLE stays masked (level
     * storm). Reset/error/port-change remain interrupt-driven. */
    XUsbPs_IntrEnable(&dev->usb, XUSBPS_IXR_UR_MASK | XUSBPS_IXR_UE_MASK |
                                    XUSBPS_IXR_PC_MASK);
    vPortEnableInterrupt(LA_USB_GIC_ENCODED);
    return 0;
}

int la_usb_reconfigure(la_usb_dev_t *dev)
{
    /* Bench finding 2026-09-18: calling XUsbPs_ConfigureDevice (which resets
     * the controller) from task context while the USB interrupt is live
     * wedged the image. On SET_CONFIGURATION the vendor reset handler has
     * already flushed the endpoints, so re-prime the OUT ring and let the
     * driver resume; a full reconfiguration is deferred until the ISR/task
     * interaction is proven on the bench. */
    dev->tx_inflight = -1;
    return (XUsbPs_EpPrime(&dev->usb, 1u, XUSBPS_EP_DIRECTION_OUT) ==
            XST_SUCCESS)
               ? 0
               : -2;
}

int la_usb_enqueue(la_usb_dev_t *dev, const uint8_t *frame, size_t length)
{
    int slot;

    if (dev == NULL || frame == NULL || length == 0u ||
        length > LA_USB_TX_FRAME_BYTES) {
        if (dev != NULL) {
            dev->tx_drops++;
        }
        return -1;
    }
    if (xSemaphoreTake(dev->tx_free_sem, pdMS_TO_TICKS(LA_USB_TX_WAIT_MS)) !=
        pdTRUE) {
        dev->tx_drops++;
        return -1;
    }
    for (slot = 0; slot < (int)LA_USB_TX_FRAME_COUNT; slot++) {
        if (dev->tx_slot_state[slot] == 0u) {
            break;
        }
    }
    if (slot == (int)LA_USB_TX_FRAME_COUNT) {
        (void)xSemaphoreGive(dev->tx_free_sem);
        dev->tx_drops++;
        return -1;
    }
    memcpy(dev->tx_frames[slot], frame, length);
    dev->tx_slot_len[slot] = (uint32_t)length;
    dev->tx_slot_state[slot] = 1u;
    if (dev->task != NULL) {
        (void)xTaskNotify(dev->task, LA_USB_NOTIFY_TX_FRAME, eSetBits);
    }
    return 0;
}

void la_usb_tx_pump(la_usb_dev_t *dev)
{
    int slot;

    if (dev->tx_inflight >= 0) {
        return;
    }
    for (slot = 0; slot < (int)LA_USB_TX_FRAME_COUNT; slot++) {
        if (dev->tx_slot_state[slot] == 1u) {
            break;
        }
    }
    if (slot == (int)LA_USB_TX_FRAME_COUNT) {
        return;
    }
    /* Mark in-flight before the submission: a fast completion callback can
     * otherwise observe an empty in-flight slot and never free it. */
    dev->tx_slot_state[slot] = 2u;
    dev->tx_inflight = slot;
    if (XUsbPs_EpBufferSend(&dev->usb, 1u, dev->tx_frames[slot],
                            dev->tx_slot_len[slot]) != XST_SUCCESS) {
        dev->tx_slot_state[slot] = 1u;
        dev->tx_inflight = -1;
        return; /* descriptor ring busy; retry on the next service pass */
    }
    dev->tx_bytes += dev->tx_slot_len[slot];
}

void la_usb_tx_reset(la_usb_dev_t *dev)
{
    int slot;

    for (slot = 0; slot < (int)LA_USB_TX_FRAME_COUNT; slot++) {
        if (dev->tx_slot_state[slot] != 0u) {
            dev->tx_slot_state[slot] = 0u;
            (void)xSemaphoreGive(dev->tx_free_sem);
        }
    }
    dev->tx_inflight = -1;
}

void la_usb_rx_release(la_usb_dev_t *dev,
                       const la_usb_completion_t *completion)
{
    XUsbPs_EpBufferRelease(completion->handle);
    (void)XUsbPs_EpPrime(&dev->usb, completion->ep,
                         XUSBPS_EP_DIRECTION_OUT);
}

int la_usb_ep0_send(la_usb_dev_t *dev, const uint8_t *data, size_t length)
{
    if (XUsbPs_EpBufferSend(&dev->usb, 0u, data, (u32)length) != XST_SUCCESS) {
        dev->ep0_errors++;
        return -1;
    }
    return 0;
}

void la_usb_ep0_stall(la_usb_dev_t *dev, uint8_t direction)
{
    XUsbPs_EpStall(&dev->usb, 0u, direction);
    dev->unsupported++;
}

int la_usb_high_speed(la_usb_dev_t *dev)
{
    u32 portsc = XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                                XUSBPS_PORTSCR1_OFFSET);

    return (portsc & XUSBPS_PORTSCR_HSP_MASK) != 0u;
}

#endif /* LA_TRANSPORT_USB */
