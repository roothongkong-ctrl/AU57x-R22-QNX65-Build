#!/usr/bin/env python3
"""Reproducible source adaptation of pinned donor; no vehicle installation."""
from pathlib import Path
import subprocess, shutil, sys
root=Path(__file__).resolve().parents[2]
up=Path(sys.argv[1]).resolve()
if subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()!="20c94f51c0f0ae88b154c12e4f604643ba23022c":
    raise SystemExit("wrong donor revision")
daemon=up/"wireless/daemon"
def replace(path,a,b):
    p=daemon/path;s=p.read_text()
    if s.count(a)!=1:raise SystemExit(f"unexpected donor anchor {path}: {a}")
    p.write_text(s.replace(a,b))
shutil.copyfile(root/"integration/mu1005_donor_daemon/mu1005_ap.rs",daemon/"src/mu1005_ap.rs")
ap=root/"integration/mu1005_si_native/ap_live"
# Reuse the exact audited original read-only prefix; the CSM encoder
# function at the end is not needed because Rust already encodes CSM.
s=(ap/"ap_live.c").read_text()
anchor="int mu_ap_live_session("
if s.count(anchor)!=1:raise SystemExit("unexpected C live AP source")
(daemon/"src/mu1005_ap_live.c").write_text(s.split(anchor)[0])
shutil.copyfile(ap/"ap_live.h",daemon/"src/ap_live.h")
shutil.copyfile(root/"project/source/src/cp_csm.h",daemon/"src/cp_csm.h")
replace("src/lib.rs","pub mod wifi_ie;","pub mod wifi_ie;\nmod mu1005_ap;")
replace("build.rs",'vec!["src/qnx_i2c.c", "src/qnx_process.c"]','vec!["src/qnx_i2c.c", "src/qnx_process.c", "src/mu1005_ap_live.c"]')
replace("src/control.rs","    fn from_lookup(get: &impl Fn(&str) -> Option<String>) -> Result<Self, String> {",'''    fn from_lookup(get: &impl Fn(&str) -> Option<String>) -> Result<Self, String> {
        Self::from_lookup_live(get,None)
    }
    /// The QNX adapted executable reads original AP parameters rather
    /// than trusting static environment credentials. Host tests retain
    /// the unmodified donor configuration contract.
    pub fn from_env_mu1005() -> Result<Self,String> {
        #[cfg(target_os="nto")]
        {
            let ap=crate::mu1005_ap::read_stock()?;
            Self::from_lookup_live(&|name|std::env::var(name).ok(),Some(&ap))
        }
        #[cfg(not(target_os="nto"))]
        {Self::from_env()}
    }
    /// Re-read for each accepted TCP connection, before any iAP2 byte
    /// exchange. Preserve factory identity, device ID and AirPlay profile.
    pub fn refreshed_mu1005_ap(&self) -> Result<Self,String> {
        #[cfg(target_os="nto")]
        {
            let ap=crate::mu1005_ap::read_stock()?;
            Ok(self.with_ap(&ap))
        }
        #[cfg(not(target_os="nto"))]
        {Ok(self.clone())}
    }
    fn with_ap(&self,ap:&crate::mu1005_ap::ApSnapshot)->Self {
        let mut cfg=self.clone();
        cfg.ssid=ap.ssid.clone();cfg.passphrase=ap.passphrase.clone();cfg.channel=ap.channel;
        cfg
    }
    fn from_lookup_live(get: &impl Fn(&str) -> Option<String>,ap:Option<&crate::mu1005_ap::ApSnapshot>) -> Result<Self,String> {''')
replace("src/control.rs",'''            ssid: read("MHI2Q_CP_SSID")?,
            passphrase: read("MHI2Q_CP_PASSPHRASE")?,
            channel: read("MHI2Q_CP_CHANNEL")?
                .parse()
                .map_err(|_| "invalid MHI2Q_CP_CHANNEL".to_string())?,''','''            ssid: if let Some(a)=ap {a.ssid.clone()} else {read("MHI2Q_CP_SSID")?},
            passphrase: if let Some(a)=ap {a.passphrase.clone()} else {read("MHI2Q_CP_PASSPHRASE")?},
            channel: if let Some(a)=ap {a.channel} else {read("MHI2Q_CP_CHANNEL")?
                .parse().map_err(|_| "invalid MHI2Q_CP_CHANNEL".to_string())?},''')
