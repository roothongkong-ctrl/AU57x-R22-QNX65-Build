/* Host integration of ACTUAL MU1005 inbound.c + R113 + imported GPL3
 * EntryNav2 iAP2 engine, with mocked OEM vendor API callbacks only.
 * No MFi bypass, no actual vehicle connection. */
#include "mu1005_iap2_stream.h"
#include <cassert>
#include <cstdio>
#include <cstring>
struct Fixture {
 Inbound *rf; int usb; int mfi; int sent_count;
 uint8_t sent[1024]; size_t sent_bytes;
};
static Fixture *g;
static int ok(void *){return 1;}
static int wired(void *p){return static_cast<Fixture *>(p)->usb;}
static int mfi(void *p){return static_cast<Fixture *>(p)->mfi;}
static int published(void *,uint8_t ch){return ch==7;}
static int reg(void *,uint8_t *,uint16_t){return 0;}
static int unr(void *,uint8_t *){return 0;}
static int ad(void *){return 0;}
static int rm(void *){return 0;}
static int accept(void *,int){return 2;}
static int maxlen(void *){return 2;}  /* force RF segmentation */
static int tx(void *,void *packet){
 const uint8_t *p=static_cast<const uint8_t *>(packet);
 unsigned n=(unsigned)p[12]|((unsigned)p[13]<<8);
 assert(n>0 && n<=2 && g->sent_bytes+n<sizeof g->sent);
 std::memcpy(g->sent+g->sent_bytes,g->rf->tx,n);
 g->sent_bytes+=n;g->sent_count++;return 2; /* SDK queued; not sent yet */
}
static int disc(void *){return 0;}
static int opened(void *,unsigned){return 1;}
static int closed(void *){return 1;}
static int received(void *,const uint8_t *,unsigned){return 1;}
static int credit(void *,int){return 0;}
static uint32_t addr(const void *){return 0x12345678u;}
static int reg_oem(void *,uint8_t *n){*n=7;return 0;}
static int sdp_oem(void *,uint8_t){return 0;}
static void setup(Fixture *f,Inbound *s,Au108Port *gate){
 std::memset(f,0,sizeof(*f));f->rf=s;f->usb=1;f->mfi=1;g=f;
 std::memset(s,0,sizeof(*s));InboundOps ops={};
 ops.reg=reg;ops.unreg=unr;ops.add=ad;ops.remove=rm;
 ops.accept=accept;ops.limit=maxlen;ops.send=tx;ops.disconnect=disc;
 ops.certify=ok;ops.peer_ok=ok;ops.ready=ok;
 ops.opened=opened;ops.closed=closed;ops.rx=received;
 ops.credit=credit;ops.addr=addr;ops.opaque=f;
 assert(inbound_prepare(s,&ops,0x12345678u)==0);
 /* OEM event code 2 has already returned and left native state IN_LINK. */
 s->state=IN_LINK;s->slot=7;
 au108_init(gate);
 gate->state=AU108_LIVE;gate->generation=10;
 Au108OEM owner={};owner.context=f;
 owner.exact_image_live=ok;owner.on_original_owner_thread=ok;
 owner.paired_phone=ok;owner.mfi_hardware_ready=mfi;
 owner.wired_idle=wired;owner.register_iap2_rfcomm=reg_oem;
 owner.publish_iap2_sdp=sdp_oem;owner.published_iap2_service=published;
 owner.unregister_iap2=disc;owner.callbacks_quiesced=ok;
 gate->owner=owner;
 R105Evidence e={10,7,1,1,1,1,1,0};
 gate->evidence=e;
 assert(r105_gate_arm(&gate->gate,&gate->evidence)==0);
 assert(au108_live(gate)==1);
}
static void tx_complete(Inbound *s){
 assert(s->busy==1);
 assert(inbound_event(s,&s->rf,6,0,NULL,0,addr(&s->packet))==0);
}
int main(){
 Inbound native={};Au108Port gate={};Fixture f={};
 setup(&f,&native,&gate);
 Mu1005Iap2Stream stream={};
 mu1005_iap2_stream_init(&stream,&native,&gate);
 assert(mu1005_iap2_stream_on_open(&stream,9)==-1);
 assert(f.sent_count==0);
 assert(mu1005_iap2_stream_on_open(&stream,10)==0);
 assert(f.sent_count==1 && f.sent_bytes==2 && native.busy==1);
 assert(mu1005_iap2_stream_pump(&stream)==0);/* never overwrite busy OEM packet */
 tx_complete(&native);assert(mu1005_iap2_stream_pump(&stream)==2);
 tx_complete(&native);assert(mu1005_iap2_stream_pump(&stream)==2);
 assert(f.sent_bytes==6 && f.sent_count==3);
 static const uint8_t detect[]={0xff,0x55,0x02,0x00,0xee,0x10};
 assert(std::memcmp(f.sent,detect,sizeof(detect))==0);
 tx_complete(&native);assert(mu1005_iap2_stream_pump(&stream)==0);
 /* Feed real upstream link Detect after owner RX callback; pending SYN is
  * safely buffered, and no OEM packet is issued until caller pumps. */
 assert(mu1005_iap2_stream_on_bytes(&stream,detect,sizeof(detect))==0);
 assert(stream.queued>0);
 assert(native.busy==0);
 assert(mu1005_iap2_stream_pump(&stream)>0);
 /* USB takeover: owner proof invalidated, no further writes; no unsafe
  * unregistration or state resurrection by the transport adapter. */
 const int before=f.sent_count;
 f.usb=0;
 assert(mu1005_iap2_stream_pump(&stream)<0);
 assert(stream.stopped==1 && stream.queued==0);
 assert(gate.state==AU108_SUSPENDED);
 assert(mu1005_iap2_stream_on_bytes(&stream,detect,sizeof(detect))<0);
 assert(f.sent_count==before);
 mu1005_iap2_stream_stop(&stream);
 std::puts("PASS: OEM inbound_send segments upstream iAP2 Detect, waits for code6, USB takeover blocks TX");
 return 0;
}
