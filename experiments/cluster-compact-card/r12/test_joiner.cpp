#include "dual_stream_joiner.h"
#include <cassert>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
static const int W=800,H=298;
static const size_t N=(size_t)W*H*3/2;
static const compact_card_r9::Config cfg={{170,12,350,118},70};
static const unsigned long long E=0x992371,T=8912378000ULL;
static void make_inputs(std::vector<unsigned char>&map,std::vector<unsigned char>&card){
 map.resize(N);card.resize(N);
 for(size_t i=0;i<N;i++)map[i]=(unsigned char)((i*23+51)%256);
 card=map;
 for(int plane=0;plane<3;plane++){
  int sub=plane?2:1,pw=W/sub;
  size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
  for(int y=cfg.original.y/sub;y<(cfg.original.y+cfg.original.h)/sub;y++)
   for(int x=cfg.original.x/sub;x<(cfg.original.x+cfg.original.w)/sub;x++)
    card[off+(size_t)y*pw+x]=plane?128:239;
 }
 for(int y=18;y<40;y++)for(int x=182;x<220;x++)card[(size_t)y*W+x]=21;
}
static void assert_map_preserved(const std::vector<unsigned char>&out,
                                 const std::vector<unsigned char>&map){
 int dw=((cfg.original.w*70+50)/100)&~1,dh=((cfg.original.h*70+50)/100)&~1;
 for(int p=0;p<3;p++){
  int sub=p?2:1,pw=W/sub,ph=H/sub;
  int x0=cfg.original.x/sub,y0=cfg.original.y/sub;
  int x1=x0+dw/sub,y1=y0+dh/sub;
  size_t off=p?(size_t)W*H+(p==2?(size_t)W*H/4:0):0;
  for(int y=0;y<ph;y++)for(int x=0;x<pw;x++){
   if(x<x0||x>=x1||y<y0||y>=y1)
    assert(out[off+(size_t)y*pw+x]==map[off+(size_t)y*pw+x]);
  }
 }
}
int main(){
 std::vector<unsigned char> map,card,out(N,77);
 make_inputs(map,card);
 r12::Joiner join(cfg);
 assert(join.begin(E));
 assert(!join.render(E,T+100,out.data(),out.size()));
 assert(!join.submit(110,E,T,map.data(),W,H));
 assert(join.submit(111,E,T,map.data(),W,H));
 assert(!join.render(E,T+100,out.data(),out.size()));
 assert(!join.submit(111,E,T,map.data(),W,H));
 assert(!join.submit(112,E+1,T,card.data(),W,H));
 assert(join.submit(112,E,T+33000,card.data(),W,H));
 assert(join.render(E,T+40000,out.data(),out.size()));
 assert_map_preserved(out,map);
 assert(!join.render(E,T+300000,out.data(),out.size()));
 assert(!join.render(E+1,T+40000,out.data(),out.size()));
 join.drop(112,E);
 assert(!join.render(E,T+40000,out.data(),out.size()));
 assert(join.submit(112,E,T+66000,card.data(),W,H));
 assert(join.render(E,T+70000,out.data(),out.size()));
 assert(join.submit(112,E,T+200000,card.data(),W,H));
 assert(!join.render(E,T+200000,out.data(),out.size()));
 assert(join.begin(E+2));
 assert(!join.render(E+2,T+200000,out.data(),out.size()));
 assert(!join.submit(111,E,T+210000,map.data(),W,H));
 assert(join.submit(111,E+2,T+210000,map.data(),W,H));
 assert(join.submit(112,E+2,T+220000,card.data(),W,H));
 assert(join.render(E+2,T+230000,out.data(),out.size()));
 assert_map_preserved(out,map);
 std::atomic<int> bad(0),readers(0);
 unsigned long long S=E+44,base=T+1000000;
 assert(join.begin(S));
 std::thread m([&](){for(int i=1;i<=250;i++)
    if(!join.submit(111,S,base+(unsigned long long)i*1000,map.data(),W,H))bad++;
 });
 std::thread c([&](){for(int i=1;i<=250;i++)
    if(!join.submit(112,S,base+(unsigned long long)i*1000,card.data(),W,H))bad++;
 });
 std::thread r1([&](){std::vector<unsigned char> buf(N);
   for(int i=0;i<120;i++)if(join.render(S,base+251000,buf.data(),buf.size())){
     assert_map_preserved(buf,map);readers++;
   }
 });
 m.join();c.join();r1.join();
 assert(bad.load()==0);
 assert(join.render(S,base+251000,out.data(),out.size()));
 assert_map_preserved(out,map);
 std::cout<<"PASS R12 map111+card112, 70pct original card, map pixels invariant; 110 isolated, stale/skew/epoch/reconnect guards, concurrent map/card/renderer, renders="<<readers.load()<<"\n";
}
