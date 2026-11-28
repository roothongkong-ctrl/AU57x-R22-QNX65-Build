/* Host-tested portable I420 renderer, experimental only.
 * Original 111 transport untouched. No live RGI producer is provided here.
 * Only verified clean-map input, current session metadata and basic turns.
 * No synthesized map, road text, roundabout or lane instructions.
 */
#ifndef AU57X_RGD_CARD70_H
#define AU57X_RGD_CARD70_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
namespace rgd70 {
struct Rect {int x,y,w,h;};
struct Sample {uint64_t timestamp_us; unsigned direction,distance_m;};
static inline bool valid(Rect r) {
    return r.x>=0 && r.y>=0 && r.w>=140 && r.h>=80 &&
      r.w<=800 && r.h<=298 && r.x<=800-r.w && r.y<=298-r.h &&
      !((r.x|r.y|r.w|r.h)&1);
}
static inline int scaled(int v){return ((v*70+50)/100)&~1;}
static inline bool paint(unsigned char *p,size_t bytes,int w,int h,Rect r,
                         Sample s,uint64_t now,bool map_only) {
    if(!p||!map_only||w!=800||h!=298||bytes!=(size_t)800*298*3/2||
       !valid(r)||!s.timestamp_us||s.timestamp_us>now||
       now-s.timestamp_us>2000000ULL||s.direction<1||s.direction>3||
       s.distance_m>100000)return false;
    const int dw=scaled(r.w),dh=scaled(r.h);
    const int a=dh/7, thick=a>1?a:2, cx=dh/2,cy=dh/2;
    char label[12];
    if(s.distance_m<1000)snprintf(label,sizeof label,"%u m",s.distance_m);
    else snprintf(label,sizeof label,"%u.%u km",s.distance_m/1000,(s.distance_m%1000)/100);
    // 3x5 dot glyphs, rendered only from validated numeric distance.
    const unsigned short glyph[10]={31599,11415,29671,29647,23497,31183,31215,29257,31727,31695};
    const int n=(int)strlen(label),room=dw-dh-8,unit=room/(4*n);
    const int u=unit<dh/8?unit:dh/8;
    if(u<2)return false;
    for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
        bool ink=false;
        if(x<dh){
            if(s.direction==3){
                ink=(y>=a&&y<=3*a&&abs(x-cx)<=y-a)||
                    (y>=3*a&&y<dh-a&&abs(x-cx)<=thick/2);
            }else{
                const int xx=s.direction==1?x:dh-1-x;
                ink=(xx>=a&&xx<=3*a&&abs(y-cy)<=xx-a)||
                    (xx>=3*a&&xx<dh-a&&abs(y-cy)<=thick/2)||
                    (abs(xx-(dh-a-1))<=thick/2&&y>=cy&&y<dh-a);
            }
        }else{
            int tx=x-dh-4,ty=y-(dh-5*u)/2;
            if(tx>=0&&ty>=0&&ty<5*u){
                int c=tx/(4*u),gx=(tx/u)%4,gy=ty/u;
                if(c<n&&gx<3){
                    unsigned bits=0; char ch=label[c];
                    if(ch>='0'&&ch<='9')bits=glyph[ch-'0'];
                    else if(ch=='m')bits=0x5bed;
                    else if(ch=='k')bits=0x5bad;
                    else if(ch=='.')bits=0x0002;
                    ink=(bits>>(14-gy*3-gx))&1u;
                }
            }
        }
        p[(r.y+y)*w+r.x+x]=ink?16:235;
    }
    const size_t ysize=(size_t)w*h;
    for(int plane=0;plane<2;plane++)for(int y=0;y<dh/2;y++)
        memset(p+ysize+plane*ysize/4+(r.y/2+y)*(w/2)+r.x/2,128,dw/2);
    return true;
}
}
#endif
