#include "la_app.h"

#include "xil_printf.h"

#define LA_STATUS_PERIOD_MS 5000u

static uint16_t stack_watermark(TaskHandle_t task)
{
    UBaseType_t words;

    if (task == NULL) {
        return 0u;
    }
    words = uxTaskGetStackHighWaterMark(task);
    if (words > 0xFFFFu) {
        words = 0xFFFFu;
    }
    return (uint16_t)words;
}

void la_status_task(void *parameters)
{
    la_app_t *app = (la_app_t *)parameters;

    if (app == NULL) {
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        uint16_t acq = stack_watermark(app->acq_task);
        uint16_t cmd = stack_watermark(app->cmd_task);
        uint16_t tx = stack_watermark(app->tx_task);
        uint16_t status = stack_watermark(app->status_task);
        uint16_t min = acq;

        vTaskDelay(pdMS_TO_TICKS(LA_STATUS_PERIOD_MS));
        if (cmd < min) {
            min = cmd;
        }
        if (tx < min) {
            min = tx;
        }
        if (status < min) {
            min = status;
        }
        app->min_free_stack = min;
        /* Bring-up diagnostic line (the long status blocks are disabled until
         * the print-path fault is understood). */
#if LA_TRANSPORT_USB
        xil_printf("LA st: su=%u rx=%u fr=%u ev=%u ovf=%u bad=%u err=%u txb=%u txd=%u hs=%u\r\n",
                   (unsigned)app->usb.setup_count,
                   (unsigned)app->usb.rx_bytes,
                   (unsigned)app->rx_frames,
                   (unsigned)app->usb.rx_events,
                   (unsigned)app->usb.rx_overflows,
                   (unsigned)app->usb.rx_bad,
                   (unsigned)app->usb.ep0_errors,
                   (unsigned)app->usb.tx_bytes,
                   (unsigned)app->usb.tx_drops,
                   (unsigned)la_usb_high_speed(&app->usb));
        {
            uint32_t head = app->usb.rx_trace_head & 15u;
            uint32_t index;
            uint32_t values[12];

            for (index = 0u; index < 4u; index++) {
                uint32_t slot = (head + 12u + index) & 15u;
                values[index * 3u] = app->usb.rx_trace[slot][0];
                values[index * 3u + 1u] = app->usb.rx_trace[slot][1];
                values[index * 3u + 2u] = app->usb.rx_trace[slot][2];
            }
            xil_printf("LA rx: %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u\r\n",
                       (unsigned)values[0], (unsigned)values[1],
                       (unsigned)values[2], (unsigned)values[3],
                       (unsigned)values[4], (unsigned)values[5],
                       (unsigned)values[6], (unsigned)values[7],
                       (unsigned)values[8], (unsigned)values[9],
                       (unsigned)values[10], (unsigned)values[11]);
            xil_printf("LA cur: sw=%u hw=%u ovl=%u\r\n",
                       (unsigned)(((uintptr_t)app->usb.usb.DeviceConfig.Ep[1]
                                       .Out.dTDCurr -
                                   (uintptr_t)app->usb.usb.DeviceConfig.Ep[1]
                                       .Out.dTDs) /
                                  XUSBPS_dTD_ALIGN),
                       (unsigned)(((volatile uint32_t *)app->usb.usb
                                       .DeviceConfig.Ep[1]
                                       .Out.dQH)[1] -
                                  (uintptr_t)app->usb.usb.DeviceConfig.Ep[1]
                                      .Out.dTDs) /
                           XUSBPS_dTD_ALIGN,
                       (unsigned)(((volatile uint32_t *)app->usb.usb
                                       .DeviceConfig.Ep[1]
                                       .Out.dQH)[2] -
                                  (uintptr_t)app->usb.usb.DeviceConfig.Ep[1]
                                      .Out.dTDs) /
                           XUSBPS_dTD_ALIGN);
        }
#else
        xil_printf("LA st: acq=%u cmd=%u tx=%u status=%u\r\n",
                   (unsigned)acq, (unsigned)cmd, (unsigned)tx,
                   (unsigned)status);
#endif
    }
}
