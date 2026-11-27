#include "r15_reader.h"
extern "C" {
#include "alt111.h"
}
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <signal.h>
static std::vector<uint8_t> readall(const char*p){
 std::ifstream f(p,std::ios::binary);assert(f);
 return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
}
static int listen_local(unsigned &port){
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;
 a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 assert(bind(fd,(struct sockaddr*)&a,sizeof(a))==0);
 assert(listen(fd,1)==0);
 socklen_t n=sizeof(a);assert(getsockname(fd,(struct sockaddr*)&a,&n)==0);
 port=ntohs(a.sin_port);return fd;
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 signal(SIGPIPE,SIG_IGN);
 const std::vector<uint8_t> config=readall(argv[1]),avcc=readall(argv[2]);
 const uint64_t cid=12345,epoch=777;
 const int W=800,H=298; const size_t N=(size_t)W*H*3/2;
 compact_card_r9::Config C={{170,12,350,118},70};
 r12::Joiner join(C);assert(join.begin(epoch));
 r15::Reader reader(join);assert(reader.begin(cid,epoch));
 struct alt111_video v;alt111_video_init(&v);
 const uint64_t stream=alt111_video_begin(&v,cid);
 assert(alt111_video_config(&v,stream,config.data(),config.size())==ALT111_OK);
 assert(alt111_video_attach(&v)==ALT111_OK);
 std::vector<uint8_t> map(N),out(N);
 for(size_t i=0;i<N;i++)map[i]=(uint8_t)((i*13+19)%256);
 unsigned port;int server=listen_local(port);
 int tx=r15_connect_local(port);assert(tx>=0);
 int rx=accept(server,NULL,NULL);assert(rx>=0);
 uint64_t seq=0,ts=0;
 for(int i=0;i<3;i++){
  assert(alt111_video_submit(&v,stream,avcc.data(),avcc.size(),1)==ALT111_OK);
  const uint8_t*p=NULL;size_t n=0;struct alt111_output_ticket t;
  assert(alt111_video_peek(&v,&p,&n,&t)==ALT111_OK);
  assert(t.idr&&t.offset==0);
  ts=r15_clock();assert(ts);
  assert(join.submit(111,epoch,ts,map.data(),W,H));
  assert(r15_send_record(tx,cid,t.sequence,ts,t.idr,p,n)==0);
  seq=t.sequence;
  assert(alt111_video_advance(&v,&t,n)==ALT111_OK);
  assert(reader.next(rx));
 }
 assert(reader.frames()==3);
 assert(join.render(epoch,r15_clock(),out.data(),out.size()));
 unsigned changed=0;
 for(int plane=0;plane<3;plane++){
   const int d=plane?2:1,w=W/d,h=H/d;
   const size_t off=plane?(size_t)W*H+(plane==2?(size_t)W*H/4:0):0;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){
     const size_t i=off+(size_t)y*w+x;
     if(x<170/d||x>=(170+244)/d||y<12/d||y>=(12+82)/d)
       assert(out[i]==map[i]);
     else changed+=(out[i]!=map[i]);
   }
 }
 assert(changed>10000);
 /* A sequence gap must immediately revoke the previously displayed card. */
 uint8_t fake[8]={0,0,0,1,0x65,0,0,0};
 assert(r15_send_record(tx,cid,seq+2,r15_clock(),1,fake,sizeof(fake))==0);
 assert(!reader.next(rx));
 assert(!join.render(epoch,r15_clock(),out.data(),out.size()));
 close(tx);close(rx);
 /* Reconnect uses a new epoch and requires first IDR+config. */
 assert(join.begin(epoch+1));assert(reader.begin(cid+1,epoch+1));
 tx=r15_connect_local(port);assert(tx>=0);rx=accept(server,NULL,NULL);assert(rx>=0);
 assert(r15_send_record(tx,cid+1,1,r15_clock(),0,fake,sizeof(fake))==0);
 assert(!reader.next(rx));close(tx);close(rx);
 assert(reader.begin(cid+1,epoch+1));
 tx=r15_connect_local(port);assert(tx>=0);rx=accept(server,NULL,NULL);assert(rx>=0);
 close(tx);assert(!reader.next(rx));close(rx);
 close(server);alt111_video_destroy(&v);
 std::cout<<"PASS: exact upstream AVCC parser -> real TCP loopback -> libavcodec -> R12 (3 pictures), map pixels exact, sequence loss/first IDR/disconnect guarded. Synthetic source only.\n";
}
