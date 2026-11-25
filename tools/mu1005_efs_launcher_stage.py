#!/usr/bin/env python3
"""Extract the exact original MU1005 launcher config, stage an offline review copy.
Never writes the EFS image, the car, or a live configuration. No install command.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import tempfile
import shutil

EFS_SHA = '96fbdddae01ec4e785104e9516f0a84706ff6c2b8c912c2c33ad066988bd8244'
CONFIG_SHA = '56c14940455215233f9ead5a8cd3e3a9dc984f372b0d7421ba26ff8e8f579470'
STOCK = '/eso/bin/apps/btstack'
WRAPPER = '/mnt/app/eso/bin/apps/au57x_btstack_probe_entry.sh'
PRELOAD = '/mnt/app/eso/lib/libau57x_bt_preload_qnx_REVIEW_ONLY.so'

def sha(b): return hashlib.sha256(b).hexdigest()

def unique(pairs):
    out = {}
    for k, v in pairs:
        if k in out: raise ValueError('duplicate key: ' + k)
        out[k] = v
    return out

def parse(b):
    return json.loads(re.sub(rb'(?m)^[ \t]*#[^\r\n]*', b'', b), object_pairs_hook=unique)

def extract(b):
    if len(b) != 2097152 or sha(b) != EFS_SHA:
        raise ValueError('EFS image is not the pinned original')
    if b[0x84e48:0x84e48+len(b'connectivity.json\0')] != b'connectivity.json\0':
        raise ValueError('EFS filename mismatch')
    c = b[0x84e80:0x8707b]
    if sha(c) != CONFIG_SHA or b[0x8707b] != 255:
        raise ValueError('configuration extent mismatch')
    return c

def stage_config(c):
    if sha(c) != CONFIG_SHA: raise ValueError('original config not pinned')
    obj = parse(c)
    node = obj['launcher']['btstack']
    if node['exec'] != STOCK or node['preCondition'] != '/tmp/mvloaded' or node['maxUnhealthyReports'] != 3 or obj['bluetooth']['enableIap'] is not False:
        raise ValueError('original launcher semantics mismatch')
    marker = ('"exec":"'+STOCK+'"').encode()
    if c.count(marker) != 1: raise ValueError('ambiguous executable')
    modified = c.replace(marker, ('"exec":"'+WRAPPER+'"').encode(), 1)
    expected = copy.deepcopy(obj)
    expected['launcher']['btstack']['exec'] = WRAPPER
    if parse(modified) != expected: raise ValueError('unexpected additional change')
    # No enableIap toggle: original factory endpoint and graft are distinct.
    return modified

WRAPPER_TEXT = '''#!/bin/sh
# REVIEW ONLY. Same-process exec; do not install on a head unit.
# No flag = exact original executable, no extra preload.
if [ "${AU57X_R92_PROBE_OPT_IN:-0}" = 1 ] && [ -r /mnt/app/eso/lib/libau57x_bt_preload_qnx_REVIEW_ONLY.so ]; then
    AU57X_R92_PROBE=1
    LD_PRELOAD="/mnt/app/eso/lib/libau57x_bt_preload_qnx_REVIEW_ONLY.so${LD_PRELOAD:+:$LD_PRELOAD}"
    export AU57X_R92_PROBE LD_PRELOAD
fi
exec /eso/bin/apps/btstack "$@"
'''

def run(image, out):
    c = extract(image.read_bytes())
    modified = stage_config(c)
    if out.exists(): raise ValueError('output already exists')
    out.parent.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix='mu1005-review-', dir=out.parent))
    try:
        (temp/'connectivity.original.json').write_bytes(c)
        (temp/'connectivity.STAGED_REVIEW_ONLY.json').write_bytes(modified)
        (temp/'au57x_btstack_probe_entry.sh').write_text(WRAPPER_TEXT)
        obj = parse(c)
        report = dict(scope='OFFLINE_REVIEW_ONLY', installable=False, vehicle_verified=False,
                      efs_sha256=EFS_SHA, original_sha256=sha(c), staged_sha256=sha(modified),
                      changed_field='launcher.btstack.exec', original_exec=STOCK, staged_exec=WRAPPER,
                      original_precondition=obj['launcher']['btstack']['preCondition'],
                      original_bluetooth_enableIap=obj['bluetooth']['enableIap'],
                      original_bit_value=next(x['bitValue'] for x in obj['launcher']['applications'] if x['process']=='btstack'),
                      original_initRunMode=obj['launcher']['initRunMode'],
                      preserved_failure_actions=True, no_efs_write=True, probe_opt_in_required=True,
                      runtime_config_path_verified=False, real_hook_bound=False)
        (temp/'STATUS.json').write_text(json.dumps(report,indent=2)+'\n')
        temp.rename(out)
    finally:
        if temp.exists(): shutil.rmtree(temp)
    return report

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--efs', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    a=p.parse_args()
    print(json.dumps(run(a.efs,a.out),indent=2))
