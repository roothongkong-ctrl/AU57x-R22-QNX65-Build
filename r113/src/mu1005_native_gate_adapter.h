#ifndef AU57X_MU1005_NATIVE_GATE_ADAPTER_H
#define AU57X_MU1005_NATIVE_GATE_ADAPTER_H
#include "au108_oem_port.h"
#include "inbound.h"
/* Real SDK witnesses must come from verified btstack owner; no fake defaults. */
typedef struct {
 void *context;
 int (*exact_image_live)(void *);
 int (*on_original_owner_thread)(void *);
 int (*paired_phone)(void *);
 int (*mfi_hardware_ready)(void *);
 int (*wired_idle)(void *);
 int (*rf_sdp_visible)(void *,const Inbound *,uint8_t);
 int (*callbacks_quiesced)(void *,const Inbound *);
 int (*withdraw_barrier)(void *,const Inbound *);
} Mu1005NativeProof;
typedef struct {
 Inbound *service;
 Mu1005NativeProof proof;
} Mu1005NativeGateAdapter;
int mu1005_native_gate_make(Mu1005NativeGateAdapter *,Inbound *,
                            const Mu1005NativeProof *,Au108OEM *);
#endif
