use au57x_r103_bridge::{Frame, ReviewBridge, CONNECTED, DATA};
use iap2_link::{
    gen_checksum, LinkPacketHeader, LinkState, LinkSynchronizationPayload, LspSession,
    IAP2_MARKER,
};

fn frame(kind: u8, generation: u32, payload: &[u8]) -> Frame {
    Frame { kind, generation, payload: payload.to_vec() }
}

fn iap2_packet(control: u8, seq: u8, ack: u8, session_id: u8, payload: Option<&[u8]>) -> Vec<u8> {
    let length = payload.map_or(9, |p| p.len() as u16 + 10);
    let header = LinkPacketHeader { length, control, seq, ack, session_id };
    let mut out = header.pack().to_vec();
    if let Some(p) = payload {
        out.extend_from_slice(p);
        out.push(gen_checksum(p));
    }
    out
}

fn main() {
    let mut bridge = ReviewBridge::new();

    // The native side announces one RFCOMM peer. The bridge emits the real
    // upstream iAP2 marker; this is not an MFi or CarPlay-success assertion.
    let out = bridge.accept(frame(CONNECTED, 7, &[1, 2, 3, 4, 5, 6]), 0)
        .expect("connected frame accepted");
    assert_eq!(out.len(), 1);
    assert_eq!(out[0].payload, IAP2_MARKER);

    // The phone marker causes the original LuKa LinkEngine to emit its SYN.
    let out = bridge.accept(frame(DATA, 7, &IAP2_MARKER), 1)
        .expect("phone marker accepted");
    assert_eq!(out.len(), 1);
    assert_eq!(&out[0].payload[..2], &[0xff, 0x5a]);
    assert_eq!(out[0].payload[4], 0x80);

    // Complete a real passive iAP2 link-layer negotiation using a valid peer
    // LSP/SYN and ACK packet, then assert the engine reaches Normal.
    let peer_lsp = LinkSynchronizationPayload {
        max_outgoing: 8,
        max_len: 1024,
        retransmission_timeout: 1000,
        ack_timeout: 100,
        max_retransmissions: 3,
        max_ack: 2,
        sessions: vec![LspSession { id: 10, kind: 0, version: 1 }],
    };
    let lsp_bytes = peer_lsp.pack();
    let peer_syn = iap2_packet(0x80, 0x2a, 0, 0, Some(&lsp_bytes));
    let out = bridge.accept(frame(DATA, 7, &peer_syn), 2)
        .expect("peer SYN accepted");
    assert_eq!(out.len(), 1);
    assert_eq!(out[0].payload.len(), 9);
    assert_eq!(out[0].payload[4], 0x40);

    let peer_ack = iap2_packet(0x40, 0, 99, 0, None);
    let out = bridge.accept(frame(DATA, 7, &peer_ack), 3)
        .expect("peer ACK accepted");
    assert!(out.is_empty());
    assert_eq!(bridge.link_state(), Some(LinkState::Normal));

    println!("AU57X_R103_QNX_IAP2_LINK_PASS");
}
