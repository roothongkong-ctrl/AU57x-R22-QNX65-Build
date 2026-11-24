#ifndef MU1005_SI_REGISTRATION_H
#define MU1005_SI_REGISTRATION_H
#include "si_mu1005_layout.h"
/* Target's native info is record+8, with three constructed 40-byte strings.
 * Caller retains ownership of all C++ objects and executes on native owner
 * thread. Do NOT pass UTF-8 char* in place of a constructed UnicodeString8.
 * This ABI adapter does not authenticate a peer or establish owner identity. */
#define MU_INFO_SIZE 160u
#define MU_INFO_STRING0 24u
#define MU_INFO_STRING1 64u
#define MU_INFO_STRING2 104u
#define MU_REG_DENIED -10
#define MU_REG_UNCERTAIN -11
struct mu_reg_ops {
    void *context;
    int (*owner_session_current)(void *context,uint32_t proxy,uint64_t epoch,uint32_t session);
    int32_t (*add_unique_id)(void *proxy,void *native_key,void *native_secondary);
    void (*update_record)(void *proxy,uint32_t id,void *native_info);
    struct mu_si_reader reader;
};
/* Native key is distinct; info+24 is the writable secondary parameter.
 * Authenticated session/owner validation remains external and mandatory. */
int mu_si_register_prepared(const struct mu_reg_ops *ops,uint32_t proxy,
    uint64_t epoch,uint32_t session,void *key,void *info,size_t info_size,uint32_t *id);
#endif
