/* GPL-3.0-only: AU57x 70 OEM instruction-derived OBSERVATIONS, not API.
 * Confirmed in two vendor ARM ELFs by verify_native_callbacks_r5.py.
 * DO NOT register/load this layout as a transport implementation.
 */
#ifndef CP_IPOD_CALLBACK_OBSERVED_H
#define CP_IPOD_CALLBACK_OBSERVED_H
#include <stdint.h>
#include <stddef.h>
/* The callback at +0x18 writes these observed bytes to its r1 output.
 * Byte 1 and byte 9 are NOT written in either verified implementation.
 * Field semantics, optional variants and length preconditions UNPROVEN.
 */
struct cp_observed_link_params_12 {
    uint8_t byte0;             /* 1 */
    uint8_t untouched1;
    uint16_t u16_at2;          /* 2000, little endian */
    uint16_t u16_at4;          /* 22 */
    uint8_t byte6;             /* 30 */
    uint8_t byte7;             /* 3 */
    uint8_t byte8;             /* 3 */
    uint8_t untouched9;
    uint16_t u16_at10;         /* 4096 */
};
#define CP_OBS_STATIC_ASSERT(name, expr) typedef char name[(expr) ? 1 : -1]
CP_OBS_STATIC_ASSERT(cp_observed_link_is_12, sizeof(struct cp_observed_link_params_12) == 12);
CP_OBS_STATIC_ASSERT(cp_observed_u16at10, offsetof(struct cp_observed_link_params_12,u16_at10) == 10);
/* usbhost_type_get returns byte 1, usbdevice_type_get returns byte 2.
 * Bluetooth type number / enum is NOT proven on the AU57x. */
#define CP_OEM_TYPE_USB_HOST 1u
#define CP_OEM_TYPE_USB_DEVICE 2u
#endif
