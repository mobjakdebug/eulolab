// Shared PS4/host software rasterizer. Pixel layout: 0xAARRGGBB.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eulo {
inline uint32_t rgb(int r, int g, int b) {
    return 0xff000000u | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}
struct FontAtlas {
    std::vector<unsigned char> data;
    struct Style { unsigned cellW=0, cellH=0; size_t start=0; } styles[2];
    bool load(const char* path);
    const unsigned char* glyph(int code, int style, int* advance) const;
};

class Canvas {
public:
    Canvas(uint32_t* pixels, int w, int h, const FontAtlas& font)
        : pixels_(pixels), w_(w), h_(h), font_(font) {}
    int width() const { return w_; }
    int height() const { return h_; }
    void clear(uint32_t color);
    void rect(int x, int y, int w, int h, uint32_t color);
    void roundRect(int x, int y, int w, int h, int radius, uint32_t color);
    void border(int x, int y, int w, int h, uint32_t color, int thickness=1);
    void gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    void line(int x0, int y0, int x1, int y1, uint32_t color, int thickness=1);
    void circle(int cx, int cy, int radius, uint32_t color);
    void ring(int cx, int cy, int radius, int thickness, uint32_t color);
    void triangle(int ax, int ay, int bx, int by, int cx, int cy, uint32_t color);
    void text(int x, int y, const std::string& value, uint32_t color, int style=0, int maxWidth=10000);
    int textWidth(const std::string& value, int style=0) const;
private:
    uint32_t* pixels_;
    int w_, h_;
    const FontAtlas& font_;
    void pixel(int x, int y, uint32_t color);
};
} // namespace eulo
