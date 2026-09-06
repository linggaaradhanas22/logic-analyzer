#include "la_regs_drv.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                 \
    do {                                          \
        if (!(condition)) {                       \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                             \
        }                                         \
    } while (0)

#define FAKE_WORDS 64u
#define FAKE_ID_INDEX ((unsigned)(LA_REG_ID_OFFSET / 4u))
#define FAKE_DIV_INDEX ((unsigned)(LA_REG_SAMPLE_DIV_OFFSET / 4u))

typedef struct fake_mmio {
    uint32_t mem[FAKE_WORDS];
    uintptr_t base;
    /* 1 = store all writes, 2 = drop SAMPLE_DIV writes, 3 = drop only the
     * clear write (value 0) to SAMPLE_DIV. */
    int mode;
} fake_mmio_t;

static fake_mmio_t fake;

static uint32_t fake_read(uintptr_t address)
{
    if (address < fake.base) {
        return 0u;
    }
    address -= fake.base;
    if (address >= FAKE_WORDS * 4u) {
        return 0u;
    }
    return fake.mem[address / 4u];
}

static void fake_write(uintptr_t address, uint32_t value)
{
    if (address < fake.base) {
        return;
    }
    address -= fake.base;
    if (address >= FAKE_WORDS * 4u) {
        return;
    }
    if (fake.mode == 2 && address / 4u == FAKE_DIV_INDEX) {
        return;
    }
    if (fake.mode == 3 && address / 4u == FAKE_DIV_INDEX && value == 0u) {
        return;
    }
    fake.mem[address / 4u] = value;
}

static void fake_reset(uintptr_t base, uint32_t id, uint32_t sample_div, int mode)
{
    memset(&fake, 0, sizeof(fake));
    fake.base = base;
    fake.mode = mode;
    fake.mem[FAKE_ID_INDEX] = id;
    fake.mem[FAKE_DIV_INDEX] = sample_div;
    /* la_regs.sv STATUS reset is IDLE=1; the fake mirrors the RTL. */
    fake.mem[LA_REG_STATUS_OFFSET / 4u] = LA_REG_STATUS_IDLE_MASK;
}

static la_regs_dev_t make_dev(void)
{
    la_regs_dev_t dev;

    la_regs_dev_init(&dev, fake.base, fake_read, fake_write);
    return dev;
}

static int test_raw_access(void)
{
    la_regs_dev_t dev;
    uint32_t value;

    fake_reset(UINT32_C(0x40000000), LA_REG_ID_VALUE, 0u, 1);
    dev = make_dev();
    CHECK(la_regs_read_id(&dev) == LA_REG_ID_VALUE, "ID readback");
    CHECK(la_regs_read_status(&dev) == LA_REG_STATUS_IDLE_MASK, "STATUS reset value");
    la_regs_write32(&dev, LA_REG_CTRL_OFFSET, UINT32_C(0x00000010));
    CHECK(la_regs_read32(&dev, LA_REG_CTRL_OFFSET) == UINT32_C(0x00000010),
          "raw CTRL write/read");
    /* la_regs.sv semantics: out-of-range addresses read 0 and ignore
     * writes; the fake mirrors that behaviour. */
    value = la_regs_read32(&dev, UINT32_C(0x1000));
    CHECK(value == 0u, "out-of-range read is zero");
    la_regs_write32(&dev, UINT32_C(0x1000), UINT32_C(0xDEADBEEF));
    CHECK(la_regs_read32(&dev, UINT32_C(0x1000)) == 0u, "out-of-range write ignored");
    return 0;
}

static int test_selftest_ok(void)
{
    la_regs_dev_t dev;
    int result;

    fake_reset(UINT32_C(0x40000000), LA_REG_ID_VALUE, LA_REG_SAMPLE_DIV_RESET, 1);
    dev = make_dev();
    result = la_regs_selftest(&dev);
    CHECK(result == LA_REGS_SELFTEST_OK, "selftest passes with sane hardware");
    CHECK(la_regs_read32(&dev, LA_REG_SAMPLE_DIV_OFFSET) == LA_REG_SAMPLE_DIV_RESET,
          "selftest leaves SAMPLE_DIV at reset");
    CHECK(strcmp(la_regs_selftest_name(result), "OK") == 0, "selftest name");
    return 0;
}

static int test_selftest_bad_id(void)
{
    la_regs_dev_t dev;

    fake_reset(UINT32_C(0x40000000), UINT32_C(0x12345678), 0u, 1);
    dev = make_dev();
    CHECK(la_regs_selftest(&dev) == LA_REGS_SELFTEST_BAD_ID, "wrong ID detected");
    return 0;
}

static int test_selftest_bad_reset(void)
{
    la_regs_dev_t dev;

    fake_reset(UINT32_C(0x40000000), LA_REG_ID_VALUE, UINT32_C(0x00000005), 1);
    dev = make_dev();
    CHECK(la_regs_selftest(&dev) == LA_REGS_SELFTEST_BAD_SAMPLE_DIV_RESET,
          "SAMPLE_DIV not at reset detected");
    return 0;
}

static int test_selftest_roundtrip_fail(void)
{
    la_regs_dev_t dev;

    fake_reset(UINT32_C(0x40000000), LA_REG_ID_VALUE, 0u, 2);
    dev = make_dev();
    CHECK(la_regs_selftest(&dev) == LA_REGS_SELFTEST_BAD_SAMPLE_DIV_ROUNDTRIP,
          "SAMPLE_DIV write loss detected");
    return 0;
}

static int test_selftest_clear_fail(void)
{
    la_regs_dev_t dev;

    fake_reset(UINT32_C(0x40000000), LA_REG_ID_VALUE, 0u, 3);
    dev = make_dev();
    CHECK(la_regs_selftest(&dev) == LA_REGS_SELFTEST_BAD_SAMPLE_DIV_CLEAR,
          "SAMPLE_DIV clear loss detected");
    return 0;
}

static int test_selftest_names(void)
{
    CHECK(strcmp(la_regs_selftest_name(LA_REGS_SELFTEST_BAD_ID), "BAD_ID") == 0,
          "BAD_ID name");
    CHECK(strcmp(la_regs_selftest_name(99), "UNKNOWN") == 0, "unknown code name");
    return 0;
}

int main(void)
{
    if (test_raw_access() != 0 || test_selftest_ok() != 0 || test_selftest_bad_id() != 0 ||
        test_selftest_bad_reset() != 0 || test_selftest_roundtrip_fail() != 0 ||
        test_selftest_clear_fail() != 0 || test_selftest_names() != 0) {
        return 1;
    }
    puts("PASS: la_native_regs");
    return 0;
}
