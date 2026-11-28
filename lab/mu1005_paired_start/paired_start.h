/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_PAIRED_START_H
#define MU1005_PAIRED_START_H
#include "cp_wcp_autoconnect.h"
/* Serialized existing OEM owner. snapshot returns 1 for the uniquely
 * selected paired, connected phone with native generation; 0 none;
 * -1 unknown/ambiguous. Never derive identity from trace hint.
 * Initial snapshot supports a phone connected before observer startup. */
typedef struct {
 struct cp_wcp_autoconnect bt;
 void *ctx;
 int (*snapshot)(void*,uint32_t*);
 unsigned (*take)(void*);
 int (*released)(void*,uint32_t);
} MuPairedStart;
int mu_paired_start_init(MuPairedStart*,const struct cp_wcp_bt_ops*,
 int(*)(void*,uint32_t*),unsigned(*)(void*),int(*)(void*,uint32_t));
int mu_paired_start_tick(MuPairedStart*,uint64_t);
#endif
