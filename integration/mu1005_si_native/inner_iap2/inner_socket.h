/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU_INNER_SOCKET_H
#define MU_INNER_SOCKET_H
#include "inner_iap2.h"
/* DIO-side stream endpoint. The peer is the actual mm-ipod transport
 * adapter, NOT the Bluetooth bootstrap connection. Caller authenticates
 * the OS peer and binds it to the original session before handing it over.
 * Call only on the original AirPlay owner, no concurrent callbacks. */
typedef struct {
 MuInner relay;
 int fd;
 void *ctx;
 int (*verified)(void *,int,uint64_t);
 int (*event)(void *,const unsigned char *,size_t);
} MuInnerSocket;
/* Initialize with zeroed storage. Successful adoption owns fd; failure
 * leaves it caller-owned. No listener or fixed socket pathname. */
int mu_inner_socket_adopt(MuInnerSocket *,int,uint64_t,void *,
 int (*)(void *,int,uint64_t),int (*)(void *,const unsigned char *,size_t));
int mu_inner_socket_tick(MuInnerSocket *,uint64_t);
void mu_inner_socket_close(MuInnerSocket *);
#endif
