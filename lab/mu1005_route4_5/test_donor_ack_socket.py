#!/usr/bin/env python3
"""Real modified upstream daemon process and loopback TCP; synthetic radio.
No auth or receiver success claimed."""
import os,socket,struct,subprocess,sys,tempfile,time
from pathlib import Path
bin=Path(sys.argv[1]).resolve()
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
def connect():
    for _ in range(100):
        try:return socket.create_connection(("127.0.0.1",57342),timeout=1)
        except OSError:time.sleep(.05)
    raise AssertionError("daemon not listening")
def closed(s):
    s.settimeout(3)
    try:assert s.recv(1)==b""
    except ConnectionResetError:pass
def marker(s,g):
    s.sendall(frame(1,g,b"\x02\x01\x02\x03\x04\x05"))
    s.sendall(frame(2,g,b"\xff\x55\x02\x00\xee\x10"))
    k,v,p=recv(s);assert k==2 and v==g and p
def run(ack):
    with tempfile.TemporaryFile() as log:
        p=subprocess.Popen([str(bin)]+(["--mu1005-ack"] if ack else []),env=env,stdout=log,stderr=log)
        try:
            with connect() as s:
                marker(s,1)
                if ack:
                    s.settimeout(.08)
                    try:assert not s.recv(1), "unexpected pre-ACK output"
                    except socket.timeout:pass
                    s.settimeout(1)
                    s.sendall(frame(4,1))
                    k,g,data=recv(s);assert k==2 and g==1 and data
                    s.sendall(frame(4,2));closed(s)
                else:
                    k,g,data=recv(s);assert k==2 and g==1 and data
                    s.sendall(frame(4,1));closed(s)
            if ack:
                with connect() as s:
                    marker(s,3)
                    # Missing completion must end the actual socket session.
                    s.settimeout(4)
                    closed(s)
            assert p.poll() is None
        finally:
            p.terminate()
            try:p.wait(timeout=5)
            except subprocess.TimeoutExpired:p.kill();p.wait()
            log.seek(0);print(log.read().decode(errors="replace"))
run(True);run(False)
print("MU1005_DONOR_ACK_REAL_TCP_OPT_IN_AND_LEGACY_PASS_NO_OEM")
