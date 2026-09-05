#include "la_proto.h"

#include <stdio.h>
#include <string.h>

typedef struct byte_ring {
    uint8_t bytes[64];
    size_t read_index;
    size_t write_index;
    size_t count;
} byte_ring_t;

static int ring_push(byte_ring_t *ring, const uint8_t *data, size_t length)
{
    size_t index;

    if (ring == NULL || data == NULL || length > sizeof(ring->bytes) - ring->count) {
        return -1;
    }
    for (index = 0u; index < length; ++index) {
        ring->bytes[ring->write_index] = data[index];
        ring->write_index = (ring->write_index + 1u) % sizeof(ring->bytes);
    }
    ring->count += length;
    return 0;
}

static int ring_pop(byte_ring_t *ring, uint8_t *data, size_t length)
{
    size_t index;

    if (ring == NULL || data == NULL || length > ring->count) {
        return -1;
    }
    for (index = 0u; index < length; ++index) {
        data[index] = ring->bytes[ring->read_index];
        ring->read_index = (ring->read_index + 1u) % sizeof(ring->bytes);
    }
    ring->count -= length;
    return 0;
}

int main(void)
{
    static const uint8_t payload[] = {0x10u, 0x20u, 0x30u};
    byte_ring_t ring = {0};
    uint8_t frame[32];
    uint8_t round_trip[sizeof(frame)];
    size_t frame_length = la_proto_encode(0x01u, 0x02u, payload, (uint16_t)sizeof(payload),
                                          frame, sizeof(frame));

    if (frame_length == 0u || ring_push(&ring, frame, frame_length) != 0 ||
        ring_pop(&ring, round_trip, frame_length) != 0 ||
        memcmp(frame, round_trip, frame_length) != 0 || ring.count != 0u) {
        fprintf(stderr, "FAIL: ESP32 UART/ring-buffer mock\n");
        return 1;
    }
    puts("PASS: esp32-mocks");
    return 0;
}
