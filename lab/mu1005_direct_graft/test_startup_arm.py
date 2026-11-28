#!/usr/bin/env python3
"""Execute actual generated ARM startup and callbacks; OEM calls are fixtures.
No OEM SDK, Bluetooth controller, or handset is emulated as authenticated.
No firmware or patched executable is exported.
"""
import hashlib,json,struct,sys
from pathlib import Path
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR
from build import CODE,STATE,HOOK,headers,word,STOCK_WORD
RESUME=HOOK+4
STACK=0x700000
OBJECT=0x740000
END=0x750000
INPUT=0x760000
def execute(b,report,fail):
    u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
    # Map only the original entry instruction, new code, and scratch space.
    # Vendor code is replaced by explicit call witnesses, never executed.
    u.mem_map(0x100000,0x400000)
    u.mem_map(CODE,0x10000)
    u.mem_map(STATE,0x20000)
    for a,n in [(STACK,0x10000),(OBJECT,0x1000),(END,0x1000),(INPUT,0x1000)]:
        u.mem_map(a,n)
    for k,o,v,_,f,m,flags,al in headers(b):
        if k==1 and v==CODE:u.mem_write(v,b[o:o+f])
    u.mem_write(HOOK,struct.pack("<I",word(b,HOOK)))
    native={name:int(v,16) for name,v in report["native"].items()}
    targets={v:k for k,v in native.items()}
    seen=[];records=[];channel=None;slot=None;callback=None;worker=None
    def rd(a,n):return bytes(u.mem_read(a,n))
    def le(a):return struct.unpack("<I",rd(a,4))[0]
    def reg(n):return u.reg_read(n)
    def trace(m,a,size,data):
        nonlocal channel,slot,callback,worker
        if a in (RESUME,END):m.emu_stop();return
        name=targets.get(a)
        if name is None:return
        x,y,z=reg(UC_ARM_REG_R0),reg(UC_ARM_REG_R1),reg(UC_ARM_REG_R2)
        seen.append(name)
        result=0
        if name=="RF_REGISTER":
            assert channel is None and STATE<=x<STATE+0x20000 and z==4
            channel,slot=x,y
            callback=le(x+8)
            assert CODE<=callback<STATE and rd(x+12,2)==struct.pack("<H",5035)
            assert le(x+20)==STATE
            assert rd(OBJECT+0x1b0,1)==b"\x01"
            if fail=="rf":result=18
            else:m.mem_write(slot,b"\x03")
        elif name=="SDP_ADD":
            n=rd(x+8,1)[0];attrs=le(x+12)
            assert n in (1,2)
            values=[]
            for i in range(n):
                aid,length,ptr,cache,reserved=struct.unpack("<HHIHH",rd(attrs+i*12,12))
                values.append((aid,rd(ptr,length)))
            if not records:
                assert n==2 and values[0][0]==1 and values[0][1].hex()=="35111c00000000decafadedecadeafdecacaff"
                assert values[1]==(4,bytes.fromhex("350c350319010035051900030803"))
            else:
                assert n==1 and values[0]==(1,bytes.fromhex("35111cec884348cd4140a29727575d50bf1fd3"))
            records.append(x)
            if fail==("iap" if n==2 else "cp"):result=18
        elif name=="PTHREAD_CREATE":
            assert len(records)==2 and y==0 and CODE<=z<STATE
            worker=z;result=1 if fail=="worker" else 0
        elif name=="SDP_REMOVE":
            assert x in records
        elif name=="RF_DEREGISTER":
            assert x==channel and y==slot
        elif name=="RF_ACCEPT":
            assert x==channel and y==0
            result=2
        elif name=="EIR_REQUEST":assert worker is not None
        else:raise AssertionError("unexpected vendor call "+name)
        m.reg_write(UC_ARM_REG_R0,result)
        m.reg_write(UC_ARM_REG_PC,reg(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE,trace)
    original={UC_ARM_REG_R0:0x1234,UC_ARM_REG_R1:0x2345,UC_ARM_REG_R2:0x3456,UC_ARM_REG_R3:1,UC_ARM_REG_R4:0x4567,UC_ARM_REG_R5:OBJECT,UC_ARM_REG_R12:0x5678,UC_ARM_REG_SP:STACK+0xf000,UC_ARM_REG_LR:END}
    u.reg_write(UC_ARM_REG_CPSR,0x60000010)
    for r,v in original.items():u.reg_write(r,v)
    u.emu_start(HOOK,0,count=50000)
    assert reg(UC_ARM_REG_PC)==RESUME and rd(OBJECT+0x1b0,1)==b"\x01"
    assert all(reg(r)==v for r,v in original.items())
    assert reg(UC_ARM_REG_CPSR)&0xf0000000==0x60000000
    expected={
        "ok":["RF_REGISTER","SDP_ADD","SDP_ADD","PTHREAD_CREATE","EIR_REQUEST"],
        "rf":["RF_REGISTER"],
        "iap":["RF_REGISTER","SDP_ADD","RF_DEREGISTER"],
        "cp":["RF_REGISTER","SDP_ADD","SDP_ADD","SDP_REMOVE","RF_DEREGISTER"],
        "worker":["RF_REGISTER","SDP_ADD","SDP_ADD","PTHREAD_CREATE","SDP_REMOVE","SDP_REMOVE","RF_DEREGISTER"],
    }
    assert seen==expected[fail],(fail,seen)
    assert le(STATE)==1
    status=le(STATE+4)
    assert status=={"ok":0,"rf":18,"iap":18,"cp":18,"worker":0xffff0001}[fail]
    # No duplicate registration when original start hook is encountered again.
    u.emu_start(HOOK,0,count=50000)
    assert reg(UC_ARM_REG_PC)==RESUME and seen==expected[fail]
    if fail=="ok":
        # Current review build deliberately declines all handset connections.
        # Verify the real ARM callback cannot report a false connection.
        for event in (1,2):
            u.mem_write(INPUT,struct.pack("<BBHHBBHHI",event,0,0,0,0,0,0,0,0))
            u.reg_write(UC_ARM_REG_R0,channel);u.reg_write(UC_ARM_REG_R1,INPUT)
            u.reg_write(UC_ARM_REG_SP,STACK+0xf000);u.reg_write(UC_ARM_REG_LR,END)
            u.emu_start(callback,0,count=10000)
            assert reg(UC_ARM_REG_PC)==END and le(STATE+16)==0 and le(STATE+28)==0
        assert seen==expected[fail]+["RF_ACCEPT"]
    return {"scenario":fail,"calls":seen,"status":hex(status),"original_startup_replayed":True,"registers_restored":True}
def main():
    binary,report,out=map(Path,sys.argv[1:])
    b=binary.read_bytes();r=json.loads(report.read_text())
    assert hashlib.sha256(b).hexdigest()==r["generated_sha256"]
    assert r["remote_accept"] is False and r["installable"] is False
    rows=[execute(b,r,s) for s in ("ok","rf","iap","cp","worker")]
    out.write_text(json.dumps({"scope":"ACTUAL_PATCHED_ARM_INSTRUCTIONS_WITH_VENDOR_CALL_FIXTURES","scenarios":rows,"real_sdk_registration":False,"phone_connected":False,"installable":False},indent=2)+"\n")
    print("MU1005_DIRECT_STARTUP_ARM_FIVE_SCENARIOS_PASS_FIXTURE_ONLY")
if __name__=="__main__":main()
