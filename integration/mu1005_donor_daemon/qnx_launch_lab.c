/* Actual QNX native spawn and wait, two real TCP sessions.
 * Separate laboratory executable, no OEM paths or processes touched. */
#define _QNX_SOURCE 1
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern int mu1005_bridge_peer_main(void);
static const char *const env[]={
 "PATH=/proc/boot","LD_LIBRARY_PATH=/proc/boot",
 "IPL_CONFIG_DIR_DIO_MANAGER=/proc/boot",
 "MHI2Q_CP_MODEL=LAB-ONLY","MHI2Q_CP_SERIAL=LAB-ONLY","MHI2Q_CP_HARDWARE=LAB-ONLY",
 "MHI2Q_CP_BT_MAC=02:00:00:00:00:01","MHI2Q_CP_DEVICE_ID=02:00:00:00:00:02",
 "MHI2Q_CP_SSID=LAB-ONLY","MHI2Q_CP_PASSPHRASE=labfixturepassword",
 "MHI2Q_CP_CHANNEL=11","MHI2Q_CP_IP_ADDRESS=127.0.0.1","MHI2Q_CP_AIRPLAY_PORT=5000",
 "MHI2Q_CP_RECEIVER=stock210",NULL
};
int main(void){
 char *argv[]={"/proc/boot/mu1005-donor-lab",NULL};
 int fds[]={0,1,2};
 pid_t pid=spawn(argv[0],3,fds,NULL,argv,(char *const *)env);
 if(pid<0){perror("QNX donor spawn");return 2;}
 printf("MU1005_DONOR_QNX_SPAWN pid=%ld\n",(long)pid);fflush(stdout);
 int rc=mu1005_bridge_peer_main();
 int status=0;
 for(unsigned i=0;i<200;i++){
  pid_t got=waitpid(pid,&status,WNOHANG);
  if(got==pid){
   if(rc||!WIFEXITED(status)||WEXITSTATUS(status)!=0)return 3;
   puts("MU1005_DONOR_QNX_SPAWN_AND_FOUR_GENERATIONS_PASS_NO_OEM");return 0;
  }
  if(got<0&&errno!=EINTR)return 4;
  struct timespec t={0,50000000};nanosleep(&t,NULL);
 }
 kill(pid,SIGTERM);
 return 5;
}
