/* R12 SOURCE-ONLY. Two independent CarPlay cluster frames:
 * type111 = unoccluded map; type112 = original white instruction card.
 * Requires the PHONE to really offer both. Not installable.
 */
#ifndef R12_DUAL_STREAM_JOINER_H
#define R12_DUAL_STREAM_JOINER_H
#include <stdint.h>
#include <pthread.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include "../r9/compact_card_r9.h"
namespace r12 {
static const unsigned long long kMaxAgeUs=200000ULL;
static const unsigned long long kMaxSkewUs=100000ULL;
enum Source { CLEAN_MAP_111=111, ORIGINAL_CARD_112=112 };
class Joiner {
 pthread_mutex_t mutex_;
 std::vector<unsigned char> map_,card_;
 unsigned long long epoch_,map_ts_,card_ts_;
 bool map_live_,card_live_;
 compact_card_r9::Config rect_;
 Joiner(const Joiner&);
 Joiner& operator=(const Joiner&);
public:
 explicit Joiner(compact_card_r9::Config cfg):
  epoch_(0),map_ts_(0),card_ts_(0),map_live_(false),card_live_(false),rect_(cfg) {
  pthread_mutex_init(&mutex_,NULL);
 }
 ~Joiner(){pthread_mutex_destroy(&mutex_);}
 bool begin(unsigned long long epoch){
  if(!epoch)return false;
  pthread_mutex_lock(&mutex_);
  epoch_=epoch;map_live_=false;card_live_=false;map_ts_=card_ts_=0;
  map_.clear();card_.clear();
  pthread_mutex_unlock(&mutex_);
  return true;
 }
 void drop(unsigned source,unsigned long long epoch){
  pthread_mutex_lock(&mutex_);
  if(epoch_==epoch){
   if(source==111){map_live_=false;map_.clear();}
   if(source==112){card_live_=false;card_.clear();}
  }
  pthread_mutex_unlock(&mutex_);
 }
 bool submit(unsigned source,unsigned long long epoch,unsigned long long ts,
             const unsigned char*frame,int w,int h){
  if((source!=111&&source!=112)||!epoch||!ts||!frame||
     !compact_card_r9::valid_config(w,h,rect_))return false;
  const size_t n=compact_card_r9::bytes_i420(w,h);
  pthread_mutex_lock(&mutex_);
  bool good=epoch_==epoch;
  if(good){
   std::vector<unsigned char>&dst=(source==111)?map_:card_;
   unsigned long long &last=(source==111)?map_ts_:card_ts_;
   bool &live=(source==111)?map_live_:card_live_;
   if(ts<=last)good=false;
   else {dst.assign(frame,frame+n);last=ts;live=true;}
  }
  pthread_mutex_unlock(&mutex_);
  return good;
 }
 /* No valid pair => caller keeps original 111 video. Never invent map. */
 bool render(unsigned long long epoch,unsigned long long now,
             unsigned char*out,size_t capacity,int w=800,int h=298){
  const size_t n=compact_card_r9::bytes_i420(w,h);
  if(!out||capacity<n||!compact_card_r9::valid_config(w,h,rect_))return false;
  std::vector<unsigned char> map,card;
  pthread_mutex_lock(&mutex_);
  bool good=epoch_==epoch&&epoch!=0&&map_live_&&card_live_&&
   map_ts_<=now&&card_ts_<=now&&
   now-map_ts_<=kMaxAgeUs&&now-card_ts_<=kMaxAgeUs&&
   (map_ts_>card_ts_?map_ts_-card_ts_:card_ts_-map_ts_)<=kMaxSkewUs&&
   map_.size()==n&&card_.size()==n;
  if(good){map=map_;card=card_;}
  pthread_mutex_unlock(&mutex_);
  if(!good)return false;
  return compact_card_r9::composite(card.data(),map.data(),out,w,h,rect_);
 }
};
} // namespace r12
#endif
