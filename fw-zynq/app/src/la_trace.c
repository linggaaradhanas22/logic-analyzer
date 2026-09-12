#include "la_trace.h"

#include "xiltimer.h"

#include <stdio.h>
#include <string.h>

static la_trace_core_t s_trace;

/* Serialize ring writes with the CPU's own IRQ-disable bit. The FreeRTOS
 * port's ulPortSetInterruptMask()/vPortClearInterruptMask() pair must NOT be
 * used here: vPortClearInterruptMask() executes `cpsie i` unconditionally
 * when the mask was changed, so calling it from inside the SWI/IRQ context
 * switch re-enables interrupts mid-switch and lets a nested IRQ save the
 * handler's context into the selected task's TCB (observed as a corrupted
 * pxTopOfStack and a data abort). Saving and restoring CPSR.I keeps an
 * exception context masked and leaves a task's interrupt state untouched;
 * the GIC priority mask of any enclosing critical section is not modified. */
static uint32_t trace_irq_lock(void)
{
    uint32_t cpsr;

    __asm volatile ("mrs %0, cpsr" : "=r" (cpsr));
    __asm volatile ("cpsid i" ::: "memory");
    return cpsr;
}

static void trace_irq_unlock(uint32_t cpsr)
{
    if ((cpsr & 0x80u) == 0u) {
        __asm volatile ("cpsie i" ::: "memory");
    }
}

static void trace_append(uint8_t event, uint32_t id, uint8_t object,
                         uint16_t value)
{
    uint32_t cpsr;
    XTime now;

    if (s_trace.enabled == 0u) {
        return;
    }
    cpsr = trace_irq_lock();
    if (s_trace.enabled != 0u) {
        XTime_GetTime(&now);
        (void)la_trace_core_record(&s_trace, event, id, object, value,
                                   (uint64_t)now);
    }
    trace_irq_unlock(cpsr);
}

static const char *event_token(uint8_t event)
{
    switch (event) {
    case LA_TRACE_EVENT_TASK_IN:
        return "IN";
    case LA_TRACE_EVENT_TASK_OUT:
        return "OUT";
    case LA_TRACE_EVENT_QUEUE_SEND:
        return "QS";
    case LA_TRACE_EVENT_QUEUE_RECV:
        return "QR";
    case LA_TRACE_EVENT_QUEUE_SEND_FAIL:
        return "QF";
    case LA_TRACE_EVENT_QUEUE_RECV_FAIL:
        return "RX";
    case LA_TRACE_EVENT_ISR_PL:
        return "PL";
    case LA_TRACE_EVENT_ISR_DMA:
        return "DM";
    case LA_TRACE_EVENT_MARK_SEGMENT:
        return "SEG";
    case LA_TRACE_EVENT_MARK_DECODE:
        return "DEC";
    case LA_TRACE_EVENT_MARK_TX_FRAME:
        return "TX";
    case LA_TRACE_EVENT_MARK_OVERRUN:
        return "OVR";
    case LA_TRACE_EVENT_STREAM_SEND:
        return "SS";
    case LA_TRACE_EVENT_STREAM_RECV:
        return "SR";
    case LA_TRACE_EVENT_STREAM_SEND_FAIL:
        return "SF";
    case LA_TRACE_EVENT_STREAM_RECV_FAIL:
        return "SE";
    default:
        return "?";
    }
}

void la_trace_init(void)
{
    la_trace_core_init(&s_trace);
}

void la_trace_start(void)
{
    uint32_t cpsr = trace_irq_lock();

    la_trace_core_start(&s_trace);
    trace_irq_unlock(cpsr);
}

void la_trace_stop(void)
{
    la_trace_core_stop(&s_trace);
}

int la_trace_enabled(void)
{
    return la_trace_core_enabled(&s_trace);
}

void la_trace_register_task(TaskHandle_t task, uint8_t number, const char *name)
{
    if (task == NULL) {
        return;
    }
    (void)la_trace_core_register(&s_trace, (uint32_t)(uintptr_t)task,
                                 LA_TRACE_OBJECT_TASK, number, name);
}

void la_trace_register_queue(void *queue, const char *name)
{
    if (queue == NULL) {
        return;
    }
    (void)la_trace_core_register(&s_trace, (uint32_t)(uintptr_t)queue,
                                 LA_TRACE_OBJECT_QUEUE, 0u, name);
}

