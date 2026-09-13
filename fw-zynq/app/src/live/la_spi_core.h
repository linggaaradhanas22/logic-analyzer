#ifndef LA_SPI_CORE_H
#define LA_SPI_CORE_H

/*
 * P9.1 SPI link frame ring core (pure logic, portable).
 *
 * Buffers whole encoded la_proto frames for the axi_quad_spi slave TX FIFO.
 * The producer (TxTask, or the boot self-test) pushes complete frames; the
 * SpiLinkTask adapter feeds bytes into the 256-byte hardware FIFO as it
 * drains, so a frame can outlive many FIFO refills. Frames are pushed
 * all-or-nothing and fed in order; the ring compacts when the free space is
 * split across the wrap boundary. The adapter owns the hardware and calls
 * la_spi_core_note_underrun() when the IP reports DTR underrun.
 *
 * No FreeRTOS/ESP-IDF/Xilinx calls; host tests live in
 * tests/native/spi_core_host_tests.c (la_native_spi_core CTest target).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_SPI_CORE_API __declspec(dllexport)
#else
#define LA_SPI_CORE_API
#endif

typedef struct la_spi_core {
    uint8_t *buffer;
    size_t capacity;
    size_t head; /* read index */
    size_t tail; /* write index */
    size_t used; /* bytes buffered */
    uint32_t frames_queued;
    uint32_t frames_dropped; /* push rejected: not enough room */
    uint32_t bytes_fed;
    uint32_t underruns; /* DTR underruns reported by the adapter */
} la_spi_core_t;

LA_SPI_CORE_API void la_spi_core_init(la_spi_core_t *core, uint8_t *buffer,
                                      size_t capacity);

LA_SPI_CORE_API size_t la_spi_core_space(const la_spi_core_t *core);
LA_SPI_CORE_API size_t la_spi_core_used(const la_spi_core_t *core);
LA_SPI_CORE_API int la_spi_core_ready(const la_spi_core_t *core);

/* Pushes one complete frame. Returns 0 on success; -1 on invalid arguments or
 * when the frame does not fit (frames_dropped increments). */
LA_SPI_CORE_API int la_spi_core_push(la_spi_core_t *core, const uint8_t *frame,
                                     size_t length);

/* Copies up to max_bytes buffered bytes into out and returns the count. */
LA_SPI_CORE_API size_t la_spi_core_feed(la_spi_core_t *core, uint8_t *out,
                                        size_t max_bytes);

LA_SPI_CORE_API void la_spi_core_note_underrun(la_spi_core_t *core);

#endif /* LA_SPI_CORE_H */
