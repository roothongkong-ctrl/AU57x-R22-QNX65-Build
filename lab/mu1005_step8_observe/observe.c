/* Lab-only observation of original parser call. Tail-forward original ABI. */
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static const char sym[]="_ZN4util6config10JSONParser16parseFileToArrayEPKcPPNS0_5ArrayEj";
void *step8_observe(const char *name){
 int e=errno;
 void *h=dlopen("/proc/boot/libutil.so",RTLD_NOW|RTLD_LOCAL);
 void *p=h?dlsym(h,sym):NULL;
 if(!p){const char s[]="STEP8_OBSERVER_FORWARD_UNAVAILABLE\n";write(2,s,sizeof s-1);_exit(91);}
 const char prefix[]="STEP8_ORIGINAL_PARSE_ARGUMENT ";
 write(2,prefix,sizeof prefix-1);
 if(name){size_t n=0;while(n<512 && name[n]!='\0')++n;write(2,name,n);}else write(2,"(null)",6);
 write(2,"\n",1);errno=e;return p;
}
