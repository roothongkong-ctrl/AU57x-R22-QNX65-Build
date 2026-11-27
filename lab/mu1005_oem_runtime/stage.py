"""Stage exact OEM library bytes, without publishing them."""
from pathlib import Path
import hashlib,shutil,sys
root=Path(sys.argv[1]).resolve(strict=True)
out=Path(sys.argv[2])
pins={
"libiap2client.so.1":"065cdb1300a30e1068e04c30213b61e233197a95109181a37fbfef843fe369b5",
"libstrm.so.1":"63868ee326c553cf80303936ffe65a05d69cbe76c436314ebbd86af3f0c330b2",
"libdns_sd.so.1":"9636f6a0b650ce5ba0d0c1550e6f64e0d67dc172f03b98a48c2bd72b852bec35"}
out.mkdir(mode=0o700)
for name,digest in pins.items():
    p=root/"app70/filesystems/MMX2/app/70/default/app.img/armle/usr/lib"/name
    if not p.is_file() or not p.resolve().is_relative_to(root) or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
        raise ValueError("wrong original "+name)
    shutil.copyfile(p,out/name)
    print("OEM_PIN_OK",name,digest)
