/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "wired_first.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
static const unsigned char magic[8]={'M','U','W','C','P','I','D',1};
static int allio(int fd,void*buf,size_t n,int wr){
 char*p=buf;while(n){ssize_t k=wr?write(fd,p,n):read(fd,p,n);
 if(k<0&&errno==EINTR)continue;
 if(k<=0)return 0;
 p+=k;n-=(size_t)k;}return 1;
}
static int private_regular(const struct stat*s){
 return S_ISREG(s->st_mode)&&s->st_uid==geteuid()&&(s->st_mode&077)==0&&s->st_nlink==1;
}
int mu_phone_pref_load(const char*path,MuPhoneId*out){
 if(!path||!out)return -1;
 struct stat before,after;
 if(lstat(path,&before)<0)return errno==ENOENT?0:-1;
 if(!private_regular(&before)||before.st_size!=24)return -1;
 int fd=open(path,O_RDONLY|O_NONBLOCK);if(fd<0)return -1;
 unsigned char buf[24];
 int ok=fstat(fd,&after)==0&&private_regular(&after)&&after.st_dev==before.st_dev&&
 after.st_ino==before.st_ino&&after.st_size==24&&allio(fd,buf,sizeof buf,0)&&
 !memcmp(buf,magic,sizeof magic);
 if(close(fd)<0)ok=0;
 if(!ok)return -1;
 memcpy(out->bytes,buf+8,16);return 1;
}
int mu_phone_pref_save(const char*path,const MuPhoneId*id){
 if(!path||!id||strlen(path)>450)return -1;
 struct stat st;
 if(lstat(path,&st)==0){if(!private_regular(&st))return -1;}
 else if(errno!=ENOENT)return -1;
 char tmp[480];int n=snprintf(tmp,sizeof tmp,"%s.tmp.XXXXXX",path);
 if(n<0||(size_t)n>=sizeof tmp)return -1;
 int fd=mkstemp(tmp);if(fd<0)return -1;
 unsigned char buf[24];memcpy(buf,magic,8);memcpy(buf+8,id->bytes,16);
 int ok=fchmod(fd,0600)==0&&allio(fd,buf,sizeof buf,1)&&fsync(fd)==0;
 if(close(fd)<0)ok=0;
 if(ok&&rename(tmp,path)==0)return 1;
 unlink(tmp);return -1;
}
