#include "compact_card_r9.h"
#include <cassert>
#include <vector>
#include <cstdio>
#include <cstring>
#include <iostream>
using namespace compact_card_r9;
static void put32(unsigned char*p,unsigned v){for(int j=0;j<4;j++)p[j]=(unsigned char)(v>>(j*8));}
static void put64(unsigned char*p,unsigned long long v){put32(p,(unsigned)v);put32(p+4,(unsigned)(v>>32));}
static void write_map(const char*path,const unsigned char*map,int w,int h,
 unsigned long long t,bool shortfile=false,int fakew=0) {
 FILE*f=fopen(path,"wb");assert(f);
 unsigned char hdr[24]={};
 memcpy(hdr,"CMBG",4);put32(hdr+4,(unsigned)(fakew?fakew:w));
 put32(hdr+8,(unsigned)h);put64(hdr+12,t);
 size_t n=bytes_i420(w,h);assert(fwrite(hdr,1,24,f)==24);
 assert(fwrite(map,1,shortfile?n/2:n,f)==(shortfile?n/2:n));fclose(f);
}
int main() {
 const int W=800,H=298;
 const size_t N=bytes_i420(W,H);
 assert(N==357600);
 Config cfg={{170,12,350,118},70};
 assert(valid_config(W,H,cfg));
 std::vector<unsigned char> map(N),full(N),out(N),backup(N);
 for(size_t i=0;i<N;i++){map[i]=(unsigned char)((31*i+17)%256);full[i]=map[i];}
 /* Fake opaque, original white card, drawn over the real map */
 for(int plane=0;plane<3;plane++) {
  int sub=plane?2:1,pw=W/sub;
  size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
  for(int y=cfg.original.y/sub;y<(cfg.original.y+cfg.original.h)/sub;y++)
   for(int x=cfg.original.x/sub;x<(cfg.original.x+cfg.original.w)/sub;x++)
    full[off+(size_t)y*pw+x]=(unsigned char)(plane?128:235);
 }
 for(int y=28;y<50;y++)for(int x=182;x<220;x++)full[(size_t)y*W+x]=32;
 backup=full;
 assert(composite(full.data(),map.data(),out.data(),W,H,cfg));
 const int dw=((cfg.original.w*70+50)/100)&~1;
 const int dh=((cfg.original.h*70+50)/100)&~1;
 assert(dw==244&&dh==82);
 int changed=0;
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  size_t i=(size_t)y*W+x;
  bool hit=(x>=170&&x<170+dw&&y>=12&&y<12+dh);
  if(!hit)assert(out[i]==map[i]);
  else if(out[i]!=map[i])changed++;
 }
 assert(changed>10000);
 for(int plane=1;plane<3;plane++) {
  size_t off=(size_t)W*H+(plane==2?(size_t)W*H/4:0);
  for(int y=0;y<H/2;y++)for(int x=0;x<W/2;x++) {
   if(x<85||x>=85+dw/2||y<6||y>=6+dh/2)
    assert(out[off+(size_t)y*(W/2)+x]==map[off+(size_t)y*(W/2)+x]);
  }
 }
 assert(full==backup);
 assert(!valid_config(W,H,Config{{170,13,350,118},70}));
 assert(!valid_config(800,480,cfg));
 assert(!valid_config(W,H,Config{{170,12,700,118},70}));
 assert(!valid_config(W,H,Config{{170,12,350,118},140}));
 assert(!composite(full.data(),map.data(),full.data(),W,H,cfg));
 const char *p="/tmp/r9_card_map_test.i420";const unsigned long long t=817234567890ULL;
 assert(try_frame(full.data(),W,H,t,NULL,p)==NULL);
 assert(try_frame(full.data(),W,H,t,"170,12,350,118",p)==NULL);
 write_map(p,map.data(),W,H,t);
 assert(try_frame(full.data(),W,H,t-1,"170,12,350,118",p)==NULL);
 assert(try_frame(full.data(),W,H,t+150001,"170,12,350,118",p)==NULL);
 const unsigned char*result=try_frame(full.data(),W,H,t+150000,"170,12,350,118",p);
 assert(result&&memcmp(result,out.data(),N)==0);
 write_map(p,map.data(),W,H,t,true);
 assert(try_frame(full.data(),W,H,t+1,"170,12,350,118",p)==NULL);
 write_map(p,map.data(),W,H,t,false,808);
 assert(try_frame(full.data(),W,H,t+1,"170,12,350,118",p)==NULL);
 unsigned long long rng=1;
 for(int j=0;j<400;j++) {
  for(size_t i=0;i<N;i++){rng=rng*6364136223846793005ULL+1ULL;map[i]=(unsigned char)(rng>>32);}
  std::vector<unsigned char> mapCopy=map;
  assert(composite(full.data(),map.data(),out.data(),W,H,cfg));
  assert(map==mapCopy&&full==backup);
 }
 remove(p);
 std::cout<<"PASS R9: 70% white card YUV420, preserve map outside card, invalid/stale map -> original frame, source unmodified, 400 frame stress\n";
}
