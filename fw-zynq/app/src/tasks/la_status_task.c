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
        {
            uint32_t spi_tx = 0u;
            uint32_t spi_rx = 0u;
            uint32_t spi_underruns = 0u;
            uint32_t spi_errors = 0u;
            la_spi_debug_t dbg;

            la_spi_snapshot(&spi_tx, &spi_rx, &spi_underruns, &spi_errors);
            la_spi_debug(&dbg);
            xil_printf("LA status: spi tx=%u rx=%u underrun=%u err=%u\r\n",
                       (unsigned)spi_tx, (unsigned)spi_rx,
                       (unsigned)spi_underruns, (unsigned)spi_errors);
            xil_printf("LA status: spi2 ring=%u fifo=%u ready=%u isr=%u ue=%u he=%u oth=%u rise=%u fall=%u drop=%u tdrop=%u pass=%u buf=%u\r\n",
                       (unsigned)dbg.ring_used, (unsigned)dbg.fifo_occ,
                       (unsigned)dbg.ready, (unsigned)dbg.isr_events,
                       (unsigned)dbg.isr_underrun, (unsigned)dbg.isr_half_empty,
                       (unsigned)dbg.isr_other, (unsigned)dbg.ready_rises,
                       (unsigned)dbg.ready_falls, (unsigned)dbg.frames_dropped,
                       (unsigned)app->tx_drops, (unsigned)dbg.task_passes,
                       (unsigned)dbg.buffered);
        }
    }
}
