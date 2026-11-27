/* SPDX-License-Identifier: GPL-3.0-only
 * Exact ARM calling convention test; synthetic original trace function.
 * Not original btstack, not a live hook. */
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
typedef void (*trace_fn)(void*,int,const char*,...);
extern void mu_bt_trace_tail(void*,int,const char*,...);
static int object,seen,forwarded,deny;
static uintptr_t caller_lr;
static const char fmt[]="fixture";
static void original(void *p,int level,const char *format,...){
 va_list args;
 assert(p==&object&&level==3&&format==fmt);
 assert((uintptr_t)__builtin_return_address(0)==caller_lr);
 va_start(args,format);
 assert(va_arg(args,unsigned)==0x11223344u);
 assert(va_arg(args,unsigned)==0x55667788u);
 assert(va_arg(args,uint64_t)==UINT64_C(0x0123456789abcdef));
 assert(va_arg(args,double)==1.25);
 assert(!strcmp(va_arg(args,const char*),"stack tail"));
 va_end(args);forwarded++;
}
trace_fn mu_bt_trace_target(void *p,uintptr_t lr,const char *format){
 assert(p==&object&&lr&&format==fmt);caller_lr=lr;seen++;
 return deny?NULL:original;
}
extern void mu_bt_trace_latch_test(void);
void mu_bt_trace_tail_test(void){
 mu_bt_trace_latch_test();
 for(int i=0;i<3;i++)mu_bt_trace_tail(&object,3,fmt,0x11223344u,0x55667788u,UINT64_C(0x0123456789abcdef),1.25,"stack tail");
 assert(seen==3&&forwarded==3);
 deny=1;mu_bt_trace_tail(&object,3,fmt,0x11223344u,0x55667788u,UINT64_C(0x0123456789abcdef),1.25,"stack tail");
 assert(seen==4&&forwarded==3);
 puts("MU1005_TRACE_TAIL_ARM_VARIADIC_FORWARD_PASS_FIXTURE_ONLY");
}
