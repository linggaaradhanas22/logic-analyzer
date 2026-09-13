#include "la_spi_link.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                    \
    do {                                             \
        if (!(condition)) {                          \
            fprintf(stderr, "FAIL: %s\n", message);  \
            return 1;                                \
        }                                            \
    } while (0)

#define FAKE_BASE UINT32_C(0x40800000)
#define FAKE_DEPTH 8u

/* Fake AXI Quad SPI slave: enough of the register semantics to exercise the
 * driver. TX/RX FIFOs are byte arrays; SPISR is computed; IPISR is
 * write-1-to-clear; SRR resets the model. The SLAVE_MODE bit is set because
 * the real IP is hard-configured as a slave. */
typedef struct fake_spi {
    uint32_t spicr;
    uint32_t ipisr;
    uint32_t ipier;
    uint32_t dgier;
    uint32_t sr_errors;  /* injected SPISR error bits */
    uint32_t master_mode; /* 1 = suppress the slave-mode select bit */
    uint8_t tx[FAKE_DEPTH];
    size_t tx_count;
    uint8_t rx[FAKE_DEPTH];
    size_t rx_count;
} fake_spi_t;

static fake_spi_t fake;

static void fake_reset_model(void)
{
    memset(&fake, 0, sizeof(fake));
}

static void fake_rx_inject(uint8_t byte)
{
    if (fake.rx_count < FAKE_DEPTH) {
        fake.rx[fake.rx_count++] = byte;
    }
}

static uint32_t fake_status(void)
{
    uint32_t status = fake.sr_errors;

    if (fake.master_mode == 0u) {
        status |= LA_SPI_SR_SLAVE_MODE;
    }

    if (fake.rx_count == 0u) {
        status |= LA_SPI_SR_RX_EMPTY;
    }
    if (fake.rx_count >= FAKE_DEPTH) {
        status |= LA_SPI_SR_RX_FULL;
    }
    if (fake.tx_count == 0u) {
        status |= LA_SPI_SR_TX_EMPTY;
    }
    if (fake.tx_count >= FAKE_DEPTH) {
        status |= LA_SPI_SR_TX_FULL;
    }
    return status;
}

static uint32_t fake_read(uintptr_t address)
{
    uintptr_t offset = address - FAKE_BASE;

    switch (offset) {
    case LA_SPI_SPICR_OFFSET:
        return fake.spicr;
    case LA_SPI_SPISR_OFFSET:
        return fake_status();
    case LA_SPI_SPIDRR_OFFSET:
        if (fake.rx_count == 0u) {
            return 0u;
        }
        {
            uint8_t byte = fake.rx[0];
            memmove(&fake.rx[0], &fake.rx[1], fake.rx_count - 1u);
            fake.rx_count--;
            return byte;
        }
    case LA_SPI_TFO_OFFSET:
        return fake.tx_count == 0u ? 0u : (uint32_t)(fake.tx_count - 1u);
    case LA_SPI_RFO_OFFSET:
        return fake.rx_count == 0u ? 0u : (uint32_t)(fake.rx_count - 1u);
    case LA_SPI_IPISR_OFFSET:
        return fake.ipisr;
    case LA_SPI_IPIER_OFFSET:
        return fake.ipier;
    case LA_SPI_DGIER_OFFSET:
        return fake.dgier;
    default:
        return 0u;
    }
}

