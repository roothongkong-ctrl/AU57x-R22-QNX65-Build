#!/usr/bin/env python3
"""Adapt the pinned donor's hardware MFi configuration and handshake order.
No signing implementation, credentials, external signing service or bypass.
Apply after adapt_live_ap.py and adapt_receiver_unix.py."""
from pathlib import Path
import subprocess,sys
up=Path(sys.argv[1]).resolve()
if subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()!="20c94f51c0f0ae88b154c12e4f604643ba23022c":
    raise SystemExit("wrong donor revision")
d=up/"wireless/daemon"
def rep(p,a,b):
    p=d/p;s=p.read_text()
    if s.count(a)!=1:raise SystemExit(f"unexpected anchor: {p}: {a}")
    p.write_text(s.replace(a,b))
# The previously audited MU1005 libairplay and factory configuration both
# use i2c8/address 0x11. This is static OEM evidence, NOT a runtime lease.
# Preserve explicit configuration and reject accidental other peripherals.
rep("src/mfi/qnx.rs",'''        if address > 0x7f {
            return Err("MFI I2C address must be 7-bit".into());
        }''','''        if path != "/dev/i2c8" || address != 17 {
            return Err("MU1005 MFi endpoint differs from original firmware evidence".into());
        }''')
# At the protocol layer do not turn AA05 into evidence of a signature if
# no provider call succeeded in this ControlMachine's generation.
rep("src/control.rs","    authentication_complete: bool,","    authentication_complete: bool,\n    certificate_prepared: bool,\n    signature_prepared: bool,")
rep("src/control.rs","            authentication_complete: false,","            authentication_complete: false,\n            certificate_prepared: false,\n            signature_prepared: false,")
rep("src/control.rs",'''                let cert = self.auth.certificate()?;
                Ok(Some(
                    AuthenticationCertificate { certificate: cert }.encode(),
                ))''','''                self.certificate_prepared=false;
                self.signature_prepared=false;
                let cert=match self.auth.certificate() {
                    Ok(v) if !v.is_empty() && v.len()<=1280=>v,
                    _=>{self.phase=Phase::Failed;return Err("original MFi certificate unavailable".into());}
                };
                self.certificate_prepared=true;
                Ok(Some(AuthenticationCertificate {certificate:cert}.encode()))''')
rep("src/control.rs",'''                let request = RequestAuthenticationChallengeResponse::decode(frame)
                    .map_err(|e| e.to_string())?;
                let response = self.auth.sign(&request.challenge)?;
                Ok(Some(AuthenticationResponse { response }.encode()))''','''                self.signature_prepared=false;
                if !self.certificate_prepared {
                    self.phase=Phase::Failed;
                    return Err("MFi challenge without certificate response".into());
                }
                let request=match RequestAuthenticationChallengeResponse::decode(frame) {
                    Ok(v)=>v,
                    Err(_)=>{self.phase=Phase::Failed;return Err("invalid MFi challenge".into());}
                };
                let response=match self.auth.sign(&request.challenge) {
                    Ok(v) if !v.is_empty() && v.len()<=128=>v,
                    _=>{self.phase=Phase::Failed;return Err("original MFi signature unavailable".into());}
                };
                self.signature_prepared=true;
                Ok(Some(AuthenticationResponse {response}.encode()))''')
rep("src/control.rs",'''            (_, 0xAA05) => {
                self.authentication_complete = true;
                self.update_phase();
                Ok(None)
            }''','''            (_, 0xAA05) => {
                if !self.certificate_prepared || !self.signature_prepared || frame.len()!=6 {
                    self.phase=Phase::Failed;
                    return Err("MFi success without current certificate and signature".into());
                }
                self.authentication_complete = true;
                self.update_phase();
                Ok(None)
            }''')
# Keep all existing valid-order tests. One old metadata fixture used
# a bare AA05; replace its setup with the existing full mock exchange.
rep("src/control.rs",'''        machine
            .handle(&AuthenticationSucceeded {}.encode())
            .unwrap();
        for _ in 0..100 {''','''        authenticate(&mut machine);
        for _ in 0..100 {''')
rep("src/control.rs","    #[test]\n    fn metadata_waits_for_authentication_and_has_bounded_backlog()",'''    #[test]
    fn mu1005_mfi_requires_current_real_provider_exchange() {
        let accepted=IdentificationAccepted {}.encode();
        let success=AuthenticationSucceeded {}.encode();
        let cert=RequestAuthenticationCertificate {}.encode();
        let challenge=RequestAuthenticationChallengeResponse{challenge:vec![4,5,6]}.encode();
        let wifi=RequestAccessoryWiFiConfigurationInformation {}.encode();
        let mut m=ControlMachine::new(config(),TestAuth);
        m.handle(&accepted).unwrap();
        assert!(m.handle(&success).is_err());
        assert_eq!(m.phase(),Phase::Failed);
        assert!(m.handle(&wifi).is_err());
        let mut m=ControlMachine::new(config(),TestAuth);
        assert!(m.handle(&challenge).is_err());
        let mut m=ControlMachine::new(config(),TestAuth);
        m.handle(&cert).unwrap();
        assert!(m.handle(&success).is_err());
        let mut m=ControlMachine::new(config(),UnavailableAuth);
        assert!(m.handle(&cert).is_err());
        assert_eq!(m.phase(),Phase::Failed);
        assert!(m.handle(&success).is_err());
        let mut m=ControlMachine::new(config(),TestAuth);
        m.handle(&accepted).unwrap();
        m.handle(&cert).unwrap();m.handle(&challenge).unwrap();
        // A renewed certificate request invalidates the old signature.
        m.handle(&cert).unwrap();
        assert!(m.handle(&success).is_err());
        let mut m=ControlMachine::new(config(),TestAuth);
        m.handle(&accepted).unwrap();authenticate(&mut m);
        assert_eq!(m.phase(),Phase::Authenticated);
        assert!(m.handle(&wifi).unwrap().is_some());
    }
    #[test]
    fn metadata_waits_for_authentication_and_has_bounded_backlog()''')
# C transport may carry split data. The existing link send_control path
# returns errors on non-normal link; do not fabricate successful send.
print("MU1005_DONOR_ORIGINAL_MFI_CONFIG_AND_ORDER_ADAPTED_NO_INSTALL")
