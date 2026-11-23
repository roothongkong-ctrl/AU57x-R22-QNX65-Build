#ifndef AU57X_UPSTREAM_IAP2_CAPI_H
#define AU57X_UPSTREAM_IAP2_CAPI_H
/* C API for the unmodified GPL-3.0 EntryNav2/DiPlay iAP2 link engine.
 * This DOES NOT open Bluetooth, provide OEM authentication or bypass MFi.
 * Caller must enforce MU1005 owner, paired-MAC and USB-priority policy.
 * All calls and callbacks MUST be serialized on one caller-owned context.
 * TX accepted means handed off to callback; it is NOT a radio ACK.
 */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* QNX6.5 GCC may fail to expose size_t through C11 stddef.h.
 * Use compiler ABI type; do not modify OEM headers. */
#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ au57x_iap2_size_t;
#else
typedef size_t au57x_iap2_size_t;
#endif
typedef struct au57x_iap2_link au57x_iap2_link;
typedef int (*au57x_iap2_tx_cb)(void *ctx,const uint8_t *bytes,au57x_iap2_size_t count);
typedef void (*au57x_iap2_rx_cb)(void *ctx,uint8_t session_id,const uint8_t *bytes,au57x_iap2_size_t count);
typedef void (*au57x_iap2_state_cb)(void *ctx,int state);
typedef void (*au57x_iap2_error_cb)(void *ctx,const char *reason);
typedef struct {
 void *ctx;
 au57x_iap2_tx_cb tx;
 au57x_iap2_rx_cb rx;
 au57x_iap2_state_cb state;
 au57x_iap2_error_cb error;
} au57x_iap2_callbacks;
/* NULL on missing callbacks or allocation/constructor failure. */
au57x_iap2_link *au57x_iap2_new(const au57x_iap2_callbacks *callbacks);
/* Return 1 success / 0 failure. State: 0 idle,1 detecting,2 SYN,3 connected,4 failed. */
int au57x_iap2_start(au57x_iap2_link *);
int au57x_iap2_feed(au57x_iap2_link *,const uint8_t *bytes,au57x_iap2_size_t count);
int au57x_iap2_send(au57x_iap2_link *,uint8_t session,const uint8_t *bytes,au57x_iap2_size_t count);
int au57x_iap2_tick(au57x_iap2_link *,int64_t monotonic_ms);
int au57x_iap2_state(const au57x_iap2_link *);
void au57x_iap2_reset(au57x_iap2_link *);
void au57x_iap2_free(au57x_iap2_link *);
#ifdef __cplusplus
}
#endif
#endif
