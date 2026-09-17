/*
 * U03 host tests for the direct-USB CDC-ACM descriptor set.
 *
 * Validates the exact descriptor lengths and fields that Windows uses for
 * automatic usbser.sys binding (IAD, interface classes, endpoint sizes) and
 * the ASCII-to-UTF-16LE string builder.
 */

#include "la_usb_desc.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message)                   \
    do {                                            \
        if (!(condition)) {                         \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1;                               \
        }                                           \
    } while (0)

static int test_device_descriptor(void)
{
    const uint8_t *d = la_usb_desc_device();

    CHECK(la_usb_desc_device_len() == 18u, "device length");
    CHECK(d[0] == 18u && d[1] == 0x01u, "device header");
    CHECK(d[2] == 0x00u && d[3] == 0x02u, "bcdUSB");
    CHECK(d[4] == 0x02u && d[5] == 0x00u && d[6] == 0x00u, "device class");
    CHECK(d[7] == 64u, "EP0 max packet");
    CHECK(d[8] == (LA_USB_VID & 0xFFu) && d[9] == (LA_USB_VID >> 8),
          "VID little endian");
    CHECK(d[10] == (LA_USB_PID & 0xFFu) && d[11] == (LA_USB_PID >> 8),
          "PID little endian");
    CHECK(d[12] == 0x00u && d[13] == 0x01u, "bcdDevice");
    CHECK(d[14] == 1u && d[15] == 2u && d[16] == 3u, "string indices");
    CHECK(d[17] == 1u, "one configuration");
    return 0;
}

static int check_endpoint(const uint8_t *d, size_t offset, uint8_t address,
                          uint8_t attributes, uint16_t packet)
{
    CHECK(d[offset + 0u] == 7u, "endpoint length");
    CHECK(d[offset + 1u] == 0x05u, "endpoint type");
    CHECK(d[offset + 2u] == address, "endpoint address");
    CHECK(d[offset + 3u] == attributes, "endpoint attributes");
    CHECK(d[offset + 4u] == (uint8_t)(packet & 0xFFu), "packet low");
    CHECK(d[offset + 5u] == (uint8_t)(packet >> 8), "packet high");
    return 0;
}

static int test_config_descriptor(int high_speed, uint16_t bulk)
{
    size_t length = 0u;
    const uint8_t *d = la_usb_desc_config(&length, high_speed);

    CHECK(length == LA_USB_CONFIG_TOTAL, "config total length");
    CHECK(d[0] == 9u && d[1] == 0x02u, "config header");
    CHECK(d[2] == (uint8_t)(length & 0xFFu) &&
              d[3] == (uint8_t)(length >> 8),
          "wTotalLength");
    CHECK(d[4] == 2u, "two interfaces");
    CHECK(d[5] == 1u, "configuration value");
    CHECK(d[7] == 0x80u, "bus powered");
    CHECK(d[8] == 50u, "100 mA");

    /* IAD: first interface 0, two interfaces, CDC/ACM function. */
    CHECK(d[9] == 8u && d[10] == 0x0Bu, "IAD header");
    CHECK(d[11] == 0u && d[12] == 2u, "IAD interfaces");
    CHECK(d[13] == 0x02u && d[14] == 0x02u && d[15] == 0x01u, "IAD class");

    /* Control interface with the functional descriptors. */
    CHECK(d[17] == 9u && d[18] == 0x04u, "control interface");
    CHECK(d[19] == 0u && d[20] == 0u && d[21] == 1u, "control iface index");
    CHECK(d[22] == 0x02u && d[23] == 0x02u && d[24] == 0x01u,
          "control class");
    CHECK(d[26] == 5u && d[27] == 0x24u && d[28] == 0x00u, "header FD");
    CHECK(d[31] == 5u && d[32] == 0x24u && d[33] == 0x01u, "call mgmt FD");
    CHECK(d[36] == 4u && d[37] == 0x24u && d[38] == 0x02u, "ACM FD");
    CHECK(d[40] == 5u && d[41] == 0x24u && d[42] == 0x06u, "union FD");
    CHECK(d[43] == 0u && d[44] == 1u, "union interfaces");
    if (check_endpoint(d, 45u, 0x82u, 0x03u, 16u) != 0) {
        return 1;
    }

    /* Data interface with the two bulk endpoints. */
    CHECK(d[52] == 9u && d[53] == 0x04u, "data interface");
    CHECK(d[54] == 1u && d[55] == 0u && d[56] == 2u, "data iface index");
    CHECK(d[57] == 0x0Au && d[58] == 0x00u && d[59] == 0x00u, "data class");
    if (check_endpoint(d, 61u, 0x01u, 0x02u, bulk) != 0) {
        return 1;
    }
    if (check_endpoint(d, 68u, 0x81u, 0x02u, bulk) != 0) {
        return 1;
    }
    return 0;
}

