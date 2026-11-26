#ifndef MU1005_BT_STARTUP_SNAPSHOT_H
#define MU1005_BT_STARTUP_SNAPSHOT_H
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
/* Original pinned MU1005 only. Read on certified serialized owner, through
 * a checked memory reader, never dereference an untrusted object address. */
int mu_bt_sdk_started(uint32_t service,int(*read_live)(void*,uint32_t,void*,size_t),void*);
/* Recover the live BluetoothServices instance from the stock BtLinkKeys
 * listener set during original startup. Caller holds the existing startup
 * lifetime lease and exact mapped-image proof. Returns 0 until ready. */
uint32_t mu_bt_sdk_service_address(int(*read_live)(void*,uint32_t,void*,size_t),void*);
#endif
