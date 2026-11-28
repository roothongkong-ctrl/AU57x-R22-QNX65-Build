/* Exercise actual AirPlay wrapper with fixture callbacks, never OEM. */
#include "mu1005_r34_ap_server.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct{int mode,live,sets,starts,stops,polls;} F;
static int live(void*v){return ((F*)v)->live;}
static int cf(void*v,const void*p){(void)v;return p!=NULL;}
static int name(void*v,const void*p,char*out,size_t cap){(void)v;(void)p;assert(cap>4);memcpy(out,"uap0",5);return 4;}
static int set(void*v,void*s,const void*k,const void*p){F*f=v;assert(s&&k&&p);f->sets++;if(f->mode==0)f->live=0;return 0;}
static int control(void*v,void*s,const void*k,const void*o){F*f=v;assert(s&&k&&o);if(!strcmp(k,"start"))f->starts++;else {f->stops++;if(f->mode==2)f->live=0;}return 0;}
static int poll_listen(void*v,void*s,const void*o,unsigned port){F*f=v;assert(s&&o&&port==5000);f->polls++;if(f->mode==1)f->live=0;return 1;}
int main(int argc,char**argv){
 assert(argc==2);int mode=atoi(argv[1]);assert(mode>=0&&mode<=2);
 F f={.mode=mode,.live=1};Mu34ApServer s={0};
 Mu34ApServerBinding b={.library_verified=live,.cf_string_ok=cf,.cf_string_utf8=name,.owner_thread_ok=live,.set_property=set,.server_control=control,.listening_verified=poll_listen,.opaque=&f,.server=&f,.owner=&f,.interface_key="iface",.start_command="start",.stop_command="stop",.generation=7};
 assert(mu34_ap_server_init(&s,&b)==0);
 int rc=mu34_ap_server_set_interface(&s,7,"uap0");
 if(mode==0){assert(rc==MU34_AP_ERR_NOT_OWNER&&!s.interface_set&&f.sets==1&&!f.starts);}
 else{
  assert(rc==MU34_AP_OK);
  rc=mu34_ap_server_start(&s,7,5000);
  if(mode==1){assert(rc==MU34_AP_ERR_NOT_OWNER&&s.start_requested&&!s.listening_verified&&f.starts==1&&f.polls==1);}
  else{assert(rc==MU34_AP_R_LISTENING_VERIFIED);rc=mu34_ap_server_stop(&s,7);assert(rc==MU34_AP_ERR_NOT_OWNER&&s.start_requested&&!s.listening_verified&&f.stops==1);}
  assert(mu34_ap_server_close(&s,7)==MU34_AP_ERR_STATE);
 }
 assert(s.state==MU34_AP_FAILED);
 printf("RECEIVER_OWNER_REVOCATION_%d_PASS_FIXTURE_ONLY\n",mode);
}
