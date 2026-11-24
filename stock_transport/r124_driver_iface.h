#ifndef AU57X_R124_DRIVER_IFACE_H
#define AU57X_R124_DRIVER_IFACE_H
#include <stdint.h>
#include <stddef.h>
#include "r123_rf_transport.h"
/* Review-only native interface shape. No loader export, no boot hook.
 * The caller must independently establish actual mm-ipod owner, serialized
 * lifecycle, pairing and USB exclusion. This module cannot establish trust. */
typedef struct R124Iface R124Iface;
struct R124Iface {
 const char *name; void *next; void *module; void *owner; void *private_state;
 int (*init)(R124Iface *,const char *,const char *);
 int (*link)(R124Iface *,void *);
 int (*type)(R124Iface *,uint8_t *);
 int (*caps)(R124Iface *,unsigned,void *,unsigned);
 int (*audio)(R124Iface *);
 int (*send)(void *device,const void *,unsigned);
 int (*recv)(void *device,void *,unsigned,int,void *);
 uint8_t unused[168-48];
};
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(R124Iface)==168,"ARM32 OEM observed extent");
_Static_assert(offsetof(R124Iface,owner)==0x0c,"owner");
_Static_assert(offsetof(R124Iface,init)==0x14,"init");
_Static_assert(offsetof(R124Iface,link)==0x18,"link");
_Static_assert(offsetof(R124Iface,type)==0x1c,"type");
_Static_assert(offsetof(R124Iface,caps)==0x20,"caps");
_Static_assert(offsetof(R124Iface,audio)==0x24,"audio");
_Static_assert(offsetof(R124Iface,send)==0x28,"send");
_Static_assert(offsetof(R124Iface,recv)==0x2c,"recv");
#endif
/* One exclusive consumer. Proof must be live, not just a cached flag.
 * Caller serializes all calls, including revoke. No runtime plugin export.
 * Exact 16-byte link params use original driver default values, NOT validated BT tuning. */
typedef struct {
 void *context;
 int (*authorized)(void *,void *device,uint32_t generation);
} R124Proof;
int r124_driver_prepare(R123Transport *,void *device,const R124Proof *,R124Iface **);
/* One dispatcher call on the certified owner context; callback must be
 * supplied by actual driver owner (not a raw address). May call recv inline.
 * Return 1 when notified, 0 no data, -1 on revocation/invalid state. */
int r124_driver_poll(void (*input_ready)(void *,void *),void *context);
int r124_driver_revoke(void);
/* Retire after external proof of no outstanding callback or driver reference.
 * This deliberately does not unload code or permit reuse. */
#endif
