#ifndef LA_USB_DESC_H
#define LA_USB_DESC_H

/*
 * U03 direct-USB CDC-ACM descriptor set.
 *
 * WARNING: the identity below is the pid.codes TEST PID. It is not
 * universally unique and MUST NOT be used on any redistributed, sold or
 * manufactured device. Release artifacts need a real VID/PID allocation
 * (U07/U09 gate).
 *
 * Pure module: no Xilinx or FreeRTOS dependency, so the host tests validate
 * the exact bytes and the string builder.
 */

#include <stddef.h>
#include <stdint.h>

#define LA_USB_VID 0x1209u
#define LA_USB_PID 0x0001u
#define LA_USB_BCD_DEVICE 0x0100u
#define LA_USB_CONFIG_TOTAL 75u
#define LA_USB_BULK_HS 512u
#define LA_USB_BULK_FS 64u
#define LA_USB_STRING_MAX 64u
#define LA_USB_STRING_COUNT 4u

const uint8_t *la_usb_desc_device(void);
size_t la_usb_desc_device_len(void);

/* high_speed selects the 512-byte (1) or 64-byte (0) bulk packet size. */
const uint8_t *la_usb_desc_config(size_t *length, int high_speed);
const uint8_t *la_usb_desc_qualifier(size_t *length);

/* Builds string descriptor `index` (0 = LangID, 1..3 = ASCII strings) into
 * buf and returns the descriptor length, or 0 for an unknown index or a
 * capacity that cannot hold the result. */
size_t la_usb_desc_string(uint8_t index, uint8_t *buf, size_t capacity);

#endif /* LA_USB_DESC_H */
