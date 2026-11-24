#ifndef R132_TRACE_BRIDGE_H
#define R132_TRACE_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
/* Exact MU1005 review-only event tap. No patcher or preload activation.
 * Installers MUST NOT invoke the thunk without one-time binding to the actual
 * original logCV. Captured events are hints only, never owner/peer proof. */
typedef struct {uint32_t r0,r1,r2,r3,r4,r5,r12,lr;} R132TraceFrame;
_Static_assert(sizeof(R132TraceFrame)==32,"AAPCS32 frame");
_Static_assert(offsetof(R132TraceFrame,lr)==28,"saved return PC");
typedef struct {uint32_t events,owner_hint,conflict;} R132TraceHints;
int r132_trace_bind(uintptr_t original);
uintptr_t r132_trace_dispatch(const R132TraceFrame *frame);
void r132_trace_take(R132TraceHints *out);
/* Observe readiness on a still-live object, outside log callback. Certify
 * image and object lifetime for BOTH reads; no vendor call or authorization. */
typedef struct {void *context;int (*certify)(void *,uint32_t);int (*read)(void *,uint32_t,void *,size_t);} R132Live;
int r132_start_observed(const R132TraceHints *,const R132Live *);
#endif
