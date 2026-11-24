#ifndef R125_MODULE_H
#define R125_MODULE_H
#include "r124_driver_iface.h"
/* Candidate module layout based on pinned OEM ELF. Owner supplied at loader init.
 * No default activation, no process hook, no install. All calls serialized by
 * independently certified mm-ipod owner. The caller retains transport storage. */
typedef struct { const char *name; uint16_t version,reserved; void *slots[6]; R124Iface *interfaces; } R125Module;
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(R125Module)==36,"OEM ARM32 module extent");
_Static_assert(offsetof(R125Module,interfaces)==0x20,"OEM module interface");
#endif
extern R125Module ipod_module;
typedef struct {
 void *context;
 int (*owner_context)(void *,void *device); /* live image, thread, object ownership */
 int (*authorized)(void *,void *device,uint32_t generation); /* paired, USB idle, session */
 int (*start_driver)(void *,void *device); /* verified original driver +0x18; 0 success */
 void (*input_ready)(void *,void *device); /* owner-bound original driver +0x20 */
 int (*stop_driver)(void *,void *device); /* independently proven teardown; 0 success */
} R125Owner;
enum {R125_EMPTY,R125_ARMED,R125_BOUND,R125_STARTING,R125_RUNNING,R125_STOP_REQUIRED,R125_RETIRED,R125_DIRTY};
int r125_arm(R123Transport *,const R125Owner *);
int r125_dispatch(void);
int r125_stop(void);
int r125_state(void);
#endif
