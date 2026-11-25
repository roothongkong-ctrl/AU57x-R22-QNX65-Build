//! QNX-only laboratory executable. Uses the actual adapted donor engine,
 //! not the production entry point; no radio, MFi or receiver hardware.
use mhi2q_wireless_daemon::{control::{Config,UnavailableAuth},receiver_control::{ReceiverControl,ReceiverWorker},serve_stream_with_receiver};
use std::{io,net::TcpListener,time::{Duration,Instant}};
struct NoReceiver;
impl ReceiverControl for NoReceiver {
 fn start(&mut self,_:u32)->io::Result<()> {Err(io::Error::other("LAB: no OEM receiver"))}
 fn stop(&mut self,_:u32)->io::Result<()> {Ok(())}
}
fn main()->io::Result<()> {
 // Identical Config parser used by upstream; only IFS fixture identity.
 // Intentionally never calls from_env_mu1005, qnx_i2c or Wi-Fi IE setters.
 let config=Config::from_env().map_err(io::Error::other)?;
 let receiver=ReceiverWorker::new(NoReceiver);
 let listener=TcpListener::bind("127.0.0.1:57342")?;
 eprintln!("MU1005_DONOR_QNX_LAB_LISTENING");
 for _ in 0..2 {
  let(mut stream,_)=listener.accept()?;
  stream.set_read_timeout(Some(Duration::from_millis(20)))?;
  stream.set_write_timeout(Some(Duration::from_secs(2)))?;
  let start=Instant::now();
  serve_stream_with_receiver(&mut stream,||start.elapsed().as_millis() as u64,&config,||UnavailableAuth,&receiver)?;
  receiver.reset()?;receiver.set_peer(None)?;
 }
 println!("MU1005_DONOR_QNX_RUST_ENGINE_TWO_TCP_PASS_NO_OEM");
 Ok(())
}
