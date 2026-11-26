#ifndef MU1005_AP_LIVE_H
#define MU1005_AP_LIVE_H
#include "cp_csm.h"
#include <stdint.h>
#include <stddef.h>
struct mu_ap_live { char ssid[33], key[65]; uint8_t bssid[6], channel, security; };
/* serialized worker, never on original SDK callback. Do not log snapshots. */
typedef int (*mu_ap_query)(void*,unsigned,char*,size_t);
int mu_ap_live_read(struct mu_ap_live*,mu_ap_query,void*);
int mu_ap_stock_query(void*,unsigned,char*,size_t);
void mu_ap_live_clear(void*,size_t);
/* References caller-owned live snapshot and original receiver identity.
 * Caller holds both immutable until session ends. No synthetic identity. */
int mu_ap_live_session(struct cp_session*,const struct mu_ap_live*,const struct cp_session*);
/* Read-only 2.4 GHz check of pinned upstream Device IE and frame mask.
 * 1 matching, 0 absent/empty, -1 mismatch/unsupported/malformed.
 * Never infer a free slot or write driver state from this result. */
int mu_ap_device_ie_verify(const char *,unsigned,const uint8_t[6],const uint8_t[6]);
int mu_ap_live_verify_device_ie(const struct mu_ap_live*,mu_ap_query,void*,unsigned,const uint8_t[6],const uint8_t[6]);
#endif
