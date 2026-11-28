#!/usr/bin/env python3
"""AU57x music skin R17: isolated bootclasspath integration review. No vehicle writes."""
import pathlib, subprocess, tempfile, sys
helper=pathlib.Path(sys.argv[1]).resolve()
anchor='BOOTCLASSPATH="$BOOTCLASSPATH:$LSD_DIR/lsd.jxe"'
name='MusicSkinR17.jar'
line='BOOTCLASSPATH="$BOOTCLASSPATH:/eso/hmi/lsd/jars/'+name+'"'
with tempfile.TemporaryDirectory() as d:
    p=pathlib.Path(d); base=p/'eso/hmi/lsd'; jars=base/'jars'; jars.mkdir(parents=True)
    lsd=base/'lsd.sh'
    prefix='#!/bin/sh\n'+'# independent fixture\n'*64
    old=prefix+'BOOTCLASSPATH="$BOOTCLASSPATH:/eso/hmi/lsd/jars/ClusterMapGfxKeep.jar"\n'+anchor+'\n'+'on -p 12 /ifs/jre/bin/j9 $VMOPTIONS -Xbootclasspath:$BOOTCLASSPATH -jxe:$LSD_DIR/lsd.jxe &\n'
    lsd.write_text(old)
    existing=jars/'ClusterMapGfxKeep.jar'; existing.write_bytes(b'cluster fixture')
    payload=p/'fixture.jar'; payload.write_bytes(b'music fixture only')
    env=dict(__import__('os').environ,LSD_JAR_TEST_APP=d)
    def run(action,ok=True):
        cmd=['sh',str(helper),action,name]
        if action=='add':cmd.append(str(payload))
        result=subprocess.run(cmd,env=env,capture_output=True,text=True)
        print(action,result.returncode,result.stdout,result.stderr)
        assert (result.returncode==0)==ok
    run('add')
    expected=old.replace(anchor,line+'\n'+anchor)
    assert lsd.read_text()==expected and existing.read_bytes()==b'cluster fixture'
    run('loaded');run('add');assert lsd.read_text()==expected
    run('remove');assert lsd.read_text()==old and existing.read_bytes()==b'cluster fixture'
    run('loaded',False);run('remove');assert lsd.read_text()==old
    print('PASS: independent AU57x loader add/remove, order, idempotence, original cluster fixture preserved; host simulation only')
