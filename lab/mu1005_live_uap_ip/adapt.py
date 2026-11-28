#!/usr/bin/env python3
"""Sixth adapter: bind live AP IPv4 to real donor Config and StartSession."""
from pathlib import Path
import sys,subprocess
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
d=up/"wireless/daemon"
def rep(name,a,b):
    p=d/name;s=p.read_text()
    if s.count(a)!=1:raise ValueError("anchor: "+name+" "+a[:100])
    p.write_text(s.replace(a,b))
# Existing build.rs already compiles the AP C translation unit for QNX.
with (d/"src/mu1005_ap_live.c").open("a") as f:
    f.write("\n"+Path(__file__).with_name("uap_ipv4.c").read_text())
rep("src/mu1005_ap.rs","    pub(crate) channel: u8,","    pub(crate) channel: u8,\n    pub(crate) ip_address: Option<String>,")
rep("src/mu1005_ap.rs","Ok(Self {ssid,passphrase,channel})","Ok(Self {ssid,passphrase,channel,ip_address:None})")
rep("src/mu1005_ap.rs","        fn mu_ap_live_clear(p:*mut c_void,n:usize);","        fn mu_ap_live_clear(p:*mut c_void,n:usize);\n        fn mu1005_uap_ipv4(out:*mut u8)->c_int;")
rep("src/mu1005_ap.rs","    let mut uncertain:c_int=0;","    let mut uncertain:c_int=0;\n    let mut before=[0u8;4];let mut after=[0u8;4];\n    let before_rc=unsafe{mu1005_uap_ipv4(before.as_mut_ptr())};")
rep("src/mu1005_ap.rs","    let result=(|| {","    let after_rc=unsafe{mu1005_uap_ipv4(after.as_mut_ptr())};\n    let result=(|| {")
rep("src/mu1005_ap.rs","        ApSnapshot::checked(string(&raw.ssid)?,string(&raw.key)?,raw.channel)","""        if before_rc!=0||after_rc!=0||before!=after {
            return Err("MU1005 uap0 IPv4 unavailable or changed".into());
        }
        let mut ap=ApSnapshot::checked(string(&raw.ssid)?,string(&raw.key)?,raw.channel)?;
        ap.ip_address=Some(std::net::Ipv4Addr::from(after).to_string());
        Ok(ap)""")
rep("src/control.rs","cfg.ssid=ap.ssid.clone();cfg.passphrase=ap.passphrase.clone();cfg.channel=ap.channel;","cfg.ssid=ap.ssid.clone();cfg.passphrase=ap.passphrase.clone();cfg.channel=ap.channel;\n        if let Some(ip)=&ap.ip_address {cfg.ip_address=ip.clone();}")
rep("src/control.rs",'            ip_address: read("MHI2Q_CP_IP_ADDRESS")?,','            ip_address: match ap.and_then(|a|a.ip_address.as_ref()) {Some(ip)=>ip.clone(),None=>read("MHI2Q_CP_IP_ADDRESS")?},')
# Host fixtures prove the live address overrides the previous configured one.
rep("src/control.rs",'let ap=crate::mu1005_ap::ApSnapshot::checked("live-fixture".into(),"live-test-key".into(),ch).unwrap();','let mut ap=crate::mu1005_ap::ApSnapshot::checked("live-fixture".into(),"live-test-key".into(),ch).unwrap();\n            ap.ip_address=Some("192.0.2.1".into());')
rep("src/control.rs","assert_eq!(cfg.ip_address,old.ip_address);",'assert_eq!(cfg.ip_address,"192.0.2.1");')
rep("src/control.rs","assert_eq!(wireless.channel,Some(ch));",'assert_eq!(wireless.channel,Some(ch));\n            assert_eq!(wireless.ip_address,vec!["192.0.2.1".to_string()]);')
rep("src/control.rs",'let live=crate::mu1005_ap::ApSnapshot::checked("actual-ap".into(),"actual-key".into(),149).unwrap();','let mut live=crate::mu1005_ap::ApSnapshot::checked("actual-ap".into(),"actual-key".into(),149).unwrap();\n        live.ip_address=Some("192.0.2.1".into());')
rep("src/control.rs",'assert_eq!(cfg.passphrase,"actual-key");','assert_eq!(cfg.passphrase,"actual-key");\n        assert_eq!(cfg.ip_address,"192.0.2.1");')
print("MU1005_LIVE_UAP_IPV4_SOURCE_ADAPTED")
