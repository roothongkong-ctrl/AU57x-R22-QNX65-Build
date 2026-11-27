#include "radio_classify.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(mu_channel_band(2412,MU_CHAN_2)==24);
 assert(mu_channel_band(2484,MU_CHAN_2)==24);
 assert(mu_channel_band(5180,MU_CHAN_5)==5);
 assert(mu_channel_band(5500,MU_CHAN_5|MU_CHAN_PASSIVE)==5);
 assert(!mu_channel_band(5180,MU_CHAN_2));
 assert(!mu_channel_band(2412,MU_CHAN_5));
 assert(!mu_channel_band(5180,MU_CHAN_2|MU_CHAN_5));
 assert(!mu_channel_band(2413,MU_CHAN_2));
 assert(!mu_channel_band(5955,MU_CHAN_5));
 assert(!mu_channel_band(0,0));
 puts("RADIO_CLASSIFY_FIXTURES_PASS");return 0;
}
