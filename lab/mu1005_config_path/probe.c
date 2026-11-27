/* R105.155 read-only config path identity; no modifications, no restart.
 * A matching inode proves an alias, NOT that an OEM service reads it. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct observation { struct stat st; int valid; };
static void inspect(const char *path,struct observation *o){
 struct stat lst;struct statvfs fs;
 memset(o,0,sizeof *o);
 if(lstat(path,&lst)!=0){printf("PATH_UNAVAILABLE %s errno=%d\n",path,errno);return;}
 printf("FINAL_SYMLINK %s %d\n",path,S_ISLNK(lst.st_mode)?1:0);
 int fd=open(path,O_RDONLY|O_NONBLOCK);
 if(fd<0){printf("OPEN_READ_FAILED %s errno=%d\n",path,errno);return;}
 if(fstat(fd,&o->st)!=0||!S_ISREG(o->st.st_mode)){
  printf("NOT_REGULAR_OR_STAT_FAILED %s\n",path);close(fd);return;
 }
 o->valid=1;
 printf("REGULAR_READABLE %s\n",path);
 if(fstatvfs(fd,&fs)==0)printf("MOUNT_READONLY %s %d\n",path,(fs.f_flag&ST_RDONLY)?1:0);
 else printf("MOUNT_STATUS_UNKNOWN %s errno=%d\n",path,errno);
 printf("ACCESS_W_OK %s %d\n",path,access(path,W_OK)==0?1:0);
 close(fd);
}
int main(int argc,char **argv){
 const char *paths[]={"/etc/eso/production/connectivity.json","/mnt/system/etc/eso/production/connectivity.json",
 "/mnt/efs-system/etc/eso/production/connectivity.json","/mnt/efs/etc/eso/production/connectivity.json",
 "/fs/efs-system/etc/eso/production/connectivity.json"};
 const char **p=paths;unsigned n=sizeof paths/sizeof paths[0];
#ifdef HOST_TEST
 const char *fixture[2];
 if(argc==3){fixture[0]=argv[1];fixture[1]=argv[2];p=fixture;n=2;}
 else return 2;
#else
 (void)argv;if(argc!=1)return 2;
#endif
 struct observation o[5];
 puts("CONFIG_PATH_PROBE_READ_ONLY_BEGIN");
 for(unsigned i=0;i<n;i++)inspect(p[i],&o[i]);
 for(unsigned i=0;i<n;i++)for(unsigned j=i+1;j<n;j++){
  if(!o[i].valid||!o[j].valid)continue;
  if(!o[i].st.st_ino||!o[j].st.st_ino){
   printf("FILE_ID_UNKNOWN %s %s\n",p[i],p[j]);continue;
  }
  printf("%s %s %s\n",(o[i].st.st_dev==o[j].st.st_dev&&o[i].st.st_ino==o[j].st.st_ino)?"SAME_FILE":"DISTINCT_FILE",p[i],p[j]);
 }
 puts("CONFIG_PATH_PROBE_END_NOT_CONSUMER_PROOF");
 return 0;
}