static void fake_write(uintptr_t address, uint32_t value)
{
    uintptr_t offset = address - FAKE_BASE;

    switch (offset) {
    case LA_SPI_SRR_OFFSET:
        if (value == LA_SPI_SRR_RESET) {
            fake_reset_model();
        }
        break;
    case LA_SPI_SPICR_OFFSET:
        fake.spicr = value;
        if ((value & LA_SPI_CR_TX_FIFO_RESET) != 0u) {
            fake.tx_count = 0u;
        }
        if ((value & LA_SPI_CR_RX_FIFO_RESET) != 0u) {
            fake.rx_count = 0u;
        }
        /* The FIFO reset bits are self-clearing. */
        fake.spicr &= ~(LA_SPI_CR_TX_FIFO_RESET | LA_SPI_CR_RX_FIFO_RESET);
        break;
    case LA_SPI_SPIDTR_OFFSET:
        if (fake.tx_count < FAKE_DEPTH) {
            fake.tx[fake.tx_count++] = (uint8_t)(value & 0xFFu);
        }
        break;
    case LA_SPI_IPISR_OFFSET:
        fake.ipisr &= ~value; /* write-1-to-clear */
        break;
    case LA_SPI_IPIER_OFFSET:
        fake.ipier = value;
        break;
    case LA_SPI_DGIER_OFFSET:
        fake.dgier = value;
        break;
    default:
        break;
    }
}

static la_spi_dev_t make_dev(void)
{
    la_spi_dev_t dev;

    la_spi_dev_init(&dev, FAKE_BASE, fake_read, fake_write);
    return dev;
}

static int test_reset_and_start(void)
{
    la_spi_dev_t dev;

    fake_reset_model();
    fake.spicr = LA_SPI_CR_ENABLE | LA_SPI_CR_MASTER;
    fake.tx_count = 2u;
    fake.rx_count = 3u;
    dev = make_dev();
    CHECK(la_spi_reset(&dev) == 0, "reset completes");
    CHECK(fake.spicr == 0u, "reset clears SPICR");
    CHECK(fake.tx_count == 0u && fake.rx_count == 0u, "reset clears FIFOs");
    CHECK(la_spi_start(&dev) == 0, "start completes");
    CHECK((fake.spicr & LA_SPI_CR_ENABLE) != 0u, "start sets SPE");
    CHECK((fake.spicr & LA_SPI_CR_MASTER) == 0u, "start keeps slave mode");
    CHECK((fake.spicr & (LA_SPI_CR_TX_FIFO_RESET | LA_SPI_CR_RX_FIFO_RESET)) == 0u,
          "start leaves the reset bits clear");
    CHECK(fake.dgier == LA_SPI_DGIER_GIE, "start enables the global interrupt");
    CHECK(la_spi_check(&dev) == LA_SPI_CHECK_OK, "slave mode check");
    return 0;
}

static int test_tx_push_and_full(void)
{
    la_spi_dev_t dev;
    uint8_t index;

    fake_reset_model();
    dev = make_dev();
    CHECK(la_spi_start(&dev) == 0, "start for TX");
    CHECK((la_spi_status(&dev) & LA_SPI_SR_TX_EMPTY) != 0u, "TX empty at start");
    CHECK(la_spi_tx_occupancy(&dev) == 0u, "empty TX occupancy is zero");
    for (index = 0u; index < FAKE_DEPTH; ++index) {
        CHECK(la_spi_tx_push(&dev, (uint8_t)(0x40u + index)) == 0, "TX push room");
    }
    CHECK(la_spi_tx_occupancy(&dev) == FAKE_DEPTH, "TX occupancy full");
    CHECK((la_spi_status(&dev) & LA_SPI_SR_TX_FULL) != 0u, "TX full flag");
    CHECK((la_spi_status(&dev) & LA_SPI_SR_TX_EMPTY) == 0u, "TX not empty");
    CHECK(la_spi_tx_push(&dev, 0x55u) == -1, "TX push rejects when full");
    for (index = 0u; index < FAKE_DEPTH; ++index) {
        CHECK(fake.tx[index] == (uint8_t)(0x40u + index), "TX FIFO order");
    }
    return 0;
}

