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
#include "r14/card112_ffmpeg_decoder.h"
namespace r17 {
class Sidecar {
    static const int kW=800,kH=298,kPort=19822;
    static const size_t kBytes=(size_t)kW*kH*3/2;
    r12::Joiner joined_;
    std::atomic<bool> stopped_;
    std::atomic<int> fd_;
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
    void run(){
        r14::Card112Decoder decoder(joined_);
        if(!decoder.begin(epoch_)){
            std::fprintf(stderr,"[R17 LAB] 112 FFmpeg decoder unavailable\n");
            return;
        }
        int sock=-1;
        /* R16 advertises its optional loopback socket only after the 112
         * encrypted iPhone transport is accepted. Never block 111 decoder. */
        for(int i=0;i<120 && !stopped_.load();i++){
            sock=socket(AF_INET,SOCK_STREAM,0);
            if(sock>=0){
                struct sockaddr_in peer;
                memset(&peer,0,sizeof(peer));
                peer.sin_family=AF_INET;
                peer.sin_port=htons(kPort);
                peer.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
                if(connect(sock,(struct sockaddr*)&peer,sizeof(peer))==0)break;
                close(sock);sock=-1;
            }
            usleep(50000); /* at most 6 seconds waiting for optional stream */
        }
        if(sock<0 || stopped_.load()){
            if(sock>=0)close(sock);
            decoder.drop();
            return;
        }
        int flags=fcntl(sock,F_GETFL,0);
        if(flags>=0)(void)fcntl(sock,F_SETFL,flags|O_NONBLOCK);
        fd_.store(sock);
        std::fprintf(stderr,"[R17 LAB] independent card 112 loopback connected\n");
        unsigned char chunk[32768];
        while(!stopped_.load()){
            ssize_t n=recv(sock,chunk,sizeof(chunk),0);
            if(n>0){
                /* 112 clock strictly CLOCK_MONOTONIC, same as decoded 111. */
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC,&ts);
                unsigned long long now=(unsigned long long)ts.tv_sec*1000000ULL+
                                       (unsigned long long)ts.tv_nsec/1000ULL;
                if(!decoder.feed_annexb(chunk,(size_t)n,now)){
                    std::fprintf(stderr,"[R17 LAB] 112 decode rejected; keeping original map\n");
                    break;
                }
                continue;
            }
            if(n==0)break;
            if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR){
                usleep(10000);
                continue;
            }
            break;
        }
        fd_.store(-1);
        close(sock);
        decoder.drop(); /* stale card never survives loss of its 112 socket */
    }
public:
    Sidecar():
        joined_(compact_card_r9::Config{{170,12,350,118},70}),
        stopped_(false),fd_(-1),started_(false),epoch_(0),
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
        int sock=fd_.load();
        if(sock>=0)shutdown(sock,SHUT_RDWR);
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
