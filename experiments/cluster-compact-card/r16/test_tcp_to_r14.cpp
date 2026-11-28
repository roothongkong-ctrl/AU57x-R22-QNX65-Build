/* R16 HOST end-to-end acceptance: official alt111 AVCC parser and real
 * local TCP delivery, FFmpeg H264 decoder, R12 map-preserving 70% compositor.
 * Uses locally generated ORIGINAL H264 data (not any GaoDe/iPhone capture).
 */
#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
extern "C" {
#include "alt111.h"
#include "r16_card_tee.h"
}
#include "../r14/card112_ffmpeg_decoder.h"
static const int W=800,H=298;
static const size_t YUV=(size_t)W*H*3/2;
static const uint64_t NOW=87123000000ULL;
static const compact_card_r9::Config CFG={{170,12,350,118},70};

static std::vector<unsigned char> read_file(const char*path){
 std::ifstream f(path,std::ios::binary);
 assert(f.good());
 return std::vector<unsigned char>(std::istreambuf_iterator<char>(f),
                                   std::istreambuf_iterator<char>());
}
static size_t start_code(const std::vector<unsigned char>&s,size_t p,size_t&n){
 for(;p+3<s.size();p++){
  if(s[p]==0&&s[p+1]==0){
   if(s[p+2]==1){n=3;return p;}
   if(p+3<s.size() &&s[p+2]==0&&s[p+3]==1){n=4;return p;}
  }
 }
 n=0;return s.size();
}
static void be16(std::vector<unsigned char>&o,size_t n){
 assert(n<65536);o.push_back((unsigned char)(n>>8));o.push_back((unsigned char)n);
}
static void be32(std::vector<unsigned char>&o,size_t n){
 assert(n<INT_MAX);o.push_back((unsigned char)(n>>24));o.push_back((unsigned char)(n>>16));
 o.push_back((unsigned char)(n>>8));o.push_back((unsigned char)n);
}
static void append_nal(std::vector<unsigned char>&au,
                       const std::vector<unsigned char>&nal){
 be32(au,nal.size());au.insert(au.end(),nal.begin(),nal.end());
}
struct Input{
 std::vector<unsigned char> avcc,au;
};
static Input from_real_annexb(const std::vector<unsigned char>&encoded){
 std::vector<unsigned char> sps,pps,idr,sei;
 size_t pos=0,n=0;
 while(true){
  size_t start=start_code(encoded,pos,n);
  if(start==encoded.size())break;
  size_t begin=start+n,k=0;
  size_t next=start_code(encoded,begin,k);
  size_t end=next==encoded.size()?encoded.size():next;
  if(begin<end){
   std::vector<unsigned char> nal(encoded.begin()+begin,encoded.begin()+end);
   unsigned type=nal.front()&31u;
   if(type==7&&sps.empty())sps=nal;
   if(type==8&&pps.empty())pps=nal;
   if(type==5&&idr.empty())idr=nal;
   if(type==6&&sei.empty())sei=nal;
  }
  pos=end;
 }
 assert(sps.size()>=4&&pps.size()>=2&&!idr.empty());
 Input out;
 out.avcc.push_back(1); // version
 out.avcc.push_back(sps[1]);out.avcc.push_back(sps[2]);out.avcc.push_back(sps[3]);
 out.avcc.push_back(0xFF); // NAL size is 4 bytes
 out.avcc.push_back(0xE1); // exactly one SPS
 be16(out.avcc,sps.size());out.avcc.insert(out.avcc.end(),sps.begin(),sps.end());
 out.avcc.push_back(1); // exactly one PPS
 be16(out.avcc,pps.size());out.avcc.insert(out.avcc.end(),pps.begin(),pps.end());
 if(!sei.empty())append_nal(out.au,sei);
 append_nal(out.au,idr);
 return out;
}
static bool map_unchanged_outside(const std::vector<unsigned char>&out,
                                  const std::vector<unsigned char>&map){
 const int cw=((CFG.original.w*70+50)/100)&~1;
 const int ch=((CFG.original.h*70+50)/100)&~1;
 bool overlay_changed=false;
 for(int plane=0;plane<3;plane++){
  const int factor=plane?2:1,pw=W/factor,ph=H/factor;
  const int x0=CFG.original.x/factor,y0=CFG.original.y/factor;
  const int x1=x0+cw/factor,y1=y0+ch/factor;
  const size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
  for(int y=0;y<ph;y++)for(int x=0;x<pw;x++){
   const size_t i=off+(size_t)y*pw+x;
   if(x<x0||x>=x1||y<y0||y>=y1){
    if(map[i]!=out[i])return false;
   }else if(map[i]!=out[i])overlay_changed=true;
  }
 }
 return overlay_changed;
}
static int once(const Input&in,int port){
 struct alt111_video video;
 alt111_video_init(&video);
 const uint64_t stream=alt111_video_begin(&video,44331);
 assert(stream!=0);
 struct r16_card_tee tee;
 assert(r16_card_tee_open(&tee,&video,port)==0);
 assert(tee.listen_fd>=0&&tee.client_fd<0);
 assert(alt111_video_config(&video,stream,in.avcc.data(),in.avcc.size())==ALT111_OK);
 // Opening the sidecar must not change 111 stream: the core lives only here.
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 struct sockaddr_in to;memset(&to,0,sizeof(to));
 to.sin_family=AF_INET;to.sin_port=htons((uint16_t)port);
 to.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(connect(fd,(struct sockaddr*)&to,sizeof(to))==0);
 r16_card_tee_pump(&tee);assert(tee.connected==1);
 const size_t expected=video.config_length+3u*(4u+in.au.size()-
       (in.au.size()>4?(in.au.size()>=8?8u:4u):4u)); // overwritten below
 (void)expected;
 // Each complete AVCC AU is a separate keyframe. Need 3 for parser flush.
 for(int i=0;i<3;i++){
  int rc=alt111_video_submit(&video,stream,in.au.data(),in.au.size(),1);
  assert(rc==ALT111_OK);
  r16_card_tee_pump(&tee);
 }
 const size_t wanted=video.config_length+
     3u*(in.au.size()); // each four-byte AVCC NAL size becomes a four-byte startcode
 std::vector<unsigned char> incoming;
 incoming.reserve(wanted+4096);
 for(int n=0;n<350 && incoming.size()<wanted;n++){
  r16_card_tee_pump(&tee);
  unsigned char buf[8192];
  ssize_t got=recv(fd,buf,sizeof(buf),MSG_DONTWAIT);
  if(got>0)incoming.insert(incoming.end(),buf,buf+got);
  else if(got<0 && errno!=EAGAIN && errno!=EWOULDBLOCK)assert(false);
  if(incoming.size()<wanted)usleep(500);
 }
 assert(incoming.size()==wanted);
 assert(incoming.size()>=video.config_length);
 assert(tee.sent_bytes==wanted);
 assert(tee.errors==0);
 r12::Joiner join(CFG);
 const uint64_t epoch=88201;
 assert(join.begin(epoch));
 std::vector<unsigned char> map(YUV),out(YUV);
 for(size_t i=0;i<YUV;i++)map[i]=(unsigned char)((i*31+4)%256);
 assert(join.submit(r12::CLEAN_MAP_111,epoch,NOW+3000,map.data(),W,H));
 r14::Card112Decoder decoder(join);
 assert(decoder.ready()&&decoder.begin(epoch));
 // Feed different network chunk sizes, no assumption of H264 packet boundary.
 for(size_t i=0;i<incoming.size();i+=137){
  size_t n=incoming.size()-i;if(n>137)n=137;
  assert(decoder.feed_annexb(incoming.data()+i,n,NOW+4000));
 }
 assert(decoder.finish(NOW+5000));
 assert(decoder.frames()>=1);
 assert(join.render(epoch,NOW+5500,out.data(),out.size()));
 assert(map_unchanged_outside(out,map));
 // Losing the 112 decoder invalidates card overlay; not the clean map.
 decoder.drop();
 assert(!join.render(epoch,NOW+6000,out.data(),out.size()));
 // An unnegotiated session cannot silently reuse the old card.
 assert(join.begin(epoch+1));
 assert(!join.render(epoch+1,NOW+6500,out.data(),out.size()));
 close(fd);
 r16_card_tee_pump(&tee);
 r16_card_tee_close(&tee);
 alt111_video_destroy(&video);
 std::cout<<"PASS real AVCC->AnnexB local TCP->FFmpeg decode->R12 70pct card; "
    <<"bytes="<<wanted<<" decoded_frames="<<decoder.frames()
    <<" map_outside_exact=no-change, no real GaoDe video\n";
 return 0;
}
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"usage: test_r16 encoded.h264 port\n";return 2;}
 const Input sample=from_real_annexb(read_file(argv[1]));
 const int port=atoi(argv[2]);if(port<1024||port>65535)return 2;
 return once(sample,port);
}
