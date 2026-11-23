/* Real existing AU57x C RFCOMM queue wire bytes, to be consumed by the
 * unmodified upstream iap2-link engine through the R103 Rust adapter. */
#include "mu1005_rf_ingress.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int send_noop(void *ctx, const uint8_t *data, size_t n) { (void)ctx;(void)data;(void)n;return 0; }
static void check(const AuRfWire *w, unsigned k, unsigned n, unsigned generation) {
 assert(w->len==8u+n);
 assert(w->bytes[0]==k && w->bytes[1]==0);
 assert(((unsigned)w->bytes[2]|((unsigned)w->bytes[3]<<8))==n);
 assert((unsigned)w->bytes[4]==generation && w->bytes[5]==0 && w->bytes[6]==0 && w->bytes[7]==0);
}
int main(void){
 AuRfIngress b;AuRfWire w;
 const uint8_t mac[6]={1,2,3,4,5,6};
 const uint8_t marker[6]={0xff,0x55,2,0,0xee,0x10};
 au_rf_init(&b,send_noop,0);
 assert(au_rf_connected(&b,mac)==0);
 assert(au_rf_take(&b,&w)==1);check(&w,1,6,1);assert(memcmp(w.bytes+8,mac,6)==0);
 assert(au_rf_received(&b,marker,6)==0);
 assert(au_rf_take(&b,&w)==1);check(&w,2,6,1);assert(memcmp(w.bytes+8,marker,6)==0);
 assert(au_rf_disconnected(&b)==0);
 assert(au_rf_take(&b,&w)==1);check(&w,3,0,1);
 assert(b.generation==2);
 assert(au_rf_received(&b,marker,6)==-1); /* fail closed on late DATA */
 assert(b.failed);
 puts("R103_C_ORIGINAL_RFCOMM_FRAMES_AND_DISCONNECT_PASS");
 return 0;
}
