#!/usr/bin/env python3
"""Patch an upstream QNX toolchain Dockerfile's *downloads only* for curl 18.
Only two exact, checksum-verified GNU tarball URLs are touched. Abort if
upstream source layout changes. Hash verification remains in Dockerfile.
"""
import argparse
from pathlib import Path
import sys

TARGETS=(
 'curl -fsSL "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.xz"',
 'curl -fsSL "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.xz"',
)
RETRY='curl --retry 6 --retry-all-errors --retry-delay 5 --http1.1 -fsSL'

def patch(contents: str):
    for target in TARGETS:
        if contents.count(target) != 1:
            raise ValueError('toolchain Dockerfile changed, expected exact checksum-verified curl URL')
        contents=contents.replace(target,target.replace('curl -fsSL',RETRY))
    if contents.count('sha256sum -c -') < 2:
        raise ValueError('Dockerfile missing SHA-256 verification')
    return contents

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dockerfile')
    args=p.parse_args()
    path=Path(args.dockerfile)
    try:
        before=path.read_text()
        after=patch(before)
        path.write_text(after)
    except (OSError,ValueError) as e:
        print('R95 toolchain retry patch BLOCKED:',e,file=sys.stderr)
        return 2
    print('R95 transient download retry patch applied; GNU SHA256 checks retained')
    return 0
if __name__=='__main__':sys.exit(main())
