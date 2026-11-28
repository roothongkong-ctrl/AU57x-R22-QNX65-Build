from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
a="-display none -serial stdio"
assert s.count(a)==1
s=s.replace(a,"-nic user,restrict=on -display none -serial stdio")
a="( printf '\\n'; sleep 4"
assert s.count(a)==1
s=s.replace(a,"( for _i in 1 2 3 4 5 6 7 8 9 10; do printf '\\n'; sleep 1; done")
p.write_text(s)
