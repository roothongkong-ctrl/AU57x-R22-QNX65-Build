#include "rgd_card70_i420.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
int main(){
 using namespace rgd70;
 const Rect r={170,12,350,118}; // synthetic only, not production ROI
 const size_t n=800*298*3/2;
 const std::vector<unsigned char> original(n,77);
 std::vector<unsigned char> f=original;
 Sample s={10000000ULL,2,450};
 assert(paint(f.data(),n,800,298,r,s,10000001ULL,true));
 assert(f!=original);
 for(int p=0;p<3;p++){
  int d=p?2:1,w=800/d,h=298/d;
  size_t off=p?800*298+(p==2?800*298/4:0):0;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   bool in=x>=r.x/d&&x<(r.x+scaled(r.w))/d&&y>=r.y/d&&y<(r.y+scaled(r.h))/d;
   if(!in)assert(f[off+y*w+x]==original[off+y*w+x]);
  }
 }
 f=original;
 assert(!paint(f.data(),n,800,298,r,s,13000000ULL,true)&&f==original);
 assert(!paint(f.data(),n,800,298,r,s,10000001ULL,false)&&f==original);
 s.direction=28;
 assert(!paint(f.data(),n,800,298,r,s,10000001ULL,true)&&f==original);
 s.direction=1;
 assert(!paint(f.data(),n,800,298,Rect{799,0,350,118},s,10000001ULL,true)&&f==original);
 assert(!paint(f.data(),n-1,800,298,r,s,10000001ULL,true)&&f==original);
 puts("PASS: native I420 card70; all three planes unchanged outside card; invalid/stale map untouched");
}
