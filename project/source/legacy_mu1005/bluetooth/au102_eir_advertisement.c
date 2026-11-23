/* Original donor implementation in third_party/luka-dev/.../eir.c (GPLv3).
 * This adaptor is written for AU57x and must not be treated as a verified
 * OEM BlueSDK EIR callback hook. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "au102_eir_advertisement.h"
#include <stdint.h>
#include <stddef.h>
extern unsigned int eir_append_carplay_uuid(unsigned char *, unsigned int);
int au102_eir_append_if_ready(uint8_t *eir, size_t length, size_t capacity,
                               const Au102EirGate *gate, size_t *out_length) {
    unsigned int new_len;
    if (!out_length || !gate || !eir || capacity < 240u || length >= 240u)
        return -1;
    *out_length = length;
    if (gate->wired_carplay_active)
        return 0;
    if (gate->stock_btstack_verified != 1 ||
        gate->iap2_rfcomm_registered != 1 ||
        gate->iap2_sdp_published != 1)
        return 0;
    new_len = eir_append_carplay_uuid(eir, (unsigned int)length);
    if (new_len < length || new_len >= 240u)
        return -1;
    *out_length = new_len;
    return new_len == length ? 0 : 1;
}
