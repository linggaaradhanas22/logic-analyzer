/*
 * P4.1 hello FreeRTOS on the Zybo console UART1 (BSP stdout).
 *
 * Static-allocation only (see the FreeRTOSConfig.h policy fragment). Two
 * statically created tasks: LaHello prints a banner and counts, LaProbe
 * reports its own stack high-water mark after a delay so the first bench
 * session can confirm scheduling and stack checking on the console.
 * vApplicationGetIdleTaskMemory / vApplicationGetTimerTaskMemory feed the
 * idle and timer daemon tasks; the stack-overflow and malloc-failed hooks
 * print and halt so faults are visible instead of silent.
 *
 * P4.3 boot contract: before the scheduler starts, the linker geometry is
 * checked (firmware end below the non-cached DDR window; constants in
 * fw-zynq/app/src/la_mem_geom.h and fw-zynq/lscript.ld) and every 1 MiB
 * MMU section of the window is programmed Normal Non-cacheable so the
 * capture payload and SG descriptors the DMA writes later are coherent
 * without cache maintenance.
 */

#include "FreeRTOS.h"
#include "la_mem_geom.h"
#include "task.h"
#include "xil_mmu.h"
#include "xil_printf.h"

#define LA_PRIORITY (tskIDLE_PRIORITY + 1)

extern uint8_t __la_fw_end[];
extern uint8_t __la_nc_base[];

static void la_program_noncached(void *context, uintptr_t section_base)
{
    (void)context;
    Xil_SetTlbAttributes((INTPTR)section_base, NORM_NONCACHE);
}

static TaskHandle_t hello_handle;
static StackType_t  hello_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t hello_tcb;

static TaskHandle_t probe_handle;
static StackType_t  probe_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t probe_tcb;

static void la_hello_task(void *pvParameters)
{
    uint32_t count = 0;
    (void)pvParameters;
    xil_printf("LA P4.1 FreeRTOS hello on UART1\r\n");
    for (;;) {
        xil_printf("LA hello tick %lu\r\n", (unsigned long)count);
        count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void la_probe_task(void *pvParameters)
{
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(5000));
    xil_printf("LA probe: hello stack high-water %lu/%lu words\r\n",
               (unsigned long)uxTaskGetStackHighWaterMark(hello_handle),
               (unsigned long)configMINIMAL_STACK_SIZE);
    vTaskDelete(NULL);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t timer_tcb;
    static StackType_t  timer_stack[configTIMER_TASK_STACK_DEPTH];
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
    if (la_mem_geometry_check((uintptr_t)__la_fw_end) != LA_MEM_CHECK_OK) {
        xil_printf("LA fatal: memory geometry check failed\r\n");
        for (;;) {
        }
    }
    la_mem_apply_noncached(la_program_noncached, NULL);
    hello_handle = xTaskCreateStatic(la_hello_task, "la_hello",
                                     configMINIMAL_STACK_SIZE, NULL,
                                     LA_PRIORITY, hello_stack, &hello_tcb);
    probe_handle = xTaskCreateStatic(la_probe_task, "la_probe",
                                     configMINIMAL_STACK_SIZE, NULL,
                                     LA_PRIORITY + 1, probe_stack, &probe_tcb);
    if (hello_handle == NULL || probe_handle == NULL) {
        xil_printf("LA fatal: task creation failed\r\n");
        for (;;) {
        }
    }
    vTaskStartScheduler();
    for (;;) {
    }
}
