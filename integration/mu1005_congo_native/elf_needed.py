"""Read real ARM ELF DT_NEEDED via program headers, even without sections."""
import struct
from pathlib import Path

def needed(path):
    data=Path(path).read_bytes()
    def get(fmt,off):
        if off<0 or off+struct.calcsize(fmt)>len(data):raise ValueError("ELF bounds")
        return struct.unpack_from(fmt,data,off)
    if data[:6]!=b"\x7fELF\x01\x01" or get("<H",18)[0]!=40:raise ValueError("expected ARM ELF32 LE")
    off=get("<I",28)[0];size,count=get("<HH",42)
    if size<32 or count>4096:raise ValueError("invalid program headers")
    loads=[];dynamic=[]
    for i in range(count):
        typ,fo,va,_,fs,ms,_,_=get("<8I",off+i*size)
        if fo+fs>len(data) or fs>ms and typ==1:raise ValueError("invalid segment")
        if typ==1:loads.append((fo,va,fs))
        elif typ==2:dynamic.append((fo,fs))
    if not dynamic:return []
    if len(dynamic)!=1:raise ValueError("ambiguous PT_DYNAMIC")
    fo,fs=dynamic[0];tags={};deps=[];end=False
    for pos in range(fo,fo+fs-7,8):
        tag,v=get("<II",pos)
        if tag==0:end=True;break
        if tag==1:deps.append(v)
        elif tag in (5,10):
            if tag in tags and tags[tag]!=v:raise ValueError("conflicting dynamic tag")
            tags[tag]=v
    if not end:raise ValueError("unterminated dynamic table")
    if not deps:return []
    if 5 not in tags or 10 not in tags:raise ValueError("missing dynamic strings")
    va,n=tags[5],tags[10]
    starts={f+va-a for f,a,s in loads if a<=va and va+n<=a+s}
    if len(starts)!=1 or not n:raise ValueError("unmapped dynamic strings")
    start=starts.pop();result=[]
    for x in deps:
        if x>=n:raise ValueError("bad DT_NEEDED offset")
        end=data.find(b"\0",start+x,start+n)
        if end<0:raise ValueError("unterminated dependency")
        name=data[start+x:end].decode("ascii")
        if not name or "/" in name or "\\" in name or name in (".",".."):raise ValueError("unsafe dependency name")
        result.append(name)
    return result

if __name__=="__main__":
    import tempfile
    b=bytearray(512)
    b[:6]=b"\x7fELF\x01\x01"
    struct.pack_into("<H",b,18,40)
    struct.pack_into("<I",b,28,52)
    struct.pack_into("<HH",b,42,32,2)
    struct.pack_into("<8I",b,52,1,0,0x1000,0,len(b),len(b),5,4)
    struct.pack_into("<8I",b,84,2,128,0x1080,0,32,32,6,4)
    for i,(t,v) in enumerate([(5,0x1100),(10,32),(1,1),(0,0)]):struct.pack_into("<II",b,128+8*i,t,v)
    b[256:269]=b"\0libtest.so\0\0"
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)/"elf";p.write_bytes(b);assert needed(p)==["libtest.so"]
        struct.pack_into("<I",b,148,99);p.write_bytes(b)
        try:needed(p)
        except ValueError:pass
        else:raise AssertionError("bad offset accepted")
    print("SECTIONLESS_ARM_ELF_DEPENDENCY_PASS")
