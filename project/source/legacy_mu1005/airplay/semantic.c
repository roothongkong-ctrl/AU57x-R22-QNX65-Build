#include "semantic.h"
#include <string.h>
static uint32_t u32(const unsigned char*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int mu_sem_csm(const unsigned char*p,size_t n){return p&&n>=6&&p[0]==0x40&&p[1]==0x40&&(((size_t)p[2]<<8)|p[3])==n;}
int mu_sem_input(MuSemantic*s,const unsigned char*p,size_t n,void*ctx,int(*receive)(void*,unsigned,const unsigned char*,size_t)){
 if(!s||!p||!receive||n<6||n>sizeof s->input||p[0]>2)return -1;
 uint32_t seq=u32(p+1);if(!seq)return -1;
 if(seq==s->input_seq)return n==s->input_size&&!memcmp(p,s->input,n)?1:-1;
 if(seq<s->input_seq||(seq!=s->input_seq+1&&p[0]!=0))return -1;
 int rc=receive(ctx,p[0],p+5,n-5);if(rc==1){s->input_seq=seq;s->input_size=n;memcpy(s->input,p,n);}return rc==1?1:rc==0?0:-1;
}
int mu_sem_output(MuSemantic*s,uint32_t ack,void*ctx,int(*next)(void*,unsigned char*,size_t)){
 if(!s||!next||ack>s->output_seq)return -1;
 if(ack==s->output_seq)s->output_size=0;
 else if(!s->output_size||ack!=s->output_seq-1)return -1;
 if(!s->output_size){if(s->output_seq==UINT32_MAX)return -1;int n=next(ctx,s->output,sizeof s->output);if(n<0||n>(int)sizeof s->output)return -1;if(n){if(!mu_sem_csm(s->output,(size_t)n))return -1;unsigned id=(unsigned)s->output[4]<<8|s->output[5];if(id!=0x5000&&id!=0x5200&&id!=0x5203&&id!=0xa101&&id!=0xfffb)return -1;++s->output_seq;s->output_size=(size_t)n;}}
 return (int)s->output_size;
}