replace("src/control.rs","if !(1..=11).contains(&config.channel) {","if ap.is_none() && !(1..=11).contains(&config.channel) {")
p=daemon/"src/main.rs";s=p.read_text()
a="Config::from_env().map_err(std::io::Error::other)?"
if s.count(a)!=2:raise SystemExit("unexpected main config anchors")
p.write_text(s.replace(a,"Config::from_env_mu1005().map_err(std::io::Error::other)?"))
replace("src/main.rs","        let mut stream = accepted?;","        let mut stream = accepted?;\n        let config = config.refreshed_mu1005_ap().map_err(std::io::Error::other)?;")
# Hardware signer remains upstream, no synthetic identity or alternate MFi.
# Explicitly require real per-unit path/address, not donor default.
replace("src/main.rs","    let mfi_bus = QnxBus::from_env().map_err(std::io::Error::other)?;",'''    let mfi_bus = {
        if std::env::var("MHI2Q_CP_MFI_PATH").ok().filter(|s|!s.is_empty()).is_none()
           || std::env::var("MHI2Q_CP_MFI_ADDR").ok().filter(|s|!s.is_empty()).is_none() {
            return Err(std::io::Error::other("MU1005 requires verified original MFi path and address"));
        }
        QnxBus::from_env().map_err(std::io::Error::other)?
    };''')
# New regression uses donor's own protocol decoder and existing test-only
# authentication fixture. This is not the production authenticator.
replace("src/control.rs","    #[test]\n    fn authentication_first_keeps_bootstrap_gated_until_identification()",'''    #[test]
    fn mu1005_live_ap_both_bands_reach_real_donor_wire() {
        for ch in [11,149] {
            let ap=crate::mu1005_ap::ApSnapshot::checked("live-fixture".into(),"live-test-key".into(),ch).unwrap();
            let old=config();
            let cfg=old.with_ap(&ap);
            assert_eq!(cfg.device_id,old.device_id);
            assert_eq!(cfg.ip_address,old.ip_address);
            assert_eq!(cfg.source_version,old.source_version);
            let mut m=ControlMachine::new(cfg,TestAuth);
            authenticate(&mut m);
            m.handle(&IdentificationAccepted {}.encode()).unwrap();
            let wifi=m.handle(&RequestAccessoryWiFiConfigurationInformation {}.encode()).unwrap().unwrap();
            let wifi=AccessoryWiFiConfigurationInformation::decode(&wifi).unwrap();
            assert_eq!(wifi.channel,ch);
            assert_eq!(wifi.ssid.as_deref(),Some("live-fixture"));
            assert_eq!(wifi.passphrase.as_deref(),Some("live-test-key"));
            let availability=CarPlayAvailability {wired_attributes:None,
                wireless_attributes:Some(CarPlayAvailabilityWirelessAttributes{available:Some(true),bluetooth_transport_identifier:None})}.encode();
            let start=m.handle(&availability).unwrap().unwrap();
            let parsed=CarPlayStartSession::decode(&start).unwrap();
            let wireless=parsed.wireless_attributes.unwrap();
            assert_eq!(wireless.channel,Some(ch));
            assert_eq!(wireless.wifi_ssid.as_deref(),Some("live-fixture"));
            assert_eq!(wireless.passphrase.as_deref(),Some("live-test-key"));
        }
    }
    #[test]
    fn authentication_first_keeps_bootstrap_gated_until_identification()''')
# Test live config does not need/accept stale static AP environment fields.
replace("src/control.rs","        let config = Config::from_lookup(&lookup).unwrap();",'''        let live=crate::mu1005_ap::ApSnapshot::checked("actual-ap".into(),"actual-key".into(),149).unwrap();
        let cfg=Config::from_lookup_live(&lookup,Some(&live)).unwrap();
        assert_eq!(cfg.channel,149);
        assert_eq!(cfg.ssid,"actual-ap");
        assert_eq!(cfg.passphrase,"actual-key");
        let config = Config::from_lookup(&lookup).unwrap();''')
print("MU1005_DONOR_AP_ADAPTED_PINNED_SOURCE_NO_INSTALL")
