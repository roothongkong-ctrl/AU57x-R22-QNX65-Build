/* SPDX-License-Identifier: GPL-3.0-only
 * R103's exact 8-byte RFCOMM IPC header, in dependency-free C for QNX/host.
 * Parsing does not equate to BlueSDK ownership or Apple authentication.
 */
#ifndef AU57X_R106M1_WIRE_H
#define AU57X_R106M1_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define WC_RF_MAX 5035u
#define WC_RF_HEADER 8u
typedef enum { WC_RF_CONNECTED=1,WC_RF_DATA=2,WC_RF_DISCONNECTED=3 } WcWireKind;
typedef struct {
 uint8_t kind;
 uint32_t generation;
 const uint8_t *data;
 size_t size;
} WcWireFrame;
int wc_wire_decode(const uint8_t *buf,size_t size,WcWireFrame *out);
int wc_wire_encode(uint8_t kind,uint32_t generation,const uint8_t *data,size_t size,
                   uint8_t *out,size_t cap,size_t *written);
#endif
