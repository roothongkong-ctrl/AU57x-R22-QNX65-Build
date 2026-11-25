/* QNX self-address space, independent fixture object; no OEM data. */
#include "bt_self_reader.h"
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
typedef struct{uint32_t lo;size_t n;int allowed;} Range;
static int allowed(void*p,uint32_t a,size_t n){
 Range*r=p;return r->allowed&&a>=r->lo&&(uint64_t)a-r->lo<=r->n&&
 n<=r->n-((uint64_t)a-r->lo);
}
int main(void){
 long page=sysconf(_SC_PAGESIZE);assert(page>0);
 unsigned char*mem=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
 assert(mem!=MAP_FAILED);memset(mem,0x5a,(size_t)page*2);
 Range range={(uint32_t)(uintptr_t)mem,(size_t)page*2,1};
 MuBtSelfReader*r=mu_bt_self_reader_open(allowed,&range);assert(r);
 unsigned char out[8]={0};assert(mu_bt_self_reader_read(r,range.lo,out,sizeof out)==1);
 for(size_t i=0;i<sizeof out;i++)assert(out[i]==0x5a);
 range.allowed=0;memset(out,0x33,sizeof out);
 assert(!mu_bt_self_reader_read(r,range.lo,out,sizeof out)&&out[0]==0x33);
 range.allowed=1;
 assert(mprotect(mem+page,(size_t)page,PROT_NONE)==0);
 assert(!mu_bt_self_reader_read(r,range.lo+(uint32_t)page,out,sizeof out)&&out[0]==0x33);
 assert(!mu_bt_self_reader_read(r,range.lo+(uint32_t)page-4,out,sizeof out)&&out[0]==0x33);
 assert(!mu_bt_self_reader_read(r,UINT32_MAX-3,out,sizeof out));
 assert(munmap(mem,(size_t)page*2)==0);
 assert(!mu_bt_self_reader_read(r,range.lo,out,sizeof out)&&out[0]==0x33);
 mu_bt_self_reader_close(r);puts("BT_SELF_READER_QNX_MAP_AND_READ_PASS_NO_OEM");return 0;
}
