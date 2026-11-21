/* SPDX-License-Identifier: GPL-3.0-only
 * Review-only AU57x OEM-bound bounded media/control forwarding adapter.
 * No codecs, decoder hook, microphone capture or real DIO media endpoint.
 */
#ifndef CP_CLOUD_MEDIA_H
#define CP_CLOUD_MEDIA_H
#include "cp_cloud_pipeline.h"
#include <stddef.h>
#include <stdint.h>
#define CP_CLOUD_MEDIA_MAX 8192u
enum cp_cloud_forward_result { CP_FWD_OK=0,CP_FWD_INVALID=-1,
 CP_FWD_OWNER=-2,CP_FWD_STALE=-3,CP_FWD_ORDER=-4,CP_FWD_BACKPRESSURE=-5 };
struct cp_cloud_media_ops {
 void *ctx;
 int (*on_owner_thread)(void *,uint32_t epoch);
 int (*source_verified)(void *,uint32_t epoch,enum cp_cloud_media, uint32_t seq);
 /* Returns 1 only when a native sink explicitly accepts the complete buffer;
  * zero/backpressure is NOT proof of delivery. */
 int (*native_deliver)(void *,uint32_t epoch,enum cp_cloud_media,
                       const uint8_t *,size_t,uint32_t seq);
};
struct cp_cloud_media_router {
 struct cp_cloud_pipeline *pipeline;
 struct cp_cloud_media_ops ops;
 uint32_t epoch,last_seq[5];
 int closed;
};
int cp_cloud_media_bind(struct cp_cloud_media_router *,
                        struct cp_cloud_pipeline *,const struct cp_cloud_media_ops *);
int cp_cloud_media_forward(struct cp_cloud_media_router *,enum cp_cloud_media,
                           const uint8_t *,size_t,uint32_t seq,uint32_t event_epoch);
void cp_cloud_media_close(struct cp_cloud_media_router *);
#endif
