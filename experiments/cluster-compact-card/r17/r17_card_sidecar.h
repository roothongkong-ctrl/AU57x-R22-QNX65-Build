/* R17 HOST+SOURCE-ONLY player sidecar. NOT car-tested / NOT an SD installer.
 * Original 111 decoder stays exactly as-is. This sidecar reads independent
 * type112 Annex-B from R16 127.0.0.1:19822 on its own thread, decodes via
 * R14 FFmpeg, and offers a 70% card composite only for fresh synchronized
 * independent 111 MAP-ONLY pictures at 800x298. Fail closed otherwise.
 * The original-card ROI is an unverified R9 fixture: 170,12,350,118.
 * Compiled ONLY under CLUSTERMAP_R17_CARD70_LAB; runtime additionally gated.
 */
#ifndef CLUSTER_CARD_R17_SIDECAR_H
#define CLUSTER_CARD_R17_SIDECAR_H
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include "r14/card112_ffmpeg_decoder.h"
namespace r17 {
class Sidecar {
    static const int kW=800,kH=298,kPort=19822;
    static const size_t kBytes=(size_t)kW*kH*3/2;
    r12::Joiner joined_;
    std::atomic<bool> stopped_;
    pthread_t thread_;
    bool started_;
    unsigned long long epoch_;
    std::vector<unsigned char> composite_;
    Sidecar(const Sidecar&);
    Sidecar& operator=(const Sidecar&);
    static void* trampoline(void *raw){
        static_cast<Sidecar*>(raw)->run();
        return NULL;
    }
    /* Worker exclusively owns socket; stop never acts on a recycled fd.
     * All waits bounded, and every reconnect creates a fresh decoder. */
    int open_local(){
        int sock=socket(AF_INET,SOCK_STREAM,0);
        if(sock<0)return -1;
        int flags=fcntl(sock,F_GETFL,0);
        if(flags<0||fcntl(sock,F_SETFL,flags|O_NONBLOCK)<0){
            close(sock);return -1;
        }
        struct sockaddr_in peer;
        memset(&peer,0,sizeof(peer));
        peer.sin_family=AF_INET;
        peer.sin_port=htons(kPort);
        peer.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(connect(sock,(struct sockaddr*)&peer,sizeof(peer))==0)return sock;
        if(errno==EINPROGRESS){
            struct pollfd p={sock,POLLOUT,0};
            if(poll(&p,1,50)>0){
                int err=0;socklen_t len=sizeof(err);
                if(getsockopt(sock,SOL_SOCKET,SO_ERROR,&err,&len)==0&&err==0)
                    return sock;
            }
        }
        close(sock);return -1;
    }
    void run(){
        while(!stopped_.load()){
            int sock=open_local();
            if(sock<0){usleep(100000);continue;}
            r14::Card112Decoder decoder(joined_);
            if(!decoder.begin(epoch_)){
                close(sock);
                std::fprintf(stderr,"[R18 LAB] 112 decoder unavailable; original map preserved\n");
                return;
            }
            std::fprintf(stderr,"[R18 LAB] 112 connected\n");
            unsigned char chunk[32768+AV_INPUT_BUFFER_PADDING_SIZE];
            while(!stopped_.load()){
                struct pollfd p={sock,POLLIN,0};
                int rc=poll(&p,1,50);
                if(rc==0||(rc<0&&errno==EINTR))continue;
                if(rc<0)break;
                ssize_t n=recv(sock,chunk,32768,0);
                if(n>0){
                    memset(chunk+n,0,AV_INPUT_BUFFER_PADDING_SIZE);
                    struct timespec ts;
                    if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)break;
                    unsigned long long now=(unsigned long long)ts.tv_sec*1000000ULL+
                                           (unsigned long long)ts.tv_nsec/1000ULL;
                    if(!decoder.feed_annexb(chunk,(size_t)n,now))break;
                    continue;
                }
                if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))continue;
                break;
            }
            decoder.drop(); /* invalidate old overlay before reconnect */
            close(sock);
            if(!stopped_.load())usleep(100000);
        }
    }
public:
    Sidecar():
        joined_(compact_card_r9::Config{{170,12,350,118},70}),
        stopped_(false),started_(false),epoch_(0),
        composite_(kBytes,0){}
    ~Sidecar(){stop();}
    bool start(unsigned long long epoch){
        if(started_ || !epoch)return false;
        if(!joined_.begin(epoch))return false;
        epoch_=epoch;
        stopped_.store(false);
        if(pthread_create(&thread_,NULL,trampoline,this)!=0)return false;
        started_=true;
        return true;
    }
    void stop(){
        if(!started_)return;
        stopped_.store(true);
        pthread_join(thread_,NULL);
        joined_.drop(r12::ORIGINAL_CARD_112,epoch_);
        started_=false;
    }
    bool active()const{return started_;}
    /* Caller owns the 111 frame, and calls this only after full I420 decode,
     * before publishing it to the stock render frame-pool. Output buffer is
     * private to the 111 decoder thread. The 112 decoder never touches it.
     */
    bool compose_111(unsigned char*maponly,int w,int h,
                     unsigned long long timestamp){
        if(!started_||!maponly||!timestamp||w!=kW||h!=kH)return false;
        if(!joined_.submit(r12::CLEAN_MAP_111,epoch_,timestamp,maponly,w,h))
            return false;
        if(!joined_.render(epoch_,timestamp,composite_.data(),
                           composite_.size(),w,h))return false;
        memcpy(maponly,composite_.data(),composite_.size());
        return true;
    }
};
} // namespace r17
#endif
