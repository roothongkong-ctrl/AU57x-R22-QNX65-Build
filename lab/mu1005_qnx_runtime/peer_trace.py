"""Add bounded diagnostics to disposable QNX fixture source; do not relax proof."""
from pathlib import Path
import sys
p,t=map(Path,sys.argv[1:])
s=p.read_text()
def one(s,a,b):
    if s.count(a)!=1:raise ValueError("unexpected anchor "+a)
    return s.replace(a,b)
s=one(s,"static int nonblock(int fd)",'''static void peer_trace(const char *stage,int rc,int family){
 static unsigned count;
 int saved=errno;
 if(count++<48)fprintf(stderr,"QNX_CPRX_TRACE %s rc=%d errno=%d family=%d\\n",stage,rc,saved,family);
 errno=saved;
}
static int nonblock(int fd)''')
s=one(s,''' if(fd<0||!uid||!gid||getsockname(fd,(struct sockaddr*)&sa,&len)||
    sa.ss_family!=AF_UNIX)return 0;
#ifdef __QNXNTO__
 return getpeereid(fd,uid,gid)==0;''',''' if(fd<0||!uid||!gid)return 0;
 memset(&sa,0,sizeof sa);
 int rc=getsockname(fd,(struct sockaddr*)&sa,&len);
 peer_trace("getsockname",rc,sa.ss_family);
 if(rc||sa.ss_family!=AF_UNIX)return 0;
#ifdef __QNXNTO__
 rc=getpeereid(fd,uid,gid);
 peer_trace("getpeereid",rc,sa.ss_family);
 return rc==0;''')
s=one(s,'''  if(!peer_verified(s,fd)||nonblock(fd)||safe_send(fd)){close(fd);return 0;}''','''  if(!peer_verified(s,fd)){peer_trace("accept_peer_denied",-1,0);close(fd);return 0;}
  if(nonblock(fd)||safe_send(fd)){peer_trace("accept_socket_flags",-1,0);close(fd);return 0;}''')
# Add the exact remote path connect trace, not the local client path.
s=one(s,''' rc=connect(fd,(struct sockaddr*)&addr,(socklen_t)SUN_LEN(&addr));
#else
 rc=connect(fd,(struct sockaddr*)&addr,
     (socklen_t)(offsetof(struct sockaddr_un,sun_path)+n+1));''',''' rc=connect(fd,(struct sockaddr*)&addr,(socklen_t)SUN_LEN(&addr));
 peer_trace("remote_connect",rc,AF_UNIX);
#else
 rc=connect(fd,(struct sockaddr*)&addr,
     (socklen_t)(offsetof(struct sockaddr_un,sun_path)+n+1));''')
p.write_text(s)
s=t.read_text()
s=one(s,''' assert(acked&&native_start_calls==1&&p.remote_phase==2);''',''' fprintf(stderr,"QNX_FIRST_LINK_ACK_STATE ack=%d starts=%u phase=%u client_phase=%u server_client=%d server_used=%u\\\\n",acked,native_start_calls,(unsigned)p.remote_phase,(unsigned)p.remote_client.phase,rx.client,rx.used);
 assert(acked&&native_start_calls==1&&p.remote_phase==2);''')
s=one(s,''' native_remote_owner_cloud_pipeline();
 puts("MU1005_QNX_FIRST_LINK_PIPELINE_PASS_FIXTURE_ONLY");''','''#ifdef MU1005_QNX_LOCAL_OWNER_FIXTURE
 native_dio_bound_pipeline();
 puts("QNX_LOCAL_OWNER_START_RESTORE_PASS_FIXTURE_ONLY");
#else
 native_remote_owner_cloud_pipeline();
 puts("MU1005_QNX_FIRST_LINK_PIPELINE_PASS_FIXTURE_ONLY");
#endif''')
t.write_text(s)
