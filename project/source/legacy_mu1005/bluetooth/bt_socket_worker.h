#ifndef MU1005_BT_SOCKET_WORKER_H
#define MU1005_BT_SOCKET_WORKER_H
#include <pthread.h>
/* One prepared descriptor at a time. Start/stop on supervisor, never
 * inside SDK dispatch. No stop concurrent with owner poll; object stays
 * resident until all notify callbacks are excluded. */
typedef struct{
 pthread_mutex_t lock;pthread_cond_t changed;pthread_t thread;
 unsigned port;
 int running,stopping,requested,busy,fd;
} MuBtSocketWorker;
int mu_bt_worker_start(MuBtSocketWorker*,unsigned);
/* Owner-side nonblocking poll. -1 = invalid/error, 0 = not ready,
 * 1 = transferred descriptor (caller owns it). Also requests work if idle. */
int mu_bt_worker_poll(MuBtSocketWorker*,int*);
int mu_bt_worker_stop(MuBtSocketWorker*);
#endif
