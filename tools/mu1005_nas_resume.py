#!/usr/bin/env python3
"""Single sequential NAS acquisition with bounded curl resumes. No URL in logs."""
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.parse

MAX=1_200_000_000
MIN=800_000_000
TRANSIENT={7,18,28,35,52,55,56,92}
def download(url,dest,*,budget=4800):
    u=urllib.parse.urlsplit(url)
    if u.scheme!="https" or not u.hostname or u.username or u.password:
        raise ValueError("HTTPS NAS share required")
    dest=Path(dest)
    if dest.exists():raise ValueError("destination already exists")
    part=dest.with_name(dest.name+".part")
    if part.is_symlink():raise ValueError("unexpected partial symlink")
    deadline=time.monotonic()+budget
    restarted=False
    for attempt in range(1,9):
        left=int(deadline-time.monotonic())
        if left<=0:break
        offset=part.stat().st_size if part.exists() else 0
        if offset>MAX:raise ValueError("partial exceeds size limit")
        if offset:
            with part.open("rb") as f:
                if f.read(6)!=bytes.fromhex("377abcaf271c"):
                    raise ValueError("partial not 7z")
        command=["curl","--http1.1","--fail","--silent","--location","--max-redirs","5",
            "--proto","=https","--proto-redir","=https","--connect-timeout","30",
            "--max-time",str(min(1800,left)),"--max-filesize",str(MAX),
            "--speed-time","90","--speed-limit","1024","--output",str(part),
            "--write-out","HTTP %{http_code}; bytes %{size_download}; seconds %{time_total}\\n"]
        if offset:command.extend(["--continue-at","-"])
        command.append(url)
        print(f"NAS_ATTEMPT {attempt} resume_at={offset}",flush=True)
        proc=subprocess.Popen(command,stderr=subprocess.DEVNULL)
        try:
            while proc.poll() is None:
                try:proc.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    count=part.stat().st_size if part.exists() else 0
                    print(f"NAS_DOWNLOADED_BYTES {count}",flush=True)
                    if count>MAX or time.monotonic()>deadline:
                        proc.kill();proc.wait()
                        raise ValueError("download exceeds limit")
        finally:
            if proc.poll() is None:proc.kill();proc.wait()
        rc=proc.returncode
        count=part.stat().st_size if part.exists() else 0
        print(f"NAS_ATTEMPT_RESULT exit={rc} total_bytes={count}",flush=True)
        if count>MAX:raise ValueError("download exceeds size limit")
        if rc==0:
            if not MIN<=count<=MAX:raise ValueError("download size unexpected")
            with part.open("rb") as f:
                if f.read(6)!=bytes.fromhex("377abcaf271c"):raise ValueError("download not 7z")
            # Full archive validation is the next mandatory workflow step.
            part.rename(dest)
            return
        if rc==33 and offset and not restarted:
            # Server explicitly refused the Range request. Curl has not
            # accepted an unvalidated append. Restart only once.
            print("NAS_RANGE_UNSUPPORTED_RESTART_ONCE",flush=True)
            part.unlink(missing_ok=True)
            restarted=True
        elif rc not in TRANSIENT:
            raise RuntimeError(f"NAS_DOWNLOAD_FAILED curl_exit={rc}")
        time.sleep(min(5,max(0,deadline-time.monotonic())))
    raise RuntimeError("NAS_DOWNLOAD_RETRY_BUDGET_EXHAUSTED")
if __name__=="__main__":
    try:download(os.environ["NAS_URL"],sys.argv[1])
    except (ValueError,RuntimeError,OSError) as exc:
        # OS error text can include local path but never the share URL.
        print(type(exc).__name__,str(exc),file=sys.stderr)
        raise SystemExit(2)
