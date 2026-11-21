#define _POSIX_C_SOURCE 200809L
#include "txn.h"
#include "sha256.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define P 4096
#define MAX_ROWS 32
/* POSIX maintenance prototype: intentionally no automatic remount or service stop. */
static int fail(const char *s){fprintf(stderr,"SHIBAI: %s (%s)\n",s,strerror(errno));return -1;}
static int path(char o[P],const char *d,const char *n){int v=snprintf(o,P,"%s/%s",d,n);return v<0||v>=P?-1:0;}
static int validname(const char *n){return n&&*n&&n[0]!='.'&&!strchr(n,'/')&&!strchr(n,'\\')&&!strchr(n,'\n')&&strlen(n)<128;}
static int hexok(const char *s){return s&&strlen(s)==64&&strspn(s,"0123456789abcdef")==64;}
static int statdir(const char *d,struct stat *st){return lstat(d,st)||!S_ISDIR(st->st_mode)?-1:0;}
static int syncdir(const char *d){int fd=open(d,O_RDONLY),r;if(fd<0)return -1;r=fsync(fd);if(close(fd)&&!r)r=-1;return r;}
static int safeopen(const char *p,struct stat *st){struct stat a;int fd;if(lstat(p,&a)||!S_ISREG(a.st_mode)||a.st_nlink!=1)return -1;fd=open(p,O_RDONLY
#ifdef O_NOFOLLOW
|O_NOFOLLOW
#endif
);if(fd<0)return -1;if(fstat(fd,st)||!S_ISREG(st->st_mode)||st->st_nlink!=1||a.st_dev!=st->st_dev||a.st_ino!=st->st_ino){close(fd);return -1;}return fd;}
static void hex(const unsigned char b[32],char h[65]){static const char a[]="0123456789abcdef";size_t i;for(i=0;i<32;i++){h[2*i]=a[b[i]>>4];h[2*i+1]=a[b[i]&15];}h[64]=0;}
static int hashfd(int fd,char h[65]){unsigned char buf[32768],b[32];ssize_t n;sha256_ctx c;sha256_init(&c);while((n=read(fd,buf,sizeof buf))!=0){if(n<0){if(errno==EINTR)continue;return -1;}sha256_update(&c,buf,(size_t)n);}sha256_final(&c,b);hex(b,h);return 0;}
static int check(const char *p,const txn_row *r,int candidate){struct stat st;char h[65];int fd=safeopen(p,&st),v;if(fd<0)return -1;v=hashfd(fd,h);close(fd);return v||strcmp(h,candidate?r->new_sha:r->old_sha)||(st.st_mode&07777)!=(candidate?r->new_mode:r->old_mode)||st.st_uid!=r->uid||st.st_gid!=r->gid?-1:0;}
static int wr(int fd,const void *v,size_t n){const unsigned char *b=v;while(n){ssize_t q=write(fd,b,n);if(q<0&&errno==EINTR)continue;if(q<=0)return -1;b+=q;n-=(size_t)q;}return 0;}
static int meta(int fd,mode_t mode,uid_t uid,gid_t gid){struct stat st;if(fstat(fd,&st))return -1;if((st.st_uid!=uid||st.st_gid!=gid)&&fchown(fd,uid,gid))return -1;return fchmod(fd,mode);}
static int copy(const char *src,const char *dir,const txn_row *r,int cand,int exclusive){char dst[P],tmp[P];struct stat st,ds;unsigned char buf[32768];ssize_t n;int in=-1,out=-1,rc=-1,created=0;int v=snprintf(tmp,P,"%s/.txn-XXXXXX",dir);if(v<0||v>=P||path(dst,dir,r->name))return -1;if(exclusive){if(!lstat(dst,&ds)||errno!=ENOENT)return -1;}else if(lstat(dst,&ds)||!S_ISREG(ds.st_mode)||ds.st_nlink!=1)return -1;in=safeopen(src,&st);if(in<0)goto done;out=mkstemp(tmp);if(out<0)goto done;created=1;while((n=read(in,buf,sizeof buf))!=0){if(n<0&&errno==EINTR)continue;if(n<0||wr(out,buf,(size_t)n))goto done;}if(meta(out,cand?r->new_mode:r->old_mode,r->uid,r->gid)||fsync(out))goto done;if(close(out)){out=-1;goto done;}out=-1;if(check(tmp,r,cand))goto done;if(rename(tmp,dst)||syncdir(dir)||check(dst,r,cand))goto done;rc=0;
done:if(in>=0)close(in);if(out>=0)close(out);if(created)unlink(tmp);return rc;}
/* Bind journal to exact manifest and canonical directory names (caller owns dirs). */
static int journal_id(const txn_spec *s,char h[65]){sha256_ctx c;unsigned char b[32];char line[1024];size_t i;const char *dirs[]={s->target,s->backup,s->payload,s->state};sha256_init(&c);for(i=0;i<4;i++)sha256_update(&c,dirs[i],strlen(dirs[i])+1);for(i=0;i<s->count;i++){const txn_row *r=&s->rows[i];int n=snprintf(line,sizeof line,"%s %s %s %lo %lo %lu %lu\n",r->name,r->old_sha,r->new_sha,(unsigned long)r->old_mode,(unsigned long)r->new_mode,(unsigned long)r->uid,(unsigned long)r->gid);if(n<0||n>=(int)sizeof line)return -1;sha256_update(&c,line,(size_t)n);}sha256_final(&c,b);hex(b,h);return 0;}
static int state_write(const txn_spec *s,const char *phase){char p[P],tmp[P],id[65],line[128];int n,fd,rc=-1;if(path(p,s->state,"journal")||path(tmp,s->state,".journal-XXXXXX")||journal_id(s,id))return -1;n=snprintf(line,sizeof line,"MU1005-TXN1 %s %s\n",id,phase);fd=mkstemp(tmp);if(fd<0)return -1;if(!wr(fd,line,(size_t)n)&&!fsync(fd)){if(!close(fd)){fd=-1;if(!rename(tmp,p)&&!syncdir(s->state))rc=0;}else fd=-1;}if(fd>=0)close(fd);unlink(tmp);return rc;}
static int state_read(const txn_spec *s,char phase[32]){char p[P],id[65],line[128],expected[128];struct stat st;int fd,n;ssize_t q;if(path(p,s->state,"journal")||journal_id(s,id))return -1;fd=safeopen(p,&st);if(fd<0)return -1;q=read(fd,line,sizeof line-1);close(fd);if(q<=0||q>=127||q!=st.st_size)return -1;line[q]=0;if(sscanf(line,"MU1005-TXN1 %*64s %31s",phase)!=1)return -1;n=snprintf(expected,sizeof expected,"MU1005-TXN1 %s %s\n",id,phase);return n!=q||memcmp(expected,line,(size_t)n)?-1:0;}
static int check_at(const char *d,const txn_row *r,int cand){char p[P];return path(p,d,r->name)||check(p,r,cand)?-1:0;}
static int copy_at(const char *d,const char *to,const txn_row *r,int cand,int ex){char p[P];return path(p,d,r->name)||copy(p,to,r,cand,ex)?-1:0;}
static void point(const txn_spec *s,const char *p,size_t i){if(s->checkpoint)s->checkpoint(p,i);}
static int restore_precheck(const txn_spec *s){size_t i;for(i=0;i<s->count;i++){const txn_row *r=&s->rows[i];if(check_at(s->backup,r,0)||(check_at(s->target,r,0)&&check_at(s->target,r,1)))return fail("backup/current conflict");}return 0;}
static int restore(const txn_spec *s){size_t i;if(restore_precheck(s))return -1;if(state_write(s,"ROLLING_BACK"))return fail("restore journal");for(i=s->count;i>0;i--){const txn_row *r=&s->rows[i-1];if(check_at(s->target,r,0)&&copy_at(s->backup,s->target,r,0,0))return fail("restore file");point(s,"restore",i-1);}for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],0))return -1;return state_write(s,"RESTORED");}
static int run(const txn_spec *s,const char *action){size_t i;char phase[32],p[P];struct stat st;int trial=!strcmp(action,"trial");if(!strcmp(action,"prepare")||trial){if(path(p,s->state,"journal")||!lstat(p,&st)||errno!=ENOENT)return fail("existing journal");for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],0)||check_at(s->payload,&s->rows[i],1))return fail("original/candidate mismatch");for(i=0;i<s->count;i++){const txn_row *r=&s->rows[i];if(check_at(s->backup,r,0)&&copy_at(s->target,s->backup,r,0,1))return fail("backup copy");point(s,"backup",i);}if(state_write(s,"PREPARED"))return fail("prepare journal");if(!trial)return 0;point(s,"prepared",0);}
if(state_read(s,phase))return fail("journal invalid");
if(strcmp(phase,"PREPARED")&&strcmp(phase,"APPLYING")&&strcmp(phase,"COMMITTED")&&strcmp(phase,"PENDING_HEALTH")&&strcmp(phase,"ROLLING_BACK")&&strcmp(phase,"RESTORED"))return fail("journal phase");
if(!strcmp(action,"restore"))return restore(s);
/* Explicit confirmation belongs to a trusted caller AFTER real runtime checks.
 * Never make it automatic based on successful file writes. */
