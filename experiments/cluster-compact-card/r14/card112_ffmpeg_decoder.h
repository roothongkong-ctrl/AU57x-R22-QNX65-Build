/* R14: FFmpeg H.264 Annex-B -> decoded 112 I420 -> R12 card70 compositor.
 * SOURCE ONLY; not connected to a live iPhone, nor an SD install package.
 * Reuses the same libavcodec family as original MIB v1.2.14 stream-player.
 *
 * Caller must provide a COMPLETE independent 112 Annex-B transport (normally
 * R13 packet parser + stream-to-AnnexB conversion). Do not pass raw encrypted
 * AVCC frames; this accepts decrypted Annex-B H.264 only.
 */
#ifndef CLUSTERMAP_R14_DECODE_112_H
#define CLUSTERMAP_R14_DECODE_112_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <cstring>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}
#include "../r12/dual_stream_joiner.h"
namespace r14 {
class Card112Decoder {
    AVCodecParserContext *parser_;
    AVCodecContext *codec_;
    AVFrame *frame_;
    AVPacket *pkt_;
    r12::Joiner &out_;
    uint64_t session_;
    uint64_t completed_;
    uint64_t rejected_;
    uint64_t last_submit_us_;
    bool active_;
    bool decode_packet(const uint8_t *bytes,int len,uint64_t now) {
        if(len<=0||!bytes||len>2*1024*1024){++rejected_;return false;}
        av_packet_unref(pkt_);
        if(av_new_packet(pkt_,len)<0){++rejected_;return false;}
        memcpy(pkt_->data,bytes,(size_t)len);
        int rc=avcodec_send_packet(codec_,pkt_);
        av_packet_unref(pkt_);
        if(rc<0){++rejected_;return false;}
        return receive(now);
    }
    bool receive(uint64_t now) {
        for(;;){
            int rc=avcodec_receive_frame(codec_,frame_);
            if(rc==AVERROR(EAGAIN)||rc==AVERROR_EOF)return true;
            if(rc<0){++rejected_;return false;}
            bool good=(frame_->width==800&&frame_->height==298&&
                frame_->format==AV_PIX_FMT_YUV420P);
            if(good){
                std::vector<uint8_t> packed((size_t)800*298*3/2);
                size_t off=0;
                for(int plane=0;plane<3;plane++){
                    const int w=(plane?400:800),h=(plane?149:298);
                    if(!frame_->data[plane]||frame_->linesize[plane]<w){good=false;break;}
                    for(int y=0;y<h;y++)
                        memcpy(&packed[off+(size_t)y*w],
                               frame_->data[plane]+(size_t)y*frame_->linesize[plane],w);
                    off+=(size_t)w*h;
                }
                /* One network read may yield multiple decoded pictures.
                 * R12 requires strictly increasing per-source timestamps.
                 * Preserve real receive time as baseline; advance by 1us only
                 * for frames from the SAME read. Reset for every session. */
                uint64_t ordered=now;
                if(ordered<=last_submit_us_ && last_submit_us_!=UINT64_MAX)
                    ordered=last_submit_us_+1;
                if(good && ordered>=now &&
                   out_.submit(r12::ORIGINAL_CARD_112,session_,ordered,
                               packed.data(),800,298)){
                    last_submit_us_=ordered;
                    ++completed_;
                }else {good=false;++rejected_;}
            }else ++rejected_;
            av_frame_unref(frame_);
            if(!good)return false;
        }
    }
    Card112Decoder(const Card112Decoder&);
    Card112Decoder& operator=(const Card112Decoder&);
public:
    explicit Card112Decoder(r12::Joiner &dst):
        parser_(NULL),codec_(NULL),frame_(NULL),pkt_(NULL),
        out_(dst),session_(0),completed_(0),rejected_(0),last_submit_us_(0),active_(false) {
        const AVCodec *dec=avcodec_find_decoder(AV_CODEC_ID_H264);
        if(dec){
            codec_=avcodec_alloc_context3(dec);
            if(codec_){
                codec_->thread_count=1; /* predictable low-delay, separate from 111 */
                codec_->flags |= AV_CODEC_FLAG_LOW_DELAY;
                if(avcodec_open2(codec_,dec,NULL)<0){avcodec_free_context(&codec_);}
            }
            parser_=av_parser_init(AV_CODEC_ID_H264);
        }
        frame_=av_frame_alloc();
        pkt_=av_packet_alloc();
    }
    ~Card112Decoder(){
        if(pkt_)av_packet_free(&pkt_);
        if(frame_)av_frame_free(&frame_);
        if(parser_)av_parser_close(parser_);
        if(codec_)avcodec_free_context(&codec_);
    }
    bool ready()const{return codec_&&parser_&&frame_&&pkt_;}
    bool begin(uint64_t session){
        if(!session||!ready())return false;
        avcodec_flush_buffers(codec_);
        if(parser_)av_parser_close(parser_);
        parser_=av_parser_init(AV_CODEC_ID_H264);
        if(!parser_)return false;
        av_packet_unref(pkt_);av_frame_unref(frame_);
        session_=session;active_=true;completed_=rejected_=last_submit_us_=0;
        return true;
    }
    void drop(){
        if(active_)out_.drop(r12::ORIGINAL_CARD_112,session_);
        active_=false;session_=0;last_submit_us_=0;
    }
    bool feed_annexb(const uint8_t* data,size_t len,uint64_t now){
        if(!active_||!parser_||!codec_||!data||!len||!now||len>2*1024*1024){
            ++rejected_;return false;
        }
        unsigned zero_packet_retries=0;
        while(len){
            uint8_t* packet=NULL;int packet_len=0;
            int unit=(int)(len>(size_t)INT_MAX?INT_MAX:len);
            int used=av_parser_parse2(parser_,codec_,&packet,&packet_len,
                data,unit,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
            if(used<0||used>unit){++rejected_;return false;}
            if(packet_len && !decode_packet(packet,packet_len,now))return false;
            if(used==0){
                /* Valid FFmpeg behavior: output a buffered picture before
                 * consuming the next input chunk. Retry same bytes, but cap
                 * zero-progress packets so bad H264 cannot spin forever. */
                if(!packet_len || ++zero_packet_retries>16){
                    ++rejected_;return false;
                }
                continue;
            }
            zero_packet_retries=0;
            data+=used;len-=(size_t)used;
        }
        return true;
    }
    bool finish(uint64_t now){
        if(!active_||!now)return false;
        uint8_t* packet=NULL;int packet_len=0;
        int used=av_parser_parse2(parser_,codec_,&packet,&packet_len,
                                 NULL,0,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
        if(used<0){++rejected_;return false;}
        if(packet_len&&!decode_packet(packet,packet_len,now))return false;
        if(avcodec_send_packet(codec_,NULL)<0){++rejected_;return false;}
        return receive(now);
    }
    uint64_t frames()const{return completed_;}
    uint64_t rejects()const{return rejected_;}
};
} // namespace r14
#endif
