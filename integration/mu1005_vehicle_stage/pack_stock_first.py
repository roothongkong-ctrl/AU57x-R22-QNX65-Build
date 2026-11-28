#!/usr/bin/env python3
"""Verify a prior compiled payload and assemble separated stock-first / fallback review routes."""
import hashlib,json,stat,sys,zipfile
from pathlib import Path,PurePosixPath
archive,out=map(Path,sys.argv[1:])
repo=Path(__file__).resolve().parents[2]
def sha(data):return hashlib.sha256(data).hexdigest()
with zipfile.ZipFile(archive) as z:
    names=z.namelist()
    assert len(names)==len(set(names)) and z.testzip() is None
    for n in names:
        p=PurePosixPath(n)
        assert not p.is_absolute() and '..' not in p.parts and '\\' not in n
        assert not stat.S_ISLNK(z.getinfo(n).external_attr>>16)
    m=json.loads(z.read('manifest.json'))
    assert m['installable'] is False and m['vehicle_verified'] is False
    assert m['native_run']==46102648356 and m['luka_arm_run']==46105174013
    listed={r['path']:r for r in m['files']}
    assert set(names)==set(listed)|{'manifest.json','DO_NOT_INSTALL.txt'}
    for n,r in listed.items():
        b=z.read(n);assert len(b)==r['bytes'] and sha(b)==r['sha256']
    payload={f'fallback-review/{n}':z.read(n) for n in names}
for name in ('backup.sh','recover.sh','config_transaction.sh','stock_config_apply.sh','congo_port_table.json','stock_first_rehearse.sh'):
    payload['stock-first-review/'+name]=(repo/'integration/mu1005_vehicle_stage'/name).read_bytes()
payload['README_CN.txt']=('原厂优先：复用现有蓝牙配对、有线CarPlay和原厂认证。仅在原厂无线入口不足时选择备用桥。'
    '\n本包仅供复核及云端夹具测试，不可装车。原厂实际启动、无线传输接线尚未验证。'
    '\nstock-first-review：配置事务、备份及恢复；fallback-review：已编译ARM备用模块。两个路线不得同时启动。\n').encode()
manifest={'target':'MU1005','installable':False,'vehicle_verified':False,'stock_first':True,
 'reused_runs':[46105174013,46105440844,46105722271,46105722185,46105722197],
 'remaining':['live OEM wireless trigger and transport handoff','vehicle configuration path and recovery','real iPhone session'],
 'files':[{'path':n,'sha256':sha(b),'bytes':len(b)} for n,b in sorted(payload.items())]}
payload['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(payload.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None and all(sha(z.read(r['path']))==r['sha256'] for r in manifest['files'])
print('STOCK_FIRST_AND_FALLBACK_REVIEW_ROUTES_PACKAGED_NO_INSTALL')
