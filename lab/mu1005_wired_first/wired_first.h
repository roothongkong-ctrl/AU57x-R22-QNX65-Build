/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_WIRED_FIRST_H
#define MU1005_WIRED_FIRST_H
#include "paired_start.h"
#include <stdint.h>
/* Opaque stable OEM paired-device ID, never a USB authentication token.
 * Native owner must establish the association, not infer it from names. */
typedef struct {uint8_t bytes[16];} MuPhoneId;
typedef struct {
 void *ctx;
 int (*usb_phone)(void*,uint64_t,MuPhoneId*); /* successful stock wired session */
 int (*acl_phone)(void*,uint32_t,MuPhoneId*); /* current uniquely selected bonded ACL */
 int (*load)(void*,MuPhoneId*); /* 1 found, 0 absent, -1 unavailable */
 int (*save)(void*,const MuPhoneId*); /* 1 committed */
} MuWiredFirstOps;
typedef struct {
 MuPairedStart *paired;
 MuWiredFirstOps ops;
 MuPhoneId remembered;
 uint64_t last_usb;
 int valid;
} MuWiredFirst;
int mu_wired_first_init(MuWiredFirst*,MuPairedStart*,const MuWiredFirstOps*);
int mu_wired_first_learn(MuWiredFirst*,uint64_t);
int mu_wired_first_tick(MuWiredFirst*,uint64_t);
/* Optional durable private preference file; no keys, no session material.
 * Caller provides a verified writable private data directory and owns it. */
int mu_phone_pref_load(const char*,MuPhoneId*);
int mu_phone_pref_save(const char*,const MuPhoneId*);
#endif
