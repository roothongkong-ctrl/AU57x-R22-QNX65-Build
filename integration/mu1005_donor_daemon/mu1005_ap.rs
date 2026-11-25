//! MU1005 live AP source. Only reads the original uap0, never sets radio
//! parameters. No fabricated SSID, key, channel or hardware identity.
#[derive(Clone, PartialEq, Eq)]
pub(crate) struct ApSnapshot {
    pub(crate) ssid: String,
    pub(crate) passphrase: String,
    pub(crate) channel: u8,
}
impl ApSnapshot {
    // Conservative common 2.4/5 GHz primary channels. Actual driver
    // band/channel proof is separately required by the C reader.
    pub(crate) fn checked(ssid: String, passphrase: String, channel: u8) -> Result<Self, String> {
        let channel_ok = (1..=13).contains(&channel)
            || matches!(channel,36|40|44|48|52|56|60|64|100|104|108|112|116|120|124|128|132|136|140|144|149|153|157|161|165);
        if !channel_ok || ssid.is_empty() || ssid.len()>32
            || ssid.chars().any(char::is_control)
            || !(8..=63).contains(&passphrase.len())
            || !passphrase.bytes().all(|b| (32..=126).contains(&b)) {
            return Err("MU1005 unsupported live AP wire parameters".into());
        }
        Ok(Self {ssid,passphrase,channel})
    }
}
#[cfg(target_os="nto")]
pub(crate) fn read_stock() -> Result<ApSnapshot,String> {
    use std::ffi::{c_void,c_int,c_uint,c_char};
    #[repr(C)]
    struct Raw { ssid:[u8;33], key:[u8;65], bssid:[u8;6], channel:u8, security:u8 }
    type Query = unsafe extern "C" fn(*mut c_void,c_uint,*mut c_char,usize)->c_int;
    unsafe extern "C" {
        fn mu_ap_live_read(out:*mut Raw,query:Query,ctx:*mut c_void)->c_int;
        fn mu_ap_stock_query(ctx:*mut c_void,id:c_uint,out:*mut c_char,cap:usize)->c_int;
        fn mu_ap_live_clear(p:*mut c_void,n:usize);
    }
    fn string(buf:&[u8])->Result<String,String>{
        let end=buf.iter().position(|b|*b==0).ok_or("unterminated AP field")?;
        std::str::from_utf8(&buf[..end]).map(str::to_owned).map_err(|_|"non-UTF8 AP field".into())
    }
    let mut raw=Raw{ssid:[0;33],key:[0;65],bssid:[0;6],channel:0,security:0};
    let mut uncertain:c_int=0;
    // C provider performs two equal six-query snapshots and validates
    // the driver-reported BSS, WPA2, channel/band and unicast BSSID.
    let rc=unsafe{mu_ap_live_read(&mut raw,mu_ap_stock_query,(&mut uncertain as *mut c_int).cast())};
    let result=(|| {
        if rc!=0||uncertain!=0||raw.security!=2||raw.bssid==[0;6]||raw.bssid[0]&1!=0 {
            return Err("MU1005 live AP unavailable or changed".into());
        }
        ApSnapshot::checked(string(&raw.ssid)?,string(&raw.key)?,raw.channel)
    })();
    unsafe{mu_ap_live_clear((&mut raw as *mut Raw).cast(),std::mem::size_of::<Raw>())};
    result
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn both_bands_and_reject_unknown(){
        for ch in [1,11,13,36,44,149,165] {
            assert!(ApSnapshot::checked("AP".into(),"testpass".into(),ch).is_ok());
        }
        for ch in [0,14,15,35,37,166,255] {
            assert!(ApSnapshot::checked("AP".into(),"testpass".into(),ch).is_err());
        }
        assert!(ApSnapshot::checked("AP".into(),"x".repeat(64),149).is_err());
        assert!(ApSnapshot::checked("".into(),"testpass".into(),149).is_err());
    }
}
