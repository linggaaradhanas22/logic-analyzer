/*
 * P4 Zynq FreeRTOS core entry point.
 *
 * Boot contract (P4.3/P4.4): check the linker geometry, program the
 * non-cached DDR window, run the register driver selftest through the AXI
 * fabric, then bring up the application (la_app_init creates the static
 * tasks, queues, stream buffers and the UART/DMA/IRQ drivers) and start the
 * scheduler. The console stays on UART1 (BSP stdout); UART0 is the analyzer
 * link through EMIO on JD.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "la_app.h"
#include "la_mem_geom.h"
#include "la_regs.h"

#include "xil_io.h"
#include "xil_mmu.h"
#include "xil_printf.h"

extern uint8_t __la_fw_end[];
extern uint8_t __la_nc_base[];

static la_app_t s_app;

static uint32_t boot_read32(uintptr_t address)
{
    return Xil_In32((UINTPTR)address);
}

static void boot_write32(uintptr_t address, uint32_t value)
{
    Xil_Out32((UINTPTR)address, value);
}

static void la_program_noncached(void *context, uintptr_t section_base)
{
    (void)context;
    Xil_SetTlbAttributes((INTPTR)section_base, NORM_NONCACHE);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t idle_tcb;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t timer_tcb;
    static StackType_t timer_stack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &timer_tcb;
    *ppxTimerTaskStackBuffer = timer_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationMallocFailedHook(void)
{
    xil_printf("LA fatal: malloc failed (static-only build)\r\n");
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    xil_printf("LA fatal: stack overflow in %s\r\n", pcTaskName);
    for (;;) {
    }
}

int main(void)
{
    la_regs_dev_t regs;
    int selftest;

    if (la_mem_geometry_check((uintptr_t)__la_fw_end) != LA_MEM_CHECK_OK) {
        xil_printf("LA fatal: memory geometry check failed\r\n");
        for (;;) {
        }
    }
    la_mem_apply_noncached(la_program_noncached, NULL);
    xil_printf("\r\nLA P4 FreeRTOS core on UART1\r\n");
    xil_printf("LA memory: fw end 0x%08X nc base 0x%08X\r\n",
               (unsigned)(uintptr_t)__la_fw_end,
               (unsigned)(uintptr_t)__la_nc_base);

    la_regs_dev_init(&regs, LA_REG_BASE_ADDRESS, boot_read32, boot_write32);
    selftest = la_regs_selftest(&regs);
    xil_printf("LA P4.4 register selftest: %s (ID 0x%08X)\r\n",
               la_regs_selftest_name(selftest),
               (unsigned)la_regs_read_id(&regs));
    if (selftest != LA_REGS_SELFTEST_OK) {
        xil_printf("LA fatal: register selftest failed\r\n");
        for (;;) {
        }
    }

    if (la_app_init(&s_app) != 0) {
        xil_printf("LA fatal: application init failed\r\n");
        for (;;) {
        }
    }
    xil_printf("LA app ready: %u tasks, UART0 921600 RTS/CTS\r\n",
               (unsigned)uxTaskGetNumberOfTasks());
    vTaskStartScheduler();
    for (;;) {
    }
}
