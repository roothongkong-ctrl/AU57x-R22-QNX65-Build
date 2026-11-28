/* R17 host integration: actual FFmpeg H264 through TCP sidecar thread,
 * player 111 decoded buffer composition, source-only; not real phone.
 */
#include "r17_card_sidecar.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static const int W=800,H=298;
static const size_t N=(size_t)W*H*3/2;
static const compact_card_r9::Config CFG={{170,12,350,118},70};
static unsigned long long clock_us(){
 struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
 return (unsigned long long)ts.tv_sec*1000000ULL+(unsigned long long)ts.tv_nsec/1000ULL;
}
static std::vector<unsigned char> readall(const char*path){
 std::ifstream f(path,std::ios::binary);assert(f.good());
 return std::vector<unsigned char>(std::istreambuf_iterator<char>(f),
                                   std::istreambuf_iterator<char>());
}
static void map_fill(std::vector<unsigned char>&map){
 map.resize(N);
 for(size_t i=0;i<N;i++)map[i]=(unsigned char)((i*43+19)%256);
}
static bool same_map_outside(const std::vector<unsigned char>&a,
                             const std::vector<unsigned char>&b){
 int ow=((CFG.original.w*70+50)/100)&~1;
 int oh=((CFG.original.h*70+50)/100)&~1;
 bool changed=false;
 for(int plane=0;plane<3;plane++){
  int sub=plane?2:1,pw=W/sub,ph=H/sub,x0=CFG.original.x/sub,y0=CFG.original.y/sub;
  int x1=x0+ow/sub,y1=y0+oh/sub;
  size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
  for(int y=0;y<ph;y++)for(int x=0;x<pw;x++){
   size_t pos=off+(size_t)y*pw+x;
   if(x<x0||x>=x1||y<y0||y>=y1){
    if(a[pos]!=b[pos])return false;
   }else if(a[pos]!=b[pos])changed=true;
  }
 }
 return changed;
}
static bool sendall(int fd,const unsigned char*buf,size_t n){
 size_t i=0;
 while(i<n){
   ssize_t k=send(fd,buf+i,n-i,0);
   if(k<=0)return false;
   i+=(size_t)k;
 }
 return true;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 const std::vector<unsigned char> annexb=readall(argv[1]);
 assert(!annexb.empty());
 const int fd=socket(AF_INET,SOCK_STREAM,0);
 assert(fd>=0);
 int one=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
 struct sockaddr_in bindto;memset(&bindto,0,sizeof(bindto));
 bindto.sin_family=AF_INET;bindto.sin_port=htons(19822);
 bindto.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(fd,(struct sockaddr*)&bindto,sizeof(bindto))==0);
 assert(listen(fd,1)==0);
 std::atomic<bool> finished(false),failed(false);
 std::thread server([&](){
  const int client=accept(fd,NULL,NULL);
  if(client<0){failed.store(true);return;}
  for(size_t i=0;i<annexb.size();i+=193){
   size_t n=annexb.size()-i;if(n>193)n=193;
   if(!sendall(client,annexb.data()+i,n)){failed.store(true);break;}
   usleep(120);
  }
  // Keep source connected while 111 decodes & renders card.
  for(int k=0;k<240&&!finished.load();k++)usleep(10000);
  shutdown(client,SHUT_RDWR);close(client);
 });
 r17::Sidecar card;
 assert(card.start(clock_us()));
 std::vector<unsigned char> original,frame;
 map_fill(original);
 frame=original;
 assert(!card.compose_111(frame.data(),W,H,clock_us()));
 assert(frame==original);
 int draws=0;
 for(int i=0;i<160&&!failed.load();i++){
  frame=original;
  if(card.compose_111(frame.data(),W,H,clock_us())){
    assert(same_map_outside(frame,original));
    draws++;
    break;
  }
  usleep(10000);
 }
 finished.store(true);
 server.join();
 assert(!failed.load());
 assert(draws>0); /* real FFmpeg decoder filled R12 Joiner */
 // Only after sidecar EOF do we know 112 is dropped; no stale overlay.
 usleep(260000);
 frame=original;
 assert(!card.compose_111(frame.data(),W,H,clock_us()));
 assert(frame==original);
 card.stop();
 frame=original;
 assert(!card.compose_111(frame.data(),W,H,clock_us()));
 assert(frame==original);
 // Invalid dimensions are never silently resized to fit ROI.
 r17::Sidecar off;
 assert(off.start(clock_us()));
 frame=original;
 assert(!off.compose_111(frame.data(),640,360,clock_us()));
 off.stop();
 std::cout<<"PASS R17 real H264 TCP sidecar->FFmpeg 112->R12 joiner->"
          <<"original 111 buffer exact-map 70% card, no 112/stale/wrong-size fail-close; "
          <<"generated fixture ONLY\n";
 close(fd);
}
