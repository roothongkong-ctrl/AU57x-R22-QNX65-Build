#!/usr/bin/env python3
"""Select an available iPhone from a simctl JSON snapshot, newest runtime first."""
import json, re, sys
def select(data):
    phones=[]
    for runtime, devices in data.get('devices', {}).items():
        if not re.search(r'\.iOS-\d',runtime): continue
        version=tuple(map(int,re.findall(r'\d+',runtime.split('.iOS-')[-1])))
        for device in devices:
            if device.get('isAvailable') is True and device.get('name','').startswith('iPhone') and device.get('udid'):
                phones.append((version,device.get('state')=='Booted',device['name'],device['udid']))
    if not phones: raise ValueError('No available iPhone simulator. Install an iOS simulator runtime in Xcode.')
    return max(phones)[-1]
if __name__=='__main__':
    try: print(select(json.load(sys.stdin)))
    except (ValueError, TypeError, KeyError) as exc: sys.exit(str(exc))
