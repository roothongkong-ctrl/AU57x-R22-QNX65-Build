/* GPL-3.0-or-later. MU1005 R29 lab-only link profile. */
#ifndef WCP_BT_LINK_PROFILE_H
#define WCP_BT_LINK_PROFILE_H
#include <stdint.h>
#define WCP_OEM_TYPE_USB_HOST 1u
#define WCP_OEM_TYPE_USB_DEVICE 2u
#define WCP_OEM_TYPE_BT_IDENTIFICATION 3u
#define WCP_OEM_IAP2_LINK_DEFAULT_BYTES 16u
int wcp_bt_link_profile_default(void *out, unsigned capacity);
int wcp_bt_type_candidate(uint8_t *out);
#endif
