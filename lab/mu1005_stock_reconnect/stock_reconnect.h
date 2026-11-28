/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MU1005_STOCK_RECONNECT_H
#define MU1005_STOCK_RECONNECT_H
#include "paired_start.h"
/* Existing OEM paired snapshot/ACL and stock iAP2 controller only.
 * No new HFP/ACL pairing, connect or disconnect requests.
 * Install only on the existing serialized native owner. */
int mu_stock_reconnect_step(MuPairedStart*,int handoff_ready,uint64_t now_ms);
int mu_stock_reconnect_bind(MuPairedStart*);
#endif
