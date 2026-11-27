#include "card_compose.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace compactcard;
int main(){
 const int w=800,h=298;std::vector<uint8_t> map(w*h*4),card(w*h*4,0),out(w*h*4,7);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
    uint8_t* p=map.data()+(y*w+x)*4;
    p[0]=uint8_t(x%255);p[1]=uint8_t(y%255);p[2]=45;p[3]=255;
 }
 Rect roi{16,20,338,126};
 for(int y=roi.y;y<roi.y+roi.h;++y)for(int x=roi.x;x<roi.x+roi.w;++x){
    uint8_t* p=card.data()+(y*w+x)*4;
    p[0]=245;p[1]=245;p[2]=245;p[3]=255;
 }
 Frame bg{map.data(),w,h,w*4};Frame cp{card.data(),w,h,w*4};
 MutableFrame dest{out.data(),w,h,w*4};
 auto r=composite(bg,cp,dest,roi,16,20,70);
 assert(r.ok&&r.card_rect.w==237&&r.card_rect.h==88);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   size_t pos=(y*w+x)*4;
   if(x<16||x>=253||y<20||y>=108)
     for(int k=0;k<4;++k)assert(map[pos+k]==out[pos+k]);
 }
 assert(out[(22*w+22)*4]==245);
 assert(!composite(bg,cp,dest,roi,16,20,101).ok);
 assert(!composite(bg,cp,dest,Rect{799,20,338,126},16,20,70).ok);
 assert(!composite(bg,cp,{map.data(),w,h,w*4},roi,16,20,70).ok);
 assert(composite(bg,cp,dest,roi,-30,-20,60).ok);
 std::puts("PASS card 70% / background unchanged / bounds / invalid config / clipping");
}
