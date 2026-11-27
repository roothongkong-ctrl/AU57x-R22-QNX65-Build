#ifndef MU1005_RF_INGRESS_H
#define MU1005_RF_INGRESS_H
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#define AU_RF_MAX_DATA 5035u
#define AU_RF_QSIZE 8u
#define AU_RF_WIRE_MAX (8u + AU_RF_MAX_DATA)
/* Normalized AU48/R5 API; NOT OEM Blue SDK ABI. Serialize all callbacks. */
typedef int (*AuRfSdkSend)(void*,const uint8_t*,size_t);
typedef struct {uint8_t bytes[AU_RF_WIRE_MAX];size_t len;} AuRfWire;
/* Receipt is local evidence of all SDK completion callbacks for one
 * normalized frame, not proof of peer reception or MFi authentication. */
typedef struct {
 uint64_t sequence;
 uint32_t generation;
 size_t bytes;
} AuRfTxReceipt;
typedef struct {
 AuRfWire queue[AU_RF_QSIZE];
 unsigned head,tail,count;
 uint32_t generation;
 int connected,failed,tx_busy,tx_cancelled;
 uint8_t peer[6];
 uint8_t tx[AU_RF_MAX_DATA];
 size_t tx_len,tx_offset,tx_inflight,max_payload;
 AuRfSdkSend sdk_send;void *sdk_ctx;
 AuRfTxReceipt tx_receipt;
} AuRfIngress;
void au_rf_init(AuRfIngress*,AuRfSdkSend,void*);
int au_rf_connected(AuRfIngress*,const uint8_t[6]);
int au_rf_set_max_payload(AuRfIngress*,size_t);
int au_rf_received(AuRfIngress*,const void*,size_t);
int au_rf_disconnected(AuRfIngress*);
int au_rf_take(AuRfIngress*,AuRfWire*);
int au_rf_daemon_data(AuRfIngress*,const uint8_t*,size_t);
int au_rf_send_complete(AuRfIngress*,int);
void au_rf_abort(AuRfIngress*);
/* Serialized owner only. Returns 1 for a new receipt of the current live
 * generation, 0 when absent. Never use an old receipt for a new submission.
 * Caller records sequence before sending, allows only one outstanding frame,
 * and independently verifies native event token and owner. */
int au_rf_tx_receipt(const AuRfIngress*,uint32_t,uint64_t,AuRfTxReceipt*);
#endif
