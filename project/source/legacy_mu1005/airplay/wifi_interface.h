#ifndef AU57X_WIFI_INTERFACE_H
#define AU57X_WIFI_INTERFACE_H
#include <stddef.h>
/* Exact local AP IPv4 provided by the real owner; no guessed subnet or band.
 * Returns 0 for a unique, up, non-loopback interface; -1 otherwise.
 * Does not establish radio band, AP mode, iPhone connectivity, or authorization. */
int mu_wifi_interface_for_ipv4(const char *address,char *name,size_t capacity);
#endif
