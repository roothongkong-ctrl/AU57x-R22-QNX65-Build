/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "usb_exit_watch.h"
#include <signal.h>
#include <errno.h>
int mu_usb_exit_arm(MuUsbExitWatch*w,MuPairedStart*s,pid_t pid,uint64_t cycle,
 int(*original_usb)(void*,pid_t,uint64_t,uint32_t)){
 uint32_t gen=0,after=0;
 if(!w||!s||!original_usb||!s->snapshot||!s->bt.ops.wired_idle_verified||
    pid<=1||!cycle)return CP_WCP_INVALID;
 if(w->armed||cycle<=w->last_cycle||cycle<=s->last_usb_cycle)return CP_WCP_STALE;
 if(s->snapshot(s->ctx,&gen)!=1||!gen||
    s->bt.ops.wired_idle_verified(s->ctx)!=0||
    kill(pid,0)!=0||original_usb(s->ctx,pid,cycle,gen)!=1||
    s->snapshot(s->ctx,&after)!=1||after!=gen||
    s->bt.ops.wired_idle_verified(s->ctx)!=0||kill(pid,0)!=0)
  return CP_WCP_DENIED;
 w->dio_pid=pid;w->cycle=cycle;w->generation=gen;w->armed=1;w->exit_seen=0;
 return CP_WCP_OK;
}
int mu_usb_exit_tick(MuUsbExitWatch*w,MuPairedStart*s,uint64_t now,
 int(*verify)(void*,uint64_t,uint32_t)){
 uint32_t gen=0;
 if(!w||!s||!s->snapshot||!verify)return CP_WCP_INVALID;
 if(!w->armed)return CP_WCP_WAIT;
 if(s->snapshot(s->ctx,&gen)!=1||gen!=w->generation){
  /* A different/ambiguous phone never inherits this USB cycle. */
  w->last_cycle=w->cycle;w->armed=0;return CP_WCP_DENIED;
 }
 if(!w->exit_seen){
  if(kill(w->dio_pid,0)==0)return CP_WCP_WAIT;
  if(errno!=ESRCH)return CP_WCP_WAIT;
  w->exit_seen=1;
 }
 /* The old DIO process really ended; if USB is still present, the
  * verified cycle/owner checks defer all new wireless requests. */
 if(verify(s->ctx,w->cycle,gen)!=1)return CP_WCP_WAIT;
 int rc=mu_paired_start_usb_ended(s,w->cycle,now,verify);
 if(rc==CP_WCP_OK||rc==CP_WCP_STALE){
  w->last_cycle=w->cycle;w->armed=0;
 }
 return rc;
}
