use au57x_r103_bridge::{BridgeError, CONNECTED, DATA, DISCONNECTED, Frame, ReviewBridge};
use iap2_link::{IAP2_MARKER, LinkState};

fn incoming(k: u8, g: u32, p: &[u8]) -> Frame {
    Frame { kind: k, generation: g, payload: p.to_vec() }
}
fn connected(g: u32) -> Frame { incoming(CONNECTED, g, &[1, 2, 3, 4, 5, 6]) }

#[test]
fn exact_c_little_endian_header_and_decode() {
    let f = incoming(DATA, 0x01020304, &[0xff, 0x55, 2]);
    let bytes = f.encode().unwrap();
    assert_eq!(bytes, [2, 0, 3, 0, 4, 3, 2, 1, 0xff, 0x55, 2]);
    assert_eq!(Frame::decode(&bytes), Ok(f));
}
#[test]
fn rejects_invalid_wire_frames() {
    for f in [vec![2,0,1,0], vec![2,1,1,0,1,0,0,0,7], vec![2,0,0,0,1,0,0,0],
              vec![1,0,0,0,1,0,0,0], vec![3,0,1,0,1,0,0,0,9],
              vec![2,0,1,0,0,0,0,0,7], vec![2,0,2,0,1,0,0,0,7]] {
        assert_eq!(Frame::decode(&f), Err(BridgeError::Malformed));
    }
    assert!(incoming(DATA, 1, &vec![7; 5036]).encode().is_err());
}
#[test]
fn connect_sends_real_upstream_iap2_marker_then_syn() {
    let mut b = ReviewBridge::new();
    let v = b.accept(connected(1), 0).unwrap();
    assert_eq!(v.len(), 1);
    assert_eq!(v[0].payload, IAP2_MARKER);
    let v = b.accept(incoming(DATA, 1, &IAP2_MARKER), 50).unwrap();
    assert!(v[0].payload.starts_with(&[0xff, 0x5a]));
    assert_eq!(b.link_state(), Some(LinkState::Negotiate));
    assert_eq!(v[0].payload[4], 0x80); // real upstream SYN, not MOCK MFi
}
#[test]
fn no_second_owner_or_cross_generation_replay() {
    let mut b = ReviewBridge::new();
    b.accept(connected(7), 0).unwrap();
    assert_eq!(b.accept(connected(7), 1), Err(BridgeError::UnexpectedState));
    assert_eq!(b.accept(incoming(DATA, 9, &[0xff]), 1), Err(BridgeError::WrongGeneration));
    assert_eq!(b.accept(incoming(DISCONNECTED, 9, &[]), 1), Err(BridgeError::WrongGeneration));
}
#[test]
fn disconnect_or_socket_loss_is_terminal() {
    let mut b = ReviewBridge::new();
    b.accept(connected(11), 0).unwrap();
    assert_eq!(b.accept(incoming(DISCONNECTED, 11, &[]), 10), Ok(Vec::new()));
    assert!(b.is_terminal());
    assert_eq!(b.accept(connected(12), 11), Err(BridgeError::Terminated));
    let mut c = ReviewBridge::new();
    c.accept(connected(20), 0).unwrap();
    c.socket_lost();
    assert_eq!(c.advance_time(2000), Err(BridgeError::Terminated));
}
#[test]
fn timeout_is_handled_by_real_upstream_link_engine() {
    let mut b = ReviewBridge::new();
    b.accept(connected(42), 0).unwrap();
    let repeat = b.advance_time(1000).unwrap();
    assert_eq!(repeat.len(), 1);
    assert_eq!(repeat[0].payload, IAP2_MARKER);
}
#[test]
fn wrong_phone_marker_terminates_session() {
    let mut b = ReviewBridge::new();
    b.accept(connected(13), 0).unwrap();
    assert_eq!(b.accept(incoming(DATA, 13, &[0,0,0,0,0,0]), 1), Err(BridgeError::LinkDead));
    assert!(b.is_terminal());
}
