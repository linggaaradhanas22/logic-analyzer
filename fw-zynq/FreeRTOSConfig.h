/*
 * P4.1 FreeRTOS policy overrides for the la_zynq BSP.
 *
 * This fragment is APPENDED by fw-zynq/scripts/platform.py to the BSP's
 * generated FreeRTOSConfig.h (the BSP owns every other value, including
 * configCPU_CLOCK_HZ derived from xparameters). Only the policy knobs below
 * are overridden, so the fragment stays valid across BSP regenerations.
 * The original generated header is kept next to it as
 * FreeRTOSConfig.h.bsp-orig for diff evidence.
 */
#ifndef LA_FREERTOS_CONFIG_H
#define LA_FREERTOS_CONFIG_H

#define configTICK_RATE_HZ                 (1000U)
#define configSUPPORT_STATIC_ALLOCATION    1
/* Dynamic allocation stays enabled because the BSP kernel source set
 * compiles heap_4.c unconditionally and FreeRTOS.h #errors when
 * configSUPPORT_DYNAMIC_ALLOCATION is 0; the la tasks are all created
 * statically and idle/timer tasks use the static memory hooks, so the
 * heap remains unused by policy. */
#define configSUPPORT_DYNAMIC_ALLOCATION   1
#define configUSE_TIMERS                   1
/* The ARM_CA9 port runs every ISR on the interrupted task's stack (there is
 * no dedicated ISR stack), so the idle and timer tasks must also cover the
 * UART/DMA handler worst case. Raised after a capture-time overflow in the
 * P4/P5 bench session. */
#undef configMINIMAL_STACK_SIZE
#define configMINIMAL_STACK_SIZE           (512U)
#undef configTIMER_TASK_STACK_DEPTH
#define configTIMER_TASK_STACK_DEPTH       (512U)
#define configUSE_TRACE_FACILITY           1
/* configUSE_STATS_FORMATTING_FUNCTIONS is off until the P8 statistics
 * phase reintroduces vTaskList-style reporting; run-time statistics need a
 * validated A9 timer counter scheme. Enable both at P8, not before the
 * first hello build is verified. */
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS      0
#define configCHECK_FOR_STACK_OVERFLOW     2
#define configUSE_MALLOC_FAILED_HOOK       1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configASSERT(x)                    do { if (!(x)) { for (;;) {} } } while (0)

/* P8.3 tracing: the kernel compiles these macros into tasks.c/queue.c, so the
 * BSP build cannot include the application header; the entry points are
 * declared here and implemented in fw-zynq/app/src/la_trace.c. They are cheap
 * no-ops while tracing is stopped. See docs/rtos-design.md "P8.3 results". */
extern void la_trace_task_switch(void *tcb, unsigned int in);
extern void la_trace_queue(void *queue, unsigned int event);
extern void la_trace_stream(void *stream, unsigned int event, unsigned int bytes);
#define traceTASK_SWITCHED_IN()    la_trace_task_switch((void *)pxCurrentTCB, 1u)
#define traceTASK_SWITCHED_OUT()   la_trace_task_switch((void *)pxCurrentTCB, 0u)
#define traceQUEUE_SEND(pxQueue)          la_trace_queue((void *)(pxQueue), 1u)
#define traceQUEUE_RECEIVE(pxQueue)       la_trace_queue((void *)(pxQueue), 2u)
#define traceSTREAM_BUFFER_SEND(xStreamBuffer, xBytesSent) \
    la_trace_stream((void *)(xStreamBuffer), 1u, (unsigned int)(xBytesSent))
#define traceSTREAM_BUFFER_RECEIVE(xStreamBuffer, xReceivedLength) \
    la_trace_stream((void *)(xStreamBuffer), 2u, (unsigned int)(xReceivedLength))
/* The *_FAILED hooks are intentionally not wired: tasks poll queues with
 * zero timeouts, so failed polls would flood the ring with idle noise. The
 * glue still supports the tokens if a future build needs them. */

#endif /* LA_FREERTOS_CONFIG_H */
