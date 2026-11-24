// SPDX-License-Identifier: GPL-3.0-only
// Test executes upstream BridgeLink, not a second reimplementation.
use mhi2q_wireless_daemon::{BridgeLink,FrameDecoder,TYPE_CONNECTED,TYPE_DATA};
use std::{io::{Read,Write,ErrorKind},net::TcpListener,process::Command,time::{Duration,Instant}};
#[test]
fn r45_c_socket_to_upstream_link(){
    let listener=TcpListener::bind("127.0.0.1:57342").unwrap();
    listener.set_nonblocking(true).unwrap();
    let mut child=Command::new(std::env::var("AU57X_C_FIXTURE").unwrap()).spawn().unwrap();
    let start=Instant::now();
    let mut stream=loop{
        match listener.accept(){
            Ok((s,_))=>break s,
            Err(e) if e.kind()==ErrorKind::WouldBlock && start.elapsed()<Duration::from_secs(5)=>std::thread::sleep(Duration::from_millis(5)),
            Err(e)=>panic!("accept: {e}"),
        }
    };
    stream.set_read_timeout(Some(Duration::from_millis(100))).unwrap();
    stream.set_write_timeout(Some(Duration::from_secs(1))).unwrap();
    let mut decoder=FrameDecoder::default();
    let mut link=BridgeLink::default();
    let mut bytes=[0u8;37];
    let mut connected=0;let mut phone_bytes=Vec::new();let mut returned=Vec::new();
    while start.elapsed()<Duration::from_secs(7){
        let n=match stream.read(&mut bytes){
            Ok(0)=>break,
            Ok(n)=>n,
            Err(e) if matches!(e.kind(),ErrorKind::WouldBlock|ErrorKind::TimedOut)=>continue,
            Err(e) if e.kind()==ErrorKind::ConnectionReset=>break,
            Err(e)=>panic!("read: {e}"),
        };
        for frame in decoder.feed(&bytes[..n]).unwrap(){
            assert_eq!(frame.generation,1);
            if frame.kind==TYPE_CONNECTED{
                connected+=1;assert_eq!(frame.data,[1,2,3,4,5,6]);
            }else if frame.kind==TYPE_DATA{
                phone_bytes.extend_from_slice(&frame.data);
            }else{panic!("unexpected frame");}
            link.handle(frame,start.elapsed().as_millis() as u64);
            while let Some(reply)=link.next_outbound(){
                assert_eq!(reply.kind,TYPE_DATA);assert_eq!(reply.generation,1);
                returned.extend_from_slice(&reply.data);
                let encoded=reply.encode().unwrap();
                // Deliberately fragment TCP writes across header and payload.
                for chunk in encoded.chunks(3){stream.write_all(chunk).unwrap();}
            }
        }
    }
    assert!(child.wait().unwrap().success());
    assert_eq!(connected,1);
    assert_eq!(phone_bytes,[0xff,0x55,2,0,0xee,0x10]);
    assert!(returned.starts_with(&[0xff,0x55,2,0,0xee,0x10,0xff,0x5a]));
    println!("AU57X_C_TO_UPSTREAM_LINK_OK_NO_OEM");
}
