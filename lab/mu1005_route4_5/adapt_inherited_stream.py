#!/usr/bin/env python3
"""Opt-in inherited stream on actual pinned donor, after existing adapters.
The supervisor must attest exact executable, descriptor and child lifecycle.
No independent socket listener in inherited mode. No OEM activation."""
import subprocess,sys
from pathlib import Path
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
p=up/"wireless/daemon/src/main.rs"
s=p.read_text()
def rep(a,b):
 global s
 if s.count(a)!=1:raise ValueError("anchor "+a[:100])
 s=s.replace(a,b)
rep('    let ack_mode=argument.as_deref()==Some("--mu1005-ack");','''    let inherited=argument.as_deref()==Some("--mu1005-ack-fd");
    let ack_mode=inherited || argument.as_deref()==Some("--mu1005-ack");''')
rep('''    let listener = TcpListener::bind("127.0.0.1:57342")?;
    eprintln!("[bridge] listening on 127.0.0.1:57342");
    if ack_mode { eprintln!("MU1005_RF_ACK_MODE_ENABLED"); }
    for accepted in listener.incoming() {''','''    // In inherited mode, only the supervisor-provided connected endpoint
    // is consumed. Never open a public replacement listener on failure.
    let incoming: Box<dyn Iterator<Item=std::io::Result<std::net::TcpStream>>> =
        if inherited {
            #[cfg(unix)]
            {
                use std::os::fd::FromRawFd;
                let fd=std::env::var("MU1005_BRIDGE_FD").ok()
                    .and_then(|s|s.parse::<i32>().ok()).filter(|fd|*fd>=3)
                    .ok_or_else(||std::io::Error::other("missing inherited stream"))?;
                // Caller owns the live, exclusively transferred FD. Avoid
                // from_raw_fd on an invalid descriptor: probe it first.
                let stream=std::net::TcpStream::from(
                    inherited_tcp(fd)?);
                eprintln!("MU1005_INHERITED_ACK_STREAM");
                Box::new(std::iter::once(Ok(stream)))
            }
            #[cfg(not(unix))]
            {return Err(std::io::Error::other("inherited mode requires Unix"));}
        } else {
            let listener=TcpListener::bind("127.0.0.1:57342")?;
            eprintln!("[bridge] listening on 127.0.0.1:57342");
            Box::new(std::iter::from_fn(move ||Some(listener.accept().map(|(s,_)|s))))
        };
    if ack_mode { eprintln!("MU1005_RF_ACK_MODE_ENABLED"); }
    for accepted in incoming {''')
# Reuse Rust libc-independent std API via external C validator compiled by
# existing QNX/host local-connect build. Return a dup, close inherited fd only
# after validation. No raw invalid fd ownership or guessed QNX constants.
s=s.replace('                use std::os::fd::FromRawFd;\n','').replace('let stream=std::net::TcpStream::from(\n                    inherited_tcp(fd)?);','let stream=inherited_tcp(fd)?;')
s+='''
#[cfg(unix)]
fn inherited_tcp(fd:i32)->std::io::Result<std::net::TcpStream>{
    use std::os::fd::FromRawFd;
    unsafe extern "C" {fn mu1005_inherited_stream_validate(fd:i32)->i32;}
    let rc=unsafe{mu1005_inherited_stream_validate(fd)};
    if rc<0{return Err(std::io::Error::from_raw_os_error(-rc));}
    Ok(unsafe{std::net::TcpStream::from_raw_fd(fd)})
}
'''
p.write_text(s)
c=up/"wireless/daemon/src/mu1005_local_connect.c"
with c.open("a") as f:f.write(r'''
#include <netinet/in.h>
#include <arpa/inet.h>
/* Supervisor-provided accepted endpoint. Address is a topology check only,
 * not executable identity. No descriptor duplication or fallback. */
int mu1005_inherited_stream_validate(int fd){
 struct sockaddr_in a,b; socklen_t n=sizeof a;int type=0,fl;
 if(fd<3)return -EINVAL;
 fl=fcntl(fd,F_GETFD);if(fl<0)return -errno;
 if(getsockname(fd,(struct sockaddr*)&a,&n)||n!=sizeof a||
    a.sin_family!=AF_INET||a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
    a.sin_port!=htons(57342))return -EINVAL;
 n=sizeof b;
 if(getpeername(fd,(struct sockaddr*)&b,&n)||n!=sizeof b||
    b.sin_family!=AF_INET||b.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))return -EINVAL;
 n=sizeof type;
 if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&n)||n!=sizeof type||type!=SOCK_STREAM)return -EINVAL;
 if(fcntl(fd,F_SETFD,fl|FD_CLOEXEC)<0)return -errno;
 return 0;
}
''')
print("MU1005_DONOR_INHERITED_STREAM_SOURCE_ADAPTED")
