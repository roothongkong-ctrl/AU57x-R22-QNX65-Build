#ifndef MIB_CARD112_H
#define MIB_CARD112_H
#include <stdint.h>
#include <stddef.h>
#include "alt111.h"
enum {C112_CONFIG=11,C112_FRAME=12,C112_IGNORED=13,C112_WAIT=14,
      C112_CLOSED=-1,C112_INVALID=-2,C112_AUTH=-3,C112_STALE=-4};
typedef int (*c112_decrypt_t)(void*,uint8_t*,size_t);
struct c112_receiver {
 struct alt111_video video;
 uint64_t session, connection, stream, configs, frames, rejected;
 c112_decrypt_t decrypt;
 void *crypto;
 unsigned active;
};
void c112_init(struct c112_receiver*);
int c112_begin(struct c112_receiver*,uint64_t,uint64_t,c112_decrypt_t,void*);
int c112_receive_one(struct c112_receiver*,int);
void c112_end(struct c112_receiver*);
#endif
