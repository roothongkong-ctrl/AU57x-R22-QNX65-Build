#ifndef MU1005_CPRX_SERVICE_H
#define MU1005_CPRX_SERVICE_H
#include "native_exports.h"
#include "semantic.h"
/* Port override is for isolated host tests only. */
#if defined(MU_CPRX_HOST_TEST) && !defined(__QNXNTO__)
#define MU_CPRX_PORT 57344u
#else
#define MU_CPRX_PORT 57343u
#endif
/* CPRX v2 / SI-bound v3 basic control, one authorized generation, owner-thread nonblocking tick.
 * Not a DIO loader. Production caller must supply actual owner and admission,
 * factory Bonjour pi and media observation. Never run from a socket worker. */
typedef struct {
 void *opaque;
 int (*admitted)(void *,uint32_t); /* 1 only while paired generation owns receiver */
 int (*identity)(void *,char *,size_t); /* actual advertised pi length; 0 pending, negative failure */
 int (*media)(void *); /* verified native flags 0,4,5,7; negative unknown */
 int (*si_admitted)(void *,uint32_t,uint64_t,uint32_t); /* actual SI epoch/session, not wire trust */
 int (*metadata)(void *,const unsigned char *,size_t); /* 1 accepted, 0 retry, -1 rejected */
 int (*semantic_in)(void *,unsigned,const unsigned char *,size_t);
 int (*semantic_next)(void *,unsigned char *,size_t); /* size, 0 empty, negative failure */
 /* R74: same ORIGINAL SI session as si_admitted. The caller, not the CPRX
  * wire, must establish both proofs independently on the OEM owner thread.
  * Epoch-only assertions from another AirPlay/iAP2 session are insufficient. */
 int (*si_inner_iap2_mfi)(void *,uint32_t generation,uint64_t epoch,uint32_t session);
 int (*si_media_channel)(void *,uint32_t generation,uint64_t epoch,uint32_t session,
                         unsigned channel); /* 1 video, 2 audio, 3 MMI, 4 steering */
} MuCprxEvidence;
/* One backend owns the entire receiver lifecycle for this generation.
 * Caller supplies real verified owner callbacks, never synthetic readiness.
 * START/POLL return: 1 pending, 2 confirmed listener/identity, negative error.
 * RESTORE returns: 0 fully restored, 1 pending, negative error. It must be
 * safe after a partially failed START and retain ownership until confirmed.
 * No change of backend inside a service generation. */
typedef struct {
 void *opaque;
 uint32_t generation;
 int (*start)(void *,unsigned port);
 int (*poll)(void *);
 int (*restore)(void *);
} MuReceiverBackend;
typedef struct {
 MuReceiverBackend backend;
 int backend_released;
 MuCprxEvidence evidence;
 uint32_t generation, session;
 uint64_t epoch;
 unsigned version, required;
 unsigned port;
 int listener, client, started, start_confirmed, cleaning, terminal, mutating;
 unsigned used, sent, size;
 unsigned identity_len;
 char identity_value[252];
 uint64_t deadline;
 unsigned char req[32], reply[526], payload[8192], length_bytes[2];
 unsigned length_used,payload_used,payload_need;
 MuSemantic semantic;
} MuCprxService;
int mu_cprx_open(MuCprxService *,MuDioNative *,const MuCprxEvidence *,unsigned airplay_port);
/* Generic entry for a previously reviewed backend; SI admission is mandatory.
 * Evidence identity/media must come from the same live backend generation.
 * No donor adapter is implemented by this interface.
 * Failed open leaves caller object unchanged; successful open requires an unused
 * or fully restored service object. */
int mu_cprx_open_backend_si(MuCprxService *,const MuReceiverBackend *,const MuCprxEvidence *,unsigned,uint64_t,uint32_t);
/* Operations 8–10 require verified owner callbacks; missing backend is rejected. */
int mu_cprx_open_si(MuCprxService *,MuDioNative *,const MuCprxEvidence *,unsigned,uint64_t,uint32_t);
/* Call on genuine owner periodically, with monotonic milliseconds. 0 active,
 * 1 ended, -1 cleanup pending/error (retain object and retry shutdown). */
int mu_cprx_tick(MuCprxService *,uint64_t now);
int mu_cprx_shutdown(MuCprxService *);
#endif