static int test_config_differences(void)
{
    size_t hs_len = 0u;
    size_t fs_len = 0u;
    const uint8_t *hs = la_usb_desc_config(&hs_len, 1);
    const uint8_t *fs = la_usb_desc_config(&fs_len, 0);
    size_t index;
    size_t differences = 0u;

    CHECK(hs_len == fs_len, "config lengths match");
    for (index = 0u; index < hs_len; index++) {
        if (hs[index] != fs[index]) {
            differences++;
        }
    }
    CHECK(differences == 4u, "only the two bulk packet sizes differ");
    return 0;
}

static int test_qualifier(void)
{
    size_t length = 0u;
    const uint8_t *q = la_usb_desc_qualifier(&length);

    CHECK(length == 10u, "qualifier length");
    CHECK(q[0] == 10u && q[1] == 0x06u, "qualifier header");
    CHECK(q[2] == 0x00u && q[3] == 0x02u, "qualifier bcdUSB");
    CHECK(q[4] == 0x02u && q[5] == 0x00u && q[6] == 0x00u, "qualifier class");
    CHECK(q[7] == 64u && q[8] == 1u && q[9] == 0u, "qualifier tail");
    return 0;
}

static int test_strings(void)
{
    uint8_t buffer[LA_USB_STRING_MAX];
    size_t length;

    length = la_usb_desc_string(0u, buffer, sizeof(buffer));
    CHECK(length == 4u, "langid length");
    CHECK(buffer[0] == 4u && buffer[1] == 0x03u, "langid header");
    CHECK(buffer[2] == 0x09u && buffer[3] == 0x04u, "langid value");

    length = la_usb_desc_string(1u, buffer, sizeof(buffer));
    CHECK(length == 10u, "manufacturer length");
    CHECK(buffer[0] == 10u && buffer[1] == 0x03u, "manufacturer header");
    CHECK(buffer[2] == 'K' && buffer[3] == 0u && buffer[4] == 'C' &&
              buffer[5] == 0u,
          "manufacturer UTF-16LE");
    CHECK(buffer[8] == 'I' && buffer[9] == 0u, "manufacturer tail");

    length = la_usb_desc_string(3u, buffer, sizeof(buffer));
    CHECK(length == 34u, "serial length");
    CHECK(buffer[2] == 'L' && buffer[3] == 0u, "serial first char");
    CHECK(buffer[32] == '1' && buffer[33] == 0u, "serial last char");

    CHECK(la_usb_desc_string(4u, buffer, sizeof(buffer)) == 0u,
          "unknown index");
    CHECK(la_usb_desc_string(2u, buffer, 4u) == 0u, "small capacity");
    CHECK(la_usb_desc_string(1u, NULL, sizeof(buffer)) == 0u, "NULL buffer");
    return 0;
}

int main(void)
{
    if (test_device_descriptor() != 0 ||
        test_config_descriptor(1, LA_USB_BULK_HS) != 0 ||
        test_config_descriptor(0, LA_USB_BULK_FS) != 0 ||
        test_config_differences() != 0 || test_qualifier() != 0 ||
        test_strings() != 0) {
        return 1;
    }
    puts("PASS: la_native_usb_desc");
    return 0;
}
