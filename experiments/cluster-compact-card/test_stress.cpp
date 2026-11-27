#include "card_compose.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
using namespace compactcard;
static uint32_t seed=0xA6C70014u;
static uint32_t rnd(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(){
 int count=0;
 for(int trial=0;trial<450;++trial){
  int w=(trial%9==0)?800:24+(int)(rnd()%210), h=(trial%9==0)?298:20+(int)(rnd()%130);
  int stride=w*4+(int)(rnd()%8)*4;
  std::vector<uint8_t> map(stride*h+64,0xa1),card(stride*h+64,0xa2),out(stride*h+64,0x7c);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   size_t pos=(size_t)y*stride+(size_t)x*4;
   for(int c=0;c<3;++c){map[pos+c]=(uint8_t)rnd();card[pos+c]=(uint8_t)rnd();}
   map[pos+3]=255;card[pos+3]=(uint8_t)(rnd()%256);
  }
  int rw=1+(int)(rnd()%w),rh=1+(int)(rnd()%h);
  int rx=(int)(rnd()%(w-rw+1)),ry=(int)(rnd()%(h-rh+1));
  int ax=(rnd()%4==0)?-(int)(rnd()%rw):(int)(rnd()%w);
  int ay=(rnd()%4==0)?-(int)(rnd()%rh):(int)(rnd()%h);
  int pct=30+(int)(rnd()%71);
  int dw=std::max(1,(rw*pct+50)/100),dh=std::max(1,(rh*pct+50)/100);
  auto r=composite({map.data(),w,h,stride},{card.data(),w,h,stride},{out.data(),w,h,stride},{rx,ry,rw,rh},ax,ay,pct);
  assert(r.ok&&r.card_rect.w==dw&&r.card_rect.h==dh);
  for(int y=0;y<h;++y){
   for(int x=0;x<w;++x){
    size_t pos=(size_t)y*stride+(size_t)x*4;
    if(x<ax||x>=ax+dw||y<ay||y>=ay+dh)
     for(int c=0;c<4;++c)assert(out[pos+c]==map[pos+c]);
    else assert(out[pos+3]==255);
   }
   for(int z=w*4;z<stride;++z)assert(out[(size_t)y*stride+z]==0x7c);
  }
  for(size_t k=(size_t)stride*h;k<out.size();++k)assert(out[k]==0x7c);
  assert(!composite({map.data(),w,h,stride},{card.data(),w,h,stride},{out.data(),w,h,stride},{rx,ry,rw,rh},ax,ay,29).ok);
  ++count;
 }
 std::printf("PASS %d randomized cases; background bit-exact; guard bytes intact\n",count);
}
