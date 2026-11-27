/* GPL-3.0-only */
#include "ipc_finish.h"
#include "byte_contract.h"
#include <limits.h>
typedef struct {Route3IpcFinish *f;unsigned long before;uint32_t radio;} Witness;
static int finish(void *p,struct cp_step2_transport*t,uint32_t g,size_t n){
 Witness*w=p;Route3IpcFinish*f=w->f;struct cp_step2_ipc_link*l=f->link;
 if(t!=&l->transport||!n||w->before==ULONG_MAX)return 0;
 for(unsigned i=0;i<f->max_steps;i++){
  if(l->poisoned||l->finished||!l->connected||l->local_generation!=g||
     l->radio_generation!=w->radio||f->monotonic_ms(f->opaque)>=f->deadline_ms)
   return 0;
  if(l->tx_confirmed==w->before+1u&&!l->bridge.tx_pending)return 1;
  if(l->tx_confirmed!=w->before||!l->bridge.tx_pending)return 0;
  if(f->step(f->opaque,l,f->deadline_ms)!=1)return 0;
 }
 return l->connected&&!l->poisoned&&!l->finished&&l->radio_generation==w->radio&&
        l->local_generation==g&&l->tx_confirmed==w->before+1u&&!l->bridge.tx_pending&&
        f->monotonic_ms(f->opaque)<f->deadline_ms;
}
int route3_ipc_send(Route3IpcFinish*f,const uint8_t*p,size_t n){
 Witness w;int rc;struct cp_step2_ipc_link*l;
 if(!f||!f->link||!f->step||!f->monotonic_ms||!f->max_steps||
    f->max_steps>4096||!p||!n||n>CP_BT_PAYLOAD_MAX||
    f->monotonic_ms(f->opaque)>=f->deadline_ms)return -1;
 l=f->link;
 if(!l->connected||l->poisoned||l->finished||l->bridge.tx_pending||
    l->tx_confirmed==ULONG_MAX)return -1;
 w.f=f;w.before=l->tx_confirmed;w.radio=l->radio_generation;
 rc=route3_send(&l->transport,p,n,finish,&w);
 if(rc<0)cp_step2_ipc_lost(l);
 return rc;
}
