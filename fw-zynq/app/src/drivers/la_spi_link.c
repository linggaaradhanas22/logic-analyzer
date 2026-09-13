#include "la_spi_link.h"

#define LA_SPI_POLL_LIMIT 1000u

void la_spi_dev_init(la_spi_dev_t *dev, uintptr_t base,
                     la_spi_read32_fn read32, la_spi_write32_fn write32)
{
    dev->base = base;
    dev->read32 = read32;
    dev->write32 = write32;
}

uint32_t la_spi_read32(const la_spi_dev_t *dev, uintptr_t offset)
{
    return dev->read32(dev->base + offset);
}

void la_spi_write32(const la_spi_dev_t *dev, uintptr_t offset, uint32_t value)
{
    dev->write32(dev->base + offset, value);
}

int la_spi_reset(const la_spi_dev_t *dev)
{
    uint32_t poll;

    la_spi_write32(dev, LA_SPI_SRR_OFFSET, LA_SPI_SRR_RESET);
    for (poll = 0u; poll < LA_SPI_POLL_LIMIT; ++poll) {
        if (la_spi_read32(dev, LA_SPI_SPICR_OFFSET) == 0u) {
            return 0;
        }
    }
    return -1;
}

int la_spi_start(const la_spi_dev_t *dev)
{
    uint32_t control = la_spi_read32(dev, LA_SPI_SPICR_OFFSET);
    uint32_t poll;

    control &= ~(LA_SPI_CR_MASTER | LA_SPI_CR_LOOPBACK |
                 LA_SPI_CR_TRANS_INHIBIT | LA_SPI_CR_LSB_FIRST |
                 LA_SPI_CR_MANUAL_SS);
    control |= LA_SPI_CR_ENABLE | LA_SPI_CR_TX_FIFO_RESET |
               LA_SPI_CR_RX_FIFO_RESET;
    la_spi_write32(dev, LA_SPI_SPICR_OFFSET, control);
    for (poll = 0u; poll < LA_SPI_POLL_LIMIT; ++poll) {
        uint32_t now = la_spi_read32(dev, LA_SPI_SPICR_OFFSET);
        if ((now & (LA_SPI_CR_TX_FIFO_RESET | LA_SPI_CR_RX_FIFO_RESET)) == 0u) {
            /* The IP only drives ip2intc_irpt while DGIER.GIE is set; the
             * service task depends on the TX half-empty/DRR interrupts to
             * keep the FIFO fed inside a 256-byte master burst. */
            la_spi_write32(dev, LA_SPI_DGIER_OFFSET, LA_SPI_DGIER_GIE);
            return 0;
        }
    }
    return -1;
}

void la_spi_stop(const la_spi_dev_t *dev)
{
    uint32_t control = la_spi_read32(dev, LA_SPI_SPICR_OFFSET);

    la_spi_write32(dev, LA_SPI_SPICR_OFFSET, control & ~LA_SPI_CR_ENABLE);
}

uint32_t la_spi_status(const la_spi_dev_t *dev)
{
    return la_spi_read32(dev, LA_SPI_SPISR_OFFSET);
}

int la_spi_tx_push(const la_spi_dev_t *dev, uint8_t byte)
{
    if ((la_spi_status(dev) & LA_SPI_SR_TX_FULL) != 0u) {
        return -1;
    }
    la_spi_write32(dev, LA_SPI_SPIDTR_OFFSET, (uint32_t)byte);
    return 0;
}

int la_spi_rx_pop(const la_spi_dev_t *dev, uint8_t *byte)
{
    uint32_t value;

    if (byte == NULL || (la_spi_status(dev) & LA_SPI_SR_RX_EMPTY) != 0u) {
        return -1;
    }
    value = la_spi_read32(dev, LA_SPI_SPIDRR_OFFSET);
    *byte = (uint8_t)(value & 0xFFu);
    return 0;
}

uint32_t la_spi_tx_occupancy(const la_spi_dev_t *dev)
{
    if ((la_spi_status(dev) & LA_SPI_SR_TX_EMPTY) != 0u) {
        return 0u;
    }
    return (la_spi_read32(dev, LA_SPI_TFO_OFFSET) &
            LA_SPI_FIFO_OCCUPANCY_MASK) + 1u;
}

uint32_t la_spi_rx_occupancy(const la_spi_dev_t *dev)
{
    if ((la_spi_status(dev) & LA_SPI_SR_RX_EMPTY) != 0u) {
        return 0u;
    }
    return (la_spi_read32(dev, LA_SPI_RFO_OFFSET) &
            LA_SPI_FIFO_OCCUPANCY_MASK) + 1u;
}

uint32_t la_spi_irq_status(const la_spi_dev_t *dev)
{
    return la_spi_read32(dev, LA_SPI_IPISR_OFFSET);
}

void la_spi_irq_clear(const la_spi_dev_t *dev, uint32_t mask)
{
    /* IPISR is write-1-to-clear. */
    la_spi_write32(dev, LA_SPI_IPISR_OFFSET, mask);
}

void la_spi_irq_enable(const la_spi_dev_t *dev, uint32_t mask)
{
    la_spi_write32(dev, LA_SPI_IPIER_OFFSET, mask);
}

int la_spi_check(const la_spi_dev_t *dev)
{
    uint32_t control = la_spi_read32(dev, LA_SPI_SPICR_OFFSET);
    uint32_t status = la_spi_status(dev);

    if ((control & LA_SPI_CR_ENABLE) == 0u ||
        (control & LA_SPI_CR_MASTER) != 0u) {
        return LA_SPI_CHECK_NOT_SLAVE;
    }
    if ((status & LA_SPI_SR_ERROR_MASK) != 0u) {
        return LA_SPI_CHECK_ERROR_FLAGS;
    }
    return LA_SPI_CHECK_OK;
}

const char *la_spi_check_name(int result)
{
    switch (result) {
    case LA_SPI_CHECK_OK:
        return "OK";
    case LA_SPI_CHECK_NOT_SLAVE:
        return "NOT_SLAVE";
    case LA_SPI_CHECK_ERROR_FLAGS:
        return "ERROR_FLAGS";
    default:
        return "UNKNOWN";
    }
}
