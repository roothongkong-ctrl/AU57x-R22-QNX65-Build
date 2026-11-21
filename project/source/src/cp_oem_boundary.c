#include "cp_oem_boundary.h"
int cp_oem_dispatch(struct cp_wireless *w, const struct cp_oem_assertions *p,
                    const uint8_t *csm,size_t n) {
    if(!w || !p || !csm || !n) return CP_WL_BAD_FRAME;
    /* USB always has priority; prevent any second DIO owner. */
    if(p->wired_active || !p->sole_dio_owner || !p->verified_mfi ||
       !p->verified_control_session || !p->live_hotspot) {
        w->authenticated=0;
        return CP_WL_NOT_READY;
    }
    w->authenticated=1;
    w->hotspot_ready=1;
    w->airplay_ready=(p->network_airplay_ready!=0);
    return cp_wireless_receive(w,csm,n);
}
