/* v1.2.14 dual instrument screen state gate; experiment, NO INSTALL.
 * Type 110 always remains stock. Type 111 is the working map.
 * Type 112 must not be enabled until distinct receiver, keys, decoder,
 * display and alpha mask are actually proven on the target hardware.
 */
#include <stdint.h>
#include <string.h>
enum { G_OK=0,G_WAIT=1,G_STOCK=2,G_REJECT=-1,G_STALE=-2,G_DUP=-3 };
enum { V_BLANK=0,V_MAP=1,V_DUAL=2 };
typedef struct {
 unsigned opted_in,ios_112_observed,isolated_aes,receiver112_ready;
 unsigned unique_uuids,teardown_proven,card_mask_proven;
} Proof;
typedef struct {
 uint64_t session,map_conn,card_conn,map_frame,card_frame;
 unsigned enabled,map_up,card_up;
} Gate;
void gate_init(Gate *g){if(g)memset(g,0,sizeof(*g));}
int gate_proof(Gate *g,const Proof *p){
 if(!g||!p)return G_REJECT;
 g->enabled=!!(p->opted_in&&p->ios_112_observed&&p->isolated_aes&&
 p->receiver112_ready&&p->unique_uuids&&p->teardown_proven&&p->card_mask_proven);
 return g->enabled?G_OK:G_REJECT;
}
int gate_begin(Gate *g,uint64_t s){
 if(!g||!s)return G_REJECT;
 unsigned e=g->enabled;memset(g,0,sizeof(*g));g->enabled=e;g->session=s;return G_OK;
}
int gate_setup(Gate *g,unsigned type,uint64_t s,uint64_t cid){
 if(!g||!cid||!s)return G_REJECT;
 if(type==110)return G_STOCK;
 if(type!=111&&type!=112)return G_REJECT;
 if(s!=g->session)return G_STALE;
 if(type==111){
  if(g->map_up)return G_DUP;
  if(g->card_up)return G_REJECT;
  g->map_up=1;g->map_conn=cid;g->map_frame=0;return G_OK;
 }
 if(!g->enabled)return G_REJECT;
 if(!g->map_up)return G_WAIT;
 if(g->card_up||cid==g->map_conn)return G_DUP;
 g->card_up=1;g->card_conn=cid;g->card_frame=0;return G_OK;
}
int gate_frame(Gate *g,unsigned type,uint64_t s,uint64_t cid,uint64_t ts,uint64_t now){
 if(!g||!cid||!ts||!now||now<ts)return G_REJECT;
 if(type==110)return G_STOCK;
 if(s!=g->session)return G_STALE;
 if(type==111){
  if(!g->map_up||cid!=g->map_conn||now-ts>400000||ts<g->map_frame)return G_STALE;
  g->map_frame=ts;return G_OK;
 }
 if(type==112){
  if(!g->enabled||!g->card_up||cid!=g->card_conn||now-ts>400000||ts<g->card_frame)return G_STALE;
  g->card_frame=ts;return G_OK;
 }
 return G_REJECT;
}
int gate_end(Gate *g,unsigned type,uint64_t s,uint64_t cid){
 if(!g||!cid)return G_REJECT;
 if(type==110)return G_STOCK;
 if(s!=g->session)return G_STALE;
 if(type==112){
  if(!g->card_up||g->card_conn!=cid)return G_STALE;
  g->card_up=0;g->card_conn=0;g->card_frame=0;return G_OK;
 }
 if(type==111){
  if(!g->map_up||g->map_conn!=cid)return G_STALE;
  g->map_up=g->card_up=0;g->map_conn=g->card_conn=0;
  g->map_frame=g->card_frame=0;return G_OK;
 }
 return G_REJECT;
}
int gate_full_end(Gate *g,uint64_t s){
 if(!g||!s||g->session!=s)return G_STALE;
 unsigned e=g->enabled;memset(g,0,sizeof(*g));g->enabled=e;return G_OK;
}
int gate_view(const Gate *g,uint64_t now){
 if(!g||!g->map_up||!g->map_frame||now<g->map_frame||
 now-g->map_frame>400000)return V_BLANK;
 if(!g->enabled||!g->card_up||!g->card_frame||
 now<g->card_frame||now-g->card_frame>400000||
 (g->map_frame>g->card_frame?g->map_frame-g->card_frame:g->card_frame-g->map_frame)>160000)
 return V_MAP;
 return V_DUAL;
}
int gate_invariant(const Gate *g){
 return g&&(!g->card_up||(g->enabled&&g->map_up&&g->card_conn&&g->card_conn!=g->map_conn))&&
 (!g->map_up||g->map_conn)&&(!g->map_up||g->session)&&
 (g->card_up||(!g->card_conn&&!g->card_frame))&&
 (g->map_up||(!g->map_conn&&!g->map_frame&&!g->card_up));
}
