/* AU57x OEM gate. NO IAP2 MFI is implemented here; caller validates it. */
#ifndef CP_OEM_BOUNDARY_H
#define CP_OEM_BOUNDARY_H
#include "cp_wireless.h"
#include <stddef.h>
#include <stdint.h>
/* Capability is proven by platform adapter only, never inferred by seeing CSM. */
struct cp_oem_assertions {
    int verified_mfi;
    int verified_control_session;
    int live_hotspot;
    int network_airplay_ready;
    int sole_dio_owner;
    int wired_active;
};
/* Guarded dispatch: does not auto-start SI/DIO or assume generic /dev/iapDevice
 * is equivalent to /dev/ipod0; physical RFCOMM data must be reassembled
 * through an authenticated iAP2 session by a REAL OEM adapter. */
int cp_oem_dispatch(struct cp_wireless *wireless,
                    const struct cp_oem_assertions *proof,
                    const uint8_t *complete_csm,size_t n);
#endif
