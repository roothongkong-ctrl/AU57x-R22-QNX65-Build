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
/* Worker-only, exclusive original AP/slot owner required for every mutation.
 * Caller must retain lease storage and uncertain capture marker across the
 * entire radio lifetime; state 2 requires independent recovery, no retry.
 * No automatic write, no 5GHz assumption, no OEM owner discovery. */
struct mu_ap_ie_lease { unsigned slot; uint8_t bt[6],id[6],bssid[6]; int state; };
struct mu_ap_ie_ops {
 void *ctx;
 mu_ap_query query;
 int (*write)(void*,unsigned,const uint8_t[6],const uint8_t[6],int);
 int (*exclusive)(void*,const uint8_t[6],unsigned);
};
int mu_ap_stock_ie_write(void*,unsigned,const uint8_t[6],const uint8_t[6],int);
int mu_ap_ie_install(struct mu_ap_ie_lease*,const struct mu_ap_ie_ops*,const struct mu_ap_live*,unsigned,const uint8_t[6],const uint8_t[6]);
int mu_ap_ie_restore(struct mu_ap_ie_lease*,const struct mu_ap_ie_ops*);
/* Select first explicitly empty exclusively owned slot, or observe existing
 * identical advertisement. 0 free candidate, 1 existing (not owned), -1 none.
 * Slot selection is read-only; install rechecks original lease and bytes. */
int mu_ap_ie_select(const struct mu_ap_live*,const struct mu_ap_ie_ops*,const uint8_t[6],const uint8_t[6],unsigned*);
int mu_ap_ie_install_first_free(struct mu_ap_ie_lease*,const struct mu_ap_ie_ops*,const struct mu_ap_live*,const uint8_t[6],const uint8_t[6]);
#endif
