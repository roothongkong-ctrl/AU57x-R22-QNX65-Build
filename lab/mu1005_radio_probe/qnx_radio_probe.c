/* Read-only lab probe. No wireless configuration, credentials, or association. */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include "radio_classify.h"
#if IEEE80211_CHAN_2GHZ != MU_CHAN_2 || IEEE80211_CHAN_5GHZ != MU_CHAN_5
#error unexpected QNX channel flag ABI
#endif
int main(int argc,char **argv){
 struct ieee80211req req;
 struct ieee80211req_chaninfo info;
 const char *name=argc==2?argv[1]:"uap0";
 if(argc>2 || (strcmp(name,"uap0")&&strcmp(name,"wlan0")&&strcmp(name,"mlan0")))return 2;
 if(strlen(name)>=sizeof req.i_name)return 2;
 int fd=socket(AF_INET,SOCK_DGRAM,0);
 if(fd<0){perror("socket");return 3;}
 memset(&req,0,sizeof req);memset(&info,0,sizeof info);
 memcpy(req.i_name,name,strlen(name)+1);
 req.i_type=IEEE80211_IOC_CHANINFO;
 req.i_len=sizeof info;req.i_data=&info;
 if(ioctl(fd,SIOCG80211,&req)<0){int e=errno;close(fd);printf("RADIO_QUERY_UNSUPPORTED errno=%d\n",e);return 4;}
 close(fd);
 if(req.i_len<(int)offsetof(struct ieee80211req_chaninfo,ic_chans) ||
    info.ic_nchans>IEEE80211_CHAN_MAX ||
    (size_t)req.i_len<offsetof(struct ieee80211req_chaninfo,ic_chans)+info.ic_nchans*sizeof info.ic_chans[0])return 5;
 unsigned n24=0,n5=0,passive=0,unknown=0;
 for(unsigned i=0;i<info.ic_nchans;i++){
  const struct ieee80211_channel *ch=&info.ic_chans[i];
  int b=mu_channel_band(ch->ic_freq,ch->ic_flags);
  if(b==24)n24++;else if(b==5)n5++;else unknown++;
  if(ch->ic_flags&IEEE80211_CHAN_PASSIVE)passive++;
 }
 printf("RADIO_DRIVER_CHANNELS interface=%s total=%u band24=%u band5=%u passive=%u unknown=%u\n",name,info.ic_nchans,n24,n5,passive,unknown);
 puts("AP_MODE_AND_ASSOCIATION_UNVERIFIED");
 return 0;
}
