#!/usr/bin/env python3
"""Source adapter only. Separate upstream projects, no installed car changes.
Usage: prepare_rgi_bridge.py <mib-cluster-map clone> <luka-rgi clone>
Pinned anchors fail if upstream changes. The LuKa hook still needs target
firmware compatibility; never preload both competing AirPlay hooks.
"""
from pathlib import Path
import sys, subprocess, shutil
if len(sys.argv)!=3:raise SystemExit(__doc__)
mib,luka=map(Path,sys.argv[1:])
here=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here/"prepare_player.py"),str(mib)],check=True)
p=mib/"player/opengl_gpu.cc"
s=p.read_text()
def replace(a,b,count=1):
 global s
 assert s.count(a)==count,a
 s=s.replace(a,b)
replace('#include "r17/r17_card_sidecar.h"\nstatic r17::Sidecar g_r17_card;',
        '#include "r17/rgd_card70_bridge.h"\nstatic rgd70::Receiver g_r17_card;')
replace('CLUSTERMAP_R17_CARD70','CLUSTERMAP_RGD_CARD70',s.count('CLUSTERMAP_R17_CARD70'))
replace('const char *verified=getenv("CLUSTERMAP_R17_CARD112_VERIFIED");',
        'const char *verified=getenv("CLUSTERMAP_RGD_LIVE_SOURCE_VERIFIED");')
replace('g_r17_card.start(now_us())','g_r17_card.start(now_us(),getenv("CLUSTERMAP_RGD_CARD_RECT"))')
replace('R17 LAB: listening for independent 112 card video','RGD LAB: local navigation receiver started')
replace('R17 LAB: sidecar startup failed; stock 111 preserved','RGD LAB: receiver unavailable; stock 111 preserved')
replace('R17 LAB refused: requires explicitly verified independent map/card sources; stock 111 unchanged','RGD LAB refused: requires verified clean map and live RGI source; stock 111 unchanged')
p.write_text(s)
for n in ("rgd_card70_bridge.h","rgd_card70_i420.h"):
 shutil.copy2(here/n,mib/"player/r17"/n)

p=luka/"hook/routeguidance/rgd_hook.c"
s=p.read_text()
anchor="static void write_bus_snapshot_from_cache(int extra_slot, const rgd_maneuver_t* extra_man,\n                                          const rgd_update_t* current_upd) {"
assert s.count(anchor)==1
sender=r'''/* Optional compact-card snapshot bridge, off unless explicitly enabled.
 * Only selected, coherent and fresh RGI data; no interception or replacement
 * of the normal CPHB bus. Nonblocking loopback datagram, never waits.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
static void card70_be32(unsigned char *p, uint32_t v) {
 p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);
 p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;
}
static void card70_publish(unsigned direction,unsigned distance,unsigned generation) {
 const char *enabled=getenv("CLUSTERMAP_RGD_EXPORT");
 if(!enabled||strcmp(enabled,"1")!=0)return;
 struct timespec ts;
 if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)return;
 uint64_t us=(uint64_t)ts.tv_sec*1000000ULL+(uint64_t)ts.tv_nsec/1000ULL;
 unsigned char b[24]={'R','G','7','0'};
 card70_be32(b+4,direction);card70_be32(b+8,distance);
 card70_be32(b+12,(uint32_t)(us>>32));card70_be32(b+16,(uint32_t)us);
 card70_be32(b+20,generation);
 int fd=socket(AF_INET,SOCK_DGRAM,0);
 if(fd<0)return;
 int flags=fcntl(fd,F_GETFL,0);
 if(flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0){
  struct sockaddr_in a;memset(&a,0,sizeof a);
  a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(19824);
  (void)sendto(fd,b,sizeof b,0,(struct sockaddr*)&a,sizeof a);
 }
 close(fd);
}
'''
s=s.replace(anchor,sender+"\n"+anchor,1)
anchor="    bus_send_text(EVT_RGD_UPDATE, BUS_FLAG_STICKY, b);\n}\n\nvoid rgd_clear_state"
assert s.count(anchor)==1
publish=r'''    /* Use the authoritative first maneuver, not the first available future slot.
     * Refresh only on a real distance update; never revive stale distance
     * because a lane event or unrelated cache replay arrived. */
    unsigned direction=0, distance=0;
    const uint64_t required=RGD_UPD_ROUTE_STATE|RGD_UPD_SOURCE_NAME|
        RGD_UPD_SOURCE_SUPPORTS_RG|RGD_UPD_DIST_TO_MANEUVER;
    if ((present&required)==required && upd->route_state==RGD_STATE_ROUTE_SET &&
        upd->source_supports_route_guidance &&
        (!strcmp(upd->source_name,"AMap")||!strcmp(upd->source_name,"\xe9\xab\x98\xe5\xbe\xb7\xe5\x9c\xb0\xe5\x9b\xbe")||
         !strcmp(upd->source_name,"\xe9\xab\x98\xe5\xbe\xb7")) &&
        g_rgd.current_list_count>0 && upd->dist_to_maneuver<=100000) {
        int active_slot=rgd_find_slot_for_iap_index_no_touch(g_rgd.current_list[0]);
        if(active_slot>=0&&active_slot<MANEUVER_CACHE_SIZE) {
            const rgd_maneuver_t *m=&g_rgd.slot_cache[active_slot];
            if((m->present&RGD_MAN_TYPE)&&m->maneuver_type>=1&&m->maneuver_type<=3){
                direction=m->maneuver_type;distance=upd->dist_to_maneuver;
            }
        }
    }
    if(!direction || (current_upd&&(current_upd->present&RGD_UPD_DIST_TO_MANEUVER)))
        card70_publish(direction,distance,g_rgd.route_generation);
'''
s=s.replace(anchor,publish+anchor,1)
anchor='void rgd_clear_state(const char* reason) {\n'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'    card70_publish(0,0,0);\n',1)
p.write_text(s)
for root in (mib,luka):subprocess.run(["git","-C",str(root),"diff","--check"],check=True)
print("Source bridge applied; target RGI hook compatibility and actual map-only layout remain unverified.")
