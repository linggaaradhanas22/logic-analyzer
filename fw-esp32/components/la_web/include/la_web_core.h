#ifndef LA_WEB_CORE_H
#define LA_WEB_CORE_H

/*
 * P10.1 live web view: pure formatting core.
 *
 * Turns one EVT_DECODED payload (a sequence of la_proto event records) into a
 * compact JSON message for the browser:
 *
 *   {"s":<slot>,"e":[[<t_sample>,<kind>,"<hex data>"], ...]}
 *
 * The core is free of ESP-IDF dependencies so it is host-tested with the
 * mocked ESP32 targets (tests/native/esp32_web_host_tests.c). The server layer
 * (la_web_server.c) owns the HTTP/WebSocket glue and calls this from the
 * LiveViewTask.
 */

#include <stddef.h>
#include <stdint.h>

#define LA_WEB_CORE_SLOTS 3u
#define LA_WEB_CORE_MAX_DATA 16u

typedef struct la_web_core {
    char *buffer;
    size_t capacity;
    size_t length;
    uint32_t events;
    uint32_t slots[LA_WEB_CORE_SLOTS];
    uint32_t truncated;
} la_web_core_t;

/* Binds the core to a caller-owned text buffer and resets the counters. */
void la_web_core_init(la_web_core_t *core, char *buffer, size_t capacity);

/* Opens the message for one decoder slot. Returns 0, or -1 when the slot is
 * out of range or the buffer is exhausted. */
int la_web_core_begin(la_web_core_t *core, uint8_t slot);

/* Appends one event record (data is hex-encoded). Returns 0, or -1 when the
 * buffer cannot hold the record. */
int la_web_core_event(la_web_core_t *core, uint8_t kind, uint64_t t_sample,
                      const uint8_t *data, uint8_t length);

/* Closes the message. Returns 0 when the whole message fit, -1 when any part
 * was truncated (the caller must not send a partial message). */
int la_web_core_end(la_web_core_t *core);

#endif /* LA_WEB_CORE_H */
