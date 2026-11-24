#include "r131_trace_entry.h"
unsigned r131_trace_classify(uintptr_t pc,uintptr_t ch,int level,uintptr_t fmt){
 if(level!=1)return R131_TRACE_NONE;
 if(pc==0x11ed14u&&ch==0x2c2fbcu&&fmt==0x27cf94u)return R131_TRACE_DID;
 if(pc==0x13f1dcu&&ch==0x2c3f64u&&fmt==0x285bb0u)return R131_TRACE_ACL_EXISTING;
 if(pc==0x13f6c8u&&ch==0x2c3f64u&&fmt==0x285d44u)return R131_TRACE_ACL_ESTABLISHED;
 return R131_TRACE_NONE;
}
