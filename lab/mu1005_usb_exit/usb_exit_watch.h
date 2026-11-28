/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_USB_EXIT_WATCH_H
#define MU1005_USB_EXIT_WATCH_H
#include <sys/types.h>
#include "paired_start.h"
/* Congo's USB DIO exit -> wireless resume ordering, using the original
 * MU1005 process identity and the existing paired ACL controller.
 * Owner serializes calls outside SDK locks. No process is killed, no
 * waitpid is stolen from the stock launcher, and no USB auth is reused. */
typedef struct {
 pid_t dio_pid;
 uint64_t cycle,last_cycle;
 uint32_t generation;
 int armed,exit_seen;
} MuUsbExitWatch;
/* Caller must prove pid belongs to the current original USB CarPlay
 * DIO process, its cycle and same phone. Never arm from a name alone. */
int mu_usb_exit_arm(MuUsbExitWatch*,MuPairedStart*,pid_t,uint64_t,
 int(*)(void*,pid_t,uint64_t,uint32_t));
/* kill(pid,0) ESRCH supplies real process absence. No exit is claimed on
 * EPERM, EINTR, other errors or a still-live/reused pid. The original
 * owner must independently confirm USB and prior wireless retirement.
 * 0/CP_WCP_WAIT = no action yet, CP_WCP_OK on successful resume.
 * Call on the serialized owner loop, without blocking process waits. */
int mu_usb_exit_tick(MuUsbExitWatch*,MuPairedStart*,uint64_t,
 int(*)(void*,uint64_t,uint32_t));
#endif
