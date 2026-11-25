#!/usr/bin/env python3
"""Linux host fixture only; never runs the OEM btstack or QNX binary."""
import os, pathlib, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
C_FIXTURE = r'''
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
static void *run(void *p) {
    const char *name = (const char *)p;
    int rc = pthread_setname_np(pthread_self(), name);
    printf("%s=%d\n", name, rc);
    return (void *)(long)rc;
}
int main(void) {
    const char *names[] = {"XbluetoothProc", "mv8787bt_read", "mv8787bt_write", "bluetoothProc", "telephoneProc"};
    pthread_t t[5];
    for (int i=0;i<5;i++) if (pthread_create(&t[i], 0, run, (void *)names[i])) return 10+i;
    for (int i=0;i<5;i++) { void *v=0; if (pthread_join(t[i], &v) || (long)v) return 20+i; }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    td = pathlib.Path(td); src = td / 'fixture.c'; src.write_text(C_FIXTURE)
    so = td / 'libprobe.so'; exe = td / 'fixture'; log = td / 'probe.log'
    cc=os.environ.get('CC','cc')
    subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-fPIC','-shared','-DR142_REVIEW_ONLY=1',str(ROOT/'preload_review/r142_thread_name_probe.c'),'-ldl','-pthread','-o',str(so)],check=True)
    subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(src),'-pthread','-o',str(exe)],check=True)
    env=os.environ.copy(); env['LD_PRELOAD']=str(so); env['AU57X_R142_THREAD_PROBE']='1'; env['AU57X_R142_LOG']=str(log)
    p=subprocess.run([str(exe)],env=env,text=True,capture_output=True,check=True); data=log.read_text()
    assert 'observer_ready' in data and 'primary_bluetooth_proc_candidate' in data and 'XbluetoothProc' in data
    assert 'secondary_bluesdk_read_worker' in data and 'mv8787bt_read' in data
    assert 'secondary_bluesdk_write_worker' in data and 'mv8787bt_write' in data
    assert 'name=bluetoothProc' not in data and 'telephoneProc' not in data
    assert all(line.endswith('=0') for line in p.stdout.splitlines()), p.stdout
    offlog=td/'off.log'; env2=os.environ.copy(); env2['LD_PRELOAD']=str(so); env2.pop('AU57X_R142_THREAD_PROBE',None); env2['AU57X_R142_LOG']=str(offlog)
    subprocess.run([str(exe)],env=env2,text=True,capture_output=True,check=True)
    assert not offlog.exists()
print('PASS: RTLD_NEXT forwarding preserves pthread_setname_np results')
print('PASS: primary bluetoothProc substring and secondary BlueSDK worker tags')
print('PASS: disabled-by-default opt-in produces no log')
print('HOST_FIXTURE_ONLY; OEM_EXECUTED=false; QNX_VERIFIED=false; INSTALLABLE=false')
