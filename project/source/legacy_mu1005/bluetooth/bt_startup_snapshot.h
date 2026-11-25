#ifndef MU1005_BT_STARTUP_SNAPSHOT_H
#define MU1005_BT_STARTUP_SNAPSHOT_H
#include <stdint.h>
#include <stddef.h>
/* Original pinned MU1005 only. Read on certified serialized owner, through
 * a checked memory reader, never dereference an untrusted object address. */
int mu_bt_sdk_started(uint32_t service,int(*read_live)(void*,uint32_t,void*,size_t),void*);
#endif