if(!strcmp(action,"confirm")){
 if(strcmp(phase,"PENDING_HEALTH"))return fail("no pending trial");
 for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],1)||check_at(s->backup,&s->rows[i],0))return fail("confirmation conflict");
 point(s,"confirm",0);
 return state_write(s,"COMMITTED");
}
if(!strcmp(action,"status")){printf("MU1005_TXN_PHASE=%s\n",phase);return 0;}
if(!strcmp(action,"recover")){if(!strcmp(phase,"PREPARED")||!strcmp(phase,"APPLYING")||!strcmp(phase,"ROLLING_BACK")||!strcmp(phase,"PENDING_HEALTH"))return restore(s);for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],!strcmp(phase,"COMMITTED"))||check_at(s->backup,&s->rows[i],0))return fail("terminal state conflict");return 0;}
if((strcmp(action,"apply")&&!trial)||strcmp(phase,"PREPARED"))return fail("invalid action/phase");
for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],0)||check_at(s->backup,&s->rows[i],0)||check_at(s->payload,&s->rows[i],1))return fail("apply precheck");
if(state_write(s,"APPLYING"))return fail("apply journal");
for(i=0;i<s->count;i++){if(check_at(s->target,&s->rows[i],0)||copy_at(s->payload,s->target,&s->rows[i],1,0)){restore(s);return fail("apply failed; see journal");}point(s,"apply",i);}
for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],1)){restore(s);return fail("postcheck failed");}
if(state_write(s,trial?"PENDING_HEALTH":"COMMITTED")){restore(s);return fail("commit failed");}if(trial)point(s,"pending",0);return 0;}
static int originals_match(const txn_spec *s){size_t i;for(i=0;i<s->count;i++)if(check_at(s->target,&s->rows[i],0))return -1;return 0;}
static int life_valid(const txn_lifecycle *l){return l&&l->quiesce&&l->start_candidate&&l->verify_candidate&&l->start_original;}
static int life_trial(const txn_spec *s,const txn_lifecycle *l){char phase[32],p[P];struct stat st;
 /* Do not stop a running installation if another transaction already exists. */
 if(path(p,s->state,"journal")||!lstat(p,&st)||errno!=ENOENT)return fail("supervisor existing journal");
 /* Fail before disrupting OEM services if the release is incomplete or an
  * existing backup cannot be trusted. Recheck inside run() after quiesce. */
 if(originals_match(s))return fail("supervisor original mismatch");
 for(size_t i=0;i<s->count;i++){
  const txn_row *r=&s->rows[i];char backup[P];
  if(check_at(s->payload,r,1)||path(backup,s->backup,r->name))return fail("supervisor payload mismatch");
  if(!lstat(backup,&st)){if(check_at(s->backup,r,0))return fail("supervisor backup conflict");}
  else if(errno!=ENOENT)return fail("supervisor backup lookup");
 }
 if(l->quiesce(l->ctx))return fail("supervisor quiesce");
 if(run(s,"trial")){
  /* Trial may fail before a journal exists, or after replacement. Recover only
   * a journal we created while holding the lock; do not restart on ambiguity. */
  if(!lstat(p,&st)){if(run(s,"recover"))return TXN_LIFE_BLOCKED;}
  else if(errno!=ENOENT)return TXN_LIFE_BLOCKED;
  if(originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
  return TXN_LIFE_ROLLED_BACK;
 }
 if(!l->start_candidate(l->ctx)&&!l->verify_candidate(l->ctx)){
  if(!run(s,"confirm"))return TXN_LIFE_COMMITTED;
  /* A journal rename might succeed before fsync reports failure. Do not
   * blindly overwrite a possibly committed candidate; explicit recovery needed. */
  if(state_read(s,phase)||strcmp(phase,"PENDING_HEALTH"))return TXN_LIFE_BLOCKED;
 }
 if(l->quiesce(l->ctx))return fail("candidate stop not verified; recovery required");
 if(run(s,"recover")||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
 return TXN_LIFE_ROLLED_BACK;
}
static int life_recover(const txn_spec *s,const txn_lifecycle *l){char phase[32],p[P];struct stat st;
 /* An interruption before PREPARED may leave services stopped but files
  * intact. No journal is safe only when every original is verified. */
 if(path(p,s->state,"journal"))return TXN_LIFE_BLOCKED;
 if(lstat(p,&st)){
  if(errno!=ENOENT||originals_match(s)||l->quiesce(l->ctx)||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
  return TXN_LIFE_ROLLED_BACK;
 }
 if(state_read(s,phase))return fail("supervisor journal invalid");
 if(!strcmp(phase,"COMMITTED"))return run(s,"recover");
 if(strcmp(phase,"PREPARED")&&strcmp(phase,"APPLYING")&&strcmp(phase,"PENDING_HEALTH")&&strcmp(phase,"ROLLING_BACK")&&strcmp(phase,"RESTORED"))return fail("supervisor recovery phase");
 if(restore_precheck(s)||l->quiesce(l->ctx)||run(s,"recover")||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
 return TXN_LIFE_ROLLED_BACK;
}
/* Explicit operator restore, including a previously committed release. */
static int life_restore(const txn_spec *s,const txn_lifecycle *l){char phase[32];
 if(state_read(s,phase))return fail("restore journal invalid");
 if(strcmp(phase,"PREPARED")&&strcmp(phase,"APPLYING")&&strcmp(phase,"PENDING_HEALTH")&&strcmp(phase,"ROLLING_BACK")&&strcmp(phase,"RESTORED")&&strcmp(phase,"COMMITTED"))return fail("restore phase invalid");
 if(restore_precheck(s)||l->quiesce(l->ctx)||run(s,"restore")||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
 return TXN_LIFE_ROLLED_BACK;
}
/* Only called by a verified early-boot controller, before OEM autostart.
 * Re-arm PENDING_HEALTH before starting a committed candidate, so a reset
 * during startup is recovered on the next boot rather than retried forever. */
static int life_boot(const txn_spec *s,const txn_lifecycle *l){char phase[32],p[P];struct stat st;
 if(path(p,s->state,"journal"))return TXN_LIFE_BLOCKED;
 if(lstat(p,&st)){
  if(errno!=ENOENT||originals_match(s)||l->quiesce(l->ctx)||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
  return TXN_LIFE_ROLLED_BACK;
 }
 if(state_read(s,phase))return fail("boot journal invalid");
 if(strcmp(phase,"COMMITTED"))return life_recover(s,l);
 /* A known, complete candidate and valid backup must exist before startup.
  * Unknown or tampered files remain blocked, not blindly overwritten. */
 if(run(s,"recover")||l->quiesce(l->ctx)||run(s,"recover"))return TXN_LIFE_BLOCKED;
 if(state_write(s,"PENDING_HEALTH"))return fail("boot pending journal");
 point(s,"boot-pending",0);
 if(!l->start_candidate(l->ctx)&&!l->verify_candidate(l->ctx)){
  if(!run(s,"confirm"))return TXN_LIFE_COMMITTED;
  if(state_read(s,phase)||strcmp(phase,"PENDING_HEALTH"))return TXN_LIFE_BLOCKED;
 }
 if(restore_precheck(s)||l->quiesce(l->ctx)||run(s,"recover")||originals_match(s)||l->start_original(l->ctx))return TXN_LIFE_BLOCKED;
 return TXN_LIFE_ROLLED_BACK;
}
static int run_locked(const txn_spec *s,const char *action,const txn_lifecycle *life){struct stat d[4],st;const char *dirs[4];size_t i,j;int fd,rc,needs_payload;char p[P];struct flock lk; if(!s||!action||!s->rows||!s->count||s->count>MAX_ROWS)return -1;dirs[0]=s->target;dirs[1]=s->backup;dirs[2]=s->payload;dirs[3]=s->state;needs_payload=!strcmp(action,"prepare")||!strcmp(action,"apply")||!strcmp(action,"trial")||!strcmp(action,"supervised-trial");for(i=0;i<4;i++){if(!dirs[i]||!dirs[i][0])return fail("missing directory name");/* Keep the original payload path in journal_id, but do not access removable media on recovery/status/confirm. */if(i==2&&!needs_payload)continue;if(statdir(dirs[i],&d[i])||syncdir(dirs[i]))return fail("dir/sync unsupported");for(j=0;j<i;j++){if(j==2&&!needs_payload)continue;if(d[i].st_dev==d[j].st_dev&&d[i].st_ino==d[j].st_ino)return fail("directory aliases");}}for(i=0;i<s->count;i++){const txn_row *r=&s->rows[i];if(!validname(r->name)||!hexok(r->old_sha)||!hexok(r->new_sha)||(r->old_mode&~0777)||(r->new_mode&~0777))return fail("manifest invalid");for(j=0;j<i;j++)if(!strcmp(r->name,s->rows[j].name))return fail("duplicate manifest");}if(path(p,s->state,"lock"))return -1;fd=open(p,O_RDWR|O_CREAT
#ifdef O_NOFOLLOW
|O_NOFOLLOW
#endif
,0600);if(fd<0)return -1;if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1){close(fd);return -1;}memset(&lk,0,sizeof lk);lk.l_type=F_WRLCK;lk.l_whence=SEEK_SET;if(fcntl(fd,F_SETLK,&lk)<0){close(fd);return fail("maintenance lock busy");}rc=life?(!strcmp(action,"supervised-trial")?life_trial(s,life):!strcmp(action,"supervised-restore")?life_restore(s,life):!strcmp(action,"supervised-boot")?life_boot(s,life):life_recover(s,life)):run(s,action);close(fd);return rc;}
int txn_run(const txn_spec *s,const char *action){return run_locked(s,action,NULL);}
int txn_supervised_trial(const txn_spec *s,const txn_lifecycle *l){return life_valid(l)?run_locked(s,"supervised-trial",l):TXN_LIFE_BLOCKED;}
int txn_supervised_recover(const txn_spec *s,const txn_lifecycle *l){return life_valid(l)?run_locked(s,"supervised-recover",l):TXN_LIFE_BLOCKED;}


int txn_supervised_restore(const txn_spec *s,const txn_lifecycle *l){return life_valid(l)?run_locked(s,"supervised-restore",l):TXN_LIFE_BLOCKED;}

int txn_supervised_boot(const txn_spec *s,const txn_lifecycle *l){return life_valid(l)?run_locked(s,"supervised-boot",l):TXN_LIFE_BLOCKED;}
