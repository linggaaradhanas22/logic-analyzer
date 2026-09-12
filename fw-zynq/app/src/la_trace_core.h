#ifndef LA_TRACE_CORE_H
#define LA_TRACE_CORE_H

/*
 * P8.3 trace ring core (pure logic).
 *
 * A fixed overwriting ring of timestamped event records plus a small object
 * table (tasks and queues) that maps raw handles to readable names. The
 * firmware glue in la_trace.c is the only writer: it takes the timestamp,
 * finds the task number for a TCB and serializes access with the ISR-safe
 * interrupt mask. This core performs no FreeRTOS, Xilinx or allocation calls,
 * so host tests exercise wrap, overwrite, registration and dump ordering.
 *
 * Record ids are raw handles (TCB or queue pointer) because the kernel trace
 * macros only have the handle; the dump emits the object table first so a
 * renderer can resolve names. The `object` field carries the task number for
 * task-switch records (0 when unknown).
 */

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_TRACE_API __declspec(dllexport)
#else
#define LA_TRACE_API
#endif

#define LA_TRACE_CAPACITY 4096u
#define LA_TRACE_OBJECT_MAX 24u
#define LA_TRACE_NAME_MAX 8u

typedef enum la_trace_event {
    LA_TRACE_EVENT_NONE = 0,
    LA_TRACE_EVENT_TASK_IN = 1,   /* id = TCB, object = task number */
    LA_TRACE_EVENT_TASK_OUT = 2,  /* id = TCB, object = task number */
    LA_TRACE_EVENT_QUEUE_SEND = 3,
    LA_TRACE_EVENT_QUEUE_RECV = 4,
    LA_TRACE_EVENT_QUEUE_SEND_FAIL = 5,
    LA_TRACE_EVENT_QUEUE_RECV_FAIL = 6,
    LA_TRACE_EVENT_ISR_PL = 7,      /* PL datapath interrupt entered */
    LA_TRACE_EVENT_ISR_DMA = 8,     /* AXI DMA interrupt entered */
    LA_TRACE_EVENT_MARK_SEGMENT = 9, /* live segment published (value=slot) */
    LA_TRACE_EVENT_MARK_DECODE = 10, /* decoder segment fed (value=slot) */
    LA_TRACE_EVENT_MARK_TX_FRAME = 11, /* UART frame queued (value=type) */
    LA_TRACE_EVENT_MARK_OVERRUN = 12,  /* loss report queued (value=stage) */
    LA_TRACE_EVENT_STREAM_SEND = 13,      /* stream buffer send (value=bytes) */
    LA_TRACE_EVENT_STREAM_RECV = 14,      /* stream buffer receive */
    LA_TRACE_EVENT_STREAM_SEND_FAIL = 15, /* stream buffer send failed */
    LA_TRACE_EVENT_STREAM_RECV_FAIL = 16  /* stream buffer receive failed */
} la_trace_event_t;

typedef enum la_trace_object_kind {
    LA_TRACE_OBJECT_TASK = 1,
    LA_TRACE_OBJECT_QUEUE = 2
} la_trace_object_kind_t;

typedef struct la_trace_record {
    uint64_t time;  /* caller clock (XTime counts on target) */
    uint32_t id;    /* TCB / queue handle / marker value */
    uint8_t event;
    uint8_t object; /* task number for task-switch records, else 0 */
    uint16_t value; /* event-specific */
} la_trace_record_t;

typedef struct la_trace_object {
    uint32_t id;
    uint8_t kind;
    uint8_t number;
    char name[LA_TRACE_NAME_MAX];
} la_trace_object_t;

typedef struct la_trace_core {
    volatile uint8_t enabled;
    uint32_t head;  /* next write index */
    uint32_t total; /* records ever accepted */
    uint8_t object_count;
    la_trace_object_t objects[LA_TRACE_OBJECT_MAX];
    la_trace_record_t records[LA_TRACE_CAPACITY];
} la_trace_core_t;

LA_TRACE_API void la_trace_core_init(la_trace_core_t *trace);

/* Clears the ring and enables recording. Registration survives. */
LA_TRACE_API void la_trace_core_start(la_trace_core_t *trace);

/* Disables recording; the ring keeps its records for the next dump. */
LA_TRACE_API void la_trace_core_stop(la_trace_core_t *trace);

LA_TRACE_API int la_trace_core_enabled(const la_trace_core_t *trace);

/* Registers or replaces an object entry by id. name is truncated to
 * LA_TRACE_NAME_MAX-1 characters. Returns 0, or -1 on invalid arguments or a
 * full table. */
LA_TRACE_API int la_trace_core_register(la_trace_core_t *trace,
                                        uint32_t id,
                                        uint8_t kind,
                                        uint8_t number,
                                        const char *name);

/* Task number for a registered TCB, or 0 when unknown. */
LA_TRACE_API uint8_t la_trace_core_task_number(const la_trace_core_t *trace,
                                               uint32_t id);

/* Appends one record when enabled. Returns 0, or -1 when disabled or on
 * invalid arguments. When the ring is full the oldest record is overwritten
 * and total keeps counting. */
LA_TRACE_API int la_trace_core_record(la_trace_core_t *trace,
                                      uint8_t event,
                                      uint32_t id,
                                      uint8_t object,
                                      uint16_t value,
                                      uint64_t time);

/* Number of valid records (min(total, capacity)). */
LA_TRACE_API uint32_t la_trace_core_count(const la_trace_core_t *trace);

/* Total accepted records since start (may exceed count after wrap). */
LA_TRACE_API uint32_t la_trace_core_total(const la_trace_core_t *trace);

/* Records lost to the wrap (total - count). */
LA_TRACE_API uint32_t la_trace_core_dropped(const la_trace_core_t *trace);

/* Record index 0 is the oldest valid record. Returns NULL when out of range. */
LA_TRACE_API const la_trace_record_t *la_trace_core_record_at(const la_trace_core_t *trace,
                                                              uint32_t index);

LA_TRACE_API const la_trace_object_t *la_trace_core_object_at(const la_trace_core_t *trace,
                                                              uint32_t index);

#endif /* LA_TRACE_CORE_H */
