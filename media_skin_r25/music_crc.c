/* R25 standalone read-only CRC32 helper, no OEM content, POSIX/QNX 6.5. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
static uint32_t update(uint32_t c,const unsigned char *p,size_t n){
    while(n--){unsigned k;c^=*p++;for(k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
    return c;
}
static int digest(const char *path){
    unsigned char b[16384];size_t n;uint32_t c=0xffffffffu;
    FILE *f=fopen(path,"rb");if(!f){perror(path);return 2;}
    while((n=fread(b,1,sizeof b,f))!=0)c=update(c,b,n);
    if(ferror(f)){fclose(f);return 3;}if(fclose(f))return 3;
    printf("%08lx\n",(unsigned long)(c^0xffffffffu));return 0;
}
int main(int argc,char **argv){
    if(argc==2 && !strcmp(argv[1],"--selftest")){
        static const unsigned char s[]="123456789";
        static const unsigned char bin[]={0,255,128,10,13};
        if((update(0xffffffffu,s,9)^0xffffffffu)!=0xcbf43926u)return 4;
        if((update(0xffffffffu,s,0)^0xffffffffu)!=0u)return 5;
        if(update(update(0xffffffffu,bin,2),bin+2,3)!=update(0xffffffffu,bin,5))return 6;
        puts("MUSICSKIN_CRC32_SELFTEST_OK");return 0;
    }
    if(argc==2)return digest(argv[1]);
    if(argc==1){char cmd[512];int rc;
        if((update(0xffffffffu,(const unsigned char*)"123456789",9)^0xffffffffu)!=0xcbf43926u)return 7;
        puts("MUSICSKIN_NATIVE_CRC_OK");fflush(stdout);
        rc=system("/proc/boot/sh /proc/boot/music_r25_test.sh");
        if(rc!=0){fprintf(stderr,"shell test failed: %d\n",rc);return 8;}
        (void)cmd;puts("MUSICSKIN_QNX_TEST_OK");return 0;
    }
    return 1;
}
