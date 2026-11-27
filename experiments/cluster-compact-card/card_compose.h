// ClusterMap Compact Card R1 - portable RGBA compositor prototype.
// SPDX-License-Identifier: MIT
// Requires independent map-only and original-card frames.
// A single blended frame cannot recover occluded map pixels.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace compactcard {
struct Frame {
    const uint8_t* pixels;
    int width, height, stride;
    Frame(const uint8_t* p,int w,int h,int s):pixels(p),width(w),height(h),stride(s){}
};
struct MutableFrame {
    uint8_t* pixels;
    int width,height,stride;
    MutableFrame(uint8_t* p,int w,int h,int s):pixels(p),width(w),height(h),stride(s){}
};
struct Rect {
    int x,y,w,h;
    Rect(int px=0,int py=0,int pw=0,int ph=0):x(px),y(py),w(pw),h(ph){}
};
struct Result { bool ok=false; Rect card_rect{}; };
inline bool valid(const Frame& f){
    return f.pixels&&f.width>0&&f.height>0&&f.stride>=f.width*4;
}
inline bool valid(const MutableFrame& f){
    return f.pixels&&f.width>0&&f.height>0&&f.stride>=f.width*4;
}
inline Result composite(Frame background,Frame card_source,MutableFrame output,
                        Rect source_roi,int anchor_x,int anchor_y,int percent){
    Result result{};
    if(!valid(background)||!valid(card_source)||!valid(output)||
       background.width!=output.width||background.height!=output.height||
       percent<30||percent>100||source_roi.w<=0||source_roi.h<=0||
       source_roi.x<0||source_roi.y<0||
       source_roi.w>card_source.width-source_roi.x||
       source_roi.h>card_source.height-source_roi.y)return result;
    if(background.pixels==output.pixels||card_source.pixels==output.pixels)return result;
    for(int y=0;y<background.height;++y)
        std::memcpy(output.pixels+y*output.stride,
                    background.pixels+y*background.stride,background.width*4);
    int dw=std::max(1,(source_roi.w*percent+50)/100);
    int dh=std::max(1,(source_roi.h*percent+50)/100);
    result.card_rect=Rect(anchor_x,anchor_y,dw,dh);
    for(int dy=0;dy<dh;++dy){
        int y=anchor_y+dy;
        if(y<0||y>=output.height)continue;
        float sy=(dy+0.5f)*(float)source_roi.h/dh-0.5f;
        int y0=std::max(0,std::min(source_roi.h-1,(int)std::floor(sy)));
        int y1=std::min(source_roi.h-1,y0+1);
        float ty=std::max(0.f,std::min(1.f,sy-(float)y0));
        for(int dx=0;dx<dw;++dx){
            int x=anchor_x+dx;
            if(x<0||x>=output.width)continue;
            float sx=(dx+0.5f)*(float)source_roi.w/dw-0.5f;
            int x0=std::max(0,std::min(source_roi.w-1,(int)std::floor(sx)));
            int x1=std::min(source_roi.w-1,x0+1);
            float tx=std::max(0.f,std::min(1.f,sx-(float)x0));
            int ix[2]={x0,x1},iy[2]={y0,y1};
            float alpha=0,premul[3]={0,0,0};
            for(int j=0;j<2;++j)for(int i=0;i<2;++i){
                float weight=(i?tx:1.f-tx)*(j?ty:1.f-ty);
                const uint8_t* p=card_source.pixels+
                    (source_roi.y+iy[j])*card_source.stride+(source_roi.x+ix[i])*4;
                float a=p[3]/255.f;alpha+=weight*a;
                for(int c=0;c<3;++c)premul[c]+=weight*a*p[c];
            }
            uint8_t* dst=output.pixels+y*output.stride+x*4;
            for(int c=0;c<3;++c)
                dst[c]=(uint8_t)std::max(0.f,std::min(255.f,
                                     premul[c]+dst[c]*(1.f-alpha)+0.5f));
            dst[3]=255;
        }
    }
    result.ok=true;return result;
}
}
