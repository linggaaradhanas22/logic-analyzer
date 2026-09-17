/*
 * U03 direct-USB CDC-ACM descriptor bytes.
 *
 * Layout: one CDC-ACM function with an Interface Association Descriptor.
 * Device class 0x02, control interface 0x02/0x02/0x01 with header,
 * call-management, ACM and union functional descriptors, data interface
 * 0x0A, EP1 bulk OUT/IN and EP2 interrupt IN for notifications. The
 * interface-level class codes plus the IAD are what let Windows bind
 * usbser.sys automatically (the old AMD CDC techtip needed a custom INF and
 * is deliberately not copied).
 */

#include "la_usb_desc.h"

static const uint8_t s_device[] = {
    18u, 0x01u,             /* bLength, DEVICE */
    0x00u, 0x02u,           /* bcdUSB 2.00 */
    0x02u, 0x00u, 0x00u,    /* class CDC, subclass/protocol 0 */
    64u,                    /* bMaxPacketSize0 */
    (uint8_t)(LA_USB_VID & 0xFFu), (uint8_t)(LA_USB_VID >> 8),
    (uint8_t)(LA_USB_PID & 0xFFu), (uint8_t)(LA_USB_PID >> 8),
    (uint8_t)(LA_USB_BCD_DEVICE & 0xFFu), (uint8_t)(LA_USB_BCD_DEVICE >> 8),
    1u, 2u, 3u,             /* iManufacturer, iProduct, iSerialNumber */
    1u                      /* bNumConfigurations */
};

#define LA_USB_CONFIG_HEAD                                                        \
    9u, 0x02u,                                                                    \
    (uint8_t)(LA_USB_CONFIG_TOTAL & 0xFFu), (uint8_t)(LA_USB_CONFIG_TOTAL >> 8), \
    2u,        /* bNumInterfaces */                                               \
    1u,        /* bConfigurationValue */                                          \
    0u,        /* iConfiguration */                                               \
    0x80u,     /* bmAttributes: bus powered */                                    \
    50u        /* bMaxPower: 100 mA */

#define LA_USB_CONFIG_CDC_COMMON                                 \
    8u, 0x0Bu, 0u, 2u, 0x02u, 0x02u, 0x01u, 0u, /* IAD */        \
    9u, 0x04u, 0u, 0u, 1u, 0x02u, 0x02u, 0x01u, 0u, /* control */ \
    5u, 0x24u, 0x00u, 0x10u, 0x01u,             /* header */      \
    5u, 0x24u, 0x01u, 0x00u, 1u,                /* call mgmt */   \
    4u, 0x24u, 0x02u, 0x02u,                    /* ACM */         \
    5u, 0x24u, 0x06u, 0u, 1u,                   /* union */       \
    7u, 0x05u, 0x82u, 0x03u, 16u, 0u, 16u       /* EP2 IN int */

#define LA_USB_CONFIG_DATA                                            \
    9u, 0x04u, 1u, 0u, 2u, 0x0Au, 0x00u, 0x00u, 0u, /* data iface */ \
    7u, 0x05u, 0x01u, 0x02u

#define LA_USB_CONFIG_EP_IN(size) \
    7u, 0x05u, 0x81u, 0x02u, (uint8_t)((size) & 0xFFu), \
    (uint8_t)((size) >> 8), 0u

static const uint8_t s_config_hs[] = {
    LA_USB_CONFIG_HEAD, LA_USB_CONFIG_CDC_COMMON, LA_USB_CONFIG_DATA,
    (uint8_t)(LA_USB_BULK_HS & 0xFFu), (uint8_t)(LA_USB_BULK_HS >> 8), 0u,
    LA_USB_CONFIG_EP_IN(LA_USB_BULK_HS)
};

static const uint8_t s_config_fs[] = {
    LA_USB_CONFIG_HEAD, LA_USB_CONFIG_CDC_COMMON, LA_USB_CONFIG_DATA,
    (uint8_t)(LA_USB_BULK_FS & 0xFFu), (uint8_t)(LA_USB_BULK_FS >> 8), 0u,
    LA_USB_CONFIG_EP_IN(LA_USB_BULK_FS)
};

static const uint8_t s_qualifier[] = {
    10u, 0x06u, 0x00u, 0x02u, 0x02u, 0x00u, 0x00u, 64u, 1u, 0u
};

static const char *const s_strings[LA_USB_STRING_COUNT] = {
    NULL, "KCCI", "RTOS Logic Analyzer", "LA-USB-TEST-0001"
};

const uint8_t *la_usb_desc_device(void)
{
    return s_device;
}

size_t la_usb_desc_device_len(void)
{
    return sizeof(s_device);
}

const uint8_t *la_usb_desc_config(size_t *length, int high_speed)
{
    if (length != NULL) {
        *length = sizeof(s_config_hs);
    }
    return (high_speed != 0) ? s_config_hs : s_config_fs;
}

const uint8_t *la_usb_desc_qualifier(size_t *length)
{
    if (length != NULL) {
        *length = sizeof(s_qualifier);
    }
    return s_qualifier;
}

size_t la_usb_desc_string(uint8_t index, uint8_t *buf, size_t capacity)
{
    const char *text;
    size_t chars;
    size_t length;
    size_t position;

    if (buf == NULL || index >= LA_USB_STRING_COUNT) {
        return 0u;
    }
    if (index == 0u) {
        if (capacity < 4u) {
            return 0u;
        }
        buf[0] = 4u;
        buf[1] = 0x03u;
        buf[2] = 0x09u; /* English (United States) */
        buf[3] = 0x04u;
        return 4u;
    }
    text = s_strings[index];
    chars = 0u;
    while (text[chars] != '\0') {
        chars++;
    }
    length = 2u + (chars * 2u);
    if (capacity < length) {
        return 0u;
    }
    buf[0] = (uint8_t)length;
    buf[1] = 0x03u;
    for (position = 0u; position < chars; position++) {
        buf[2u + (position * 2u)] = (uint8_t)text[position];
        buf[3u + (position * 2u)] = 0u;
    }
    return length;
}
