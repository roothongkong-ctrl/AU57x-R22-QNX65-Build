#!/usr/bin/env python3
"""Actual donor on supervisor-owned connected TCP FD, host-only.
No original device identity, no RF/MFi claim."""
import os,socket,struct,subprocess,sys,tempfile
from pathlib import Path
binary=Path(sys.argv[1]).resolve()
cfg=Path(sys.argv[2]).resolve()
env=dict(os.environ,IPL_CONFIG_DIR_DIO_MANAGER=str(cfg),MHI2Q_CP_MODEL="LAB-ONLY",MHI2Q_CP_SERIAL="LAB-ONLY",MHI2Q_CP_HARDWARE="LAB-ONLY",MHI2Q_CP_BT_MAC="02:00:00:00:00:01",MHI2Q_CP_DEVICE_ID="02:00:00:00:00:02",MHI2Q_CP_SSID="LAB-ONLY",MHI2Q_CP_PASSPHRASE="labfixturepassword",MHI2Q_CP_CHANNEL="11",MHI2Q_CP_IP_ADDRESS="127.0.0.1",MHI2Q_CP_AIRPLAY_PORT="5000",MHI2Q_CP_RECEIVER="stock210")
def frame(k,g,p=b""):return struct.pack("<BBHI",k,0,len(p),g)+p
def exact(s,n):
 b=b""
 while len(b)<n:
  x=s.recv(n-len(b))
  if not x:raise EOFError()
  b+=x
 return b
def recv(s):
 k,r,n,g=struct.unpack("<BBHI",exact(s,8));assert r==0
 return k,g,exact(s,n)
with socket.socket() as listener:
 listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
 listener.bind(("127.0.0.1",57342));listener.listen(1)
 with socket.create_connection(listener.getsockname(),timeout=3) as client:
  accepted,peer=listener.accept()
  assert peer==client.getsockname()
  with accepted:
   listener.close()
   p=subprocess.Popen([str(binary),"--mu1005-ack-fd"],env=dict(env,MU1005_BRIDGE_FD=str(accepted.fileno())),pass_fds=(accepted.fileno(),))
   accepted.close()
   try:
    client.sendall(frame(1,1,b"\x02\x01\x02\x03\x04\x05")+frame(2,1,b"\xff\x55\x02\x00\xee\x10"))
    k,g,data=recv(client);assert k==2 and g==1 and data
    client.settimeout(.08)
    try:raise AssertionError("unexpected pre-ACK output: "+repr(client.recv(1)))
    except socket.timeout:pass
    client.settimeout(3);client.sendall(frame(4,1))
    k,g,data=recv(client);assert k==2 and g==1 and data
    client.sendall(frame(4,1)+frame(3,1))
    client.shutdown(socket.SHUT_RDWR)
    assert p.wait(timeout=8)==0
   finally:
    if p.poll() is None:p.kill();p.wait()
# Bad fd: no legacy listener fallback.
p=subprocess.run([str(binary),"--mu1005-ack-fd"],env=dict(env,MU1005_BRIDGE_FD="99999"),timeout=5)
assert p.returncode!=0
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as wrong:
 p=subprocess.run([str(binary),"--mu1005-ack-fd"],env=dict(env,MU1005_BRIDGE_FD=str(wrong.fileno())),pass_fds=(wrong.fileno(),),timeout=5)
 assert p.returncode!=0
print("MU1005_INHERITED_REAL_DONOR_ACK_AND_REJECT_PASS_NO_OEM")
