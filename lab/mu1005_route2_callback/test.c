#include "bt_callback_snapshot.h"
#include <assert.h>
#include <stdio.h>
struct sample { uint32_t want; unsigned calls; int result; unsigned char data[16]; };
static int read_header(void *p,uint32_t a,void *o,size_t n){
 struct sample *s=p; ++s->calls; assert(a==s->want&&n==16);
 memcpy(o,s->data,16);return s->result;
}
int main(void){
 struct sample s={0x1000,0,1,{5,0,0,0,0,0,0,0,3,0,0,0,1,2,3,4}};
 unsigned char out[16],zero[16]={0};
 assert(mu_bt_callback_snapshot(s.want,read_header,&s,out)==1);
 assert(s.calls==1&&!memcmp(out,s.data,16));
 s.data[0]=4;assert(out[0]==5);
 s.result=0;assert(!mu_bt_callback_snapshot(s.want,read_header,&s,out));assert(!memcmp(out,zero,16));
 assert(s.calls==2);
 assert(!mu_bt_callback_snapshot(0,read_header,&s,out));
 assert(!mu_bt_callback_snapshot(UINT32_MAX-14,read_header,&s,out));
 assert(!mu_bt_callback_snapshot(s.want,NULL,&s,out));
 assert(!mu_bt_callback_snapshot(s.want,read_header,&s,NULL));
 assert(s.calls==2);
 s.want=UINT32_MAX-15;s.result=1;
 assert(mu_bt_callback_snapshot(s.want,read_header,&s,out)==1&&s.calls==3);
 puts("ROUTE2_NEW_CALLBACK_SNAPSHOT_TEST_PASS_FIXTURE_ONLY");
 return 0;
}
