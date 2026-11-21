#define _DEFAULT_SOURCE 1
#include "wifi_interface.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <string.h>
int mu_wifi_interface_for_ipv4(const char *address,char *name,size_t capacity){
 struct in_addr requested;struct ifaddrs *all=0;char selected[17]={0};int result=-1;
 if(!name||!capacity)return -1;
 name[0]=0;
 if(!address||inet_pton(AF_INET,address,&requested)!=1)return -1;
 unsigned long v=ntohl(requested.s_addr);
 if(v==0||v==0xffffffffUL||(v>>24)==127||(v>>28)>=14)return -1;
 if(getifaddrs(&all)!=0)return -1;
 for(struct ifaddrs *p=all;p;p=p->ifa_next){
  if(!p->ifa_addr||!p->ifa_name||p->ifa_addr->sa_family!=AF_INET||!(p->ifa_flags&IFF_UP)||(p->ifa_flags&IFF_LOOPBACK))continue;
  if(((struct sockaddr_in*)p->ifa_addr)->sin_addr.s_addr!=requested.s_addr)continue;
  size_t n=strlen(p->ifa_name);if(!n||n>=sizeof selected)goto done;
  for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)p->ifa_name[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))goto done;}
  if(selected[0]&&strcmp(selected,p->ifa_name))goto done;
  memcpy(selected,p->ifa_name,n+1);
 }
 if(selected[0]&&strlen(selected)<capacity){strcpy(name,selected);result=0;}
 done:freeifaddrs(all);return result;
}
