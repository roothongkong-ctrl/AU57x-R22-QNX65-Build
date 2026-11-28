#include "route_select.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 MuRouteSelect s;mu_route_init(&s,7,MU_ROUTE_DIRECT);
 assert(mu_route_begin(&s,1,7,0)==0);
 assert(mu_route_begin(&s,1,MU_ROUTE_DIRECT,1)==0); /* wait SI */
 assert(mu_route_unavailable(&s,MU_ROUTE_SI,0,1)==-1);
 assert(mu_route_unavailable(&s,MU_ROUTE_SI,1,0)==-1);
 assert(mu_route_begin(&s,1,7,1)==MU_ROUTE_SI);
 assert(mu_route_begin(&s,2,7,1)==0);
 assert(mu_route_result(&s,2,1,1,0)==-1);
 assert(mu_route_result(&s,2,1,-1,0)==0);
 assert(mu_route_begin(&s,2,7,1)==0);
 assert(mu_route_restored(&s,2,1,0,1)==-1);
 assert(mu_route_restored(&s,2,1,1,1)==1);
 assert(mu_route_begin(&s,1,7,1)==0);
 assert(mu_route_begin(&s,2,7,1)==MU_ROUTE_DIRECT);
 assert(mu_route_result(&s,1,2,-1,0)==0);
 assert(mu_route_restored(&s,1,2,1,1)==1);
 assert(mu_route_begin(&s,3,3,1)==0);
 assert(mu_route_begin(&s,3,7,1)==MU_ROUTE_TYPE3);
 assert(mu_route_result(&s,4,3,1,1)==1&&s.preferred==4);
 assert(mu_route_usb(&s)==0&&s.state==MU_ROUTE_RESTORE);
 assert(mu_route_begin(&s,4,7,1)==0);
 assert(mu_route_restored(&s,4,3,1,0)==1&&s.state==MU_ROUTE_WIRED);
 assert(mu_route_restored(&s,0,3,1,1)==1);
 assert(mu_route_begin(&s,4,7,1)==MU_ROUTE_SI); /* original always first */
 mu_route_init(&s,3,4);
 assert(mu_route_begin(&s,5,4,1)==0);
 assert(mu_route_unavailable(&s,2,1,1)==0);
 assert(mu_route_begin(&s,5,3,1)==MU_ROUTE_DIRECT);
 assert(mu_route_result(&s,1,5,-1,0)==0);
 assert(mu_route_restored(&s,1,5,1,1)==1&&s.state==MU_ROUTE_EXHAUSTED);
 puts("R105305_ORIGINAL_FIRST_FALLBACK_RESTORE_AND_WIRED_FIXTURE_PASS");
 return 0;
}
