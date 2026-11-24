#include "r125_module.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 const char *name=argc>1?argv[1]:"ipod-transport-au57xbt-REVIEW.so";
 void*h=dlopen(name,RTLD_NOW|RTLD_LOCAL);if(!h){puts(dlerror());return 1;}
 R125Module*m=dlsym(h,"ipod_module");if(!m||m->version!=0x200||!m->name||strcmp(m->name,"transport_au57xbt")||!m->interfaces||strcmp(m->interfaces->name,"ipod_transport"))return 2;
 R124Iface*i=m->interfaces;if(i->owner||!i->init||!i->link||!i->type||!i->send||!i->recv)return 3;
 if(i->init(i,"unused","")==0||i->send(NULL,"x",1)!=-1)return 4;
 dlclose(h);
#ifdef __QNXNTO__
 puts("AU57X_R125_QNX_PLUGIN_DLOPEN_OK_NO_OEM");
#else
 puts("AU57X_R125_HOST_PLUGIN_DLOPEN_OK_NO_OEM");
#endif
 return 0;
}
