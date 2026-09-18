/*
 * Link-only FreeRTOS trace-hook stubs for the `example` profile.
 *
 * The platform BSP policy fragment (fw-zynq/FreeRTOSConfig.h) wires the
 * kernel trace macros to la_trace_* entry points that live in the product
 * application. The vendor mass-storage example never starts the scheduler,
 * so these hooks are unreachable; the definitions exist only to satisfy the
 * kernel object references at link time.
 */

void la_trace_task_switch(void *tcb, unsigned int in)
{
    (void)tcb;
    (void)in;
}

void la_trace_queue(void *queue, unsigned int event)
{
    (void)queue;
    (void)event;
}

void la_trace_stream(void *stream, unsigned int event, unsigned int bytes)
{
    (void)stream;
    (void)event;
    (void)bytes;
}
