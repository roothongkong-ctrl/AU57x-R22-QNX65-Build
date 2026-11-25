#ifndef WCP_R29_TCP_ADOPT_H
#define WCP_R29_TCP_ADOPT_H
#include "wcp_oem_graft_io.h"
/* Caller must independently verify the exact accepted peer and exclusive
 * ownership. Loopback addresses alone do not authenticate processes. */
typedef int (*wcp_peer_proof)(void *,int,unsigned,unsigned);
int wcp_r29_tcp_adopt(struct wcp_oem_graft_io *,int,unsigned,wcp_peer_proof,void *);
#endif
