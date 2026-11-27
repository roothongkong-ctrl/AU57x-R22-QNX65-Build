#ifndef MIB_CARD112_NEGOTIATION_H
#define MIB_CARD112_NEGOTIATION_H
#include <stdint.h>
#include "c112_ingest.h"
/* Protocol shape for a third CarPlay display. A real CFLite adapter,
 * iPhone SETUP integration and independent AES are NOT installed here. */
enum { N112_OK=0,N112_NO_PROOF=-1,N112_INVALID=-2,N112_SOCKET=-3,
       N112_STALE=-4,N112_DUPLICATE=-5,N112_NOT_READY=-6 };
struct n112_proof {
 unsigned explicit_opt_in, ios_112_observed, distinct_aes_context;
 unsigned codec_and_decoder_ready, overlay_alpha_ready;
 unsigned full_and_partial_teardown_verified;
};
struct n112_descriptor {
 unsigned type,width,height,width_mm,height_mm,max_fps;
 char uuid[37];
 char initial_url[96];
};
struct n112_setup {
 unsigned type;
 uint64_t stream_connection_id;
 unsigned data_port;
};
struct n112_session {
 int listener,client;
 uint64_t session_id,connection_id;
 unsigned offered,started,port;
 struct n112_descriptor display;
 /* Owner survives private VIDEO_FRAME corruption and receiver resets. */
 c112_decrypt_t session_decrypt;
 void *session_crypto;
 struct c112_receiver receiver;
};
void n112_init(struct n112_session *s);
int n112_prepare(struct n112_session *s,const struct n112_proof *proof,
    uint64_t session_id,const char *uuid,const char *map_uuid,
    const char *map_url,
    unsigned width,unsigned height,c112_decrypt_t decrypt,void *crypto);
int n112_offer(const struct n112_session *s,struct n112_descriptor *out);
int n112_setup(struct n112_session *s,unsigned type,uint64_t session_id,
    uint64_t connection_id,struct n112_setup *response);
int n112_accept(struct n112_session *s);
int n112_receive_one(struct n112_session *s);
int n112_teardown(struct n112_session *s,unsigned type,uint64_t session_id,
    uint64_t connection_id);
void n112_destroy(struct n112_session *s);
#endif
