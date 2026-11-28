#!/usr/bin/env python3
"""Run the actual host donor on an inherited TCP endpoint; no OEM/RF."""
import os,socket,subprocess,sys
binary=sys.argv[1]
def trial(bad):
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        listener.bind(("127.0.0.1",57342));listener.listen()
        with socket.create_connection(("127.0.0.1",57342),timeout=3) as peer:
            conn,_=listener.accept()
            with conn:
                env=dict(os.environ,MU1005_BRIDGE_FD=str(conn.fileno()),MU1005_BRIDGE_READY="1")
                child=subprocess.Popen([binary,"--mu1005-ack-fd"],env=env,pass_fds=(conn.fileno(),),stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                marker=b"MU1005-READY-v1\n";data=b""
                while len(data)<len(marker):
                    part=peer.recv(len(marker)-len(data))
                    assert part, "EOF before ready"
                    data+=part
                assert data==marker,data
                if bad:peer.sendall(bytes([2,1,0,0,0,0,0,0]))
                peer.shutdown(socket.SHUT_WR)
                _,err=child.communicate(timeout=8)
                assert (child.returncode!=0)==bad,(child.returncode,err)
                if bad:assert b"session ended" in err,err
            finally:
                if child.poll() is None:child.kill();child.wait()
trial(False);trial(True)
print("ACTUAL_DONOR_INHERITED_CLEAN_EOF_AND_ERROR_EXIT_PASS_NO_OEM")
