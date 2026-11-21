#include "sha256.h"
#include <stdio.h>
int main(void){sha256_ctx c;unsigned char buf[137],out[32];size_t n,i;sha256_init(&c);while((n=fread(buf,1,sizeof buf,stdin)))sha256_update(&c,buf,n);if(ferror(stdin))return 1;sha256_final(&c,out);for(i=0;i<32;i++)printf("%02x",out[i]);puts("");return 0;}
