#ifndef TXN_SHA256_H
#define TXN_SHA256_H
#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>
typedef struct { uint32_t h[8]; uint64_t bytes; unsigned char b[64]; size_t used; } sha256_ctx;
void sha256_init(sha256_ctx *s);
void sha256_update(sha256_ctx *s,const void *p,size_t n);
void sha256_final(sha256_ctx *s,unsigned char out[32]);
#endif
