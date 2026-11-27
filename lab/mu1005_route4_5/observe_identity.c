/* Read-only candidate identity and SHA, reuse R105.155 stat/fstatvfs
 * and existing review_txn SHA implementation. No arbitrary vehicle paths. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sha256.h"
struct entry {const char *name,*path,*hash;uint64_t size;};
#ifndef HOST_TEST
#include "identity_rows.h"
#endif
struct result {struct stat st;int valid;};
static int same(const struct stat*a,const struct stat*b){
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&
 a->st_mtime==b->st_mtime&&a->st_ctime==b->st_ctime;
}
static void inspect(const struct entry*e,struct result*r){
 struct stat s,last,pathnow;struct statvfs fs;unsigned char buf[8192],d[32];
 char hex[65];static const char digits[]="0123456789abcdef";sha256_ctx sha;
 uint64_t total=0;ssize_t n;int fd;
 memset(r,0,sizeof(*r));
 if(stat(e->path,&s)||!S_ISREG(s.st_mode)){printf("UNAVAILABLE %s\n",e->path);return;}
 fd=open(e->path,O_RDONLY|O_NONBLOCK);
 if(fd<0){printf("OPEN_FAILED %s\n",e->path);return;}
 if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_size<0||(uint64_t)s.st_size!=e->size){
  printf("SIZE_OR_TYPE_MISMATCH %s\n",e->path);goto done;
 }
 sha256_init(&sha);
 for(;;){
  n=read(fd,buf,sizeof buf);
  if(n<0&&errno==EINTR)continue;
  if(n<0||(uint64_t)n>e->size-total){printf("READ_FAILED %s\n",e->path);goto done;}
  if(!n)break;
  sha256_update(&sha,buf,(size_t)n);total+=(uint64_t)n;
 }
 if(total!=e->size||fstat(fd,&last)||stat(e->path,&pathnow)||!same(&s,&last)||!same(&last,&pathnow)){
  printf("IDENTITY_CHANGED %s\n",e->path);goto done;
 }
 sha256_final(&sha,d);for(unsigned i=0;i<32;i++){hex[2*i]=digits[d[i]>>4];hex[2*i+1]=digits[d[i]&15];}hex[64]=0;
 if(strcmp(hex,e->hash)){printf("DIFFERENT_BYTES %s\n",e->path);goto done;}
 printf("EXACT_STOCK %s\n",e->path);
 if(fstatvfs(fd,&fs)==0)printf("MOUNT_READONLY %s %d\n",e->path,(fs.f_flag&ST_RDONLY)?1:0);
 else printf("MOUNT_UNKNOWN %s\n",e->path);
 r->st=last;r->valid=1;
done:close(fd);
}
int main(int argc,char **argv){
#ifdef HOST_TEST
 struct entry rows[2];
 if(argc!=3)return 2;
 for(int i=0;i<2;i++)rows[i]=(struct entry){"fixture",argv[i+1],"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",3};
#else
 (void)argv;if(argc!=1)return 2;
#endif
 unsigned count=sizeof rows/sizeof rows[0];struct result res[sizeof rows/sizeof rows[0]];
 for(unsigned i=0;i<count;i++)inspect(&rows[i],&res[i]);
 for(unsigned i=0;i<count;i++)for(unsigned j=i+1;j<count;j++){
  if(strcmp(rows[i].name,rows[j].name)||!res[i].valid||!res[j].valid)continue;
  if(!res[i].st.st_ino||!res[j].st.st_ino)printf("FILE_ID_UNKNOWN %s %s\n",rows[i].path,rows[j].path);
  else printf("%s %s %s\n",res[i].st.st_dev==res[j].st.st_dev&&res[i].st.st_ino==res[j].st.st_ino?"SAME_FILE":"DISTINCT_FILE",rows[i].path,rows[j].path);
 }
 puts("ROUTE4_READ_ONLY_NOT_WRITE_AUTHORIZATION");return 0;
}
