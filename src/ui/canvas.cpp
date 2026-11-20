#include "canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace eulo {
bool FontAtlas::load(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
    const long len = std::ftell(f);
    if (len < 8 || len > 2*1024*1024 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f); return false;
    }
    data.resize(static_cast<size_t>(len));
    const bool read = std::fread(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    if (!read || std::memcmp(data.data(), "EULOFNT1", 8) != 0) { data.clear(); return false; }
    size_t offset = 8;
    for (auto& style : styles) {
        if (offset + 2 > data.size()) { data.clear(); return false; }
        style.cellW = data[offset++];
        style.cellH = data[offset++];
        style.start = offset;
        if (style.cellW < 4 || style.cellH < 4 ||
            offset + 95 * (1 + style.cellW * style.cellH) > data.size()) {
            data.clear(); return false;
        }
        offset += 95 * (1 + style.cellW * style.cellH);
    }
    if (offset != data.size()) { data.clear(); return false; }
    return true;
}

const unsigned char* FontAtlas::glyph(int code, int style, int* advance) const {
    if (data.empty() || code < 32 || code > 126 || style < 0 || style > 1) return nullptr;
    const Style& s = styles[style];
    const size_t i = s.start + static_cast<size_t>(code - 32) * (1 + s.cellW*s.cellH);
    *advance = data[i];
    return data.data() + i + 1;
}

void Canvas::pixel(int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0 && x < w_ && y < h_) pixels_[y*w_ + x] = color;
}
void Canvas::clear(uint32_t color) { std::fill(pixels_, pixels_ + w_*h_, color); }
void Canvas::rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    const int l = std::max(0, x), t = std::max(0, y);
    const int r = std::min(w_, x+w), b = std::min(h_, y+h);
    if (r <= l || b <= t) return; // fully outside the framebuffer
    for (int py=t; py<b; ++py) std::fill(pixels_ + py*w_ + l, pixels_ + py*w_ + r, color);
}
void Canvas::roundRect(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) { rect(x,y,w,h,color); return; }
    radius = std::min(radius, std::min(w,h)/2);
    rect(x+radius,y,w-2*radius,h,color);
    rect(x,y+radius,w,h-2*radius,color);
    for (int i=0; i<radius; ++i) {
        const int pad = radius-static_cast<int>(std::sqrt(radius*radius-(radius-i)*(radius-i)));
        rect(x+pad,y+i,w-2*pad,1,color);
        rect(x+pad,y+h-i-1,w-2*pad,1,color);
    }
}
void Canvas::border(int x,int y,int w,int h,uint32_t color,int thickness) {
    rect(x,y,w,thickness,color); rect(x,y+h-thickness,w,thickness,color);
    rect(x,y,thickness,h,color); rect(x+w-thickness,y,thickness,h,color);
}
void Canvas::gradient(int x,int y,int w,int h,uint32_t top,uint32_t bottom) {
    if (h <= 0) return;
    for(int i=0;i<h;++i) {
        const double t=static_cast<double>(i)/h;
        const auto lerp=[t](int a,int b){ return static_cast<int>(a+(b-a)*t); };
        rect(x,y+i,w,1,rgb(lerp((top>>16)&255,(bottom>>16)&255),
            lerp((top>>8)&255,(bottom>>8)&255),lerp(top&255,bottom&255)));
    }
}
void Canvas::line(int x0,int y0,int x1,int y1,uint32_t color,int thickness) {
    const int dx=std::abs(x1-x0), dy=-std::abs(y1-y0);
    const int sx=x0<x1?1:-1, sy=y0<y1?1:-1;
    int err=dx+dy;
    for (;;) {
        rect(x0-thickness/2,y0-thickness/2,thickness,thickness,color);
        if (x0==x1 && y0==y1) break;
        const int e2=2*err;
        if(e2>=dy) { err+=dy; x0+=sx; }
        if(e2<=dx) { err+=dx; y0+=sy; }
    }
}
void Canvas::circle(int cx,int cy,int radius,uint32_t color) {
    if(radius<0) return;
    for(int dy=-radius;dy<=radius;++dy) {
        const int span=static_cast<int>(std::sqrt(radius*radius-dy*dy));
        rect(cx-span,cy+dy,span*2+1,1,color);
    }
}
void Canvas::ring(int cx,int cy,int radius,int thickness,uint32_t color) {
    const int outer=radius*radius;
    const int inner=std::max(0,radius-thickness)*std::max(0,radius-thickness);
    for(int y=-radius;y<=radius;++y) for(int x=-radius;x<=radius;++x) {
        const int d=x*x+y*y; if(d<=outer && d>=inner) pixel(cx+x,cy+y,color);
    }
}
void Canvas::triangle(int ax,int ay,int bx,int by,int cx,int cy,uint32_t color) {
    const int minx=std::max(0,std::min({ax,bx,cx}));
    const int maxx=std::min(w_-1,std::max({ax,bx,cx}));
    const int miny=std::max(0,std::min({ay,by,cy}));
    const int maxy=std::min(h_-1,std::max({ay,by,cy}));
    const auto edge=[](int ax,int ay,int bx,int by,int x,int y){
        return (x-ax)*(by-ay)-(y-ay)*(bx-ax);
    };
    const int area=edge(ax,ay,bx,by,cx,cy);
    if(area==0) return;
    for(int y=miny;y<=maxy;++y) for(int x=minx;x<=maxx;++x) {
        const int e0=edge(ax,ay,bx,by,x,y);
        const int e1=edge(bx,by,cx,cy,x,y);
        const int e2=edge(cx,cy,ax,ay,x,y);
        if((area>0 && e0>=0 && e1>=0 && e2>=0) ||
           (area<0 && e0<=0 && e1<=0 && e2<=0)) pixel(x,y,color);
    }
}
int Canvas::textWidth(const std::string& value,int style) const {
    int total=0;
    for(unsigned char c:value) { int adv=0; if(font_.glyph(c,style,&adv)) total+=adv; }
    return total;
}
void Canvas::text(int x,int y,const std::string& value,uint32_t color,int style,int maxWidth) {
    if(style<0 || style>1) return;
    const auto& s=font_.styles[style];
    const int start=x;
    for(unsigned char c:value) {
        int adv=0;
        const unsigned char* glyph=font_.glyph(c,style,&adv);
        if(!glyph) continue;
        if(x+adv>start+maxWidth) break;
        if(x+s.cellW>0 && x<w_ && y+s.cellH>0 && y<h_) {
            for(unsigned gy=0;gy<s.cellH;++gy) {
                const int py=y+static_cast<int>(gy);
                if(py<0 || py>=h_) continue;
                for(unsigned gx=0;gx<s.cellW;++gx) {
                    const int px=x+static_cast<int>(gx);
                    if(px<0 || px>=w_) continue;
                    const unsigned alpha=glyph[gy*s.cellW+gx];
                    if(!alpha || gx>static_cast<unsigned>(adv+2)) continue;
                    uint32_t& old=pixels_[py*w_+px];
                    if(alpha==255) { old=color; continue; }
                    const unsigned a=alpha, b=255-a;
                    const unsigned r=((((old>>16)&255)*b)+(((color>>16)&255)*a))/255;
                    const unsigned g=((((old>>8)&255)*b)+(((color>>8)&255)*a))/255;
                    const unsigned bl=(((old&255)*b)+((color&255)*a))/255;
                    old=rgb(r,g,bl);
                }
            }
        }
        x+=adv;
    }
}
} // namespace eulo
