#ifndef SUPERVISOR_FIXTURE_H
#define SUPERVISOR_FIXTURE_H
#include "txn.h"
/* Disposable host test; no process names, mount or target-specific actions. */
int supervisor_fixture(const txn_spec *s,const char *action);
#endif
