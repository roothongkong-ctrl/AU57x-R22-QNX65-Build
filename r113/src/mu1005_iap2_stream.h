#ifndef AU57X_MU1005_IAP2_STREAM_H
#define AU57X_MU1005_IAP2_STREAM_H
#include <stdint.h>
#include <stddef.h>
#include "au108_oem_port.h"
#include "inbound.h"
#include "au57x_iap2_capi.h"
/* RFCOMM is a byte stream. The OEM callback may send only ONE packet before
 * completion event code 6. The adapter buffers upstream iAP2 bytes in FIFO
 * and pumps at most one OEM packet at a time, after serialized owner events.
 * NOT installable: exact btstack owner callback remains unverified. */
#ifdef __cplusplus
extern "C" {
#endif
#define MU1005_IAP2_TX_CAP 32768u
typedef struct {
 Inbound *rf;
 Au108Port *authorization;
 au57x_iap2_link *core;
 uint8_t fifo[MU1005_IAP2_TX_CAP];
 uint8_t chunk[MAX_DATA];
 size_t head, queued;
 uint8_t active, stopped;
 uint32_t generation;
} Mu1005Iap2Stream;
void mu1005_iap2_stream_init(Mu1005Iap2Stream *,Inbound *,Au108Port *);
int mu1005_iap2_stream_on_open(Mu1005Iap2Stream *,uint32_t generation);
int mu1005_iap2_stream_on_bytes(Mu1005Iap2Stream *,const uint8_t *,size_t);
int mu1005_iap2_stream_tick(Mu1005Iap2Stream *,int64_t monotonic_ms);
/* CALL after original inbound_event(code=6) has cleared busy; also after RX
 * is fully dispatched, never from within the SDK TX callback itself.
 * >0: one OEM packet queued, 0: busy/empty, <0: fatal/unauthorized. */
int mu1005_iap2_stream_pump(Mu1005Iap2Stream *);
void mu1005_iap2_stream_stop(Mu1005Iap2Stream *);
#ifdef __cplusplus
}
#endif
#endif
