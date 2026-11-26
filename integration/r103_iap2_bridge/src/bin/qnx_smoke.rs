use au57x_r103_bridge::{Frame, ReviewBridge, CONNECTED};

fn main() {
    let mut bridge = ReviewBridge::new();
    let frame = Frame {
        kind: CONNECTED,
        generation: 1,
        payload: vec![1, 2, 3, 4, 5, 6],
    };
    let output = bridge.accept(frame, 0).expect("initial iAP2 link output");
    assert!(!output.is_empty(), "upstream LinkEngine should emit its initial marker");
    assert!(output.iter().all(|f| f.kind == 2 && f.generation == 1));
    println!("AU57X_R103_QNX_IAP2_LINK_PASS");
}
