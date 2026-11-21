/* SPDX-License-Identifier: GPL-3.0-only
 * R41: Owner-mediated binding to STOCK MU1005 libairplay MFi exports.
 * No Apple keys, custom signing, direct I2C, hook, or new OEM server.
 */
#ifndef CP_STEP04_STOCK_MFI_H
#define CP_STEP04_STOCK_MFI_H
#include <stddef.h>
#include <stdint.h>
#define CP_STOCK_CERT_MAX 1280u
#define CP_STOCK_SIGNATURE_MAX 128u
#define CP_STOCK_CHALLENGE 20u
enum cp_stock_rc { CP_STOCK_OK=0, CP_STOCK_INVALID=-1,
 CP_STOCK_OWNER=-2, CP_STOCK_VENDOR=-3, CP_STOCK_OVERFLOW=-4 };
struct cp_stock_owner {
 void *opaque;
 int (*owner_verified)(void *,uint32_t epoch,uint32_t bt_generation);
 int (*stock_image_verified)(void *);
 int (*control_session_live)(void *,uint32_t epoch,uint32_t bt_generation);
 int (*wired_idle)(void *);
 void (*release_oem)(void *,void *);
};
struct cp_stock_mfi {
 struct cp_stock_owner owner;
 uint32_t epoch,bt_generation;
 void *existing_library;
 int (*copy_certificate)(uint8_t **,size_t *);
 int (*create_signature)(const uint8_t *,size_t,uint8_t **,size_t *);
 int bound;
};
/* Existing library must be the original loaded and hash-verified OEM instance,
 * borrowed on the serialized owner thread. Never dlopen a second instance. */
int cp_stock_mfi_bind(struct cp_stock_mfi *,void *existing_library,
                      const struct cp_stock_owner *,uint32_t epoch,uint32_t bt_generation);
int cp_stock_mfi_cert(struct cp_stock_mfi *,uint8_t *dst,size_t cap,size_t *n);
int cp_stock_mfi_sign(struct cp_stock_mfi *,const uint8_t *challenge,size_t len,
                      uint8_t *dst,size_t cap,size_t *n);
int cp_stock_mfi_live(struct cp_stock_mfi *);
void cp_stock_mfi_invalidate(struct cp_stock_mfi *);
#endif
