#include "radio_classify.h"
int mu_channel_band(uint16_t mhz,uint16_t flags){
 unsigned b=flags&(MU_CHAN_2|MU_CHAN_5);
 if(b==MU_CHAN_2 && ((mhz>=2412 && mhz<=2472 && (mhz-2412)%5==0)||mhz==2484))return 24;
 if(b==MU_CHAN_5 && mhz>=5000 && mhz<=5900 && mhz%5==0)return 5;
 return 0;
}
