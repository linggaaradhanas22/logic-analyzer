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
#define configTIMER_TASK_STACK_DEPTH       (configMINIMAL_STACK_SIZE * 2)
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

#endif /* LA_FREERTOS_CONFIG_H */
