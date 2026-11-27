/* QNX ARM: actual loader diagnostics, not a media or hardware proof. */
#include <dlfcn.h>
#include <stdio.h>
static int attempt(const char *name){
    char path[256];
    if(snprintf(path,sizeof path,"/proc/boot/%s",name)>=(int)sizeof path)return 0;
    printf("AIRPLAY_PROBE_BEGIN %s\n",name);fflush(stdout);
    void *h=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!h){const char *e=dlerror();printf("AIRPLAY_LOAD_FAIL %s %s\n",name,e?e:"unknown");return 0;}
    printf("AIRPLAY_LOAD_OK %s\n",name);
    if(dlclose(h)!=0){puts("AIRPLAY_CLOSE_FAIL");return 0;}
    return 1;
}
int main(void){
    (void)attempt("libnvos.so");
    (void)attempt("libnvrm.so");
    (void)attempt("libnvaudio.so");
    int ok=attempt("libairplay.so");
    printf("AIRPLAY_PROBE_COMPLETE loaded=%d; scope=loader_only_no_phone_no_mfi_no_video\n",ok);
    return 0; /* Diagnostic completion is distinct from successful loading. */
}
