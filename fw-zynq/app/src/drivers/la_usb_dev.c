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

/* U03: global so fw-zynq/scripts/usb_trace_dump.tcl can resolve the symbol
 * from the ELF and dump the ring over JTAG after a burst. */
la_usb_trace_t la_usb_trace_ring;

void la_usb_trace_event(uint32_t tag, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t saved = ulPortSetInterruptMask();

    la_usb_trace_push(&la_usb_trace_ring, tag, a, b, c);
    vPortClearInterruptMask(saved);
}

void la_usb_toggle_reset(la_usb_dev_t *dev, uint8_t number, uint8_t direction)
{
    uint32_t mask;

    if (dev == NULL || number == 0u ||
        number >= dev->usb.DeviceConfig.NumEndpoints) {
        return;
    }
    mask = ((direction & XUSBPS_EP_DIRECTION_IN) != 0u)
               ? XUSBPS_EPCR_TXR_MASK
               : XUSBPS_EPCR_RXR_MASK;
    XUsbPs_SetBits(&dev->usb, XUSBPS_EPCRn_OFFSET(number), mask);
}

static void la_usb_isr_notify(la_usb_dev_t *dev, uint32_t bits)
{
    BaseType_t woken = pdFALSE;

    if (dev->task != NULL) {
        xTaskNotifyFromISR(dev->task, bits, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void la_usb_stage_push(la_usb_dev_t *dev, uint8_t ep, const uint8_t *data,
                              uint32_t length)
{
    la_usb_stage_t *stage;
    uint32_t saved;

    if (length > LA_USB_BULK_BUF_SIZE) {
        length = LA_USB_BULK_BUF_SIZE;
    }
    /* This port exposes the ISR-safe mask pair as ulPortSetInterruptMask /
     * vPortClearInterruptMask (no taskENTER_CRITICAL_FROM_ISR macro). */
    saved = ulPortSetInterruptMask();
    if (dev->stage_count >= LA_USB_STAGE_COUNT) {
        dev->rx_overflows++;
        vPortClearInterruptMask(saved);
        return;
    }
    stage = &dev->stages[dev->stage_head];
    memcpy(stage->data, data, length);
    stage->len = (uint16_t)length;
    stage->ep = ep;
    la_usb_trace_push(&la_usb_trace_ring, LA_USB_TRACE_STAGE_PUSH, ep, length,
                      dev->stage_head);
    dev->stage_head = (dev->stage_head + 1u) % LA_USB_STAGE_COUNT;
    dev->stage_count++;
    vPortClearInterruptMask(saved);
}

/* Pops one completed OUT descriptor, copies it into the staging ring and
 * releases it. `advance` is 0 in the ISR callback because the vendor
 * XUsbPs_IntrHandleRX loop advances the cursor itself, and 1 for the task
 * sweep that has no vendor loop. Returns 1 when an entry was serviced. */
static int la_usb_rx_service(la_usb_dev_t *dev, uint8_t ep, int advance)
{
    XUsbPs_EpOut *out = &dev->usb.DeviceConfig.Ep[ep].Out;
    uint8_t *ptr = NULL;
    uint32_t length = 0u;
    uint32_t handle = 0u;

    if (XUsbPs_dTDIsActive(out->dTDCurr)) {
        return 0;
    }
    if (XUsbPs_EpBufferReceive(&dev->usb, ep, &ptr, &length, &handle) !=
        XST_SUCCESS) {
        return 0;
    }
    {
        uint32_t dtd = (uint32_t)(((uintptr_t)out->dTDCurr -
                                   (uintptr_t)out->dTDs) /
                                  XUSBPS_dTD_ALIGN);
        uint32_t head = 0u;

        if (length >= 4u) {
            head = (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) |
                   ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
        }
        la_usb_trace_event(LA_USB_TRACE_OUT_DTD,
                           ((uint32_t)ep << 8) | (dtd & 0xFFu), length, head);
    }
    la_usb_stage_push(dev, ep, ptr, length);
    XUsbPs_EpBufferRelease(handle);
    if (advance != 0) {
        out->dTDCurr = XUsbPs_dTDGetNLP(out->dTDCurr);
        XUsbPs_dTDInvalidateCache(out->dTDCurr);
    }
    return 1;
}

static void la_usb_ep_out_event(la_usb_dev_t *dev, uint8_t ep)
{
    dev->rx_events++;
    if (la_usb_rx_service(dev, ep, 0) != 0) {
        la_usb_isr_notify(dev, LA_USB_NOTIFY_RX_DONE);
    }
}

void la_usb_sweep_rx(la_usb_dev_t *dev)
{
    u32 complete;
    uint8_t ep;

    if (dev == NULL) {
        return;
    }
    /* A completion whose interrupt is lost leaves an inactive descriptor at
     * the cursor; the controller then NAKs every further packet (bench
     * 2026-09-18: exactly four packets accepted, then a silent stall). Sweep
     * and re-prime from task context with interrupts masked so the vendor
     * ISR cannot race the cursor. The same lost interrupt also strands an IN
     * completion, so process EPCOMPL here as well. */
    taskENTER_CRITICAL();
    for (ep = 0u; ep < 2u; ep++) {
        uint32_t serviced = 0u;

        while (la_usb_rx_service(dev, ep, 1) != 0) {
            dev->rx_events++;
            serviced++;
        }
        if (serviced != 0u) {
            la_usb_trace_event(LA_USB_TRACE_SWEEP, ep, serviced, 0u);
        }
        (void)XUsbPs_EpPrime(&dev->usb, ep, XUSBPS_EP_DIRECTION_OUT);
    }
    complete = XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                              XUSBPS_EPCOMPL_OFFSET);
    if (complete != 0u) {
        XUsbPs_WriteReg(dev->usb.Config.BaseAddress, XUSBPS_EPCOMPL_OFFSET,
                        complete);
        if ((complete & (0x00010000u << 1)) != 0u && dev->tx_inflight >= 0) {
            int32_t slot = dev->tx_inflight;

            if (slot < (int32_t)LA_USB_TX_FRAME_COUNT) {
                dev->tx_slot_state[slot] = 0u;
            }
            la_usb_trace_event(LA_USB_TRACE_TX_DONE, (uint32_t)slot, complete,
                               dev->tx_bytes);
            dev->tx_inflight = -1;
            (void)xSemaphoreGive(dev->tx_free_sem);
        }
    }
    taskEXIT_CRITICAL();
}

static void la_usb_ep0_handler(void *reference, u8 ep, u8 event, void *data)
{
    la_usb_dev_t *dev = (la_usb_dev_t *)reference;

    (void)data;
    if (event == XUSBPS_EP_EVENT_SETUP_DATA_RECEIVED) {
        if (XUsbPs_EpGetSetupData(&dev->usb, (int)ep, &dev->setup_data) ==
            XST_SUCCESS) {
            dev->setup_ready = 1u;
            dev->setup_count++;
            la_usb_isr_notify(dev, LA_USB_NOTIFY_EP0_SETUP);
        }
    } else if (event == XUSBPS_EP_EVENT_DATA_RX) {
        la_usb_ep_out_event(dev, ep);
    }
}

static void la_usb_ep1_handler(void *reference, u8 ep, u8 event, void *data)
{
    la_usb_dev_t *dev = (la_usb_dev_t *)reference;

    (void)data;
    if (event == XUSBPS_EP_EVENT_DATA_RX) {
        la_usb_ep_out_event(dev, ep);
    }
}

static void la_usb_tx_handler(void *reference, u8 ep, u8 event, void *data)
{
    la_usb_dev_t *dev = (la_usb_dev_t *)reference;
    BaseType_t woken = pdFALSE;
    int32_t slot;

    (void)ep;
    (void)data;
    if (event != XUSBPS_EP_EVENT_DATA_TX) {
        return;
    }
    slot = dev->tx_inflight;
    if (slot >= 0 && slot < (int32_t)LA_USB_TX_FRAME_COUNT) {
        dev->tx_slot_state[slot] = 0u;
    }
    la_usb_trace_event(LA_USB_TRACE_TX_DONE, (uint32_t)slot, 0u,
                       dev->tx_bytes);
    dev->tx_inflight = -1;
    (void)xSemaphoreGiveFromISR(dev->tx_free_sem, &woken);
    if (dev->task != NULL) {
        xTaskNotifyFromISR(dev->task, LA_USB_NOTIFY_TX_DONE, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

const uint8_t *la_usb_stage_pop(la_usb_dev_t *dev, uint16_t *len, uint8_t *ep)
{
    la_usb_stage_t *stage;
    const uint8_t *ptr = NULL;
    uint32_t tail = 0u;

    if (dev == NULL || len == NULL || ep == NULL) {
        return NULL;
    }
    taskENTER_CRITICAL();
    if (dev->stage_count > 0u) {
        stage = &dev->stages[dev->stage_tail];
        *len = stage->len;
        *ep = stage->ep;
        ptr = stage->data;
        tail = dev->stage_tail;
    }
    taskEXIT_CRITICAL();
    if (ptr != NULL) {
        la_usb_trace_event(LA_USB_TRACE_STAGE_POP, *ep, *len, tail);
    }
    return ptr;
}

void la_usb_stage_release(la_usb_dev_t *dev)
{
    if (dev == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    if (dev->stage_count > 0u) {
        dev->stage_tail = (dev->stage_tail + 1u) % LA_USB_STAGE_COUNT;
        dev->stage_count--;
    }
    taskEXIT_CRITICAL();
}

int la_usb_setup_take(la_usb_dev_t *dev, XUsbPs_SetupData *setup)
{
    if (dev == NULL || setup == NULL || dev->setup_ready == 0u) {
        return 0;
    }
    *setup = dev->setup_data;
    dev->setup_ready = 0u;
    return 1;
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
    la_usb_trace_init(&la_usb_trace_ring);
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
    (void)XUsbPs_EpSetHandler(&dev->usb, 0, XUSBPS_EP_DIRECTION_OUT,
                              la_usb_ep0_handler, dev);
    (void)XUsbPs_EpSetHandler(&dev->usb, 1, XUSBPS_EP_DIRECTION_OUT,
                              la_usb_ep1_handler, dev);
    (void)XUsbPs_EpSetHandler(&dev->usb, 1, XUSBPS_EP_DIRECTION_IN,
                              la_usb_tx_handler, dev);

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
    /* UI drives the vendor endpoint dispatch (pop/copy/release in the
     * callback); SLE stays masked because it level-storms while the bus is
     * idle. Reset/error/port-change are also enabled. */
    XUsbPs_IntrEnable(&dev->usb, XUSBPS_IXR_UR_MASK | XUSBPS_IXR_UI_MASK |
                                    XUSBPS_IXR_UE_MASK | XUSBPS_IXR_PC_MASK);
    vPortEnableInterrupt(LA_USB_GIC_ENCODED);
    return 0;
}

int la_usb_reconfigure(la_usb_dev_t *dev)
{
    /* Bench finding 2026-09-18: the driver's ConfigureDevice initializes the
     * queue heads and descriptors but never sets the endpoint control
     * registers, so EP1/EP2 stay disabled and host tokens to them time out
     * (Write timeout). Enable the data endpoints at SET_CONFIGURATION and
     * re-prime the OUT ring. A full XUsbPs_ConfigureDevice re-run from task
     * context is deferred until the ISR/task interaction is proven. */
    (void)XUsbPs_EpEnable(&dev->usb, 1u,
                          XUSBPS_EP_DIRECTION_OUT | XUSBPS_EP_DIRECTION_IN);
    (void)XUsbPs_EpEnable(&dev->usb, 2u, XUSBPS_EP_DIRECTION_IN);
    /* USB 2.0 section 9.4.7 requires SET_CONFIGURATION to reset every
     * endpoint's data toggle to DATA0. Use the EPCR TXR/RXR bits that the
     * XUsbPs headers document as "Data Toggle Reset" (docs/usb-link.md
     * section 10); an EPFLUSH is documented only as a FIFO/endpoint flush.
     * The previous EPFLUSH + tx_inflight = -1 form also stranded an in-flight
     * TX slot; the IN completion still owns and returns it. */
    la_usb_toggle_reset(dev, 1u, XUSBPS_EP_DIRECTION_OUT);
    la_usb_toggle_reset(dev, 1u, XUSBPS_EP_DIRECTION_IN);
    la_usb_toggle_reset(dev, 2u, XUSBPS_EP_DIRECTION_IN);
    la_usb_trace_event(LA_USB_TRACE_RESET, 1u,
                       XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                                      XUSBPS_EPCR1_OFFSET),
                       XUsbPs_ReadReg(dev->usb.Config.BaseAddress,
                                      XUSBPS_EPRDY_OFFSET));
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
    /* The polled service copies into the staging buffer and releases the
     * driver buffer immediately, so there is nothing left to release here.
     * The function is kept for the task's flow and future use. */
    (void)dev;
    (void)completion;
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
