/* GPL-3.0-only. AU57x C glue to unmodified upstream EntryNav2 link core.
 * Strictly source-only. No vehicle process hook, MFi impersonation, or IO.
 */
#include "au57x_iap2_capi.h"
#include "carplay/iap2_link.hpp"
#include <new>
#include <string>

namespace up=carplay::iap2;
struct au57x_iap2_link;
struct AuSink : public up::ByteSink {
 au57x_iap2_link *o;
 explicit AuSink(au57x_iap2_link *v):o(v){}
 bool sendBytes(const uint8_t *data,size_t n) override;
};
struct AuListener : public up::LinkListener {
 au57x_iap2_link *o;
 explicit AuListener(au57x_iap2_link *v):o(v){}
 void onLinkStateChanged(up::LinkState s) override;
 void onLinkPayload(uint8_t session,const uint8_t *data,size_t n) override;
 void onLinkError(const std::string &error) override;
};
struct au57x_iap2_link {
 au57x_iap2_callbacks cb;
 AuSink sink;
 AuListener listener;
 up::Link link;
 explicit au57x_iap2_link(const au57x_iap2_callbacks &v):
  cb(v),sink(this),listener(this),
  link(sink,listener,up::LinkConfig::wirelessBluetooth()) {}
};
bool AuSink::sendBytes(const uint8_t *p,size_t n){
 return o && o->cb.tx && o->cb.tx(o->cb.ctx,p,n)==1;
}
void AuListener::onLinkStateChanged(up::LinkState s){
 if(o && o->cb.state)o->cb.state(o->cb.ctx,static_cast<int>(s));
}
void AuListener::onLinkPayload(uint8_t session,const uint8_t *p,size_t n){
 if(o && o->cb.rx)o->cb.rx(o->cb.ctx,session,p,n);
}
void AuListener::onLinkError(const std::string &v){
 if(o && o->cb.error)o->cb.error(o->cb.ctx,v.c_str());
}
extern "C" au57x_iap2_link *au57x_iap2_new(const au57x_iap2_callbacks *cb){
 if(!cb||!cb->tx||!cb->rx||!cb->state)return nullptr;
 try {return new (std::nothrow) au57x_iap2_link(*cb);}catch(...){return nullptr;}
}
extern "C" int au57x_iap2_start(au57x_iap2_link *p){
 if(!p||p->link.state()!=up::kLinkIdle)return 0;
 try{p->link.start();return p->link.state()!=up::kLinkFailed;}catch(...){return 0;}
}
extern "C" int au57x_iap2_feed(au57x_iap2_link *p,const uint8_t *b,size_t n){
 if(!p||(!b&&n))return 0;
 try{return p->link.feed(b,n)?1:0;}catch(...){return 0;}
}
extern "C" int au57x_iap2_send(au57x_iap2_link *p,uint8_t session,const uint8_t *b,size_t n){
 if(!p||p->link.state()!=up::kLinkConnected||(!b&&n)||!n)return 0;
 try{return p->link.sendPayload(session,b,n)?1:0;}catch(...){return 0;}
}
extern "C" int au57x_iap2_tick(au57x_iap2_link *p,int64_t ms){
 if(!p||ms<0)return 0;
 try{p->link.tick(ms);return 1;}catch(...){return 0;}
}
extern "C" int au57x_iap2_state(const au57x_iap2_link *p){
 return p?static_cast<int>(p->link.state()):-1;
}
extern "C" void au57x_iap2_reset(au57x_iap2_link *p){
 if(!p)return;
 try{p->link.reset(false);}catch(...){}
}
extern "C" void au57x_iap2_free(au57x_iap2_link *p){delete p;}
