/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_INNER_AIRPLAY_H
#define MU_INNER_AIRPLAY_H
#include "inner_iap2.h"
/* Caller must hold the verified original library and the actual AirPlay
 * owner session, and serialize on the original dispatch queue. */
typedef struct {
 void *ctx;
 int (*library)(void *,void *);
 int (*owner)(void *,void *,uint64_t);
} MuInnerAirplayProof;
/* MU1005 completion thunk 0x25a90 passes status, borrowed response, ctx.
 * Callback is asynchronous; keep ctx alive and serialize on the actual
 * HTTPClient dispatch queue. Status is OEM plist-level status, NOT proof
 * of an HTTP success, an iAP2 ACK, or authentication. */
typedef void (*MuInnerAirplayCompletion)(int,const void *,void *);
typedef struct {
 MuInnerAirplayProof proof;
 void *handle,*session;
 uint64_t epoch;
 void *key_type,*key_params,*key_data,*cmd;
 const void *key_callbacks,*value_callbacks;
 unsigned long (*type)(const void *);
 unsigned long (*dict_type)(void),(*data_type)(void),(*string_type)(void),(*session_type)(void);
 void *(*dict)(const void *,long,const void *,const void *);
 void *(*string)(const void *,const char *,unsigned);
 int (*set_string)(void *,const void *,const char *,long);
 int (*set_data)(void *,const void *,const unsigned char *,long);
 void (*set)(void *,const void *,const void *);
 const void *(*get)(const void *,const void *);
 unsigned char (*equal)(const void *,const void *);
 long (*length)(const void *);
 const unsigned char *(*bytes)(const void *);
 void (*release)(const void *);
 int (*send)(void *,const void *,MuInnerAirplayCompletion,void *);
 int bound;
} MuInnerAirplay;
int mu_inner_ap_bind(MuInnerAirplay *,void *,void *,uint64_t,const MuInnerAirplayProof *);
void mu_inner_ap_unbind(MuInnerAirplay *);
/* Callback for MuInnerOps.send_event. Successful return means original
 * AirPlay command queued, NOT response received or MFi authenticated. */
int mu_inner_ap_send(void *,const unsigned char *,size_t);
/* Optional native completion for a verified owner. Same admission result
 * as the existing sender. No callback storage is owned by this adapter;
 * stop/unbind does not cancel a previously accepted OEM request. */
int mu_inner_ap_send_with_completion(MuInnerAirplay *,const unsigned char *,size_t,
 MuInnerAirplayCompletion,void *);
/* Caller invokes on decoded original control dispatch BEFORE normal handler.
 * 2 unrelated -> continue original handler; 1 copied -> reply success;
 * 0 busy -> do not acknowledge; -1 malformed/owner lost -> fail session.
 * Never call on encrypted bytes or arbitrary network messages. */
int mu_inner_ap_control(MuInnerAirplay *,MuInner *,uint64_t,const void *);
/* Same semantics as mu_inner_ap_control, for a verified owner receiving
 * separate decoded command and params objects. Session must match exactly.
 * NOT a drop-in OEM delegate: R105.028 shows a stock path discards the
 * delegate status. Never attach without a verified response contract. */
int mu_inner_ap_control_parts(MuInnerAirplay *,MuInner *,uint64_t,
                             const void *session,const void *command,
                             const void *params);
#endif
