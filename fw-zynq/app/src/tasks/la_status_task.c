#include "la_app.h"

#include "xil_printf.h"

#define LA_STATUS_PERIOD_MS 5000u

/* U03 bench images stay quiet: the bounded USB trace is retrieved over JTAG,
 * and the console print path is still an unresolved corruption suspect
 * (experiment 7, docs/usb-link.md section 11). Short lines were stable, so a
 * one-line counter summary can be compiled in for console-only debugging. */
#ifndef LA_USB_STATUS_PRINT
#define LA_USB_STATUS_PRINT 0
#endif

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
#if LA_TRANSPORT_USB
#if LA_USB_STATUS_PRINT
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
#endif
#else
        xil_printf("LA st: acq=%u cmd=%u tx=%u status=%u\r\n",
                   (unsigned)acq, (unsigned)cmd, (unsigned)tx,
                   (unsigned)status);
#endif
    }
}
