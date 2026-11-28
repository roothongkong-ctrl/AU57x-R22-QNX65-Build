#ifndef MU1005_STOCK_DNS_RESOLVE_H
#define MU1005_STOCK_DNS_RESOLVE_H
#include <stdint.h>
typedef struct mu_dns_witness mu_dns_witness;
/* Read-only DNSServiceResolve on the original daemon. No registration or
 * synthesized TXT. Caller supplies exact instance, type, domain, escaped
 * fullname, host, live wireless ifindex and port from the original owner.
 * Keep owner lease and library pinned; same serialized thread only.
 * Result is an observation, not proof of OEM ownership or phone discovery. */
mu_dns_witness *mu_dns_begin(void *library, int (*live)(void *), void *ctx,
 uint32_t index, const char *instance, const char *type, const char *domain,
 const char *fullname, const char *host, uint16_t port);
/* 0 pending, 1 matching reply, -1 failure. Poll with a caller deadline. */
int mu_dns_poll(mu_dns_witness *);
void mu_dns_end(mu_dns_witness *);
#endif
