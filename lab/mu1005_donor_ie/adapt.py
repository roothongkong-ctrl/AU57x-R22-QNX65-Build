#!/usr/bin/env python3
"""Ninth adapter: original AP IE ownership in the actual full donor.
Apply after R105.225. No periodic rewrite or unowned deletion."""
from pathlib import Path
import subprocess,sys
up=Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()=="20c94f51c0f0ae88b154c12e4f604643ba23022c"
d=up/"wireless/daemon/src"
p=d/"wifi_ie.rs"
s=p.read_text()
s+=r'''
// MU1005: a populated matching slot is borrowed, not owned. Never change a
// nonzero mask or infer a free slot from a truncated/omitted entry.
#[cfg(any(test,target_os="nto"))]
fn mu1005_slot_state(output:&str,index:u8,expected:&[u8])->Result<bool,String>{
    let label=format!("Index [{index}]");
    if output.lines().filter(|line|line.trim()==label).count()!=1 {
        return Err("MU1005 IE slot not explicitly present exactly once".into());
    }
    let slot=parse_custom_ie_slot(output,index)?;
    if slot.mask==0 && slot.bytes.is_empty(){return Ok(true);}
    if slot.mask==IE_MASK_REQUIRED_FRAMES && slot.bytes==expected{return Ok(false);}
    Err("MU1005 IE slot owned by another configuration".into())
}

#[cfg(target_os="nto")]
pub struct Mu1005OwnedIe {
    binary:String,index:u8,expected:Vec<u8>,lease:ControlLease,
}
#[cfg(target_os="nto")]
impl Mu1005OwnedIe {
    // An absent token means a pre-existing matching IE, which is never removed.
    pub fn install(bt:[u8;6],id:[u8;6])->Result<Option<Self>,String>{
        let requested=match std::env::var("MHI2Q_CP_IE_INDEX"){
            Ok(s)=>Some(s.parse::<u8>().map_err(|_|"invalid IE index")?),
            Err(std::env::VarError::NotPresent)=>None,
            Err(_)=>return Err("invalid IE index environment".into()),
        };
        let expected=device_ie(bt,id);
        let binary=std::env::var("MHI2Q_CP_UAPUTL").unwrap_or_else(|_|"/mnt/app/armle/sbin/uaputl".into());
        let lease=ControlLease::begin(std::path::Path::new("/tmp/mu1005_wifi_ie_owner.pending"))?;
        let query=run_uaputl_command(&binary,&["-i","uap0","sys_cfg_custom_ie"])
            .and_then(|q|mu1005_choose_slot(&q,&expected,requested));
        let (index,owned)=match query{
            Err(e)=>{lease.complete()?;return Err(e);}
            Ok(v)=>v,
        };
        if !owned{lease.complete()?;return Ok(None);}
        let args=match uaputl_args(index,&expected){
            Ok(v)=>v,Err(e)=>{lease.complete()?;return Err(e);}
        };
        // Recheck immediately before the driver write. The local lease
        // serializes our own writers, not arbitrary OEM processes.
        let current=run_uaputl_command(&binary,&["-i","uap0","sys_cfg_custom_ie"])
            .and_then(|q|mu1005_choose_slot(&q,&expected,Some(index)));
        match current {
            Ok((i,true)) if i==index=>{}
            Ok((i,false)) if i==index=>{lease.complete()?;return Ok(None);}
            _=>{lease.complete()?;return Err("MU1005 IE selection changed before write".into());}
        }
        // From this point a failed write/readback leaves the owner marker:
        // no automatic deletion or subsequent attempt on uncertain hardware.
        let refs=args.iter().map(String::as_str).collect::<Vec<_>>();
        let set=run_uaputl_command(&binary,&refs)?;
        if !set.contains("custom IE setting successful"){return Err("MU1005 IE write not confirmed".into());}
        let q=run_uaputl_command(&binary,&["-i","uap0","sys_cfg_custom_ie"])?;
        if mu1005_slot_state(&q,index,&expected)?{return Err("MU1005 IE write not present".into());}
        Ok(Some(Self{binary,index,expected,lease}))
    }
    pub fn restore(self)->Result<(),String>{
        let q=run_uaputl_command(&self.binary,&["-i","uap0","sys_cfg_custom_ie"])?;
        if !mu1005_slot_state(&q,self.index,&self.expected)?{
            let index=self.index.to_string();
            let set=run_uaputl_command(&self.binary,&["-i","uap0","sys_cfg_custom_ie",&index,"0"])?;
            if !set.contains("custom IE setting successful"){return Err("MU1005 IE removal not confirmed".into());}
            let q=run_uaputl_command(&self.binary,&["-i","uap0","sys_cfg_custom_ie"])?;
            if !mu1005_slot_state(&q,self.index,&self.expected)?{return Err("MU1005 IE still present".into());}
        }
        self.lease.complete()
    }
}
#[cfg(test)]
mod mu1005_ie_tests {
    use super::*;
    fn query(mask:u16,bytes:&[u8])->String{
        let hex=bytes.iter().map(|v|format!("{v:02x}")).collect::<Vec<_>>().join(" ");
        format!("Querying custom IE successful\nIndex [1]\nManagement Subtype Mask = 0x{mask:x}\nIE Buffer: len={}\n{hex}\n",bytes.len())
    }
    #[test]
    fn mu1005_select_live_explicit_slot(){
        let ie=device_ie([0;6],[2;6]);
        let empty=query(0,&[]);
        let occupied=query(1,&[1,2]).replace("Index [1]","Index [0]");
        let matched=query(IE_MASK_REQUIRED_FRAMES,&ie).replace("Index [1]","Index [2]");
        let mixed=format!("{occupied}{empty}");
        assert_eq!(mu1005_choose_slot(&mixed,&ie,None),Ok((1,true)));
        assert_eq!(mu1005_choose_slot(&format!("{mixed}{matched}"),&ie,None),Ok((2,false)));
        assert_eq!(mu1005_choose_slot(&empty,&ie,Some(1)),Ok((1,true)));
        assert!(mu1005_choose_slot(&empty,&ie,Some(3)).is_err());
        assert!(mu1005_choose_slot(&empty,&ie,Some(4)).is_err());
        assert!(mu1005_choose_slot(&occupied,&ie,None).is_err());
        assert!(mu1005_choose_slot("Querying custom IE successful",&ie,None).is_err());
        assert!(mu1005_choose_slot(&format!("{empty}{empty}"),&ie,None).is_err());
        assert!(mu1005_choose_slot(&format!("{matched}{}",matched.replace("Index [2]","Index [3]")),&ie,None).is_err());
        assert!(mu1005_choose_slot(&format!("{empty}{matched}"),&ie,Some(1)).is_err());
    }
    #[test]
    fn mu1005_explicit_empty_owned_vs_borrowed(){
        let ie=device_ie([0;6],[2;6]);
        assert_eq!(mu1005_slot_state(&query(0,&[]),1,&ie),Ok(true));
        assert_eq!(mu1005_slot_state(&query(IE_MASK_REQUIRED_FRAMES,&ie),1,&ie),Ok(false));
        for q in [query(1,&[]),query(0,&ie),query(IE_MASK_REQUIRED_FRAMES,&[1,2]),
            "Querying custom IE successful\n".into(),format!("{}{}",query(0,&[]),query(0,&[]))]{
            assert!(mu1005_slot_state(&q,1,&ie).is_err());
        }
    }
}
'''
p.write_text(s)
p=d/"main.rs";s=p.read_text()
def rep(a,b):
    global s
    if s.count(a)!=1:raise ValueError("anchor "+a[:120])
    s=s.replace(a,b)
