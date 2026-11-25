/* AU57x MU1005: original-code UUID advertisement gate for the GPL donor EIR helper.
 * This is a reusable building block, not a startup hook or a working radio path.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AU102_EIR_ADVERTISEMENT_H
#define AU102_EIR_ADVERTISEMENT_H
#include <stddef.h>
#include <stdint.h>
typedef struct Au102EirGate {
    int stock_btstack_verified;
    int iap2_rfcomm_registered;
    int iap2_sdp_published;
    int wired_carplay_active;
} Au102EirGate;
/* Returns 1 if one UUID was published, 0 if unchanged, -1 for rejected input.
 * Always leaves input untouched when any mandatory gate is unproven.
 * EIR capacity is checked against the donor helper's exact 240-byte contract.
 */
int au102_eir_append_if_ready(uint8_t *eir, size_t length, size_t capacity,
                               const Au102EirGate *gate, size_t *out_length);
unsigned au102_eir_build_original_first(uint8_t*,unsigned,
 unsigned(*)(uint8_t*,unsigned),int(*)(void*,const uint8_t*),void*);
#endif
