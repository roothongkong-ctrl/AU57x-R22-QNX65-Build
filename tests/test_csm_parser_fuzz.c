/* Deterministic host fuzz regression for untrusted iAP2 CSM frames.
 * NO radio / MFi / vehicle. ASan+UBSan runs in GitHub cloud.
 */
#include "cp_event.h"
#include "cp_wireless.h"
#include "cp_csm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed=0xC0FFEE21u;
static uint32_t rnd(void){seed=1664525u*seed+1013904223u;return seed;}
static void encoders(void) {
 struct cp_wifi w={"host-test","fake-pass",2,36,0};
 const char *ips[]={"192.0.2.10"};
 struct cp_session s={0};
 s.wifi=w;s.ip_addresses=ips;s.ip_address_count=1;s.airplay_port=7000;
 s.public_key="FAKE_KEY";s.source_version="test";
 for(size_t capacity=6;capacity<=180;capacity++){
  uint8_t mem[192];size_t written=999;
  memset(mem,0xA5,sizeof mem);
  int r=cp_encode_wifi_5703(&w,mem+4,capacity,&written);
  assert(r==CP_OK||r==CP_OVERFLOW);
  if(r==CP_OK){assert(written<=capacity&&written>=6);assert(mem[4]==0x40);}
  else assert(written==0);
  for(size_t j=4+capacity;j<sizeof mem;j++)assert(mem[j]==0xA5);
  memset(mem,0xA5,sizeof mem);written=999;
  r=cp_encode_start_4301(&s,mem+4,capacity,&written);
  assert(r==CP_OK||r==CP_OVERFLOW);
  if(r==CP_OK){assert(written<=capacity&&written>=6);assert(mem[4]==0x40);}
  else assert(written==0);
  for(size_t j=4+capacity;j<sizeof mem;j++)assert(mem[j]==0xA5);
 }
}
int main(void){
 encoders();
 const uint8_t good[]={0x40,0x40,0,15,0x43,0,0,9,0,1,0,5,0,0,1};
 struct cp_event event;int available=0;
 assert(cp_decode_event(good,sizeof good,&event)==0);
 assert(cp_availability_wireless_4300(good,sizeof good,&available)==1);
 assert(available==1);
 for(unsigned i=0;i<50000u;i++){
  uint8_t frame[128];
  size_t len=(size_t)(rnd()%sizeof frame)+1u;
  for(size_t j=0;j<len;j++)frame[j]=(uint8_t)rnd();
  if(i%3u==0&&len>=6u){
   frame[0]=0x40;frame[1]=0x40;
   frame[2]=(uint8_t)(len>>8);frame[3]=(uint8_t)len;
   frame[4]=(i%2u)?0x43:0x57;frame[5]=(i%2u)?0:2;
  }
  int a=cp_decode_event(frame,len,&event);
  assert(a==0||a==-1||a==-3);
  int b=cp_availability_wireless_4300(frame,len,&available);
  assert(b==-1||b==0||b==1);
 }
 puts("AU57X_CLOUD12_CSM_FUZZ_50000_PASS");
 return 0;
}
