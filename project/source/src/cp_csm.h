/*
 * DiPlay -> AU57x/QNX adaptation reference: two iAP2 CSM message encoders.
 * Derived from shihabal3amri/DiPlay (GPL-3.0-only).
 * Copyright (c) 2026 adaptation contributors. GPL-3.0-only; see LICENSE.
 * NOT a wireless CarPlay implementation or executable for a vehicle.
 */
#ifndef CP_CSM_H
#define CP_CSM_H
#include <stddef.h>
#include <stdint.h>

enum cp_error { CP_OK = 0, CP_BAD_ARGS = -1, CP_OVERFLOW = -2 };
struct cp_wifi {
    const char *ssid;
    const char *passphrase;
    uint8_t security; /* DiPlay: 0=open,1=WEP,2=WPA/WPA2,3=WPA3 transition,4=WPA3 only */
    uint8_t channel;
    const uint8_t *bssid6; /* NULL: omit optional BSSID */
};
struct cp_session {
    struct cp_wifi wifi;
    const char *const *ip_addresses;
    size_t ip_address_count;
    uint16_t airplay_port;
    const char *device_identifier; /* NULL: omit */
    const char *public_key;
    const char *source_version;
};
/* The original OEM must supply its genuine identifier, Bluetooth address,
 * advertised capabilities and Wi-Fi SSID. No test identity is installed. */
struct cp_identification {
    const char *name, *model, *manufacturer, *serial;
    const char *firmware, *hardware, *bluetooth_name, *wireless_ssid;
    const uint8_t *bluetooth_mac6;
    const uint16_t *accessory_messages, *phone_messages;
    size_t accessory_message_count, phone_message_count;
};
int cp_encode_identification_1d01(const struct cp_identification *,
                                  uint8_t *,size_t,size_t *);
/* Outputs full CSM frame: 0x4040, BE16 length, BE16 message ID, TLV body.
 * No iAP2 link framing, authentication, network or vehicle I/O is included.
 */
int cp_encode_wifi_5703(const struct cp_wifi *wifi,
                        uint8_t *output, size_t capacity, size_t *written);
int cp_encode_start_4301(const struct cp_session *session,
                         uint8_t *output, size_t capacity, size_t *written);
#endif
