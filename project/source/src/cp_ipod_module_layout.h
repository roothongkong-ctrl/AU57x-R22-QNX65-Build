/* GPL-3.0-only. STRUCTURAL VIEW based on 70-version OEM ELF layout.
 * Verified in ipod-transport-usbdevice/usbhost, not a callable ABI or plugin.
 * Private callback TABLE PREFIX also confirmed by 2 OEM transports.
 * Do not export or register this structure to mm-ipod without full contract. */
#ifndef CP_IPOD_MODULE_LAYOUT_H
#define CP_IPOD_MODULE_LAYOUT_H
#include <stdint.h>
#include <stddef.h>
struct cp_ipod_module_layout {
  uint32_t name_relocation;     /* +0x00 (ELF-relative string pointer) */
  uint32_t abi_version;         /* +0x04 = 0x200 */
  uint32_t opaque[6];           /* +0x08 through +0x1f: zero in 2 original refs */
  uint32_t private_descriptor;  /* +0x20 pointer to module's private data */
};
struct cp_ipod_callback_prefix {
  uint32_t transport_name; /* +0x00 */
  uint32_t unknown_zeros[4]; /* +0x04 - +0x13 */
  uint32_t init; /* +0x14 */
  uint32_t link_params_get; /* +0x18 */
  uint32_t type_get; /* +0x1c */
  uint32_t caps_get; /* +0x20, null on usbhost */
  uint32_t audiopath_get; /* +0x24 */
  uint32_t data_send; /* +0x28 */
  uint32_t data_recv; /* +0x2c */
};
#define CP_IPOD_LAYOUT_ASSERT(name,expr) typedef char name[(expr)?1:-1]
CP_IPOD_LAYOUT_ASSERT(cp_ipod_prefix_48,sizeof(struct cp_ipod_callback_prefix)==48);
CP_IPOD_LAYOUT_ASSERT(cp_ipod_module_size_36,sizeof(struct cp_ipod_module_layout)==36);
CP_IPOD_LAYOUT_ASSERT(cp_ipod_module_private_at_32,offsetof(struct cp_ipod_module_layout,private_descriptor)==32);
#endif
