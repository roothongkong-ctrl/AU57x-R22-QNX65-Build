/* Direct RTLD_NOW against the exact extracted OEM libraries. No call to
 * connect or auth: absent production device endpoints must not be faked. */
#include <stdio.h>
#include <dlfcn.h>
#include <stddef.h>
static int probe(const char *label,const char *path,const char *const *names){
 void *h=dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!h){const char *e=dlerror();printf("OEM_LOAD_FAIL %s %s\n",label,e?e:"unknown");return 0;}
 int ok=1;
 for(size_t i=0;names[i];i++){
  (void)dlerror();void *f=dlsym(h,names[i]);const char *e=dlerror();
  if(!f||e){printf("OEM_SYMBOL_FAIL %s %s\n",label,names[i]);ok=0;}
  else printf("OEM_SYMBOL_OK %s %s\n",label,names[i]);
 }
 if(dlclose(h)!=0)ok=0;
 if(ok)printf("OEM_RUNTIME_%s_PASS\n",label);
 return ok;
}
int main(void){
 static const char *const iap[]={"iap2_connect","iap2_disconnect","iap2_eap_open","iap2_eap_close","iap2_eap_send","iap2_eap_recv","iap2_eap_event_arm",NULL};
 static const char *const dns[]={"DNSServiceRegister","DNSServiceProcessResult","DNSServiceRefDeallocate",NULL};
 int a=probe("IAP2","/proc/boot/libiap2client.so.1",iap);
 int b=probe("BONJOUR","/proc/boot/libdns_sd.so.1",dns);
 puts("OEM_RUNTIME_SCOPE=LOADER_AND_SYMBOL_BINDING_ONLY_NO_PHONE_NO_MFI_NO_RF");
 return a&&b?0:1;
}
