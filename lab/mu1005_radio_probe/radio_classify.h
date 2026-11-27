#ifndef MU1005_RADIO_CLASSIFY_H
#define MU1005_RADIO_CLASSIFY_H
#include <stdint.h>
/* QNX 6.5 net80211 _ieee80211.h channel flags. Only diagnostic classification. */
#define MU_CHAN_2 0x0080u
#define MU_CHAN_5 0x0100u
#define MU_CHAN_PASSIVE 0x0200u
int mu_channel_band(uint16_t mhz,uint16_t flags);
#endif
