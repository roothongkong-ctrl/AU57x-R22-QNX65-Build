#!/usr/bin/env python3
import hashlib,os,pathlib,subprocess,tempfile,unittest,fcntl
from pathlib import Path
NAMES=['btstack.fixture','dio.fixture','si.fixture','iap2.fixture']
BIN=Path(os.environ['TXN_TEST_BIN']).resolve(); DIGEST=Path(os.environ['TXN_DIGEST_BIN']).resolve()
class TxnTest(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='mu1005-c-txn-',dir='/tmp'); self.addCleanup(self.tmp.cleanup); self.root=Path(self.tmp.name); (self.root/'LAB_ONLY').touch()
  for sub in ['target','backup','payload','state']: (self.root/sub).mkdir()
  for i,n in enumerate(NAMES):
   for d,p in [('target','original'),('payload','candidate')]:
    f=self.root/d/n;f.write_text(p+' '+n);f.chmod(0o755 if i<3 else 0o640)
 def invoke(self,action,expect=0,crash=None,corrupt=False):
  e=dict(os.environ);e.pop('TXN_CHILD_EXIT',None);e.pop('TXN_CORRUPT_NEXT',None)
  if corrupt:e['TXN_CORRUPT_NEXT']='1'
  if crash:e['TXN_CHILD_EXIT']=crash
  r=subprocess.run([str(BIN),str(self.root),action],env=e,capture_output=True,text=True,timeout=5);self.assertEqual(r.returncode,expect,r.stderr)
 def check_files(self,prefix):
  for i,n in enumerate(NAMES):
   p=self.root/'target'/n;self.assertEqual(p.read_text(),prefix+' '+n);self.assertEqual(p.stat().st_mode&0o777,0o755 if i<3 else 0o640);self.assertEqual(p.stat().st_uid,os.getuid());self.assertEqual(p.stat().st_gid,os.getgid())
 def test_commit_restore(self):
  self.invoke('prepare');self.invoke('apply');self.check_files('candidate');self.invoke('recover');self.invoke('restore');self.invoke('recover');self.invoke('restore');self.check_files('original')
 def test_prepare_restart(self):
  self.invoke('prepare',77,'backup:1');self.check_files('original');self.invoke('prepare');self.invoke('apply');self.check_files('candidate')
 def test_crash_apply_0(self): self.crash_apply(0)
 def test_crash_apply_1(self): self.crash_apply(1)
 def test_crash_apply_2(self): self.crash_apply(2)
 def test_crash_apply_3(self): self.crash_apply(3)
 def crash_apply(self,i):
  self.invoke('prepare');self.invoke('apply',77,f'apply:{i}');self.invoke('recover');self.check_files('original')
 def test_crash_restore(self):
  self.invoke('prepare');self.invoke('apply');self.invoke('restore',77,'restore:2');self.invoke('recover');self.check_files('original')
 def test_automatic_rollback(self):
  self.invoke('prepare');self.invoke('apply',1,corrupt=True);self.check_files('original');self.assertTrue((self.root/'state'/'journal').read_text().endswith('RESTORED\n'));self.invoke('recover')
 def test_backup_damage(self):
  self.invoke('prepare');self.invoke('apply');(self.root/'backup'/NAMES[-1]).write_text('bad');self.invoke('restore',1);self.check_files('candidate')
 def test_changed_payload(self):
  self.invoke('prepare');(self.root/'payload'/NAMES[-1]).write_text('bad');self.invoke('apply',1);self.check_files('original')
 def test_unknown_target(self):
  self.invoke('prepare');self.invoke('apply');(self.root/'target'/NAMES[-1]).write_text('unknown');self.invoke('restore',1);self.assertEqual((self.root/'target'/NAMES[0]).read_text(),'candidate '+NAMES[0])
 def test_symlink(self):
  p=self.root/'target'/NAMES[0];p.unlink();p.symlink_to(self.root/'payload'/NAMES[0]);self.invoke('prepare',1)
 def test_hardlink(self):
  p=self.root/'target'/NAMES[0];os.link(p,self.root/'alias');self.invoke('prepare',1)
 def test_bad_journal(self):
  self.invoke('prepare');(self.root/'state'/'journal').write_text('forged');self.invoke('apply',1);self.check_files('original')
 def test_lock(self):
  with (self.root/'state'/'lock').open('w') as f:
   fcntl.lockf(f,fcntl.LOCK_EX|fcntl.LOCK_NB);self.invoke('prepare',1)
 def test_mode(self):
  (self.root/'payload'/NAMES[0]).chmod(0o600);self.invoke('prepare',1);self.check_files('original')
 def test_prepared_recover(self):
  self.invoke('prepare');self.invoke('recover');self.check_files('original')
 def phase(self):
  return (self.root/'state'/'journal').read_text().split()[-1]
 def test_trial_confirm(self):
  self.invoke('trial');self.check_files('candidate');self.assertEqual(self.phase(),'PENDING_HEALTH');self.invoke('confirm');self.assertEqual(self.phase(),'COMMITTED');self.invoke('recover');self.check_files('candidate');self.invoke('restore');self.check_files('original')
 def test_trial_unconfirmed_recover(self):
  self.invoke('trial');self.invoke('recover');self.check_files('original');self.assertEqual(self.phase(),'RESTORED');self.invoke('confirm',1)
 def test_trial_crash_prepared(self):
  self.invoke('trial',77,'prepared:0');self.invoke('recover');self.check_files('original')
 def test_trial_crash_pending(self):
  self.invoke('trial',77,'pending:0');self.assertEqual(self.phase(),'PENDING_HEALTH');self.invoke('recover');self.check_files('original')
 def test_trial_crash_confirm(self):
  self.invoke('trial');self.invoke('confirm',77,'confirm:0');self.assertEqual(self.phase(),'PENDING_HEALTH');self.invoke('recover');self.check_files('original')
 def test_trial_crash_each_apply(self):
  # Fresh independent state for each interruption is exercised by test_crash_apply_*;
  # this test covers the combined public entry point.
  self.invoke('trial',77,'apply:3');self.invoke('recover');self.check_files('original')
 def test_trial_failure_rollback(self):
  self.invoke('trial',1,corrupt=True);self.check_files('original');self.assertEqual(self.phase(),'RESTORED')
 def test_trial_no_double_install(self):
  self.invoke('trial');self.invoke('trial',1);self.assertEqual(self.phase(),'PENDING_HEALTH');self.invoke('recover');self.check_files('original')
 def test_trial_confirm_corrupt(self):
  self.invoke('trial');(self.root/'backup'/NAMES[0]).write_text('bad');self.invoke('confirm',1);self.assertEqual(self.phase(),'PENDING_HEALTH')
 def test_trial_status(self):
  self.invoke('trial');r=subprocess.check_output([str(BIN),str(self.root),'status'],text=True);self.assertEqual(r,'MU1005_TXN_PHASE=PENDING_HEALTH\n');self.invoke('restore')
 def test_trial_shell_entry(self):
  script=Path(__file__).with_name('trial_entry_lab.sh')
  def go(a,code=0):
   r=subprocess.run(['sh',str(script),str(BIN),str(self.root),a],capture_output=True,text=True,timeout=5);self.assertEqual(r.returncode,code,r.stderr);return r.stdout
  go('install');self.assertEqual(go('status'),'MU1005_TXN_PHASE=PENDING_HEALTH\n');go('confirm',2);go('recover');self.check_files('original')
 def supervised(self,action='supervised-trial',mode='ok',expect=0):
  e=dict(os.environ,TXN_LIFE_MODE=mode);e.pop('TXN_CHILD_EXIT',None);e.pop('TXN_CORRUPT_NEXT',None)
  r=subprocess.run([str(BIN),str(self.root),action],env=e,capture_output=True,text=True,timeout=5);self.assertEqual(r.returncode,expect,r.stderr+'\n'+r.stdout);return r.stdout.splitlines()
 def test_supervised_success(self):
  self.assertEqual(self.supervised(),['quiesce','start-candidate','verify-candidate']);self.assertEqual(self.phase(),'COMMITTED');self.check_files('candidate')
 def test_supervised_lock_held(self):
  self.assertIn('competitor-rejected',self.supervised(mode='competing-lock'));self.assertEqual(self.phase(),'COMMITTED')
 def test_supervised_start_failure(self):
  self.assertEqual(self.supervised(mode='start-fails',expect=3),['quiesce','start-candidate','quiesce','start-original']);self.check_files('original');self.assertEqual(self.phase(),'RESTORED')
 def test_supervised_bad_health(self):
  self.assertEqual(self.supervised(mode='bad-health',expect=3),['quiesce','start-candidate','verify-candidate','quiesce','start-original']);self.check_files('original')
 def test_supervised_timeout(self):
  self.supervised(mode='timeout',expect=3);self.assertEqual(self.phase(),'RESTORED');self.check_files('original')
 def test_supervised_quiesce_failure(self):
  self.assertEqual(self.supervised(mode='stop-original-fails',expect=1),['quiesce']);self.assertFalse((self.root/'state'/'journal').exists());self.check_files('original')
 def test_supervised_stop_candidate_failure(self):
  self.supervised(mode='stop-candidate-fails',expect=1);self.assertEqual(self.phase(),'PENDING_HEALTH');self.check_files('candidate');self.supervised('supervised-recover',expect=3);self.check_files('original')
 def test_supervised_crash_and_recover(self):
  self.supervised(mode='crash-after-start',expect=79);self.assertEqual(self.phase(),'PENDING_HEALTH');self.assertEqual(self.supervised('supervised-recover',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_supervised_existing_journal(self):
  self.invoke('trial');self.assertEqual(self.supervised(expect=1),[]);self.check_files('candidate')
 def test_supervised_committed_recover(self):
  self.supervised();self.assertEqual(self.supervised('supervised-recover'),[]);self.check_files('candidate')
 def test_supervised_restart_failure(self):
  self.invoke('trial');self.supervised('supervised-recover',mode='restart-fails',expect=1);self.check_files('original');self.assertEqual(self.phase(),'RESTORED');self.supervised('supervised-recover',expect=3)
 def test_supervised_precheck_failure(self):
  (self.root/'payload'/NAMES[0]).write_text('invalid');self.assertEqual(self.supervised(expect=1),[]);self.check_files('original');self.assertFalse((self.root/'state'/'journal').exists())
 def test_supervised_backup_conflict_before_stop(self):
  (self.root/'backup'/NAMES[0]).write_text('unknown backup');self.assertEqual(self.supervised(expect=1),[]);self.check_files('original');self.assertFalse((self.root/'state'/'journal').exists())
 def test_supervised_missing_payload_before_stop(self):
  (self.root/'payload'/NAMES[0]).unlink();self.assertEqual(self.supervised(expect=1),[]);self.check_files('original');self.assertFalse((self.root/'state'/'journal').exists())
 def test_supervised_recover_before_journal(self):
  self.assertEqual(self.supervised('supervised-recover',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_supervised_no_journal_unknown_files(self):
  (self.root/'target'/NAMES[0]).write_text('unknown');self.assertEqual(self.supervised('supervised-recover',expect=1),[]);self.assertEqual((self.root/'target'/NAMES[0]).read_text(),'unknown')
 def unplug_payload(self):
  (self.root/'payload').rename(self.root/'removed-card')
 def test_recover_without_sd(self):
  self.invoke('trial');self.unplug_payload();self.invoke('status');self.invoke('recover');self.check_files('original');self.assertEqual(self.phase(),'RESTORED')
 def test_supervised_recover_without_sd(self):
  self.invoke('trial',77,'apply:2');self.unplug_payload();self.assertEqual(self.supervised('supervised-recover',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_confirm_without_sd(self):
  self.invoke('trial');self.unplug_payload();self.invoke('confirm');self.check_files('candidate');self.assertEqual(self.phase(),'COMMITTED')
 def test_supervised_restore_committed_without_sd(self):
  self.supervised();self.unplug_payload();self.assertEqual(self.supervised('supervised-restore',expect=3),['quiesce','start-original']);self.check_files('original');self.assertEqual(self.phase(),'RESTORED')
 def test_supervised_restore_bad_backup_no_stop(self):
  self.supervised();(self.root/'backup'/NAMES[0]).write_text('bad');self.assertEqual(self.supervised('supervised-restore',expect=1),[]);self.check_files('candidate')
 def test_supervised_recovery_bad_backup_no_stop(self):
  self.invoke('trial');(self.root/'backup'/NAMES[0]).write_text('bad');self.assertEqual(self.supervised('supervised-recover',expect=1),[]);self.check_files('candidate')
 def test_supervised_restore_stop_failure(self):
  self.supervised();self.assertEqual(self.supervised('supervised-restore',mode='stop-original-fails',expect=1),['quiesce']);self.check_files('candidate');self.assertEqual(self.phase(),'COMMITTED')
 def test_supervised_restore_retry_start_original(self):
  self.supervised();self.supervised('supervised-restore',mode='restart-fails',expect=1);self.check_files('original');self.assertEqual(self.phase(),'RESTORED');self.supervised('supervised-recover',expect=3)
 def test_install_without_sd_rejected_before_stop(self):
  self.unplug_payload();self.assertEqual(self.supervised(expect=1),[]);self.check_files('original')
 def test_boot_clean_oem_without_sd(self):
  self.unplug_payload();self.assertEqual(self.supervised('supervised-boot',expect=3),['quiesce','start-original']);self.check_files('original');self.assertFalse((self.root/'state'/'journal').exists())
 def test_boot_committed_recheck_without_sd(self):
  self.supervised();self.unplug_payload();self.assertEqual(self.supervised('supervised-boot'),['quiesce','start-candidate','verify-candidate']);self.assertEqual(self.phase(),'COMMITTED');self.check_files('candidate')
 def test_boot_committed_bad_health(self):
  self.supervised();self.unplug_payload();self.assertEqual(self.supervised('supervised-boot',mode='bad-health',expect=3),['quiesce','start-candidate','verify-candidate','quiesce','start-original']);self.assertEqual(self.phase(),'RESTORED');self.check_files('original')
 def test_boot_committed_start_failure(self):
  self.supervised();self.assertEqual(self.supervised('supervised-boot',mode='start-fails',expect=3),['quiesce','start-candidate','quiesce','start-original']);self.check_files('original')
 def test_boot_committed_timeout(self):
  self.supervised();self.supervised('supervised-boot',mode='timeout',expect=3);self.check_files('original')
 def test_boot_crash_during_start(self):
  self.supervised();self.supervised('supervised-boot',mode='crash-after-start',expect=79);self.assertEqual(self.phase(),'PENDING_HEALTH');self.unplug_payload();self.assertEqual(self.supervised('supervised-boot',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_boot_crash_before_start(self):
  self.supervised();self.invoke('supervised-boot',77,'boot-pending:0');self.assertEqual(self.phase(),'PENDING_HEALTH');self.assertEqual(self.supervised('supervised-boot',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_boot_pending_rollback_without_start_candidate(self):
  self.invoke('trial');self.unplug_payload();self.assertEqual(self.supervised('supervised-boot',expect=3),['quiesce','start-original']);self.check_files('original')
 def test_boot_corrupt_backup_blocks_before_service(self):
  self.supervised();(self.root/'backup'/NAMES[-1]).write_text('bad');self.assertEqual(self.supervised('supervised-boot',expect=1),[]);self.check_files('candidate')
 def test_boot_unknown_file_blocks_before_service(self):
  self.supervised();(self.root/'target'/NAMES[-1]).write_text('unknown');self.assertEqual(self.supervised('supervised-boot',expect=1),[]);self.assertEqual(self.phase(),'COMMITTED')
 def test_boot_stop_failure_preserves_pending(self):
  self.supervised();self.supervised('supervised-boot',mode='stop-candidate-fails',expect=1);self.assertEqual(self.phase(),'PENDING_HEALTH');self.check_files('candidate');self.supervised('supervised-boot',expect=3);self.check_files('original')
 def test_boot_oem_restart_failure_retry(self):
  self.invoke('trial');self.supervised('supervised-boot',mode='restart-fails',expect=1);self.assertEqual(self.phase(),'RESTORED');self.supervised('supervised-boot',expect=3);self.check_files('original')
 def test_boot_lock_held(self):
  self.supervised();self.assertIn('competitor-rejected',self.supervised('supervised-boot',mode='competing-lock'));self.assertEqual(self.phase(),'COMMITTED')
 def test_sha(self):
  for n in (0,1,55,56,63,64,65,127,128,4096,1000000):
   b=bytes((x%251 for x in range(n)));r=subprocess.check_output([str(DIGEST)],input=b).decode().strip();self.assertEqual(r,hashlib.sha256(b).hexdigest())
if __name__=='__main__': unittest.main(verbosity=2)