rep('''#[cfg(target_os = "nto")]
use std::sync::{
    Arc,
    atomic::{AtomicBool, Ordering},
};
''','')
a=s.index('    #[cfg(target_os = "nto")]\n    if argument.as_deref() == Some("--clear-ie") {')
z=s.index('    if argument.as_deref() == Some("--check-config") {',a)
s=s[:a]+'''    #[cfg(target_os = "nto")]
    if argument.as_deref() == Some("--clear-ie") {
        return Err(std::io::Error::other("MU1005 refuses unowned IE removal; recover original AP before retry"));
    }
'''+s[z:]
a=s.index('        #[cfg(target_os = "nto")]\n        let (ie_active, ie_monitor) = {')
z=s.index('        let ready_result=',a)
s=s[:a]+'''        #[cfg(target_os = "nto")]
        let owned_ie=wifi_ie::Mu1005OwnedIe::install(
            config.bt_mac,
            wifi_ie::parse_mac(&config.device_id).map_err(std::io::Error::other)?,
        ).map_err(std::io::Error::other)?;
        // First link: no periodic driver rewrite. The original AP remains
        // untouched except for the explicitly acquired advertisement slot.
'''+s[z:]
a=s.index('        #[cfg(target_os = "nto")]\n        let monitor_result=')
z=s.index('        let reset_result=receiver.reset();',a)
s=s[:a]+s[z:]
a=s.index('        #[cfg(target_os = "nto")]\n        let ie_result=monitor_result')
z=s.index('        #[cfg(not(target_os = "nto"))]',a)
s=s[:a]+'''        #[cfg(target_os = "nto")]
        let ie_result=match owned_ie {
            Some(ie)=>ie.restore().map_err(std::io::Error::other),
            None=>Ok(()),
        };
'''+s[z:]
p.write_text(s)
print("MU1005_DONOR_OWNED_IE_SOURCE_ADAPTED")
