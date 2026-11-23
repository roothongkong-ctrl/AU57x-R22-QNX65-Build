//! A single real localhost socket transport to exercise the donor LinkEngine.
//! This is a HOST-LAB service, not the MMI production daemon.  It binds no port,
//! creates no Bluetooth advertisement, and never touches a vehicle.
use crate::{BridgeError, FRAME_HEADER, Frame, RF_MAX, ReviewBridge, DISCONNECTED};
use std::io::{self, Read, Write};
use std::net::TcpStream;

fn invalid(reason: &'static str) -> io::Error { io::Error::new(io::ErrorKind::InvalidData, reason) }

pub fn serve_single_connection(mut stream: TcpStream) -> io::Result<()> {
    let mut session = ReviewBridge::new();
    let mut header = [0u8; FRAME_HEADER];
    loop {
        if let Err(e) = stream.read_exact(&mut header) { session.socket_lost(); return Err(e); }
        let len = u16::from_le_bytes([header[2], header[3]]) as usize;
        if len > RF_MAX { session.socket_lost(); return Err(invalid("oversize RFCOMM frame")); }
        let mut packet = Vec::with_capacity(FRAME_HEADER + len);
        packet.extend_from_slice(&header);
        packet.resize(FRAME_HEADER + len, 0);
        if let Err(e) = stream.read_exact(&mut packet[FRAME_HEADER..]) { session.socket_lost(); return Err(e); }
        let frame = Frame::decode(&packet).map_err(|_| invalid("bad RFCOMM frame"))?;
        let disconnected = frame.kind == DISCONNECTED;
        let replies = session.accept(frame, 0).map_err(|e| match e {
            BridgeError::WrongGeneration => invalid("replayed RFCOMM generation"),
            _ => invalid("RFCOMM/iAP2 session rejected"),
        })?;
        for reply in replies {
            stream.write_all(&reply.encode().map_err(|_|invalid("bad outbound frame"))?)?;
        }
        if disconnected { return Ok(()); } // Never accept a second session on this FD.
    }
}
