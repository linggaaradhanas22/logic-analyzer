#include "la_regs_drv.h"

#include <stddef.h>

void la_regs_dev_init(la_regs_dev_t *dev, uintptr_t base,
                      la_regs_read32_fn read32, la_regs_write32_fn write32)
{
    dev->base = base;
    dev->read32 = read32;
    dev->write32 = write32;
}

uint32_t la_regs_read32(const la_regs_dev_t *dev, uintptr_t offset)
{
    return dev->read32(dev->base + offset);
}

void la_regs_write32(const la_regs_dev_t *dev, uintptr_t offset, uint32_t value)
{
    dev->write32(dev->base + offset, value);
}

uint32_t la_regs_read_id(const la_regs_dev_t *dev)
{
    return la_regs_read32(dev, LA_REG_ID_OFFSET);
}

uint32_t la_regs_read_status(const la_regs_dev_t *dev)
{
    return la_regs_read32(dev, LA_REG_STATUS_OFFSET);
}

int la_regs_selftest(const la_regs_dev_t *dev)
{
    static const uint32_t patterns[2] = {UINT32_C(0xA5A55A5A), UINT32_C(0xFFFFFFFF)};
    size_t index;

    if (la_regs_read_id(dev) != LA_REG_ID_VALUE) {
        return LA_REGS_SELFTEST_BAD_ID;
    }
    if (la_regs_read32(dev, LA_REG_SAMPLE_DIV_OFFSET) != LA_REG_SAMPLE_DIV_RESET) {
        return LA_REGS_SELFTEST_BAD_SAMPLE_DIV_RESET;
    }
    for (index = 0u; index < 2u; ++index) {
        la_regs_write32(dev, LA_REG_SAMPLE_DIV_OFFSET, patterns[index]);
        if (la_regs_read32(dev, LA_REG_SAMPLE_DIV_OFFSET) != patterns[index]) {
            return LA_REGS_SELFTEST_BAD_SAMPLE_DIV_ROUNDTRIP;
        }
    }
    la_regs_write32(dev, LA_REG_SAMPLE_DIV_OFFSET, LA_REG_SAMPLE_DIV_RESET);
    if (la_regs_read32(dev, LA_REG_SAMPLE_DIV_OFFSET) != LA_REG_SAMPLE_DIV_RESET) {
        return LA_REGS_SELFTEST_BAD_SAMPLE_DIV_CLEAR;
    }
    return LA_REGS_SELFTEST_OK;
}

const char *la_regs_selftest_name(int result)
{
    switch (result) {
    case LA_REGS_SELFTEST_OK:
        return "OK";
    case LA_REGS_SELFTEST_BAD_ID:
        return "BAD_ID";
    case LA_REGS_SELFTEST_BAD_SAMPLE_DIV_RESET:
        return "BAD_SAMPLE_DIV_RESET";
    case LA_REGS_SELFTEST_BAD_SAMPLE_DIV_ROUNDTRIP:
        return "BAD_SAMPLE_DIV_ROUNDTRIP";
    case LA_REGS_SELFTEST_BAD_SAMPLE_DIV_CLEAR:
        return "BAD_SAMPLE_DIV_CLEAR";
    default:
        return "UNKNOWN";
    }
}
