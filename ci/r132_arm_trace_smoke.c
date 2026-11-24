#include "r132_trace_bridge.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
/* Actual ARM variadic call/return proof, entirely synthetic caller and logger. */
extern int r132_trace_thunk(void *,int,const char *,...);
static int logger(void *ctx,int lvl,const char *fmt,...){
 va_list ap;va_start(ap,fmt);const char *s=va_arg(ap,const char *);int a=va_arg(ap,int);const char *b=va_arg(ap,const char *);double c=va_arg(ap,double);va_end(ap);
 return ctx==(void*)0x1234&&lvl==7&&!strcmp(fmt,"fixture")&&!strcmp(s,"core")&&a==42&&!strcmp(b,"stack")&&c==2.5?73:0;
}
int main(void){
 if(r132_trace_bind((uintptr_t)logger)||r132_trace_thunk((void*)0x1234,7,"fixture","core",42,"stack",2.5)!=73)return 1;
 R132TraceHints h;r132_trace_take(&h);if(h.events||h.owner_hint||h.conflict)return 2;
 puts("AU57X_R132_QNX_ARM_VARIADIC_PASSTHROUGH_OK_NO_OEM");return 0;
}
