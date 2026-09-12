#ifndef LA_TRACE_H
#define LA_TRACE_H

/*
 * P8.3 firmware trace glue.
 *
 * Owns the single trace ring and the FreeRTOS entry points that
 * FreeRTOSConfig.h wires into the kernel (task switches and queue
 * send/receive, including the ISR variants). Application code adds markers
 * for events the kernel does not see (ISR entry, live segment publication,
 * decoder feed, UART frame transmission). Every entry point is a no-op while
 * tracing is stopped, so the default firmware carries only a load and branch
 * per hook.
 *
 * The dump emits text lines through a callback; CmdTask turns each line into
 * a LOG frame. The first lines are the object table (`TASK n id name`,
 * `QUEUE id name`), then one `REC time token object value id` line per
 * record (oldest first) and a final `END count total dropped` line.
 * `LA TRACE 1 <counts_per_second>` opens the dump so a renderer can convert
 * raw XTime counts to seconds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "la_trace_core.h"

typedef void (*la_trace_emit_fn)(void *context, const char *line);

/* Zeroes the ring and the object table. Call before registering objects. */
void la_trace_init(void);

/* Clears the ring and enables recording; registration survives. */
void la_trace_start(void);

/* Disables recording (the ring keeps its records for the next dump). */
void la_trace_stop(void);

int la_trace_enabled(void);

/* Registers a task or queue handle for the dump's name table. number is the
 * task number shown in task-switch records; it is ignored for queues. */
void la_trace_register_task(TaskHandle_t task, uint8_t number, const char *name);
void la_trace_register_queue(void *queue, const char *name);

/* FreeRTOS trace-macro entry points (FreeRTOSConfig.h). */
void la_trace_task_switch(void *tcb, unsigned int in);
void la_trace_queue(void *queue, unsigned int event);
void la_trace_stream(void *stream, unsigned int event, unsigned int bytes);

/* Application markers. */
void la_trace_mark(uint8_t event, uint16_t value);

/* Stops tracing, then emits the whole ring through emit (never NULL) and
 * returns the number of records dumped. */
uint32_t la_trace_dump(la_trace_emit_fn emit, void *context);

#endif /* LA_TRACE_H */
