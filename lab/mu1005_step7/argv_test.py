#!/usr/bin/env python3
"""R105.159 Step 7: execute the existing wrapper with a harmless target, never OEM."""
import hashlib,json,os,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import mu1005_efs_launcher_stage as stage

CASES={
 "factory-btstack":(["1"],"0",False,False,0),
 "factory-bluetooth-arg":(["4"],"0",False,False,0),
 "no-arguments":([],"0",False,False,0),
 "empty-argument":(["","1",""],"0",False,False,0),
 "spaces":(["a b","x	y"],"0",False,False,0),
 "literal-metacharacters":(["$(false)",";","*","\"","'","a\\b"],"0",False,False,0),
 "opt-in-missing-library":(["1"],"1",False,False,0),
 "disabled-library-present":(["1"],"0",True,False,0),
 "enabled-library-present":(["1"],"1",True,False,0),
 "preserve-inherited-preload":(["1"],"1",True,True,0),
 "stock-nonzero-exit":(["1"],"0",False,False,23),
 "unknown-opt-in":(["1"],"yes",True,False,0),
}
def render(target,lib):
 # Test substitution only. Same production wrapper body and quoting.
 s=stage.WRAPPER_TEXT
 assert s.count(stage.STOCK)==1 and s.count(stage.PRELOAD)==2
 return s.replace(stage.STOCK,str(target)).replace(stage.PRELOAD,str(lib))
def config(path):
 raw=path.read_bytes()
 assert stage.sha(raw)==stage.CONFIG_SHA
 before=stage.parse(raw);after=stage.stage_config(raw)
 node=before["launcher"]["btstack"]; changed=stage.parse(after)
 assert changed["launcher"]["btstack"]["exec"]==stage.WRAPPER
 assert after.replace(stage.WRAPPER.encode(),stage.STOCK.encode(),1)==raw
 assert node["exec"]==stage.STOCK
 # Do not expose unrelated configuration or credentials.
 arg_fields={k:v for k,v in node.items() if k.lower() in {"args","argv","argument","arguments","parameter","parameters","cmdline","commandline"}}
 for k,v in arg_fields.items():
  if not (isinstance(v,(str,int,list)) and len(str(v))<=128):raise ValueError("unrecognized argument representation")
 print(json.dumps({"scope":"EXACT_OEM_CONFIG_OFFLINE_ONLY","sha256":stage.sha(raw),"staged_sha256":stage.sha(after),"launcher_fields":sorted(node),"argument_field_names":sorted(arg_fields),"argument_field_matches_observed_1":{k:v in ("1",1,["1"],[1]) for k,v in arg_fields.items()},"only_exec_changed":True,"runtime_supervisor_verified":False},sort_keys=True))
 print("R105159_EXACT_CONFIG_ARGUMENT_PRESERVATION_PASS")
def run(case):
 args,flag,present,inherit,exitcode=CASES[case]
 with tempfile.TemporaryDirectory(prefix="step7-") as d:
  p=Path(d);target=p/"target";lib=p/"probe.so";old=p/"old.so";wrapper=p/"wrapper.sh"
  target.write_text("#!/usr/bin/python3\nimport json,os,sys\nprint(json.dumps({'args':sys.argv[1:],'pid':os.getpid(),'probe':os.environ.get('AU57X_R92_PROBE'),'preload':os.environ.get('LD_PRELOAD'),'other':os.environ.get('STEP7_KEEP')}))\nsys.exit(int(os.environ['STEP7_EXIT']))\n")
  target.chmod(0o700)
  src=p/"empty.c";src.write_text("int step7_fixture(void){return 1;}\n")
  for file,needed in [(lib,present),(old,inherit)]:
   if needed:subprocess.run(["cc","-shared","-fPIC",str(src),"-o",str(file)],check=True)
  wrapper.write_text(render(target,lib));wrapper.chmod(0o700)
  subprocess.run(["/bin/sh","-n",str(wrapper)],check=True)
  env={k:v for k,v in os.environ.items() if k not in ("LD_PRELOAD","AU57X_R92_PROBE","AU57X_R92_PROBE_OPT_IN")}
  env.update(AU57X_R92_PROBE_OPT_IN=flag,STEP7_KEEP="preserved",STEP7_EXIT=str(exitcode))
  if inherit:env["LD_PRELOAD"]=str(old)
  proc=subprocess.Popen(["/bin/sh",str(wrapper),*args],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  stdout,stderr=proc.communicate(timeout=10)
  assert proc.returncode==exitcode,(case,proc.returncode,stderr)
  row=json.loads(stdout)
  expected_active=flag=="1" and present
  expected_preload=(str(lib)+(":"+str(old) if inherit else "")) if expected_active else (str(old) if inherit else None)
  assert row["args"]==args and row["pid"]==proc.pid and row["other"]=="preserved",row
  assert row["probe"]==("1" if expected_active else None) and row["preload"]==expected_preload,row
  print("R105159_HOST_ARGV_PASS",case,"argc",len(args),"exit",exitcode,"same_pid",True)
if __name__=="__main__":
 if sys.argv[1]=="config":config(Path(sys.argv[2]))
 elif sys.argv[1]=="render-qnx":
  Path(sys.argv[2]).write_text(render("/proc/boot/step7-target","/proc/boot/step7-absent.so"))
 else:run(sys.argv[1])
