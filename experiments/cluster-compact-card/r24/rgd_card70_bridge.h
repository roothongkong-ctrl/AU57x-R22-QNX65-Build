/* Optional local RGI bridge. No traffic leaves the head unit.
 * 24-byte fixed wire: magic RG70, BE32 direction, distance, BE64 monotonic us,
 * BE32 route generation. Invalid direction is an explicit clear.
 * Single receiver in stream-player; no blocking IO in decoder.
 */
#ifndef RGD_CARD70_BRIDGE_H
#define RGD_CARD70_BRIDGE_H
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "rgd_card70_i420.h"
namespace rgd70 {
static inline uint32_t be32(const unsigned char *p) {
 return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
class Receiver {
 int fd_;
 Rect rect_;
 Sample sample_;
 uint64_t begin_;
public:
 Receiver():fd_(-1),rect_{0,0,0,0},sample_{0,0,0},begin_(0){}
 ~Receiver(){stop();}
 bool start(uint64_t now,const char *roi) {
   if(fd_>=0||!now||!roi)return false;
   char extra=0;
   if(sscanf(roi,"%d,%d,%d,%d%c",&rect_.x,&rect_.y,&rect_.w,&rect_.h,&extra)!=4||!valid(rect_))return false;
   fd_=socket(AF_INET,SOCK_DGRAM,0);
   if(fd_<0)return false;
   struct sockaddr_in addr;memset(&addr,0,sizeof addr);
   addr.sin_family=AF_INET;addr.sin_port=htons(19824);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
   int flags=fcntl(fd_,F_GETFL,0);
   if(flags<0||fcntl(fd_,F_SETFL,flags|O_NONBLOCK)<0||
      bind(fd_,(struct sockaddr*)&addr,sizeof addr)<0){stop();return false;}
   begin_=now;return true;
 }
 void stop(){if(fd_>=0)close(fd_);fd_=-1;sample_={0,0,0};begin_=0;}
 bool active()const{return fd_>=0;}
 bool compose_111(unsigned char*p,int w,int h,uint64_t now) {
   if(!active())return false;
   // Drain bounded queue; malformed or clear packets invalidate state.
   for(unsigned i=0;i<32;i++){
     unsigned char b[25];struct sockaddr_in peer;socklen_t len=sizeof peer;
     ssize_t n=recvfrom(fd_,b,sizeof b,0,(struct sockaddr*)&peer,&len);
     if(n<0)break;
     if(n!=24||len<sizeof peer||peer.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||memcmp(b,"RG70",4)) {sample_={0,0,0};continue;}
     uint64_t ts=((uint64_t)be32(b+12)<<32)|be32(b+16);
     unsigned dir=be32(b+4),dist=be32(b+8),generation=be32(b+20);
     if(ts<begin_||ts>now||now-ts>2000000ULL||!generation||dir<1||dir>3||dist>100000) {sample_={0,0,0};continue;}
     if(ts>=sample_.timestamp_us)sample_={ts,dir,dist};
   }
   if(w!=800||h!=298)return false;
   return paint(p,(size_t)w*h*3/2,w,h,rect_,sample_,now,true);
 }
};
}
#endif
