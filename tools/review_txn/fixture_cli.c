#define _POSIX_C_SOURCE 200809L
/* Host-only test harness. Do not build or use on a head unit. */
#include "txn.h"
#include "supervisor_fixture.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static char dirs[4][4096],old[4][65],newh[4][65];
static txn_row rows[4];
static const char *names[]={"btstack.fixture","dio.fixture","si.fixture","iap2.fixture"};
static void digest(const char *v,char out[65]){unsigned char b[32];sha256_ctx c;size_t j;sha256_init(&c);sha256_update(&c,v,strlen(v));sha256_final(&c,b);for(j=0;j<32;j++)sprintf(out+2*j,"%02x",b[j]);}
static void checkpoint(const char *p,size_t i){char key[64];const char *v=getenv("TXN_CHILD_EXIT");snprintf(key,sizeof key,"%s:%lu",p,(unsigned long)i);if(v&&!strcmp(v,key))_exit(77);
/* Inject a real candidate corruption after the first replacement, only in fixture. */
if(!strcmp(p,"apply")&&i==0&&getenv("TXN_CORRUPT_NEXT")){char f[4096];FILE *o;int n=snprintf(f,sizeof f,"%s/%s",dirs[2],names[1]);if(n<0||n>=(int)sizeof f)_exit(78);o=fopen(f,"wb");if(!o)_exit(78);if(fputs("corrupted candidate",o)<0||fclose(o))_exit(78);}}
int main(int argc,char **argv){txn_spec s;size_t i;char b[256],mark[4096];struct stat st;const char *sub[]={"target","backup","payload","state"};if(argc!=3||strncmp(argv[1],"/tmp/mu1005-c-txn-",17)||strchr(argv[1]+17,'/')||lstat(argv[1],&st)||!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0777)!=0700)return 2;
if(snprintf(mark,sizeof mark,"%s/LAB_ONLY",argv[1])>=(int)sizeof mark||lstat(mark,&st)||!S_ISREG(st.st_mode))return 2;
for(i=0;i<4;i++){if(snprintf(dirs[i],sizeof dirs[i],"%s/%s",argv[1],sub[i])>=(int)sizeof dirs[i])return 2;rows[i].name=names[i];snprintf(b,sizeof b,"original %s",names[i]);digest(b,old[i]);snprintf(b,sizeof b,"candidate %s",names[i]);digest(b,newh[i]);rows[i].old_sha=old[i];rows[i].new_sha=newh[i];rows[i].old_mode=rows[i].new_mode=i<3?0755:0640;rows[i].uid=getuid();rows[i].gid=getgid();}s=(txn_spec){dirs[0],dirs[1],dirs[2],dirs[3],rows,4,checkpoint};if(!strcmp(argv[2],"supervised-trial")||!strcmp(argv[2],"supervised-recover")||!strcmp(argv[2],"supervised-restore")||!strcmp(argv[2],"supervised-boot"))return supervisor_fixture(&s,argv[2]);return txn_run(&s,argv[2])?1:0;}
