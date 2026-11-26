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
                         unsigned channel); /* 1 video, 2 audio, 3 MMI, 4 steering;
     result 1 confirmed, 0 absent, -1 unknown (optional channels unset) */
 /* R85: INDEPENDENT DIO original-owner wired transition, for the SAME
  * receiver lease. Never derive from SI's wire data, SI USB events alone,
  * a root UID or global media flag. Missing callback = DENY op 12. */
 int (*dio_wired_takeover_verified)(void *,uint32_t generation,
                                  uint64_t epoch,uint32_t session);
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
/* R82: authenticated read-only DIO status query, CPRX v3 op 6. Status code
 * 0/4/5/7 is a *single observed sample*, NOT a live CarPlay media grant. */
#define MU_CPRX_OP_STATUS 6u
/* R83: read-only DIO proof snapshot for one exact SI owner session.
 * 28-byte success reply: CPRA[8], media[1], inner_mfi[1], channel_mask[1],
 * reserved[1], generation[4], epoch[8], session[4].
 * Proof is a SNAPSHOT, not a renewable authorization or OEM validation. */
#define MU_CPRX_OP_SESSION_STATUS 11u
/* R85: opt-in DIO forced restore after BOTH original SI/DIO independently
 * verify physical wired takeover. No listener/media state can authorize it. */
#define MU_CPRX_OP_WIRED_RESTORE 12u
/* R81: process-separated SI client endpoint. This is a snapshot of an
 * existing DIO process's socket identity, NOT a pointer to DIO's address
 * space. The two independently observed owner callbacks are mandatory. */
typedef struct {
 char socket_path[MU_CPRX_UNIX_PATH_MAX];
 uint32_t generation, session;
 uint64_t epoch;
 unsigned airplay_port;
 uid_t server_uid;
 gid_t server_gid;
 void *owner_ctx;
 int (*si_owner_verified)(void *,uint32_t,uint64_t,uint32_t);
 int (*dio_server_verified)(void *,int,uint32_t,uint64_t,uint32_t);
 /* R85: real SI-side physical wired takeover proof, separate from DIO's
  * independently checked evidence. Required only for op 12. */
 int (*si_wired_takeover_verified)(void *,uint32_t,uint64_t,uint32_t);
} MuCprxRemoteEndpoint;
typedef struct {
 MuCprxService *service;
 MuCprxRemoteEndpoint remote;
 int remote_mode;
 int fd, phase;
 unsigned operation; /* 1 START, 2 idle STOP, 12 verified wired restore */
 uint32_t generation, session;
 uint64_t epoch, deadline;
 unsigned sent, received, reply_need;
 int observed_media; /* -1 until a validated remote media sample */
 int observed_inner; /* -1 until op11; then 0/1 same-SI iAP2/MFi witness */
 unsigned observed_channels; /* bits 0..3 video/audio/HMI/steering */
 unsigned char request[32], reply[28];
} MuCprxClient;
void mu_cprx_client_init(MuCprxClient *);
/* Caller must be on the verified, current original SI owner thread.
 * Starts only when an owner-owned AF_UNIX v3 receiver lease is active. */
int mu_cprx_client_begin(MuCprxClient *,MuCprxService *,uint64_t now_ms);
/* Graceful, media-aware STOP uses the same *real* SI v3 control protocol.
 * Native media=7 receives BUSY, never an implied teardown; confirmed STOP
 * is emitted only after the original backend restore barrier succeeds. */
int mu_cprx_client_begin_stop(MuCprxClient *,MuCprxService *,uint64_t now_ms);
/* Actual cross-process AF_UNIX client. Both START and STOP require the
 * original SI owner and a separately verified DIO server. A local socket
 * pathname, UID/GID or valid wire token alone never proves OEM ownership.
 * This API does not dereference MuCprxService in another process. */
int mu_cprx_client_begin_remote(MuCprxClient *,
 const MuCprxRemoteEndpoint *,unsigned operation,uint64_t now_ms);
/* R85 op 12 may FORCE OEM restore while media==7 only when the DIO-side
 * original owner verifies actual wired takeover, and the SI client confirms
 * its OWN independent wired event. OEM restore must finish before ACK.
 * No production proof adapter is supplied; missing proof fails closed. */
/* R82: use mu_cprx_client_begin_remote(c, endpoint, MU_CPRX_OP_STATUS,
 * now) for an authenticated, READ-ONLY media observation from original DIO.
 * 12-byte reply includes stock media flag 0,4,5,7; a pending native listener
 * returns BUSY. For a new observation, close/init and issue a NEW transaction;
 * never infer live video/audio or same-phone MFi from a cached flag 7.
 * R83 op11 additionally samples separately supplied, independently verified
 * same-session inner-iAP2/MFi and four channels from the DIO-side OEM owner.
 * Missing callbacks are rejected, not converted into ready flags. */
/* Advance without blocking; call the server's mu_cprx_tick between ticks.
 * After BUSY, caller may close and reinitialize when media is independently
 * known idle. Any ERROR is terminal and requires a new verified SI session. */
int mu_cprx_client_tick(MuCprxClient *,uint64_t now_ms);
void mu_cprx_client_close(MuCprxClient *);
#endif
