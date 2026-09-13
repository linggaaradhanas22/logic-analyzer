#ifndef LA_SPI_LINK_H
#define LA_SPI_LINK_H

/*
 * P9.1 axi_quad_spi slave driver.
 *
 * Thin, platform-neutral access to the PL AXI Quad SPI slave (docs/spi-link.md)
 * at the AXI-Lite base assigned by fpga/scripts/build.tcl (0x4080_0000). The
 * register map, bit positions and the reset/start sequence come from the
 * installed IP (xilinx.com:ip:axi_quad_spi:3.2, spi_v4_x driver headers and
 * component.xml). The base address and the MMIO primitives are injected per
 * device so the same driver compiles and runs in host tests (fake registers
 * behind function pointers) and on the Zynq (Xil_In32/Xil_Out32 adapters
 * supplied by the caller).
 *
 * The driver only touches the register interface: it does not own rings,
 * tasks or the data-ready EMIO GPIO. The SpiLinkTask adapter builds the frame
 * service on top of this (la_spi_tx_push / la_spi_rx_pop / occupancy / IRQ).
 *
 * Host tests: tests/native/spi_link_host_tests.c (la_native_spi CTest target).
 * On-target verification of the slave transfer is a bench gate (P9.1).
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_SPI_API __declspec(dllexport)
#else
#define LA_SPI_API
#endif

/* AXI Quad SPI v3.2 register offsets (component.xml). */
#define LA_SPI_SRR_OFFSET 0x40u    /* soft reset */
#define LA_SPI_DGIER_OFFSET 0x1Cu  /* device global interrupt enable */
#define LA_SPI_IPISR_OFFSET 0x20u  /* interrupt status (write-1-to-clear) */
#define LA_SPI_IPIER_OFFSET 0x28u  /* interrupt enable */
#define LA_SPI_SPICR_OFFSET 0x60u  /* control */
#define LA_SPI_SPISR_OFFSET 0x64u  /* status */
#define LA_SPI_SPIDTR_OFFSET 0x68u /* transmit data */
#define LA_SPI_SPIDRR_OFFSET 0x6Cu /* receive data */
#define LA_SPI_SPISSR_OFFSET 0x70u /* slave select (master only) */
#define LA_SPI_TFO_OFFSET 0x74u    /* transmit FIFO occupancy */
#define LA_SPI_RFO_OFFSET 0x78u    /* receive FIFO occupancy */

#define LA_SPI_SRR_RESET 0x0000000Au

/* SPICR bits. */
#define LA_SPI_CR_LOOPBACK 0x00000001u
#define LA_SPI_CR_ENABLE 0x00000002u
#define LA_SPI_CR_MASTER 0x00000004u
#define LA_SPI_CR_CPOL 0x00000008u
#define LA_SPI_CR_CPHA 0x00000010u
#define LA_SPI_CR_TX_FIFO_RESET 0x00000020u
#define LA_SPI_CR_RX_FIFO_RESET 0x00000040u
#define LA_SPI_CR_MANUAL_SS 0x00000080u
#define LA_SPI_CR_TRANS_INHIBIT 0x00000100u
#define LA_SPI_CR_LSB_FIRST 0x00000200u

/* SPISR bits. */
#define LA_SPI_SR_RX_EMPTY 0x00000001u
#define LA_SPI_SR_RX_FULL 0x00000002u
#define LA_SPI_SR_TX_EMPTY 0x00000004u
#define LA_SPI_SR_TX_FULL 0x00000008u
#define LA_SPI_SR_MODF 0x00000010u
#define LA_SPI_SR_SLAVE_MODE 0x00000020u
#define LA_SPI_SR_CPOL_CPHA_ERROR 0x00000040u
#define LA_SPI_SR_SLAVE_MODE_ERROR 0x00000080u
#define LA_SPI_SR_MSB_ERROR 0x00000100u
#define LA_SPI_SR_LOOPBACK_ERROR 0x00000200u
#define LA_SPI_SR_COMMAND_ERROR 0x00000400u
#define LA_SPI_SR_ERROR_MASK                                                    \
    (LA_SPI_SR_MODF | LA_SPI_SR_CPOL_CPHA_ERROR | LA_SPI_SR_SLAVE_MODE_ERROR |  \
     LA_SPI_SR_MSB_ERROR | LA_SPI_SR_LOOPBACK_ERROR | LA_SPI_SR_COMMAND_ERROR)

