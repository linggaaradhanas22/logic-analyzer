/* Phase 0 configuration contract; the generated Vitis BSP owns final values. */
#ifndef LA_FREERTOS_CONFIG_H
#define LA_FREERTOS_CONFIG_H

#define configCPU_CLOCK_HZ                 (667000000UL)
#define configTICK_RATE_HZ                 (1000U)
#define configSUPPORT_STATIC_ALLOCATION    1
#define configSUPPORT_DYNAMIC_ALLOCATION   0
#define configUSE_TRACE_FACILITY           1
#define configGENERATE_RUN_TIME_STATS      1
#define configCHECK_FOR_STACK_OVERFLOW     2
#define configASSERT(x)                    do { if (!(x)) { for (;;) {} } } while (0)

#endif /* LA_FREERTOS_CONFIG_H */
