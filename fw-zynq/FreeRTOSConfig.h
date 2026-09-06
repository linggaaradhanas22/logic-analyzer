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
#define configSUPPORT_DYNAMIC_ALLOCATION   0
#define configUSE_TIMERS                   1
#define configTIMER_TASK_STACK_DEPTH       (configMINIMAL_STACK_SIZE * 2)
#define configUSE_TRACE_FACILITY           1
/* Run-time statistics need a validated A9 timer counter scheme; enable at
 * the P8 measurement phase, not before the first hello build is verified. */
#define configGENERATE_RUN_TIME_STATS      0
#define configCHECK_FOR_STACK_OVERFLOW     2
#define configUSE_MALLOC_FAILED_HOOK       1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define configASSERT(x)                    do { if (!(x)) { for (;;) {} } } while (0)

#endif /* LA_FREERTOS_CONFIG_H */
