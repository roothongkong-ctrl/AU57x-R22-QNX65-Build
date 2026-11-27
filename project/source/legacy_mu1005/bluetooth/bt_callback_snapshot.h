#ifndef MU1005_BT_CALLBACK_SNAPSHOT_H
#define MU1005_BT_CALLBACK_SNAPSHOT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Read exactly one 16-byte original callback header through the existing
 * certified owner reader. Never dereference the untrusted event address.
 * Caller still owns serialization and lifetime; no authorization granted. */
static inline int mu_bt_callback_snapshot(uint32_t address,
 int (*reader)(void*,uint32_t,void*,size_t),void *ctx,uint8_t out[16]){
 uint8_t temp[16]={0};
 if(!out)return 0;
 memset(out,0,16);
 if(!reader||!address||address>UINT32_MAX-15u)return 0;
 if(reader(ctx,address,temp,sizeof temp)!=1)return 0;
 memcpy(out,temp,sizeof temp);
 return 1;
}
#endif
