#!/usr/bin/env python3
"""R96 one-click host laboratory generator around the actual C transaction supervisor.

No OEM paths, stock executable, Apple credentials or startup hooks are installed.
Every generated program is pinned to an owner-controlled au57x-r95-* fixture.
The real car lifecycle API is deliberately unbound: lab callbacks are MOCK.
"""
import argparse
import json
from pathlib import Path
import shutil
import sys

import r95_sd_plan_codegen as base

ACTIONS = ('oneclick', 'status', 'recover', 'restore', 'boot')


def generate(root, out):
    report = base.make(root, out)
    target = Path(out) / 'r95_txn_fixture.c'
    src = target.read_text(encoding='ascii')
    prefix, sep, _ = src.partition('int main(int argc, char **argv) {')
    if not sep or 'static const txn_row rows[]' not in prefix:
        raise ValueError('original R95 C template unexpectedly changed')
    # The spec is statically bound to audited host fixture paths and hashes.
    dirs = report['paths']
    spec = ',\n'.join('    ' + base.literal(str(dirs[n]))
                     for n in ('target', 'backup', 'payload', 'state'))
    main = r'''#include "supervisor_fixture.h"
#include <stdlib.h>
int main(int argc, char **argv) {
  const txn_spec s = {
''' + spec + r''',
    rows, sizeof(rows)/sizeof(rows[0]), NULL
  };
  if (argc != 2) {
    fputs("R96 HOST LAB ONLY: oneclick|status|recover|restore|boot\n", stderr);
    return 2;
  }
  if (!strcmp(argv[1], "oneclick")) return supervisor_fixture(&s,"supervised-trial");
  if (!strcmp(argv[1], "recover")) return supervisor_fixture(&s,"supervised-recover");
  if (!strcmp(argv[1], "restore")) return supervisor_fixture(&s,"supervised-restore");
  if (!strcmp(argv[1], "boot")) return supervisor_fixture(&s,"supervised-boot");
  if (!strcmp(argv[1], "status")) return txn_run(&s,"status") ? 1 : 0;
  fputs("unsupported host laboratory action\n", stderr);
  return 2;
}
'''
    # No real OEM lifecycle implementation: only the mock process in supervisor_fixture.c.
    final = prefix + main
    p = Path(out) / 'r96_oneclick_HOST_LAB.c'
    try:
        p.write_text(final, encoding='ascii')
        target.unlink()
        report.update({
            'version': 'R96', 'scope': 'HOST_LAB_ONLY', 'installable': False,
            'oneclick_supervised': True, 'supervisor': 'synthetic process fixture only',
            'original_btstack_loaded': False, 'original_mfi_verified': False,
            'airplay_vehicle_verified': False, 'factory_supervisor_bound': False,
            'boot_recovery_vehicle_bound': False,
            'commands': ACTIONS,
            'production_health_callbacks': False,
            'warning': 'MOCK lifecycle only. Never execute on Audi MHI2 / QNX head unit.'})
        (Path(out) / 'R96_ONECLICK_LAB_MANIFEST.json').write_text(
            json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + '\n', encoding='utf-8')
    except BaseException:
        shutil.rmtree(out)
        raise
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', required=True)
    p.add_argument('--out', required=True)
    a = p.parse_args()
    try:
        report = generate(a.root, a.out)
    except (ValueError, OSError) as e:
        print('R96 LAB DENY:', str(e), file=sys.stderr)
        return 2
    print(json.dumps({'status': 'generated', 'scope': report['scope'],
                      'rows': len(report['rows']), 'installable': False}))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
