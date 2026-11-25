/* GPL-3.0-or-later. ipod-drvr-iap2 link_get_default_params 0x10898..0x108d0;
 * ident_info_tspbt compares type 3 at 0xd5e4. Default bytes are not BT tuning. */
#include "wcp_bt_link_profile.h"
#include <errno.h>
#include <string.h>
#if !defined(WCP_QNX_LAB_ONLY)
#error "This candidate link profile is not a vehicle-installable Bluetooth transport"
#endif
#if defined(WCP_LAB_BT_TYPE_CANDIDATE)
#if WCP_LAB_BT_TYPE_CANDIDATE != WCP_OEM_TYPE_BT_IDENTIFICATION
#error "MU1005 identification accepts type 3; arbitrary alternatives refused"
#endif
#endif
int wcp_bt_link_profile_default(void *out, unsigned capacity) {
    uint8_t *p = (uint8_t *)out;
    if (!p || capacity < WCP_OEM_IAP2_LINK_DEFAULT_BYTES) return EINVAL;
    memset(p, 0, WCP_OEM_IAP2_LINK_DEFAULT_BYTES);
    p[0]=1; p[2]=0xe8; p[3]=0x03; p[4]=20; p[6]=30;
    p[7]=1; p[8]=1; p[10]=128; p[12]=1; p[14]=128;
    return 0;
}
int wcp_bt_type_candidate(uint8_t *out) {
    if (!out) return EINVAL;
#ifdef WCP_LAB_BT_TYPE_CANDIDATE
    *out = WCP_OEM_TYPE_BT_IDENTIFICATION;
    return 0;
#else
    return ENOTSUP;
#endif
}
