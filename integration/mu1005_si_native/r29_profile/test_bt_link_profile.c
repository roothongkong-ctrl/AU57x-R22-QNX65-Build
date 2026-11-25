#include "wcp_bt_link_profile.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned char b[20], expected[16]={1,0,0xe8,3,20,0,30,1,1,0,128,0,1,0,128,0};
    uint8_t type=0;
    memset(b,0xa5,sizeof(b));
    assert(wcp_bt_link_profile_default(NULL,16)==EINVAL);
    assert(wcp_bt_link_profile_default(b,12)==EINVAL);
    assert(b[0]==0xa5 && b[19]==0xa5);
    assert(wcp_bt_link_profile_default(b,16)==0);
    assert(!memcmp(b,expected,16));
    assert(b[16]==0xa5 && b[19]==0xa5);
    assert(wcp_bt_type_candidate(NULL)==EINVAL);
    assert(wcp_bt_type_candidate(&type)==0 && type==3);
    puts("PASS exact OEM Bluetooth identification type 3; default link profile bounded");
    return 0;
}
