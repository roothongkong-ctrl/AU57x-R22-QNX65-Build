use au57x_r103_bridge::{Frame, CONNECTED, DATA, DISCONNECTED};
use au57x_r103_bridge::tcp_lab::serve_single_connection;
use iap2_link::IAP2_MARKER;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::thread;
use std::time::Duration;

fn packet(k: u8, generation: u32, payload: &[u8]) -> Vec<u8> {
    Frame {kind:k,generation,payload:payload.to_vec()}.encode().unwrap()
}
fn response(c: &mut TcpStream) -> Frame {
    let mut head=[0u8;8];c.read_exact(&mut head).unwrap();
    let n=u16::from_le_bytes([head[2],head[3]]) as usize;
    assert!(n<=5035);
    let mut full=head.to_vec();full.resize(n+8,0);c.read_exact(&mut full[8..]).unwrap();
    Frame::decode(&full).unwrap()
}
#[test]
fn original_iap2_engine_runs_over_real_loopback_socket() {
    let listener=TcpListener::bind("127.0.0.1:0").unwrap();
    let port=listener.local_addr().unwrap();
    let worker=thread::spawn(move || {
        let (stream,peer)=listener.accept().unwrap();
        assert!(peer.ip().is_loopback());
        serve_single_connection(stream).unwrap();
    });
    let mut client=TcpStream::connect(port).unwrap();
    client.set_read_timeout(Some(Duration::from_secs(4))).unwrap();
    client.write_all(&packet(CONNECTED,7,&[1,2,3,4,5,6])).unwrap();
    let f=response(&mut client);
    assert_eq!(f.kind,DATA);assert_eq!(f.generation,7);assert_eq!(f.payload,IAP2_MARKER);
    client.write_all(&packet(DATA,7,&IAP2_MARKER)).unwrap();
    let f=response(&mut client);
    assert_eq!(f.kind,DATA);assert_eq!(f.generation,7);
    assert_eq!(&f.payload[..2], &[0xff,0x5a]);
    assert_eq!(f.payload[4],0x80); // LuKa's genuine link-layer SYN
    client.write_all(&packet(DISCONNECTED,7,&[])).unwrap();
    worker.join().unwrap();
}
#[test]
fn localhost_rejects_other_generation_instead_of_reconnecting() {
    let listener=TcpListener::bind("127.0.0.1:0").unwrap();
    let addr=listener.local_addr().unwrap();
    let worker=thread::spawn(move||{
        let (stream,_)=listener.accept().unwrap();
        let e=serve_single_connection(stream).unwrap_err();
        assert_eq!(e.kind(),std::io::ErrorKind::InvalidData);
    });
    let mut c=TcpStream::connect(addr).unwrap();c.set_read_timeout(Some(Duration::from_secs(4))).unwrap();
    c.write_all(&packet(CONNECTED,1,&[1,2,3,4,5,6])).unwrap();
    assert_eq!(response(&mut c).payload,IAP2_MARKER);
    c.write_all(&packet(DATA,2,&IAP2_MARKER)).unwrap();
    worker.join().unwrap();
}
