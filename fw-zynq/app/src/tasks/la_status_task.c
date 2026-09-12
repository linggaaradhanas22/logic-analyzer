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

        xil_printf("LA status: stacks acq=%u cmd=%u tx=%u status=%u words\r\n",
                   (unsigned)acq, (unsigned)cmd, (unsigned)tx,
                   (unsigned)status);
        xil_printf("LA status: rx=%u tx=%u retx=%u drops=%u pl=%u dma=%u live_max_us=%u\r\n",
                   (unsigned)app->rx_frames, (unsigned)app->tx_frames,
                   (unsigned)app->tx_retransmits, (unsigned)app->tx_drops,
                   (unsigned)app->irq.pl_count, (unsigned)app->irq.dma_count,
                   (unsigned)app->live_max_latency_us);
        xil_printf("LA status: dec words %u %u %u busy_us %u %u %u\r\n",
                   (unsigned)app->decoder_words[0],
                   (unsigned)app->decoder_words[1],
                   (unsigned)app->decoder_words[2],
                   (unsigned)app->decoder_busy_us[0],
                   (unsigned)app->decoder_busy_us[1],
                   (unsigned)app->decoder_busy_us[2]);
        xil_printf("LA status: isr us %u/%u/%u wake us %u/%u/%u\r\n",
                   (unsigned)la_irq_stats_min_us(&app->irq.isr_stats),
                   (unsigned)la_irq_stats_avg_us(&app->irq.isr_stats),
                   (unsigned)la_irq_stats_max_us(&app->irq.isr_stats),
                   (unsigned)la_irq_stats_min_us(&app->irq.wake_stats),
                   (unsigned)la_irq_stats_avg_us(&app->irq.wake_stats),
                   (unsigned)la_irq_stats_max_us(&app->irq.wake_stats));
    }
}
