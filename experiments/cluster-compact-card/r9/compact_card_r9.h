/* Original-card 70% compositor for MIB Cluster Map v1.2.14.
 * SOURCE ONLY. Must have a *real* fresh map-only frame from another producer.
 * If not, draws the stock combined video unaltered.
 */
#ifndef CLUSTERMAP_COMPACT_CARD_R9_H
#define CLUSTERMAP_COMPACT_CARD_R9_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
namespace compact_card_r9 {
struct Rect {int x,y,w,h;};
struct Config {Rect original;int percent;};
static const char* const map_path="/tmp/clustermap_maponly.i420";
static const unsigned long long kFreshUs=150000ULL;
static inline size_t bytes_i420(int w,int h) {
 if(w<=0||h<=0||w>1920||h>1080||(w&1)||(h&1))return 0;
 return (size_t)w*h*3u/2u;
}
static inline bool valid_config(int w,int h,const Config &cfg) {
 const Rect &r=cfg.original;
 return bytes_i420(w,h)!=0&&w==800&&h==298&&cfg.percent==70&&
   r.x>=0&&r.y>=0&&r.w>=16&&r.h>=16&&
   !(r.x&1)&&!(r.y&1)&&!(r.w&1)&&!(r.h&1)&&
   r.x<=w-r.w&&r.y<=h-r.h;
}
static inline bool parse_rect(const char*s,Config*out) {
 if(!s||!out)return false;
 Config c={{0,0,0,0},70};char tail=0;
 if(sscanf(s,"%d,%d,%d,%d%c",&c.original.x,&c.original.y,
    &c.original.w,&c.original.h,&tail)!=4)return false;
 *out=c;return true;
}
static inline unsigned le32(const unsigned char*p) {
 return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);
}
static inline unsigned long long le64(const unsigned char*p) {
 return (unsigned long long)le32(p)|((unsigned long long)le32(p+4)<<32);
}
static inline bool inside_round(int x,int y,int w,int h,int r) {
 if(x<0||y<0||x>=w||y>=h)return false;
 if(r<=0)return true;
 int cx=x<r?r-1:(x>=w-r?w-r:x);
 int cy=y<r?r-1:(y>=h-r?h-r:y);
 int dx=x-cx,dy=y-cy;
 return dx*dx+dy*dy<=r*r;
}
static inline bool composite(const unsigned char*full,const unsigned char*map,
 unsigned char*out,int width,int height,const Config &cfg) {
 if(!full||!map||!out||full==out||map==out||!valid_config(width,height,cfg))return false;
 memcpy(out,map,bytes_i420(width,height));
 const Rect&r=cfg.original;
 const int dw=((r.w*70+50)/100)&~1,dh=((r.h*70+50)/100)&~1;
 for(int plane=0;plane<3;plane++) {
  int sub=plane?2:1,pw=width/sub,ph=height/sub;
  int sx=r.x/sub,sy=r.y/sub,sw=r.w/sub,sh=r.h/sub;
  int ow=dw/sub,oh=dh/sub,radius=plane?4:8;
  size_t off=plane?(size_t)width*height+(plane==2?(size_t)width*height/4:0):0;
  const unsigned char*src=full+off;unsigned char*dst=out+off;
  for(int yy=0;yy<oh;yy++)for(int xx=0;xx<ow;xx++) {
   if(!inside_round(xx,yy,ow,oh,radius))continue;
   int ix=sx+xx*sw/ow,iy=sy+yy*sh/oh;
   if(ix<0||iy<0||ix>=pw||iy>=ph)return false;
   dst[(sy+yy)*pw+sx+xx]=src[iy*pw+ix];
  }
 }
 return true;
}
/* File: CMBG + LE32 width, LE32 height, LE64 CLOCK_MONOTONIC us,
 * LE32 reserved=0, then exactly width*height*3/2 YUV420 I420 bytes.
 * Producer must write+atomic rename. No real producer ships in R9.
 */
static inline bool read_fresh_map(const char*path,int w,int h,
 unsigned long long now,std::vector<unsigned char>& map) {
 if(!path||!now)return false;
 size_t n=bytes_i420(w,h);if(!n)return false;
 FILE*f=fopen(path,"rb");if(!f)return false;
 unsigned char hdr[24];bool ok=false;
 if(fread(hdr,1,24,f)==24&&!memcmp(hdr,"CMBG",4)&&
    le32(hdr+4)==(unsigned)w&&le32(hdr+8)==(unsigned)h&&!le32(hdr+20)) {
   unsigned long long ts=le64(hdr+12);
   if(ts<=now&&now-ts<=kFreshUs) {
     map.resize(n);
     if(fread(map.data(),1,n,f)==n&&fgetc(f)==EOF)ok=true;
   }
 }
 fclose(f);return ok;
}
static inline const unsigned char*try_frame(const unsigned char*full,int w,int h,
 unsigned long long now,const char*rect_spec,const char*map_file) {
 static std::vector<unsigned char> backing,rendered;
 Config cfg;
 if(!full||!parse_rect(rect_spec,&cfg)||!valid_config(w,h,cfg))return NULL;
 if(!read_fresh_map(map_file,w,h,now,backing))return NULL;
 rendered.resize(bytes_i420(w,h));
 return composite(full,backing.data(),rendered.data(),w,h,cfg)?rendered.data():NULL;
}
} // namespace compact_card_r9
#endif
