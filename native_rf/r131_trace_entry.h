#ifndef R131_TRACE_ENTRY_H
#define R131_TRACE_ENTRY_H
#include <stdint.h>
/* Read-only event classification for the exact MU1005 image. No registration,
 * no trace-threshold writes, no peer data, no SDK calls. The later interposer
 * must supply the actual original return PC, channel pointer and format pointer.
 * A DID event occurs BEFORE the final startup flag; never register in this callback. */
enum { R131_TRACE_NONE=0, R131_TRACE_DID=1, R131_TRACE_ACL_EXISTING=2, R131_TRACE_ACL_ESTABLISHED=4 };
unsigned r131_trace_classify(uintptr_t return_pc, uintptr_t channel, int level, uintptr_t format);
#endif
