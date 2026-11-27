/* R11: REAL parallel stress of R9's re-entrant YUV core and read-only
 * map background loader. NOT an iPhone stream, NOT a QNX/vehicle acceptance test.
 * The R9 try_frame convenience wrapper uses static vectors and must remain
 * single-render-thread-only; this test never calls it from multiple threads.
 */
#include "../r9/compact_card_r9.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

static const int W=800,H=298;
static const size_t N=(size_t)W*H*3/2;
static const compact_card_r9::Config C={{170,12,350,118},70};
static const unsigned long long T=817234567890ULL;
static std::atomic<unsigned long long> verified(0);
static std::atomic<unsigned long long> failures(0);
static std::atomic<bool> start(false);

static void put32(unsigned char*p,unsigned v) {
 for(int j=0;j<4;j++)p[j]=(unsigned char)(v>>(j*8));
}
static void put64(unsigned char*p,unsigned long long v) {
 put32(p,(unsigned)v);put32(p+4,(unsigned)(v>>32));
}
static void make_map_file(const char*path,const std::vector<unsigned char>&map,
                         unsigned long long t,int fakewidth=0,bool trunc=false) {
 FILE*f=fopen(path,"wb");
 if(!f){failures++;return;}
 unsigned char hdr[24]={};memcpy(hdr,"CMBG",4);
 put32(hdr+4,fakewidth?fakewidth:W);put32(hdr+8,H);put64(hdr+12,t);
 if(fwrite(hdr,1,sizeof(hdr),f)!=sizeof(hdr))failures++;
 const size_t count=trunc?N/2:N;
 if(fwrite(map.data(),1,count,f)!=count)failures++;
 fclose(f);
}
static bool pixel_unmodified_outside(const std::vector<unsigned char>&out,
                                     const std::vector<unsigned char>&map) {
 const int dw=((C.original.w*70+50)/100)&~1;
 const int dh=((C.original.h*70+50)/100)&~1;
 for(int plane=0;plane<3;plane++) {
  const int div=plane?2:1,pw=W/div,ph=H/div;
  const int x0=C.original.x/div,y0=C.original.y/div;
  const int x1=x0+dw/div,y1=y0+dh/div;
  const size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
  for(int y=0;y<ph;y++)for(int x=0;x<pw;x++) {
    if(x<x0||x>=x1||y<y0||y>=y1) {
      const size_t i=off+(size_t)y*pw+x;
      if(out[i]!=map[i])return false;
    }
  }
 }
 return true;
}
static void core_worker(int id,int loops) {
 std::vector<unsigned char> map(N),full(N),out(N);
 unsigned long long rng=0x8f7564f3ULL+(unsigned)id;
 while(!start.load(std::memory_order_acquire))std::this_thread::yield();
 for(int frame=0;frame<loops;frame++) {
  for(size_t i=0;i<N;i++) {
   rng=rng*6364136223846793005ULL+1;
   map[i]=(unsigned char)(rng>>32);
  }
  full=map;
  for(int plane=0;plane<3;plane++){
    const int div=plane?2:1,pw=W/div;
    const size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
    const int y0=C.original.y/div,y1=(C.original.y+C.original.h)/div;
    const int x0=C.original.x/div,x1=(C.original.x+C.original.w)/div;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)
      full[off+(size_t)y*pw+x]=(unsigned char)(plane?128:238);
  }
  if(!compact_card_r9::composite(full.data(),map.data(),out.data(),W,H,C)||
     !pixel_unmodified_outside(out,map)) {
    failures++;
    return;
  }
  verified++;
 }
}
static void read_worker(const char*path,const std::vector<unsigned char>&golden,int loops){
 std::vector<unsigned char> buf;
 while(!start.load(std::memory_order_acquire))std::this_thread::yield();
 for(int i=0;i<loops*5;i++){
  if(!compact_card_r9::read_fresh_map(path,W,H,T+100,buf)||
     buf!=golden) { failures++;return; }
  verified++;
 }
}
int main(int argc,char**argv) {
 if(argc!=3){std::cerr<<"usage: test_r11 MODE(cores|reads|fallback) THREADS\n";return 2;}
 const char*mode=argv[1];int workers=atoi(argv[2]);
 if(workers<1||workers>16)return 2;
 if(!compact_card_r9::valid_config(W,H,C))return 2;
 std::vector<std::thread> threads;
 if(!strcmp(mode,"cores")){
   for(int i=0;i<workers;i++)threads.emplace_back(core_worker,i,48);
 } else if(!strcmp(mode,"reads")){
   std::vector<unsigned char> map(N);
   for(size_t i=0;i<N;i++)map[i]=(unsigned char)((i*13+5)%256);
   const char*path="/tmp/r11_parallel_maponly.i420";
   make_map_file(path,map,T);
   for(int i=0;i<workers;i++)threads.emplace_back(read_worker,path,std::cref(map),48);
   start.store(true,std::memory_order_release);
   for(size_t i=0;i<threads.size();i++)threads[i].join();
   if(failures.load())return 1;
   std::vector<unsigned char> tmp;
   make_map_file(path,map,T,808,false);
   if(compact_card_r9::read_fresh_map(path,W,H,T+10,tmp))return 3;
   make_map_file(path,map,T,0,true);
   if(compact_card_r9::read_fresh_map(path,W,H,T+10,tmp))return 4;
   make_map_file(path,map,T);
   if(compact_card_r9::read_fresh_map(path,W,H,T+150001,tmp))return 5;
   if(compact_card_r9::read_fresh_map(path,W,H,T-1,tmp))return 6;
   remove(path);
   std::cout<<"PASS R11 parallel background reads="<<verified.load()
     <<", bad/truncated/stale/future background rejected\n";
   return 0;
 } else if(!strcmp(mode,"fallback")){
   for(int i=0;i<workers;i++)threads.emplace_back([i](){
     std::vector<unsigned char> full(N,17),map(N,23),out(N);
     if(compact_card_r9::try_frame(full.data(),W,H,T,nullptr,
       "/tmp/r11_map_does_not_exist.i420")!=NULL)failures++;
     if(compact_card_r9::composite(full.data(),map.data(),out.data(),
        W,480,C))failures++;
     if(compact_card_r9::composite(full.data(),map.data(),full.data(),
        W,H,C))failures++;
     if(i>=0)verified++;
   });
 } else return 2;
 start.store(true,std::memory_order_release);
 for(size_t i=0;i<threads.size();i++)threads[i].join();
 if(failures.load()){
    std::cerr<<"FAILURES="<<failures.load()<<"\n";return 1;
 }
 std::cout<<"PASS R11 "<<mode<<" with "<<workers<<" concurrent workers, "
          <<verified.load()<<" checked cases; still NO real live map background\n";
 return 0;
}
