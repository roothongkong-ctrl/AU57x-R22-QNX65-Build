/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_DIO_LAUNCH_H
#define MU1005_DIO_LAUNCH_H
#include <sys/types.h>
#include <stdint.h>
/* Original SI supervisor only, holding its exclusive DIO launch/stop
 * authority across this call. Caller supplies the genuine original DIO
 * argv/environment and immutable verified native module path. No guessing
 * OEM arguments, no USB session preemption by an independent daemon. */
typedef struct {pid_t pid;uint64_t epoch;int attempted;} MuDioLaunch;
int mu_dio_launch_wireless(MuDioLaunch*,uint64_t,char*const[],char*const[],
 const char*,int(*)(void*,uint64_t,const char*),void*);
/* Return 1 running, 0 reaped, -1 unknown. No signal or forced teardown. */
int mu_dio_launch_reap(MuDioLaunch*);
#endif
