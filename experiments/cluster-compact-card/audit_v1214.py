#!/usr/bin/env python3
"""Read-only exact-package verification for original public MIB Cluster Map v1.2.14."""
import argparse
import hashlib
import json
import pathlib
import zipfile
EXPECTED='7450ac0761d97a8de05e74c961c9e44e619c279fb2faa5e7683098de115485a1'
def audit(filename):
    blob=pathlib.Path(filename).read_bytes()
    digest=hashlib.sha256(blob).hexdigest()
    assert digest==EXPECTED,"Release asset mismatch: "+digest
    with zipfile.ZipFile(filename) as z:
        names=z.namelist()
        assert len(set(names))==len(names),"duplicate ZIP names"
        assert z.read('ClusterMap/scripts/package_version.txt').decode().strip()=='v1.2.14'
        sums=z.read('ClusterMap/SHA256SUMS.txt').decode().splitlines()
        for row in sums:
            h,path=row.split(maxsplit=1)
            assert hashlib.sha256(z.read('ClusterMap/'+path)).hexdigest()==h,path
        assert len(sums)==12
        conf=z.read('ClusterMap/gal_dualscreen.conf').decode()
        assert '#CLUSTERMAP_CARPLAY_VIEW=full' in conf
        assert '#CLUSTERMAP_CARPLAY_SCALE=130' in conf
        launcher=z.read('ClusterMap/carplay/carplay_cluster.sh').decode()
        assert 'SOURCE=tcp://127.0.0.1:19820' in launcher
        assert 'PLAYER=/mnt/app/navigation/stream-player' in launcher
        assert sum(ln.startswith('SOURCE=') for ln in launcher.splitlines())==1
        player=z.read('ClusterMap/stream-player')
        assert player[:4]==b'\x7fELF' and player[18:20]==(40).to_bytes(2,'little')
        return dict(version='v1.2.14',sha256=digest,files=sum(not n.endswith('/') for n in names),manifest_matches=len(sums),player_arch='QNX ARM32',single_input='tcp://127.0.0.1:19820',independent_map_and_card_streams='NOT VERIFIED',original_modified=False,installable=False)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('zip_file');p.add_argument('--json',default='');a=p.parse_args()
    result=audit(a.zip_file)
    if a.json:pathlib.Path(a.json).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
