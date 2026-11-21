/*
 * Portable host-only tests for a hypothesis simulator, not modem access.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int au57x_ct_sim_api_version(void);
int au57x_ct_sim_format(char *out, size_t capacity,
                        int manual, int mode, const char *plmn,
                        int hypothesis);

static int test_count = 0;
static void expect(int manual, int mode, const char *plmn,
                   int hypothesis, const char *expected) {
    char out[80] = {0};
    int n = au57x_ct_sim_format(out, sizeof(out),
                                manual, mode, plmn, hypothesis);
    if (n < 0 || strcmp(out, expected)) {
        fprintf(stderr, "FAILED: manual=%d mode=%d PLMN=%s hypothesis=%d output=%s\n",
                manual, mode, plmn, hypothesis, out);
        exit(1);
    }
    test_count++;
}
static void fail_invalid(int manual, int mode, const char *plmn,
                         int hypothesis) {
    char out[80] = "UNCHANGED";
    int n = au57x_ct_sim_format(out, sizeof(out),
                                manual, mode, plmn, hypothesis);
    if (n >= 0 || strcmp(out, "UNCHANGED")) {
        fprintf(stderr, "FAILED: invalid accepted or buffer modified\n");
        exit(2);
    }
    test_count++;
}

int main(void) {
    static const char *ct[] = {"46003", "46005", "46011"};
    static const char *cm[] = {"46000", "46002", "46007", "46008"};
    static const char *cu[] = {"46001", "46006"};
    size_t i;
    if (au57x_ct_sim_api_version() != 1) return 3;
    for (i = 0; i < sizeof(ct)/sizeof(ct[0]); ++i) {
        char normal[80], hypothetical[80];
        sprintf(normal, "AT+COPS=1,2,%s,0", ct[i]);
        sprintf(hypothetical, "AT+COPS=1,2,%s", ct[i]);
        expect(1, 1, ct[i], 0, normal);
        expect(1, 1, ct[i], 1, hypothetical);
        expect(1, 2, ct[i], 1, hypothetical);
    }
    for (i = 0; i < sizeof(cm)/sizeof(cm[0]); ++i) {
        char normal[80];
        sprintf(normal, "AT+COPS=1,2,%s,0", cm[i]);
        expect(1, 1, cm[i], 0, normal);
        expect(1, 1, cm[i], 1, normal);  /* NO China Mobile changes */
    }
    for (i = 0; i < sizeof(cu)/sizeof(cu[0]); ++i) {
        char normal[80];
        sprintf(normal, "AT+COPS=1,2,%s,0", cu[i]);
        expect(1, 1, cu[i], 1, normal);
    }
    expect(0, 1, "46003", 0, "AT+COPS=0,2,,0");
    expect(0, 1, "46003", 1, "AT+COPS=0,2,,0");
    expect(0, 0, "46003", 1, "AT+COPS=0,2");
    expect(0, 2, "46000", 1, "AT+COPS=0,2");
    expect(1, 1, "46012", 1, "AT+COPS=1,2,46012,0");
    expect(1, 1, "46099", 1, "AT+COPS=1,2,46099,0");
    fail_invalid(1, -1, "46003", 1);
    fail_invalid(5, 1, "46003", 1);
    fail_invalid(1, 1, "4600x", 1);
    fail_invalid(1, 1, "46003", 10);
    fail_invalid(1, 1, "4600000", 1);
    {
        char dest[5] = "ZZZZ";
        if (au57x_ct_sim_format(dest, sizeof(dest), 1, 1, "46003", 1) >= 0 ||
            strcmp(dest, "ZZZZ")) return 4;
        test_count++;
    }
    printf("PASS: %d R04 C policy cases; ONLY strings, NO OEM or modem I/O\n", test_count);
    return 0;
}
