#include "mu1005_r34_ap_server.h"
#include <string.h>

static int safe_interface_name(const char *name, size_t length)
{
    size_t i;
    if (!name || length == 0 || length >= MU34_AP_INTERFACE_COPY_BYTES)
        return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static int oem_ready(const Mu34ApServer *s)
{
    if (!s || s->state == MU34_AP_IDLE)
        return 0;
    if (!s->binding.library_verified || s->binding.library_verified(s->binding.opaque) != 1)
        return 0;
    if (!s->binding.owner_thread_ok || s->binding.owner_thread_ok(s->binding.opaque) != 1)
        return 0;
    return 1;
}

int mu34_ap_server_init(Mu34ApServer *s, const Mu34ApServerBinding *b)
{
    if (!s || !b || !b->generation || !b->server || !b->owner ||
        !b->library_verified || !b->cf_string_ok || !b->cf_string_utf8 ||
        !b->owner_thread_ok || !b->set_property || !b->server_control ||
        !b->listening_verified || !b->interface_key || !b->start_command ||
        !b->stop_command)
        return -1;
    if (b->cf_string_ok(b->opaque, b->interface_key) != 1 ||
        b->cf_string_ok(b->opaque, b->start_command) != 1 ||
        b->cf_string_ok(b->opaque, b->stop_command) != 1)
        return -1;
    memset(s, 0, sizeof(*s));
    s->binding = *b;
    s->generation = b->generation;
    s->state = MU34_AP_BOUND;
    return 0;
}

Mu34ApResult mu34_ap_server_set_interface(Mu34ApServer *s,
    uint32_t generation, const void *cf_interface_value)
{
    char name[MU34_AP_INTERFACE_COPY_BYTES + 1];
    int length;
    if (!s) return MU34_AP_ERR_BAD_ARG;
    if (generation != s->generation) return MU34_AP_ERR_STALE;
    if (s->state != MU34_AP_BOUND && s->state != MU34_AP_INTERFACE_SET)
        return MU34_AP_ERR_STATE;
    if (!oem_ready(s)) {
        s->state = MU34_AP_FAILED;
        return (s->binding.library_verified(
                   s->binding.opaque) != 1) ? MU34_AP_ERR_NOT_OEM : MU34_AP_ERR_NOT_OWNER;
    }
    if (!cf_interface_value ||
        s->binding.cf_string_ok(s->binding.opaque, cf_interface_value) != 1)
        return MU34_AP_ERR_BAD_CF;
    memset(name, 0, sizeof(name));
    length = s->binding.cf_string_utf8(s->binding.opaque,
                                       cf_interface_value, name,
                                       MU34_AP_INTERFACE_COPY_BYTES);
    if (length <= 0 || (size_t)length >= MU34_AP_INTERFACE_COPY_BYTES ||
        !safe_interface_name(name, (size_t)length))
        return MU34_AP_ERR_BAD_INTERFACE;
    if (s->binding.set_property(s->binding.opaque, s->binding.server,
                                s->binding.interface_key,
                                cf_interface_value) != 0) {
        s->state = MU34_AP_FAILED;
        return MU34_AP_ERR_VENDOR;
    }
    memset(s->interface_name, 0, sizeof(s->interface_name));
    memcpy(s->interface_name, name, (size_t)length);
    s->interface_set = 1;
    s->state = MU34_AP_INTERFACE_SET;
    return MU34_AP_OK;
}

Mu34ApResult mu34_ap_server_start(Mu34ApServer *s,
    uint32_t generation, unsigned expected_port)
{
    if (!s) return MU34_AP_ERR_BAD_ARG;
    if (generation != s->generation) return MU34_AP_ERR_STALE;
    if (expected_port == 0 || expected_port > 65535u) return MU34_AP_ERR_BAD_ARG;
    if (s->state != MU34_AP_INTERFACE_SET || !s->interface_set)
        return MU34_AP_ERR_STATE;
    if (!oem_ready(s)) {
        s->state = MU34_AP_FAILED;
        return (s->binding.library_verified(
                   s->binding.opaque) != 1) ? MU34_AP_ERR_NOT_OEM : MU34_AP_ERR_NOT_OWNER;
    }
    if (s->binding.cf_string_ok(s->binding.opaque, s->binding.start_command) != 1)
        return MU34_AP_ERR_BAD_CF;
    /* Accepted command only. No listener, Bonjour or port claim yet. */
    s->start_requested = 1; /* preserve stop obligation even on ambiguous vendor error */
    if (s->binding.server_control(s->binding.opaque, s->binding.server,
                                  s->binding.start_command,
                                  s->binding.owner) != 0) {
        s->state = MU34_AP_FAILED;
        return MU34_AP_ERR_VENDOR;
    }
    s->expected_port = expected_port;
    s->state = MU34_AP_START_REQUESTED;
    return mu34_ap_server_poll(s, generation);
}

Mu34ApResult mu34_ap_server_poll(Mu34ApServer *s, uint32_t generation)
{
    int ready;
    if (!s) return MU34_AP_ERR_BAD_ARG;
    if (generation != s->generation) return MU34_AP_ERR_STALE;
    if (!s->start_requested || !s->expected_port ||
        (s->state != MU34_AP_START_REQUESTED &&
         s->state != MU34_AP_LISTENING_VERIFIED)) return MU34_AP_ERR_STATE;
    if (!oem_ready(s)) {
        s->listening_verified = 0;
        s->state = MU34_AP_FAILED;
        return (s->binding.library_verified(s->binding.opaque) != 1)
            ? MU34_AP_ERR_NOT_OEM : MU34_AP_ERR_NOT_OWNER;
    }
    ready = s->binding.listening_verified(s->binding.opaque, s->binding.server,
                   s->binding.owner, s->expected_port);
    if (ready != 1) {
        s->listening_verified = 0;
        if (ready < 0 || ready > 1) {
            s->state = MU34_AP_FAILED;
            return MU34_AP_ERR_NOT_LISTENING;
        }
        s->state = MU34_AP_START_REQUESTED;
        return MU34_AP_R_START_REQUESTED;
    }
    s->listening_verified = 1;
    s->state = MU34_AP_LISTENING_VERIFIED;
    return MU34_AP_R_LISTENING_VERIFIED;
}

Mu34ApResult mu34_ap_server_stop(Mu34ApServer *s, uint32_t generation)
{
    if (!s) return MU34_AP_ERR_BAD_ARG;
    if (generation != s->generation) return MU34_AP_ERR_STALE;
    if (!s->start_requested) return MU34_AP_ERR_STATE;
    if (!oem_ready(s)) {
        s->state = MU34_AP_FAILED;
        return (s->binding.library_verified(
                   s->binding.opaque) != 1) ? MU34_AP_ERR_NOT_OEM : MU34_AP_ERR_NOT_OWNER;
    }
    if (s->binding.cf_string_ok(s->binding.opaque, s->binding.stop_command) != 1)
        return MU34_AP_ERR_BAD_CF;
    if (s->binding.server_control(s->binding.opaque, s->binding.server,
                                  s->binding.stop_command,
                                  s->binding.owner) != 0) {
        s->state = MU34_AP_FAILED;
        return MU34_AP_ERR_VENDOR;
    }
    s->start_requested = 0;
    s->expected_port = 0;
    s->listening_verified = 0;
    s->interface_set = 0;
    memset(s->interface_name, 0, sizeof(s->interface_name));
    s->state = MU34_AP_BOUND;
    return MU34_AP_OK;
}

Mu34ApResult mu34_ap_server_close(Mu34ApServer *s, uint32_t generation)
{
    if (!s) return MU34_AP_ERR_BAD_ARG;
    if (generation != s->generation) return MU34_AP_ERR_STALE;
    if (s->listening_verified || s->state == MU34_AP_LISTENING_VERIFIED)
        return MU34_AP_ERR_STATE;
    if (s->start_requested) return MU34_AP_ERR_STATE;
    s->interface_set = 0;
    s->start_requested = 0;
    s->expected_port = 0;
    s->listening_verified = 0;
    memset(s->interface_name, 0, sizeof(s->interface_name));
    s->state = MU34_AP_BOUND;
    return MU34_AP_OK;
}

Mu34ApState mu34_ap_server_state(const Mu34ApServer *s)
{
    return s ? (Mu34ApState)s->state : MU34_AP_IDLE;
}

const char *mu34_ap_server_state_name(Mu34ApState state)
{
    switch (state) {
    case MU34_AP_IDLE: return "IDLE";
    case MU34_AP_BOUND: return "BOUND";
    case MU34_AP_INTERFACE_SET: return "INTERFACE_SET";
    case MU34_AP_START_REQUESTED: return "START_REQUESTED";
    case MU34_AP_LISTENING_VERIFIED: return "LISTENING_VERIFIED";
    case MU34_AP_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

const char *mu34_ap_server_interface(const Mu34ApServer *s)
{
    return s && s->interface_set ? s->interface_name : "";
}
