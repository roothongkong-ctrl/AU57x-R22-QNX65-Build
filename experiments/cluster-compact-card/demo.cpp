#include "card_compose.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstdlib>
static bool read_raw(const char* path,std::vector<uint8_t>& v,size_t bytes){
    std::ifstream f(path,std::ios::binary);if(!f)return false;
    v.assign(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
    return v.size()==bytes;
}
int main(int argc,char** argv){
    if(argc!=11){std::cerr<<"usage: demo map.rgba card.rgba out.rgba width height roi_x roi_y roi_w roi_h scale_percent\n";return 2;}
    int w=std::atoi(argv[4]),h=std::atoi(argv[5]);
    if(w<=0||w>4096||h<=0||h>4096)return 2;
    std::vector<uint8_t> map,card,out((size_t)w*h*4);
    size_t bytes=(size_t)w*h*4;
    if(!read_raw(argv[1],map,bytes)||!read_raw(argv[2],card,bytes)){std::cerr<<"bad input image\n";return 3;}
    compactcard::Rect roi{std::atoi(argv[6]),std::atoi(argv[7]),std::atoi(argv[8]),std::atoi(argv[9])};
    int percent=std::atoi(argv[10]);
    auto r=compactcard::composite({map.data(),w,h,w*4},{card.data(),w,h,w*4},
                     {out.data(),w,h,w*4},roi,roi.x,roi.y,percent);
    if(!r.ok)return 4;
    std::ofstream f(argv[3],std::ios::binary);
    f.write((const char*)out.data(),out.size());
    if(!f)return 5;
    std::cout<<"PASS "<<percent<<"% => "<<r.card_rect.w<<"x"<<r.card_rect.h<<"\n";
}
