#ifndef MU1005_R34_AP_SERVER_H
#define MU1005_R34_AP_SERVER_H
/* R34 AirPlay receiver server control adapter.
 *
 * Evidence: OEM libairplay.so SHA 193a4fd9...a46b5. Static branches:
 *   interfaceName -> AirPlayReceiverServerSetProperty writes 17 bytes at server+364
 *   startServer   -> AirPlayReceiverServerControl sets server+0x162 then calls 0x1e9d8
 *   stopServer    -> AirPlayReceiverServerControl clears server+0x162 then calls 0x1d1e8
 * return 0 from the control function is only "accepted", NOT listening proof.
 *
 * This module does not hardcode any ARM address, does not synthesize CF objects,
 * and does not touch the firewall. A trusted same-process binder must supply
 * verified CFString objects, the correct server/owner context and all checks.
 */
#include <stddef.h>
#include <stdint.h>

#define MU34_AP_LIBAIRPLAY_SHA256 \
    "193a4fd9101ec2aa05e7159cfa307b96500810d379ca74a194f172adc13a46b5"
#define MU34_AP_INTERFACE_COPY_BYTES 17u

typedef enum Mu34ApState {
    MU34_AP_IDLE = 0,
    MU34_AP_BOUND = 1,
    MU34_AP_INTERFACE_SET = 2,
    MU34_AP_START_REQUESTED = 3,
    MU34_AP_LISTENING_VERIFIED = 4,
    MU34_AP_FAILED = 5
} Mu34ApState;

typedef enum Mu34ApResult {
    MU34_AP_OK = 0,
    MU34_AP_R_START_REQUESTED = 1,
    MU34_AP_R_LISTENING_VERIFIED = 2,
    MU34_AP_ERR_BAD_ARG = -1,
    MU34_AP_ERR_UNBOUND = -2,
    MU34_AP_ERR_STALE = -3,
    MU34_AP_ERR_STATE = -4,
    MU34_AP_ERR_NOT_OEM = -5,
    MU34_AP_ERR_NOT_OWNER = -6,
    MU34_AP_ERR_BAD_CF = -7,
    MU34_AP_ERR_BAD_INTERFACE = -8,
    MU34_AP_ERR_VENDOR = -9,
    MU34_AP_ERR_NOT_LISTENING = -10
} Mu34ApResult;

/* All callbacks are binder-supplied. None is discovered by this module. */
typedef struct Mu34ApServerBinding {
    /* Must verify exact SHA256, both MFi exports and the AirPlay server symbols. */
    int (*library_verified)(void *opaque);
    /* Must prove the object is the genuine CFString type from libairplay. */
    int (*cf_string_ok)(void *opaque, const void *cf_object);
    /* Copies bounded UTF-8; returns bytes, 0 for empty, -1 for invalid CF object. */
    int (*cf_string_utf8)(void *opaque, const void *cf_object,
                          char *out, size_t capacity);
    /* Must prove this is the permitted owner thread for this server. */
    int (*owner_thread_ok)(void *opaque);
    /* 0 only means the OEM property/control call accepted the command. */
    int (*set_property)(void *opaque, void *server,
                        const void *cf_key, const void *cf_value);
    int (*server_control)(void *opaque, void *server,
                          const void *cf_command, const void *owner);
    /* 1 only after interface, port, identity and mDNS are really checked. */
    int (*listening_verified)(void *opaque, void *server,
                              const void *owner, unsigned expected_port);
    void *opaque;
    void *server;               /* OEM AirPlay server object, binder-owned */
    const void *owner;          /* OEM owner/delegate context */
    const void *interface_key;  /* CFString "interfaceName" */
    const void *start_command;  /* CFString "startServer" */
    const void *stop_command;   /* CFString "stopServer" */
    uint32_t generation;
} Mu34ApServerBinding;

typedef struct Mu34ApServer {
    Mu34ApServerBinding binding;
    uint32_t generation;
    uint8_t state;              /* Mu34ApState */
    uint8_t interface_set;
    uint8_t start_requested;
    uint8_t listening_verified;
    unsigned expected_port;     /* pinned at start, never supplied by poll */
    char interface_name[MU34_AP_INTERFACE_COPY_BYTES + 1];
} Mu34ApServer;

/* Pure local binding; performs no OEM call and accepts no guessed addresses. */
int mu34_ap_server_init(Mu34ApServer *server, const Mu34ApServerBinding *binding);

/* Must happen before start. Value must be a binder-certified CFString whose
 * UTF-8 name is 1..16 safe bytes (17-byte native buffer includes NUL), matching the observed 17-byte copy. */
Mu34ApResult mu34_ap_server_set_interface(Mu34ApServer *server,
    uint32_t generation, const void *cf_interface_value);

/* startServer rc==0 only moves to START_REQUESTED. LISTENING_VERIFIED requires
 * the binder's positive post-check; no port or mDNS success is assumed. */
Mu34ApResult mu34_ap_server_start(Mu34ApServer *server,
    uint32_t generation, unsigned expected_port);

/* Poll only from the original owner thread; 1=pending, 2=verified. */
Mu34ApResult mu34_ap_server_poll(Mu34ApServer *server, uint32_t generation);

Mu34ApResult mu34_ap_server_stop(Mu34ApServer *server, uint32_t generation);

/* Local-only reset. Refuses while listening is still verified. */
Mu34ApResult mu34_ap_server_close(Mu34ApServer *server, uint32_t generation);

Mu34ApState mu34_ap_server_state(const Mu34ApServer *server);
const char *mu34_ap_server_state_name(Mu34ApState state);
const char *mu34_ap_server_interface(const Mu34ApServer *server);

#endif /* MU1005_R34_AP_SERVER_H */
