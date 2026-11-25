#ifndef MU1005_BT_SELF_READER_H
#define MU1005_BT_SELF_READER_H
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
typedef struct MuBtSelfReader MuBtSelfReader;
/* Explicit object/address authorization is independent of map readability.
 * The caller must hold a valid object lifetime lease for every read.
 * Open/close on supervisor; close only after all callbacks have stopped.
 * QNX only, read-only current process, no debug stop or memory mutation. */
MuBtSelfReader *mu_bt_self_reader_open(int(*allow)(void*,uint32_t,size_t),void*);
/* Suitable as MuBtOwner.read_live through owner's context wrapper.
 * Output remains unchanged on failure. Reads may involve QNX IPC:
 * not suitable for arbitrary held-SDK-lock callback contexts. */
int mu_bt_self_reader_read(void*,uint32_t,void*,size_t);
void mu_bt_self_reader_close(MuBtSelfReader*);
#endif
