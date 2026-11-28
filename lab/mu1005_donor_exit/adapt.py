#!/usr/bin/env python3
"""Eighth adapter: actual full donor failure and cleanup continuity."""
from pathlib import Path
import sys,subprocess
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
p=up/"wireless/daemon/src/main.rs"
s=p.read_text()
def rep(a,b):
    global s
    if s.count(a)!=1:raise ValueError("anchor "+a[:120])
    s=s.replace(a,b)
rep('    let ack_mode=inherited || argument.as_deref()==Some("--mu1005-ack");','''    let ack_mode=inherited || argument.as_deref()==Some("--mu1005-ack");
    #[cfg(target_os="nto")]
    if inherited && std::env::var("MU1005_BRIDGE_READY").as_deref()!=Ok("1") {
        return Err(std::io::Error::other("MU1005 inherited donor requires supervisor readiness"));
    }''')
rep('''            if let Err(error) = wifi_ie::install_on_qnx(bt_mac, device_id) {
                let _ = wifi_ie::remove_on_qnx(bt_mac, device_id);
                return Err(std::io::Error::other(error));
            }''','''            // Failed installation has uncertain ownership; never delete a
            // possibly preexisting stock IE merely because bytes match.
            wifi_ie::install_on_qnx(bt_mac, device_id).map_err(std::io::Error::other)?;''')
rep('''        if inherited && std::env::var("MU1005_BRIDGE_READY").as_deref()==Ok("1") {
            use std::io::Write;
            stream.write_all(b"MU1005-READY-v1\\n")?;
        }''','''        let ready_result=if inherited && std::env::var("MU1005_BRIDGE_READY").as_deref()==Ok("1") {
            use std::io::Write;
            stream.write_all(b"MU1005-READY-v1\\n")
        } else {Ok(())};''')
for auth in ['auth_factory','|| UnavailableAuth']:
    a='''        let result = serve_stream_with_receiver_mode(
            &mut stream,
            || start.elapsed().as_millis() as u64,
            &config,
            '''+auth+''',
            &receiver,
            ack_mode,
        );'''
    rep(a,a.replace('let result = serve_stream_with_receiver_mode(', 'let result = ready_result.and_then(|()|serve_stream_with_receiver_mode(').replace('        );','        ));'))
rep('''        receiver.reset()?;
        receiver.set_peer(None)?;
        #[cfg(target_os = "nto")]
        {
            ie_active.store(false, Ordering::Release);
            if ie_monitor.join().is_err() {
                eprintln!("[wifi-ie] monitor panicked");
            }
            wifi_ie::remove_on_qnx(
                config.bt_mac,
                wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?,
            )
            .map_err(std::io::Error::other)?;
        }
        if let Err(error) = result {
            eprintln!("[bridge] session ended: {error}");
        }''','''        // Independent cleanup calls must run even when READY or receiver
        // reset failed. Stop the monitor before touching the IE.
        #[cfg(target_os = "nto")]
        let monitor_result={
            ie_active.store(false, Ordering::Release);
            ie_monitor.join().map_err(|_|std::io::Error::other("wifi IE monitor panicked"))
        };
        let reset_result=receiver.reset();
        let peer_result=receiver.set_peer(None);
        #[cfg(target_os = "nto")]
        let ie_result=monitor_result.and_then(|()| {
            wifi_ie::remove_on_qnx(
                config.bt_mac,
                wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?,
            ).map_err(std::io::Error::other)
        });
        #[cfg(not(target_os = "nto"))]
        let ie_result:std::io::Result<()>=Ok(());
        let cleanup=reset_result.and(peer_result).and(ie_result);
        if let Err(ref error)=cleanup {
            eprintln!("[bridge] cleanup failed: {error}");
        }
        if let Err(ref error)=result {
            eprintln!("[bridge] session ended: {error}");
        }
        // Inherited mode has exactly one session. Report actual errors to
        // the supervisor instead of exiting successfully after a failure.
        if inherited {return result.and(cleanup);}
        cleanup?;''')
p.write_text(s)
print("MU1005_DONOR_SESSION_FAILURE_CLEANUP_ADAPTED")
