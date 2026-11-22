#ifndef MU1005_CPRX_SERVICE_H
#define MU1005_CPRX_SERVICE_H
#include "native_exports.h"
#include "semantic.h"
#include <sys/types.h>
/* Port override is for isolated host tests only. */
#if defined(MU_CPRX_HOST_TEST) && !defined(__QNXNTO__)
#define MU_CPRX_PORT 57344u
#else
#define MU_CPRX_PORT 57343u
#endif
#define MU_CPRX_UNIX_PATH_MAX 108u
/* CPRX v2 / SI-bound v3 basic control, one authorized generation, owner-thread nonblocking tick.
 * Not a DIO loader. Production caller must supply actual owner and admission,
 * factory Bonjour pi and media observation. Never run from a socket worker. */
typedef struct {
 void *opaque;
 int (*admitted)(void *,uint32_t); /* 1 only while paired generation owns receiver */
 int (*identity)(void *,char *,size_t); /* actual advertised pi length; 0 pending, negative failure */
 int (*media)(void *); /* verified native flags 0,4,5,7; negative unknown */
 int (*si_admitted)(void *,uint32_t,uint64_t,uint32_t); /* actual SI epoch/session, not wire trust */
 /* R75: independently authenticate the OS/local IPC peer, not just the
  * generation/session numbers provided by that peer on the wire. Production
  * adapter MUST verify kernel-backed identity and current SI process/owner.
  * Called before a request is parsed and on every active owner tick.
  * Never use a constant-true callback in the car. */
 int (*si_client_verified)(void *,int accepted_fd,uint32_t generation,
                           uint64_t epoch,uint32_t session);
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
 /* v3 only: private AF_UNIX stream socket with kernel peer credentials.
  * v2 legacy loopback stays separate; never accept TCP for SI v3. */
 int unix_listener;
 uid_t peer_uid;
 gid_t peer_gid;
 dev_t unix_dev;
 ino_t unix_ino;
 char unix_path[MU_CPRX_UNIX_PATH_MAX];
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
/* Legacy v3 TCP entrypoints are intentionally fail-closed. Retained only for
 * binary/source symbol compatibility; call the explicit UNIX variants. */
int mu_cprx_open_backend_si(MuCprxService *,const MuReceiverBackend *,const MuCprxEvidence *,unsigned,uint64_t,uint32_t);
/* dir: existing owner-owned private (0700) directory; a new 'cprx.sock'
 * pathname is created there. Never pass /tmp directly or choose a car path
 * until the real SI owner/dir has been independently verified. expected
 * uid/gid are pinned from original SI owner, NOT accepted from wire data.
 * QNX uses getpeereid(); Linux host tests use SO_PEERCRED. */
int mu_cprx_open_backend_si_unix(MuCprxService *,const MuReceiverBackend *,
 const MuCprxEvidence *,unsigned,uint64_t,uint32_t,const char *dir,
 uid_t expected_uid,gid_t expected_gid);
/* Operations 8–10 require verified owner callbacks; missing backend is rejected. */
int mu_cprx_open_si(MuCprxService *,MuDioNative *,const MuCprxEvidence *,unsigned,uint64_t,uint32_t);
int mu_cprx_open_si_unix(MuCprxService *,MuDioNative *,const MuCprxEvidence *,
 unsigned,uint64_t,uint32_t,const char *dir,uid_t expected_uid,gid_t expected_gid);
/* Call on genuine owner periodically, with monotonic milliseconds. 0 active,
 * 1 ended, -1 cleanup pending/error (retain object and retry shutdown).
 * R77: the AF_UNIX pathname stays reserved until OEM restore succeeds;
 * do not reopen a live/pending MuCprxService with another directory. */
int mu_cprx_tick(MuCprxService *,uint64_t now);
int mu_cprx_shutdown(MuCprxService *);
/* R78: nonblocking, owner-thread CPRX v3 START client. The SI owner must
 * explicitly authorize this operation; its client is NEVER a substitute for
 * a genuine OEM AirPlay/inner-iAP2 session. No independent worker thread. */
#define MU_CPRX_CLIENT_ERROR (-1)
#define MU_CPRX_CLIENT_PENDING 1
#define MU_CPRX_CLIENT_CONFIRMED 2
#define MU_CPRX_CLIENT_BUSY 3 /* STOP refused: native media still active */
typedef struct {
 MuCprxService *service;
 int fd, phase;
 unsigned operation; /* 1=START, 2=graceful STOP */
 uint32_t generation, session;
 uint64_t epoch, deadline;
 unsigned sent, received;
 unsigned char request[32], reply[8];
} MuCprxClient;
void mu_cprx_client_init(MuCprxClient *);
/* Caller must be on the verified, current original SI owner thread.
 * Starts only when an owner-owned AF_UNIX v3 receiver lease is active. */
int mu_cprx_client_begin(MuCprxClient *,MuCprxService *,uint64_t now_ms);
/* Graceful, media-aware STOP uses the same *real* SI v3 control protocol.
 * Native media=7 receives BUSY, never an implied teardown; confirmed STOP
 * is emitted only after the original backend restore barrier succeeds. */
int mu_cprx_client_begin_stop(MuCprxClient *,MuCprxService *,uint64_t now_ms);
/* Advance without blocking; call the server's mu_cprx_tick between ticks.
 * After BUSY, caller may close and reinitialize when media is independently
 * known idle. Any ERROR is terminal and requires a new verified SI session. */
int mu_cprx_client_tick(MuCprxClient *,uint64_t now_ms);
void mu_cprx_client_close(MuCprxClient *);
#endif
