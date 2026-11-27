/* R15 reader: complete transport records -> R14 decoder -> R12.
 * The expected card connection and shared map epoch are explicitly separate.
 * Caller owns establishment of a trustworthy map epoch (NOT implemented).
 */
#ifndef R15_READER_H
#define R15_READER_H
#include "r15_wire.h"
#include "../r14/card112_ffmpeg_decoder.h"
namespace r15 {
class Reader {
 r14::Card112Decoder decoder_;
 uint64_t cid_,seq_,last_;
 bool active_;
public:
 explicit Reader(r12::Joiner &j):decoder_(j),cid_(0),seq_(0),last_(0),active_(false){}
 bool begin(uint64_t expected_cid,uint64_t map_epoch){
   stop();
   if(!expected_cid||!map_epoch||!decoder_.begin(map_epoch))return false;
   cid_=expected_cid;seq_=last_=0;active_=true;return true;
 }
 void stop(){decoder_.drop();active_=false;}
 bool next(int fd){
   unsigned char h[R15_HEADER];
   if(!active_||r15_read_exact(fd,h,sizeof(h),250)||memcmp(h,"C112",4)){
      stop();return false;
   }
   const size_t n=r15_get32(h+4);
   const uint64_t seq=r15_get64(h+16),ts=r15_get64(h+24),now=r15_clock();
   const unsigned idr=r15_get32(h+32);
   if(n==0||n>R15_MAX_AU||r15_get64(h+8)!=cid_||!seq||!ts||
      !now||ts>now||now-ts>200000u||ts<=last_||idr>1||
      (seq_?seq!=seq_+1:!idr)){
      stop();return false;
   }
   std::vector<unsigned char> au(n);
   if(r15_read_exact(fd,au.data(),n,250)||!decoder_.feed_complete_au(au.data(),n,ts)){
      stop();return false;
   }
   last_=ts;seq_=seq;return true;
 }
 uint64_t frames()const{return decoder_.frames();}
};
}
#endif
