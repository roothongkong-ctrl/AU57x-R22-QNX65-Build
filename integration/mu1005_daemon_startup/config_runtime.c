/* SPDX-License-Identifier: GPL-3.0-only
 * QNX runtime smoke for unmodified upstream daemon --check-config.
 * All identity and network values are disposable fixtures; no daemon serving,
 * no radio or I2C access, no MFi simulation, no installation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>
#include <errno.h>
#include <string.h>
static char *envp[]={
 "PATH=/proc/boot","LD_LIBRARY_PATH=/proc/boot",
 "IPL_CONFIG_DIR_DIO_MANAGER=/proc/boot",
 "MHI2Q_CP_MODEL=AU57x-review-only","MHI2Q_CP_SERIAL=fixture",
 "MHI2Q_CP_HARDWARE=fixture","MHI2Q_CP_BT_MAC=02:00:00:00:00:01",
 "MHI2Q_CP_DEVICE_ID=02:00:00:00:00:02","MHI2Q_CP_SSID=OFFLINE_FIXTURE",
 "MHI2Q_CP_PASSPHRASE=fixturepassword","MHI2Q_CP_CHANNEL=11",
 "MHI2Q_CP_IP_ADDRESS=10.173.189.1","MHI2Q_CP_AIRPLAY_PORT=5000",
 "MHI2Q_CP_RECEIVER=stock210","MHI2Q_CP_IE_INDEX=0",
 "MHI2Q_CP_IE_MONITOR_SECONDS=0","MHI2Q_CP_MFI_PATH=/dev/i2c8",
 "MHI2Q_CP_MFI_ADDR=17",NULL};
static int set(const char *name,char *value){
 size_t n=strlen(name);
 for(size_t i=0;envp[i];i++)if(!strncmp(envp[i],name,n)&&envp[i][n]=='='){envp[i]=value;return 0;}
 return -1;
}
static int execute(int should_pass){
 char *argv[]={"/proc/boot/mhi2q-wireless-daemon","--check-config",NULL};
 pid_t pid;int s;
 int r=posix_spawn(&pid,argv[0],NULL,NULL,argv,envp);
 if(r){printf("SPAWN_ERROR %d\n",r);return 1;}
 do{r=waitpid(pid,&s,0);}while(r<0&&errno==EINTR);
 if(r!=pid||!WIFEXITED(s)){puts("CHILD_ABNORMAL");return 1;}
 int code=WEXITSTATUS(s);printf("CONFIG_CHILD_EXIT %d expected_success=%d\n",code,should_pass);
 return should_pass?code!=0:code==0;
}
int main(void){
 if(execute(1))return 1;
 if(set("MHI2Q_CP_AIRPLAY_PORT","MHI2Q_CP_AIRPLAY_PORT=7000")||execute(0))return 2;
 if(set("MHI2Q_CP_AIRPLAY_PORT","MHI2Q_CP_AIRPLAY_PORT=5000")||
    set("MHI2Q_CP_IE_INDEX","MHI2Q_CP_IE_INDEX=4")||execute(0))return 3;
 if(set("MHI2Q_CP_IE_INDEX","MHI2Q_CP_IE_INDEX=0")||
    set("MHI2Q_CP_MFI_ADDR","MHI2Q_CP_MFI_ADDR=128")||execute(0))return 4;
 if(set("MHI2Q_CP_MFI_ADDR","MHI2Q_CP_MFI_ADDR=17")||execute(1))return 5;
 puts("AU57X_UPSTREAM_QNX_CONFIG_5_CASES_OK_NO_HARDWARE");
 return 0;
}
