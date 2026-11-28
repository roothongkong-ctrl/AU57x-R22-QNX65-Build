/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_SI_DIO_OWNER_H
#define MU1005_SI_DIO_OWNER_H
#include "../mu1005_dio_launch/dio_launch.h"
/* Original SI serial owner only; no synthetic MFi or receiver readiness.
 * Distinct epoch, original USB idle, and confirmed cleanup are prerequisites. */
typedef struct {
 MuDioLaunch launch;
 uint64_t retired_epoch;
} MuSiDioOwner;
typedef struct {
 void *ctx;
 int (*admit)(void*,uint64_t,const char*);
 int (*cleanup)(void*,uint64_t);
} MuSiDioOps;
int mu_si_dio_begin(MuSiDioOwner*,const MuSiDioOps*,uint64_t,char*const[],char*const[],const char*);
int mu_si_dio_retire(MuSiDioOwner*,const MuSiDioOps*,uint64_t);
#endif
