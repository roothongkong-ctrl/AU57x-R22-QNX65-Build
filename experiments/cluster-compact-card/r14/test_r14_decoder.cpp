#include "card112_ffmpeg_decoder.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
static const int W=800,H=298;
static const size_t N=(size_t)W*H*3/2;
static const compact_card_r9::Config C={{170,12,350,118},70};
static const uint64_t T=101001001000ULL;
static std::atomic<int> errors(0);
static std::vector<uint8_t> readall(const char*path) {
 std::ifstream f(path,std::ios::binary);
 assert(f);
 return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                             std::istreambuf_iterator<char>());
}
static bool map_preserved(const std::vector<uint8_t>&out,
                          const std::vector<uint8_t>&map){
 const int cw=((C.original.w*70+50)/100)&~1;
 const int ch=((C.original.h*70+50)/100)&~1;
 bool overlay_different=false;
 for(int p=0;p<3;p++) {
    const int div=p?2:1,pw=W/div,ph=H/div;
    const int x0=C.original.x/div,y0=C.original.y/div;
    const int x1=x0+cw/div,y1=y0+ch/div;
    const size_t off=p?(size_t)W*H+(p==2?(size_t)W*H/4:0):0;
    for(int y=0;y<ph;y++)for(int x=0;x<pw;x++){
      size_t i=off+(size_t)y*pw+x;
      if(x<x0||x>=x1||y<y0||y>=y1){
        if(out[i]!=map[i])return false;
      }else if(out[i]!=map[i])overlay_different=true;
    }
 }
 return overlay_different;
}
static bool one_worker(const std::vector<uint8_t>&encoded,int job,size_t step,
                       bool valid_size) {
 r12::Joiner join(C);const uint64_t e=1000+(uint64_t)job;
 if(!join.begin(e))return false;
 std::vector<uint8_t> map(N),out(N,0);
 for(size_t i=0;i<N;i++)map[i]=(uint8_t)((i*11+job*13)%256);
 if(!join.submit(r12::CLEAN_MAP_111,e,T+10000,map.data(),W,H))return false;
 r14::Card112Decoder dec(join);
 if(!dec.ready()||!dec.begin(e))return false;
 for(size_t pos=0;pos<encoded.size();pos+=step){
   size_t n=encoded.size()-pos;
   if(n>step)n=step;
   if(!dec.feed_annexb(encoded.data()+pos,n,T+13000)) {
      if(valid_size)return false;
      break;
   }
 }
 if(valid_size){
    if(!dec.finish(T+16000)||dec.frames()<1)return false;
    if(!join.render(e,T+20000,out.data(),out.size()))return false;
    if(!map_preserved(out,map))return false;
    /* Epoch rotation makes previously decoded frames unusable. */
    if(!join.begin(e+1))return false;
    if(join.render(e+1,T+20000,out.data(),out.size()))return false;
 }else{
    /* Mismatched source geometry cannot be substituted for clean map pixels. */
    if(dec.frames()!=0)return false;
    if(join.render(e,T+20000,out.data(),out.size()))return false;
 }
 dec.drop();
 return true;
}
int main(int argc,char**argv){
 if(argc!=4){std::cerr<<"usage: r14-test valid.h264 wrongsize.h264 workers\n";return 2;}
 const auto good=readall(argv[1]),wrong=readall(argv[2]);
 int workers=std::atoi(argv[3]);
 if(workers<1||workers>8||good.empty()||wrong.empty())return 2;
 std::vector<std::thread> threads;
 for(int i=0;i<workers;i++){
  threads.emplace_back([&,i]{
    if(!one_worker(good,i,((i%3)==0?1:((i%3)==1?17:4096)),true))++errors;
  });
 }
 for(auto &t:threads)t.join();
 if(!one_worker(wrong,99,331,false))++errors;
 if(errors.load()){
   std::cerr<<"R14 FAIL errors="<<errors.load()<<"\n";return 1;
 }
 std::cout<<"PASS R14: real libavcodec H264 decode into I420, "
          <<workers<<" concurrent independent decoder instances, "
          <<"111 map YUV unchanged outside original 112 card 70% bounds; "
          <<"wrong-size video rejected; no live GaoDe phone evidence\n";
}
