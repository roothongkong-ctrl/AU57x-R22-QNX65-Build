/* Host-only validation of exact IPv4 -> interface resolution.
 * Does not infer radio band, AP role, or phone/AirPlay connectivity. */
#define _DEFAULT_SOURCE 1
#include "wifi_interface.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
int main(void) {
    char name[64], tiny[1];
    struct ifaddrs *all = NULL;
    char candidate[INET_ADDRSTRLEN] = {0};
    int found = 0;
    assert(mu_wifi_interface_for_ipv4(NULL, name, sizeof name) == -1);
    assert(mu_wifi_interface_for_ipv4("not-an-ip", name, sizeof name) == -1);
    assert(mu_wifi_interface_for_ipv4("0.0.0.0", name, sizeof name) == -1);
    assert(mu_wifi_interface_for_ipv4("127.0.0.1", name, sizeof name) == -1);
    assert(mu_wifi_interface_for_ipv4("192.0.2.254", name, sizeof name) == -1);
    assert(getifaddrs(&all) == 0);
    for (struct ifaddrs *p = all; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET ||
            !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
        if (!inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr,
                       candidate, sizeof candidate)) continue;
        found = 1;
        break;
    }
    freeifaddrs(all);
    assert(found && candidate[0]);
    assert(mu_wifi_interface_for_ipv4(candidate, name, sizeof name) == 0);
    assert(name[0] && strlen(name) < sizeof name);
    assert(mu_wifi_interface_for_ipv4(candidate, tiny, sizeof tiny) == -1);
    printf("MU1005 exact IPv4 interface resolver host checks: PASS (%s -> %s)\n",
           candidate, name);
    return 0;
}
