#!/usr/bin/env python3
"""Minimal opt-in ACK extension of pinned GPL upstream daemon.
No change to auth, receiver, radio, AP, or legacy wire mode."""
from pathlib import Path
import subprocess,sys
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
d=up/"wireless/daemon/src"
def rep(file,a,b):
    p=d/file;s=p.read_text()
    if s.count(a)!=1:raise ValueError(f"anchor {file}: {a[:80]}")
    p.write_text(s.replace(a,b))
rep("lib.rs","pub const TYPE_DISCONNECTED: u8 = 3;","pub const TYPE_DISCONNECTED: u8 = 3;\npub const TYPE_TX_COMPLETE: u8 = 4;")
rep("lib.rs","(self.kind == TYPE_DISCONNECTED && !self.data.is_empty())","(matches!(self.kind, TYPE_DISCONNECTED | TYPE_TX_COMPLETE) && !self.data.is_empty())")
rep("lib.rs","!matches!(self.kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED)","!matches!(self.kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED | TYPE_TX_COMPLETE)")
rep("lib.rs","!matches!(kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED)","!matches!(kind, TYPE_CONNECTED | TYPE_DATA | TYPE_DISCONNECTED | TYPE_TX_COMPLETE)")
rep("lib.rs","(kind == TYPE_DISCONNECTED && length != 0)","(matches!(kind, TYPE_DISCONNECTED | TYPE_TX_COMPLETE) && length != 0)")
rep("lib.rs",'''    mut auth_factory: impl FnMut() -> A,
    receiver: &receiver_control::ReceiverWorker,
) -> io::Result<()> {
    let mut decoder = FrameDecoder::default();''','''    auth_factory: impl FnMut() -> A,
    receiver: &receiver_control::ReceiverWorker,
) -> io::Result<()> {
    serve_stream_with_receiver_mode(stream,time_ms,config,auth_factory,receiver,false)
}

/// Explicit matching MU1005 transport only; legacy donor stays unchanged.
/// The launcher must independently attest the matching daemon and exact FD.
pub fn serve_stream_with_receiver_mode<S: Read + Write, A: control::AuthProvider>(
    stream: &mut S,
    time_ms: impl Fn() -> u64,
    config: &control::Config,
    mut auth_factory: impl FnMut() -> A,
    receiver: &receiver_control::ReceiverWorker,
    ack_mode: bool,
) -> io::Result<()> {
    let mut pending_tx: Option<(u32,u64)> = None;
    let mut decoder = FrameDecoder::default();''')
rep("lib.rs","                    let old_generation = link.generation();",'''                    if frame.kind == TYPE_TX_COMPLETE {
                        if !ack_mode || pending_tx.map(|(g,_)|g)!=Some(frame.generation)
                            || link.generation()!=Some(frame.generation) {
                            return Err(io::Error::new(io::ErrorKind::InvalidData,"unexpected RF completion"));
                        }
                        pending_tx=None;
                        continue;
                    }
                    if ack_mode && frame.kind==TYPE_CONNECTED && (frame.generation==0 ||
                       link.generation().is_some()) {
                        return Err(io::Error::new(io::ErrorKind::InvalidData,"unexpected RF generation"));
                    }
                    if ack_mode && frame.kind==TYPE_DISCONNECTED && link.generation()==Some(frame.generation) {
                        pending_tx=None;
                    }
                    let old_generation = link.generation();''')
rep("lib.rs",'''        while let Some(frame) = link.next_outbound() {
            stream.write_all(&frame.encode()?)?;
        }''','''        if ack_mode {
            if pending_tx.is_some_and(|(_,since)| time_ms().saturating_sub(since)>2000) {
                return Err(io::Error::new(io::ErrorKind::TimedOut,"RF completion timeout"));
            }
            if link.outbound.len()>64 {
                return Err(io::Error::new(io::ErrorKind::InvalidData,"RF output backlog"));
            }
            if pending_tx.is_none() {
                if let Some(frame)=link.next_outbound() {
                    if frame.kind!=TYPE_DATA || link.generation()!=Some(frame.generation) {
                        return Err(io::Error::new(io::ErrorKind::InvalidData,"invalid RF output generation"));
                    }
                    stream.write_all(&frame.encode()?)?;
                    pending_tx=Some((frame.generation,time_ms()));
                }
            }
        } else {
            while let Some(frame) = link.next_outbound() {
                stream.write_all(&frame.encode()?)?;
            }
        }''')
rep("main.rs","receiver_control::ReceiverWorker, serve_stream_with_receiver,","receiver_control::ReceiverWorker, serve_stream_with_receiver_mode,")
rep("main.rs","    if argument.is_some() {","    let ack_mode=argument.as_deref()==Some(\"--mu1005-ack\");\n    if argument.is_some() && !ack_mode {")
rep("main.rs","    eprintln!(\"[bridge] listening on 127.0.0.1:57342\");","    eprintln!(\"[bridge] listening on 127.0.0.1:57342\");\n    if ack_mode { eprintln!(\"MU1005_RF_ACK_MODE_ENABLED\"); }")
p=d/"main.rs";s=p.read_text();assert s.count("let result = serve_stream_with_receiver(")==2 and s.count("            &receiver,\n        );")==2
p.write_text(s.replace("let result = serve_stream_with_receiver(","let result = serve_stream_with_receiver_mode(").replace("            &receiver,\n        );","            &receiver,\n            ack_mode,\n        );"))
print("MU1005_DONOR_ACK_EXPLICIT_MODE_SOURCE_ADAPTED")
