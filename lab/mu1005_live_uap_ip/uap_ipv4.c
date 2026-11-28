/* SPDX-License-Identifier: GPL-3.0-only
 * Read the original AP IPv4 address, never configure the interface.
 * The historical MU1005 capture identifies uap0, not mmx0 or carplay0.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <string.h>
#include <stdint.h>
static int select_uap(const struct ifaddrs *p,uint8_t out[4]){
 unsigned count=0;uint8_t selected[4]={0};
 for(;p;p=p->ifa_next){
  if(!p->ifa_name||strcmp(p->ifa_name,"uap0")||!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET)continue;
  if((p->ifa_flags&(IFF_UP|IFF_RUNNING))!=(IFF_UP|IFF_RUNNING)||(p->ifa_flags&IFF_LOOPBACK))return -1;
  const struct sockaddr_in *a=(const struct sockaddr_in*)p->ifa_addr;
  uint8_t b[4];memcpy(b,&a->sin_addr,4);
  if(!b[0]||b[0]==127||b[0]>=224||(b[0]==169&&b[1]==254))return -1;
  if(++count!=1)return -1;
  memcpy(selected,b,4);
 }
 if(count!=1)return -1;
 memcpy(out,selected,4);return 0;
}
int mu1005_uap_ipv4(uint8_t out[4]){
 struct ifaddrs *list=NULL;
 if(!out||getifaddrs(&list)!=0)return -1;
 int rc=select_uap(list,out);freeifaddrs(list);return rc;
}
#ifdef MU1005_UAP_TEST
#include <assert.h>
#include <stdio.h>
int main(void){
 struct sockaddr_in ap={0},other={0};ap.sin_family=other.sin_family=AF_INET;
 uint8_t good[4]={192,0,2,1},wrong[4]={198,51,100,1},out[4]={0};
 memcpy(&ap.sin_addr,good,4);memcpy(&other.sin_addr,wrong,4);
 struct ifaddrs b={0},a={0};
 a.ifa_name="mmx0";a.ifa_addr=(struct sockaddr*)&other;a.ifa_flags=IFF_UP|IFF_RUNNING;a.ifa_next=&b;
 b.ifa_name="uap0";b.ifa_addr=(struct sockaddr*)&ap;b.ifa_flags=IFF_UP|IFF_RUNNING;
 assert(select_uap(&a,out)==0&&!memcmp(out,good,4));
 b.ifa_flags=IFF_UP;assert(select_uap(&a,out)<0);
 b.ifa_flags=IFF_UP|IFF_RUNNING;a.ifa_name="uap0";assert(select_uap(&a,out)<0);
 a.ifa_name="carplay0";b.ifa_name="mlan0";assert(select_uap(&a,out)<0);
 b.ifa_name="uap0";good[0]=127;memcpy(&ap.sin_addr,good,4);assert(select_uap(&a,out)<0);
 puts("MU1005_UAP_IPV4_SELECTION_PASS");return 0;
}
#endif
