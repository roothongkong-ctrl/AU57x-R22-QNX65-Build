"""Read-only diagnostics injected in disposable runner copy."""
from pathlib import Path
import sys
p,t=map(Path,sys.argv[1:])
s=p.read_text()
def one(s,a,b):
    if s.count(a)!=1:raise ValueError("anchor "+a)
    return s.replace(a,b)
s=one(s,"static int nonblock(int fd)",'''static void ack_trace(const char *stage,int result,unsigned phase,unsigned n){
 static unsigned count;
 int e=errno;
 if(count++<100)fprintf(stderr,"QNX_ACK_TRACE %s result=%d phase=%u bytes=%u errno=%d\\n",stage,result,phase,n,e);
 errno=e;
}
static int nonblock(int fd)''')
s=one(s,"ssize_t w=send(s->client,s->reply+s->sent,s->size-s->sent,MSG_NOSIGNAL);if(w>0)",'''ssize_t w=send(s->client,s->reply+s->sent,s->size-s->sent,MSG_NOSIGNAL);
 ack_trace("server_send",(int)w,s->reply[5],s->size);
 if(w>0)''')
s=one(s,'''    c->remote.dio_server_verified(c->remote.owner_ctx,c->fd,
        c->generation,c->epoch,c->session)!=1)return client_fail(c);''','''    c->remote.dio_server_verified(c->remote.owner_ctx,c->fd,
        c->generation,c->epoch,c->session)!=1){
   ack_trace("client_owner_denied",-1,c->phase,c->received);
   return client_fail(c);
 }''')
s=one(s,"ssize_t n=recv(c->fd,c->reply+c->received,need-c->received,0);","ssize_t n=recv(c->fd,c->reply+c->received,need-c->received,0);\\n   ack_trace(\"client_recv\",(int)n,c->phase,c->received);".replace("\\n","\n"))
p.write_text(s)
s=t.read_text()
s=one(s," assert(acked&&native_start_calls==1&&p.remote_phase==2);",''' fprintf(stderr,"QNX_FIRST_LINK_ACK_STATE ack=%d starts=%u phase=%u client_phase=%u server_client=%d server_used=%u\\n",acked,native_start_calls,(unsigned)p.remote_phase,(unsigned)p.remote_client.phase,rx.client,rx.used);
 assert(acked&&native_start_calls==1&&p.remote_phase==2);''')
t.write_text(s)