static int test_rx_pop_and_empty(void)
{
    la_spi_dev_t dev;
    uint8_t byte = 0u;

    fake_reset_model();
    dev = make_dev();
    CHECK(la_spi_start(&dev) == 0, "start for RX");
    CHECK(la_spi_rx_pop(&dev, &byte) == -1, "RX pop rejects when empty");
    fake_rx_inject(0x11u);
    fake_rx_inject(0x22u);
    CHECK(la_spi_rx_occupancy(&dev) == 2u, "RX occupancy");
    CHECK((la_spi_status(&dev) & LA_SPI_SR_RX_EMPTY) == 0u, "RX not empty");
    CHECK(la_spi_rx_pop(&dev, &byte) == 0 && byte == 0x11u, "RX pop order 1");
    CHECK(la_spi_rx_pop(&dev, &byte) == 0 && byte == 0x22u, "RX pop order 2");
    CHECK(la_spi_rx_pop(&dev, &byte) == -1, "RX pop after drain");
    CHECK(la_spi_rx_pop(&dev, NULL) == -1, "RX pop NULL guard");
    return 0;
}

static int test_irq_mask_and_clear(void)
{
    la_spi_dev_t dev;
    uint32_t mask = LA_SPI_ISR_DTR_UNDERRUN | LA_SPI_ISR_DRR_OVERRUN;

    fake_reset_model();
    dev = make_dev();
    fake.ipisr = mask | LA_SPI_ISR_TX_FIFO_HALF_EMPTY;
    CHECK(la_spi_irq_status(&dev) == (mask | LA_SPI_ISR_TX_FIFO_HALF_EMPTY),
          "IRQ status readback");
    la_spi_irq_clear(&dev, mask);
    CHECK(la_spi_irq_status(&dev) == LA_SPI_ISR_TX_FIFO_HALF_EMPTY,
          "IRQ write-1-to-clear");
    la_spi_irq_enable(&dev, LA_SPI_ISR_DTR_UNDERRUN);
    CHECK(fake.ipier == LA_SPI_ISR_DTR_UNDERRUN, "IRQ enable mask");
    return 0;
}

static int test_check_errors(void)
{
    la_spi_dev_t dev;

    fake_reset_model();
    dev = make_dev();
    fake.spicr = LA_SPI_CR_ENABLE;
    fake.sr_errors = LA_SPI_SR_SLAVE_MODE_ERROR;
    CHECK(la_spi_check(&dev) == LA_SPI_CHECK_ERROR_FLAGS, "error flag detected");
    fake.sr_errors = 0u;
    CHECK(la_spi_check(&dev) == LA_SPI_CHECK_OK, "clean slave check");
    fake.spicr = LA_SPI_CR_ENABLE | LA_SPI_CR_MASTER;
    CHECK(la_spi_check(&dev) == LA_SPI_CHECK_NOT_SLAVE, "master mode detected");
    fake.spicr = 0u;
    CHECK(la_spi_check(&dev) == LA_SPI_CHECK_NOT_SLAVE, "disabled core detected");
    CHECK(strcmp(la_spi_check_name(LA_SPI_CHECK_ERROR_FLAGS), "ERROR_FLAGS") == 0,
          "check name");
    CHECK(strcmp(la_spi_check_name(42), "UNKNOWN") == 0, "unknown check name");
    return 0;
}

static int test_stop_preserves_config(void)
{
    la_spi_dev_t dev;

    fake_reset_model();
    dev = make_dev();
    CHECK(la_spi_start(&dev) == 0, "start before stop");
    la_spi_stop(&dev);
    CHECK((fake.spicr & LA_SPI_CR_ENABLE) == 0u, "stop clears SPE");
    CHECK((fake.spicr & LA_SPI_CR_MASTER) == 0u, "stop keeps slave mode");
    return 0;
}

int main(void)
{
    if (test_reset_and_start() != 0 || test_tx_push_and_full() != 0 ||
        test_rx_pop_and_empty() != 0 || test_irq_mask_and_clear() != 0 ||
        test_check_errors() != 0 || test_stop_preserves_config() != 0) {
        return 1;
    }
    puts("PASS: la_native_spi");
    return 0;
}
