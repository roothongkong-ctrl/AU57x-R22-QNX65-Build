//! Owner-selected private local receiver endpoints. No automatic TCP
//! fallback for an adapted QNX SI session.
use std::{io,ffi::CString,os::unix::{ffi::OsStrExt,net::UnixStream,io::FromRawFd},path::Path,time::Duration};
unsafe extern "C" {fn mu1005_local_connect(path:*const std::ffi::c_char,uid:u32,gid:u32,ms:u32)->i32;}
fn invalid(s:&str)->io::Error {io::Error::new(io::ErrorKind::InvalidInput,s)}
pub(crate) fn connect(path:&Path,uid:u32,gid:u32,timeout:Duration)->io::Result<UnixStream>{
    if !path.is_absolute(){return Err(invalid("private socket must be absolute"));}
    let path=CString::new(path.as_os_str().as_bytes()).map_err(|_|invalid("NUL in private socket"))?;
    let ms=u32::try_from(timeout.as_millis()).map_err(|_|invalid("invalid connect timeout"))?;
    let fd=unsafe{mu1005_local_connect(path.as_ptr(),uid,gid,ms)};
    if fd<0{return Err(io::Error::from_raw_os_error(-fd));}
    let s=unsafe{UnixStream::from_raw_fd(fd)};
    s.set_read_timeout(Some(timeout))?;
    s.set_write_timeout(Some(timeout))?;
    Ok(s)
}
#[cfg(target_os="nto")]
pub(crate) fn configured(kind:&str,timeout:Duration)->io::Result<UnixStream>{
    let read=|suffix:&str|std::env::var(format!("MHI2Q_CP_{kind}_{suffix}")).map_err(|_|invalid("missing original local endpoint configuration"));
    let path=read("SOCKET")?;
    let uid=read("UID")?.parse::<u32>().map_err(|_|invalid("invalid owner UID"))?;
    let gid=read("GID")?.parse::<u32>().map_err(|_|invalid("invalid owner GID"))?;
    connect(Path::new(&path),uid,gid,timeout)
}
#[cfg(test)]
mod tests{
 use super::*;
 use std::{fs,os::unix::{fs::{MetadataExt,PermissionsExt},net::UnixListener},io::{Read,Write}};
 #[test]fn private_socket_real_exchange_and_denial(){
  let dir=std::env::temp_dir().join(format!("mu1005-private-{}",std::process::id()));
  fs::create_dir(&dir).unwrap();fs::set_permissions(&dir,fs::Permissions::from_mode(0o700)).unwrap();
  let path=dir.join("cprx.sock");
  let listener=UnixListener::bind(&path).unwrap();
  fs::set_permissions(&path,fs::Permissions::from_mode(0o600)).unwrap();
  let meta=fs::metadata(&dir).unwrap();let uid=meta.uid();let gid=meta.gid();
  assert!(connect(&path,uid.wrapping_add(1),gid,Duration::from_secs(1)).is_err());
  fs::set_permissions(&dir,fs::Permissions::from_mode(0o755)).unwrap();
  assert!(connect(&path,uid,gid,Duration::from_secs(1)).is_err());
  fs::set_permissions(&dir,fs::Permissions::from_mode(0o700)).unwrap();
  let worker=std::thread::spawn(move||{
   let(mut s,_)=listener.accept().unwrap();let mut req=[0;32];s.read_exact(&mut req).unwrap();
   assert_eq!(&req[..6],b"CPRX\x03\x01");
   s.write_all(b"CPRA\x02\0\0\0").unwrap();
  });
  let mut s=connect(&path,uid,gid,Duration::from_secs(1)).unwrap();
  let mut req=[0;32];req[..6].copy_from_slice(b"CPRX\x03\x01");s.write_all(&req).unwrap();
  let mut reply=[0;8];s.read_exact(&mut reply).unwrap();assert_eq!(&reply,b"CPRA\x02\0\0\0");
  worker.join().unwrap();drop(s);fs::remove_file(path).unwrap();fs::remove_dir(dir).unwrap();
 }
}
