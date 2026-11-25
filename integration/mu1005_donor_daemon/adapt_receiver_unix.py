#!/usr/bin/env python3
"""Apply after adapt_live_ap.py. No automatic vehicle load."""
from pathlib import Path
import subprocess,shutil,sys
root=Path(__file__).resolve().parent
up=Path(sys.argv[1]).resolve()
if subprocess.check_output(["git","-C",str(up),"rev-parse","HEAD"],text=True).strip()!="20c94f51c0f0ae88b154c12e4f604643ba23022c":raise SystemExit("wrong donor")
d=up/"wireless/daemon"
def rep(p,a,b):
 p=d/p;s=p.read_text()
 if s.count(a)!=1:raise SystemExit(f"unexpected {p}: {a}")
 p.write_text(s.replace(a,b))
for name in ["mu1005_local.rs","mu1005_local_connect.c"]:shutil.copyfile(root/name,d/"src"/name)
rep("src/lib.rs","mod mu1005_ap;","mod mu1005_ap;\n#[cfg(any(target_os=\"nto\",all(test,target_os=\"linux\")))]\nmod mu1005_local;")
rep("build.rs",'"src/mu1005_ap_live.c"]','"src/mu1005_ap_live.c", "src/mu1005_local_connect.c"]')
rep("build.rs","fn main() {",'''fn main() {
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref()==Ok("linux") {
        let out=std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
        let obj=out.join("local-connect.o");let lib=out.join("libmu1005_local.a");
        assert!(std::process::Command::new("cc").args(["-std=c99","-Wall","-Wextra","-Werror","-c","src/mu1005_local_connect.c","-o"]).arg(&obj).status().unwrap().success());
        assert!(std::process::Command::new("ar").arg("crs").arg(&lib).arg(&obj).status().unwrap().success());
        println!("cargo:rustc-link-search=native={}",out.display());
        println!("cargo:rustc-link-lib=static=mu1005_local");
        println!("cargo:rerun-if-changed=src/mu1005_local_connect.c");
    }''')
rep("src/receiver_control.rs","        let mut stream = TcpStream::connect_timeout(&address.into(), timeout)?;\n        stream.set_read_timeout(Some(timeout))?;\n        stream.set_write_timeout(Some(timeout))?;",'''        #[cfg(target_os="nto")]
        let mut stream = {
            let _=address;
            if si.is_none(){return Err(io::Error::other("MU1005 requires original SI admission"));}
            crate::mu1005_local::configured("CPRX",timeout)?
        };
        #[cfg(not(target_os="nto"))]
        let mut stream = {
            let stream=TcpStream::connect_timeout(&address.into(),timeout)?;
            stream.set_read_timeout(Some(timeout))?;
            stream.set_write_timeout(Some(timeout))?;
            stream
        };''')
# Same SIW2 wire encoding; QNX never connects an unverified endpoint.
rep("src/si_control.rs","                let mut stream = UnixStream::connect(&self.path)?;",'''                #[cfg(target_os="nto")]
                let mut stream = {
                    let configured=std::env::var("MHI2Q_CP_SI_SOCKET").map_err(|_|invalid("missing original SI socket"))?;
                    if Path::new(&configured)!=self.path {return Err(invalid("SI socket mismatch"));}
                    crate::mu1005_local::configured("SI",Duration::from_secs(2))?
                };
                #[cfg(not(target_os="nto"))]
                let mut stream = UnixStream::connect(&self.path)?;''')
rep("src/si_control.rs","            Err(std::env::VarError::NotPresent) => Ok(Self::Standalone(\n                LoopbackReceiverControl::new(airplay_port)?,\n            )),",'''            Err(std::env::VarError::NotPresent) => {
                #[cfg(target_os="nto")]
                {Err(invalid("MU1005 requires original SI control endpoint"))}
                #[cfg(not(target_os="nto"))]
                {Ok(Self::Standalone(LoopbackReceiverControl::new(airplay_port)?))}
            },''')
print("MU1005_DONOR_PRIVATE_UNIX_ADAPTED_NO_INSTALL")
