/* Host fixture: call real existing RFCOMM ingress completion accounting,
 * not a native SDK owner or phone. */
#include "mu1005_rf_ingress.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {unsigned calls;size_t bytes;} Fixture;
static int send_part(void*p,const uint8_t*b,size_t n){
 Fixture*f=p;assert(n>0&&n<=2);assert(memcmp(b,"abcde"+f->bytes,n)==0);
 ++f->calls;f->bytes+=n;return 0;
}
static void setup(AuRfIngress*s,Fixture*f,uint8_t frame[13]){
 static const uint8_t peer[6]={1,2,3,4,5,6};
 memset(f,0,sizeof(*f));au_rf_init(s,send_part,f);
 assert(au_rf_connected(s,peer)==0&&au_rf_set_max_payload(s,2)==0);
 memset(frame,0,13);frame[0]=2;frame[2]=5;
 for(unsigned i=0;i<4;i++)frame[4+i]=(uint8_t)(s->generation>>(i*8));
 memcpy(frame+8,"abcde",5);
 assert(au_rf_daemon_data(s,frame,13)==0);
}
int main(void){
 AuRfIngress s;Fixture f;AuRfTxReceipt x;uint8_t frame[13];uint32_t g;
 setup(&s,&f,frame);g=s.generation;
 assert(!au_rf_tx_receipt(&s,g,0,&x)&&!x.bytes);
 assert(au_rf_send_complete(&s,1)==0&&s.tx_busy);
 assert(!au_rf_tx_receipt(&s,g,0,&x));
 assert(au_rf_send_complete(&s,1)==0&&s.tx_busy);
 assert(!au_rf_tx_receipt(&s,g,0,&x));
 assert(au_rf_send_complete(&s,1)==0&&!s.tx_busy&&f.calls==3);
 assert(au_rf_tx_receipt(&s,g,0,&x)==1&&x.sequence==1&&x.bytes==5&&x.generation==g);
 assert(!au_rf_tx_receipt(&s,g,1,&x));
 assert(!au_rf_tx_receipt(&s,g+1,0,&x));
 assert(au_rf_disconnected(&s)==0&&!au_rf_tx_receipt(&s,g,0,&x));
 setup(&s,&f,frame);g=s.generation;
 assert(au_rf_send_complete(&s,1)==0);
 assert(au_rf_disconnected(&s)==0);
 assert(au_rf_send_complete(&s,1)==0);
 assert(!s.tx_receipt.sequence&&!au_rf_tx_receipt(&s,g,0,&x));
 setup(&s,&f,frame);g=s.generation;
 assert(au_rf_send_complete(&s,0)==-1);
 assert(!s.tx_receipt.sequence&&!au_rf_tx_receipt(&s,g,0,&x));
 puts("ROUTE3_EXISTING_RF_FULL_FRAME_RECEIPT_FIXTURE_PASS_NOT_NATIVE");
 return 0;
}
