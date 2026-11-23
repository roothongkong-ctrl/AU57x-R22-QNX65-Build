#include "au57x_iap2_capi.h"
#include <cassert>
#include <cstdio>
#include <cstring>
struct S { unsigned tx_count;unsigned state_count;unsigned rx_count;uint8_t first[6];size_t n; };
static int tx(void *v,const uint8_t *b,size_t n){
 S *s=static_cast<S*>(v);s->tx_count++;s->n=n;
 if(n>=6)std::memcpy(s->first,b,6);
 return 1;
}
static void rx(void *v,uint8_t id,const uint8_t *b,size_t n){
 (void)id;(void)b;(void)n;static_cast<S*>(v)->rx_count++;
}
static void state(void *v,int s){(void)s;static_cast<S*>(v)->state_count++;}
int main(){
 S s={};au57x_iap2_callbacks cb={&s,tx,rx,state,nullptr};
 assert(!au57x_iap2_new(nullptr));
 au57x_iap2_link *p=au57x_iap2_new(&cb);assert(p);
 assert(au57x_iap2_state(p)==0);
 assert(!au57x_iap2_send(p,10,reinterpret_cast<const uint8_t*>("x"),1));
 assert(au57x_iap2_start(p)==1);
 assert(au57x_iap2_state(p)==1);
 assert(s.tx_count>=1 && s.n==6);
 static const uint8_t detect[6]={0xff,0x55,0x02,0x00,0xee,0x10};
 assert(std::memcmp(s.first,detect,6)==0);
 assert(!au57x_iap2_feed(p,nullptr,1));
 assert(au57x_iap2_tick(p,1000)==1);
 au57x_iap2_reset(p);
 assert(au57x_iap2_state(p)==0);
 au57x_iap2_free(p);
 std::puts("PASS: AU57x C API sends real upstream iAP2 Detect, blocks preauth payload");
 return 0;
}
