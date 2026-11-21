#ifndef MU1005_NATIVE_EXPORTS_H
#define MU1005_NATIVE_EXPORTS_H
#include "mu1005_r34_ap_server.h"
/* Only for the existing DIO process, its real server and its verified owner.
 * No dlopen, no new server, no replacement delegates, no ARM address calls.
 * Caller pins loaded OEM library and owns lifetime of server/manager.
 * All entries must execute on the same authorized owner; wired is excluded.
 */
typedef struct {
 void *opaque;
 int (*verified_library)(void *,void *handle); /* exact loaded OEM image */
 int (*owner_exclusive)(void *,void *server,const void *manager); /* live, idle wired, right thread */
 int (*listener)(void *,void *server,const void *manager,unsigned port);
 int (*stopped)(void *,void *server,const void *manager); /* actual stop/restore barrier */
 /* Exactly 1 only when the owner independently observes the effective interface.
  * Must not echo the request or infer from SetProperty return value. */
 int (*interface_matches)(void *,void *server,const void *manager,const char *expected);
} MuDioGuard;
typedef struct {
 Mu34ApServer ap;
 MuDioGuard guard;
 void *handle;
 void *server;
 void **server_slot; /* genuine exported singleton; borrowed */
 void *queue; /* observed server queue, NOT CDIOManager owner proof */
 void *(*get_queue)(void *);
 const void *manager;
 void *key,*start,*stop,*iface,*original;
 unsigned long (*get_type)(const void *);
 unsigned long (*string_type)(void);
 unsigned long (*server_type)(void);
 void *(*make_string)(const void *,const char *,unsigned int);
 unsigned char (*get_string)(const void *,char *,long,unsigned int);
 void (*release)(const void *);
 int (*set)(void *,unsigned int,const void *,const void *,const void *);
 int (*control)(void *,unsigned int,const void *,const void *,const void *,void **);
 int interface_dirty, initialized;
} MuDioNative;
/* Original interface must come from the genuine owner, not a guessed default.
 * Zero initialize stable object. No library/owner callback may be a stub in production.
 */
int mu_dio_bind(MuDioNative *,void *handle,void *server,const void *manager,
                uint32_t generation,const MuDioGuard *,const char *original_iface);
/* Owner must supply the confirmed interface name (not an inferred frequency).
 * No radio configuration is performed here. Original legacy entry remains for
 * source compatibility; new callers must use this explicit binding. */
int mu_dio_bind_interface(MuDioNative *,void *handle,void *server,const void *manager,
                uint32_t generation,const MuDioGuard *,const char *original_iface,
                const char *wireless_iface);
int mu_dio_start(MuDioNative *,unsigned port);
int mu_dio_poll(MuDioNative *);
/* returns 1 while native stop is not verified; retries preserve resources. */
int mu_dio_restore(MuDioNative *);
#endif
