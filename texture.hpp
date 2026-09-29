#ifndef DEERS_TEXTURE_HPP
#define DEERS_TEXTURE_HPP

#include <stdint.h>
#include <string>
#include <vector>

struct Texture
{
    int width;
    int height;

    std::vector<uint32_t> pixels;

    Texture();

    bool load(
        const std::string& path
    );

    bool valid() const;
};

void videoInit();

void clearScreen(
    uint32_t color
);

void fillRect(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
);

void drawRect(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
);

void drawTexture(
    const Texture& texture,
    int x,
    int y,
    int w,
    int h
);

#endif