void la_trace_task_switch(void *tcb, unsigned int in)
{
    uint32_t id = (uint32_t)(uintptr_t)tcb;
    uint8_t number;
    uint32_t cpsr;
    XTime now;

    if (s_trace.enabled == 0u || tcb == NULL) {
        return;
    }
    cpsr = trace_irq_lock();
    number = la_trace_core_task_number(&s_trace, id);
    if (number == 0u) {
        /* The idle and timer tasks are created inside the scheduler, so an
         * unknown TCB is registered lazily with its kernel name; 0xFF marks
         * the entry as resolved (task numbers 1..7 are the app tasks). */
        const char *name = pcTaskGetName((TaskHandle_t)tcb);

        if (name != NULL &&
            la_trace_core_register(&s_trace, id, LA_TRACE_OBJECT_TASK, 0xFFu,
                                   name) == 0) {
            number = 0xFFu;
        }
    }
    if (s_trace.enabled != 0u) {
        XTime_GetTime(&now);
        (void)la_trace_core_record(
            &s_trace,
            in != 0u ? LA_TRACE_EVENT_TASK_IN : LA_TRACE_EVENT_TASK_OUT,
            id, number, 0u, (uint64_t)now);
    }
    trace_irq_unlock(cpsr);
}

void la_trace_queue(void *queue, unsigned int event)
{
    uint8_t mapped;

    switch (event) {
    case 1u:
        mapped = LA_TRACE_EVENT_QUEUE_SEND;
        break;
    case 2u:
        mapped = LA_TRACE_EVENT_QUEUE_RECV;
        break;
    case 3u:
        mapped = LA_TRACE_EVENT_QUEUE_SEND_FAIL;
        break;
    default:
        mapped = LA_TRACE_EVENT_QUEUE_RECV_FAIL;
        break;
    }
    trace_append(mapped, (uint32_t)(uintptr_t)queue, 0u, 0u);
}

void la_trace_stream(void *stream, unsigned int event, unsigned int bytes)
{
    uint8_t mapped;
    uint16_t value;

    switch (event) {
    case 1u:
        mapped = LA_TRACE_EVENT_STREAM_SEND;
        break;
    case 2u:
        mapped = LA_TRACE_EVENT_STREAM_RECV;
        break;
    case 3u:
        mapped = LA_TRACE_EVENT_STREAM_SEND_FAIL;
        break;
    default:
        mapped = LA_TRACE_EVENT_STREAM_RECV_FAIL;
        break;
    }
    value = bytes > 0xFFFFu ? 0xFFFFu : (uint16_t)bytes;
    trace_append(mapped, (uint32_t)(uintptr_t)stream, 0u, value);
}

void la_trace_mark(uint8_t event, uint16_t value)
{
    trace_append(event, 0u, 0u, value);
}

uint32_t la_trace_dump(la_trace_emit_fn emit, void *context)
{
    char line[80];
    uint32_t count;
    uint32_t total;
    uint32_t index;
    uint32_t cpsr;

    if (emit == NULL) {
        return 0u;
    }
    /* Stop and snapshot under the lock: after this, no in-flight writer can
     * append, so the ring can be formatted outside the critical section. */
    cpsr = trace_irq_lock();
    la_trace_core_stop(&s_trace);
    count = la_trace_core_count(&s_trace);
    total = la_trace_core_total(&s_trace);
    trace_irq_unlock(cpsr);

    (void)snprintf(line, sizeof(line), "LA TRACE 1 %u",
                   (unsigned)(COUNTS_PER_SECOND));
    emit(context, line);
    for (index = 0u; index < s_trace.object_count; ++index) {
        const la_trace_object_t *object = &s_trace.objects[index];

        if (object->kind == LA_TRACE_OBJECT_TASK) {
            (void)snprintf(line, sizeof(line), "TASK %u %08X %s",
                           (unsigned)object->number, (unsigned)object->id,
                           object->name);
        } else {
            (void)snprintf(line, sizeof(line), "QUEUE %08X %s",
                           (unsigned)object->id, object->name);
        }
        emit(context, line);
    }
    for (index = 0u; index < count; ++index) {
        const la_trace_record_t *record = la_trace_core_record_at(&s_trace, index);

        if (record == NULL) {
            break;
        }
        (void)snprintf(line, sizeof(line), "REC %llu %s %u %u %08X",
                       (unsigned long long)record->time,
                       event_token(record->event), (unsigned)record->object,
                       (unsigned)record->value, (unsigned)record->id);
        emit(context, line);
    }
    (void)snprintf(line, sizeof(line), "END %u %u %u", (unsigned)count,
                   (unsigned)total, (unsigned)la_trace_core_dropped(&s_trace));
    emit(context, line);
    return count;
}
