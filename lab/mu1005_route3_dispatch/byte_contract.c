/* GPL-3.0-only */
#include "byte_contract.h"
#include <limits.h>
static int live(struct cp_step2_transport *t,uint32_t g){
    return t && t->bound && g && t->generation==g && t->bridge &&
        t->bridge->generation==g && t->bridge->connected && !t->bridge->closed &&
        t->owner.verify && t->owner.verify(t->owner.opaque,g)==1;
}
int route3_send(struct cp_step2_transport *t,const uint8_t *buf,size_t n,
                route3_finish finish,void *ctx){
    uint32_t g;
    int rc;
    if(!t || !buf || !n || n>CP_BT_PAYLOAD_MAX || n>INT_MAX || !finish)
        return -1;
    g=t->generation;
    if(!live(t,g))return -1;
    /* Never return a byte count for mere asynchronous acceptance. */
    if(t->bridge->tx_pending)return -1;
    rc=cp_step2_transport_write(t,buf,n);
    if(rc!=CP_BT_OK)return -1;
    if(finish(ctx,t,g,n)!=1 || !live(t,g) || t->bridge->tx_pending){
        /* External owner must physically close before releasing SDK storage.
         * Under the caller's exclusive lease, no fresh generation can exist. */
        if(t->generation==g)cp_step2_transport_quarantine(t);
        return -1;
    }
    return (int)n;
}
int route3_recv_now(struct cp_step2_transport *t,uint8_t *buf,size_t n){
    size_t got=0;
    int rc;
    if(!t||!buf||!n||n>INT_MAX)return -1;
    if(!live(t,t->generation))return -1;
    rc=cp_step2_transport_read(t,buf,n,&got);
    if(rc==CP_BT_EMPTY)return 0;
    if(rc!=CP_BT_OK||got>n||got>INT_MAX)return -1;
    return (int)got;
}