/* IPISR / IPIER bits. */
#define LA_SPI_ISR_MODF 0x0001u
#define LA_SPI_ISR_SLAVE_MODF 0x0002u
#define LA_SPI_ISR_DTR_EMPTY 0x0004u
#define LA_SPI_ISR_DTR_UNDERRUN 0x0008u
#define LA_SPI_ISR_DRR_FULL 0x0010u
#define LA_SPI_ISR_DRR_OVERRUN 0x0020u
#define LA_SPI_ISR_TX_FIFO_HALF_EMPTY 0x0040u
#define LA_SPI_ISR_SLAVE_SELECT_MODE 0x0080u
#define LA_SPI_ISR_DRR_NOT_EMPTY 0x0100u
#define LA_SPI_ISR_CPOL_CPHA_ERROR 0x0200u
#define LA_SPI_ISR_SLAVE_MODE_ERROR 0x0400u
#define LA_SPI_ISR_MSB_ERROR 0x0800u
#define LA_SPI_ISR_LOOPBACK_ERROR 0x1000u
#define LA_SPI_ISR_COMMAND_ERROR 0x2000u
#define LA_SPI_ISR_ERROR_MASK                                                   \
    (LA_SPI_ISR_MODF | LA_SPI_ISR_SLAVE_MODF | LA_SPI_ISR_DTR_UNDERRUN |        \
     LA_SPI_ISR_DRR_OVERRUN | LA_SPI_ISR_CPOL_CPHA_ERROR |                      \
     LA_SPI_ISR_SLAVE_MODE_ERROR | LA_SPI_ISR_MSB_ERROR |                       \
     LA_SPI_ISR_LOOPBACK_ERROR | LA_SPI_ISR_COMMAND_ERROR)

/* TFO/RFO occupancy field. */
#define LA_SPI_FIFO_OCCUPANCY_MASK 0x0000007Fu

typedef uint32_t (*la_spi_read32_fn)(uintptr_t address);
typedef void (*la_spi_write32_fn)(uintptr_t address, uint32_t value);

typedef struct la_spi_dev {
    uintptr_t base;
    la_spi_read32_fn read32;
    la_spi_write32_fn write32;
} la_spi_dev_t;

typedef enum la_spi_check_result {
    LA_SPI_CHECK_OK = 0,
    LA_SPI_CHECK_NOT_SLAVE = 1,
    LA_SPI_CHECK_ERROR_FLAGS = 2
} la_spi_check_result_t;

LA_SPI_API void la_spi_dev_init(la_spi_dev_t *dev, uintptr_t base,
                                la_spi_read32_fn read32,
                                la_spi_write32_fn write32);

LA_SPI_API uint32_t la_spi_read32(const la_spi_dev_t *dev, uintptr_t offset);
LA_SPI_API void la_spi_write32(const la_spi_dev_t *dev, uintptr_t offset,
                               uint32_t value);

/* Soft reset (SRR = 0x0A). Returns 0 when the control register reads back
 * cleared within the bounded poll, -1 otherwise. */
LA_SPI_API int la_spi_reset(const la_spi_dev_t *dev);

/* Enable the slave in SPI mode 0 (SPE=1, master bit clear) and reset both
 * FIFOs, waiting for the self-clearing reset bits. Returns 0 on success. */
LA_SPI_API int la_spi_start(const la_spi_dev_t *dev);

/* Disable the core (SPE=0) without touching the FIFOs. */
LA_SPI_API void la_spi_stop(const la_spi_dev_t *dev);

LA_SPI_API uint32_t la_spi_status(const la_spi_dev_t *dev);

/* Push one byte into the transmit FIFO. Returns 0, or -1 when TX is full. */
LA_SPI_API int la_spi_tx_push(const la_spi_dev_t *dev, uint8_t byte);

/* Pop one byte from the receive FIFO. Returns 0, or -1 when RX is empty. */
LA_SPI_API int la_spi_rx_pop(const la_spi_dev_t *dev, uint8_t *byte);

LA_SPI_API uint32_t la_spi_tx_occupancy(const la_spi_dev_t *dev);
LA_SPI_API uint32_t la_spi_rx_occupancy(const la_spi_dev_t *dev);

LA_SPI_API uint32_t la_spi_irq_status(const la_spi_dev_t *dev);
LA_SPI_API void la_spi_irq_clear(const la_spi_dev_t *dev, uint32_t mask);
LA_SPI_API void la_spi_irq_enable(const la_spi_dev_t *dev, uint32_t mask);

/* Configuration self-check: the core must report slave mode (SPISR bit 5) and
 * no latched error flags. Returns a la_spi_check_result_t. */
LA_SPI_API int la_spi_check(const la_spi_dev_t *dev);

LA_SPI_API const char *la_spi_check_name(int result);

#endif /* LA_SPI_LINK_H */
