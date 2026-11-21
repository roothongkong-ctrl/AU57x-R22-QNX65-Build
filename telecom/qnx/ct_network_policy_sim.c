/*
 * AU57x China Telecom R04 HYPOTHESIS SIMULATOR, NOT AN OEM HOOK.
 *
 * Pure string formatting, NO modem I/O, NO QNX private APIs, NO installer.
 * The hypothetical "drop forced GSM AcT" behavior is NOT proven to fix any
 * real vehicle. Never use its output in a running car without root-cause proof.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CT_SIM_API_VERSION 1
#define CT_SIM_ERR_INVALID (-1)
#define CT_SIM_ERR_BUFFER (-2)

int au57x_ct_sim_api_version(void) {
    return CT_SIM_API_VERSION;
}

static int valid_plmn(const char *plmn) {
    size_t i;
    if (plmn == NULL) return 0;
    for (i = 0; i < 7u; ++i) {
        char c = plmn[i];
        if (c == 0) return i == 5u || i == 6u;
        if (c < '0' || c > '9') return 0;
    }
    return 0;
}
static int is_ct_plmn(const char *plmn) {
    return !strcmp(plmn, "46003") || !strcmp(plmn, "46005") ||
           !strcmp(plmn, "46011") || !strcmp(plmn, "46012");
}

/*
 * hypothesis=0: reconstruct baseline (matching previously inspected OEM).
 * hypothesis=1: research-only candidate to OMIT GSM AcT specifically for
 * China Telecom PLMNs on a mode==1 manual request.
 *
 * Returns length or negative error. Does not transmit or call modem.
 */
int au57x_ct_sim_format(char *out, size_t capacity,
                        int manual, int mode, const char *plmn,
                        int hypothesis) {
    char scratch[80];
    int n;
    if (out == NULL || capacity == 0 || mode < 0 ||
        (manual != 0 && manual != 1) ||
        (hypothesis != 0 && hypothesis != 1) ||
        !valid_plmn(plmn)) {
        return CT_SIM_ERR_INVALID;
    }

    if (manual) {
        if (mode == 1 && !(hypothesis == 1 && is_ct_plmn(plmn))) {
            n = snprintf(scratch, sizeof(scratch), "AT+COPS=1,2,%s,0", plmn);
        } else {
            n = snprintf(scratch, sizeof(scratch), "AT+COPS=1,2,%s", plmn);
        }
    } else if (mode == 1) {
        /* Do not guess whether automatic-mode trailing AcT is honored. */
        n = snprintf(scratch, sizeof(scratch), "%s", "AT+COPS=0,2,,0");
    } else {
        n = snprintf(scratch, sizeof(scratch), "%s", "AT+COPS=0,2");
    }
    if (n < 0 || (size_t)n >= sizeof(scratch) || (size_t)n >= capacity)
        return CT_SIM_ERR_BUFFER;
    memcpy(out, scratch, (size_t)n + 1u);
    return n;
}
