#include "texture.hpp"

#include <pspdebug.h>
#include <pspdisplay.h>

#include <stdio.h>
#include <string.h>


static uint32_t* gVram = 0;

static const int SCREEN_W = 480;
static const int SCREEN_H = 272;
static const int BUFFER_W = 512;


Texture::Texture()
    : width(0),
      height(0)
{
}


bool Texture::valid() const
{
    return (
        width > 0 &&
        height > 0 &&
        !pixels.empty()
    );
}


bool Texture::load(
    const std::string& path
)
{
    width = 0;
    height = 0;
    pixels.clear();


    FILE* file =
        fopen(
            path.c_str(),
            "rb"
        );


    if (!file)
        return false;


    unsigned char magic[4];


    if (
        fread(
            magic,
            1,
            4,
            file
        ) != 4
    )
    {
        fclose(file);
        return false;
    }


    if (
        magic[0] != 'R' ||
        magic[1] != 'G' ||
        magic[2] != 'B' ||
        magic[3] != 'A'
    )
    {
        fclose(file);
        return false;
    }


    unsigned char header[4];


    if (
        fread(
            header,
            1,
            4,
            file
        ) != 4
    )
    {
        fclose(file);
        return false;
    }


    width =
        (int)header[0] |
        ((int)header[1] << 8);


    height =
        (int)header[2] |
        ((int)header[3] << 8);


    if (
        width <= 0 ||
        height <= 0 ||
        width > 1024 ||
        height > 1024
    )
    {
        fclose(file);

        width = 0;
        height = 0;

        return false;
    }


    pixels.resize(
        width * height
    );


    for (
        int i = 0;
        i < width * height;
        ++i
    )
    {
        unsigned char rgba[4];


        if (
            fread(
                rgba,
                1,
                4,
                file
            ) != 4
        )
        {
            fclose(file);

            pixels.clear();

            width = 0;
            height = 0;

            return false;
        }


        uint32_t r =
            rgba[0];

        uint32_t g =
            rgba[1];

        uint32_t b =
            rgba[2];

        uint32_t a =
            rgba[3];


        pixels[i] =
            r |
            (g << 8) |
            (b << 16) |
            (a << 24);
    }


    fclose(file);

    return true;
}


void videoInit()
{
    // Use the same uncached VRAM surface for software drawing and debug text.
    // No GU allocation or pspDebugScreenGetVramBase() is required.
    gVram = reinterpret_cast<uint32_t*>(0x44000000);
    pspDebugScreenInitEx(gVram, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);

    for (int i = 0; i < BUFFER_W * SCREEN_H; ++i)
        gVram[i] = 0xFF000000;

    if (sceDisplaySetMode(0, SCREEN_W, SCREEN_H) < 0 ||
        sceDisplaySetFrameBuf(gVram, BUFFER_W,
                             PSP_DISPLAY_PIXEL_FORMAT_8888,
                             PSP_DISPLAY_SETBUF_IMMEDIATE) < 0)
    {
        gVram = 0;
        return;
    }
    sceDisplayWaitVblankStart();
}


static uint32_t alphaBlend(
    uint32_t src,
    uint32_t dst
)
{
    uint32_t a =
        (src >> 24)
        & 0xFF;


    if (a == 255)
        return src;


    if (a == 0)
        return dst;


    uint32_t sr =
        src & 0xFF;

    uint32_t sg =
        (src >> 8)
        & 0xFF;

    uint32_t sb =
        (src >> 16)
        & 0xFF;


    uint32_t dr =
        dst & 0xFF;

    uint32_t dg =
        (dst >> 8)
        & 0xFF;

    uint32_t db =
        (dst >> 16)
        & 0xFF;


    uint32_t inv =
        255 - a;


    uint32_t r =
        (
            sr * a +
            dr * inv
        ) / 255;


    uint32_t g =
        (
            sg * a +
            dg * inv
        ) / 255;


    uint32_t b =
        (
            sb * a +
            db * inv
        ) / 255;


    return
        r |
        (g << 8) |
        (b << 16) |
        0xFF000000;
}


void clearScreen(
    uint32_t color
)
{
    if (!gVram)
        return;


    for (
        int y = 0;
        y < SCREEN_H;
        ++y
    )
    {
        uint32_t* row =
            gVram +
            y * BUFFER_W;


        for (
            int x = 0;
            x < SCREEN_W;
            ++x
        )
        {
            row[x] =
                color;
        }
    }
}


void fillRect(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
)
{
    if (!gVram)
        return;


    int x0 =
        x;

    int y0 =
        y;

    int x1 =
        x + w;

    int y1 =
        y + h;


    if (x0 < 0)
        x0 = 0;

    if (y0 < 0)
        y0 = 0;

    if (x1 > SCREEN_W)
        x1 = SCREEN_W;

    if (y1 > SCREEN_H)
        y1 = SCREEN_H;


    if (
        x1 <= x0 ||
        y1 <= y0
    )
    {
        return;
    }


    for (
        int py = y0;
        py < y1;
        ++py
    )
    {
        uint32_t* row =
            gVram +
            py * BUFFER_W;


        for (
            int px = x0;
            px < x1;
            ++px
        )
        {
            row[px] =
                color;
        }
    }
}


void drawRect(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
)
{
    if (w <= 0 || h <= 0)
        return;

    fillRect(
        x,
        y,
        w,
        1,
        color
    );


    fillRect(
        x,
        y + h - 1,
        w,
        1,
        color
    );


    fillRect(
        x,
        y,
        1,
        h,
        color
    );


    fillRect(
        x + w - 1,
        y,
        1,
        h,
        color
    );
}


void drawTexture(
    const Texture& texture,
    int x,
    int y,
    int w,
    int h
)
{
    if (
        !gVram ||
        !texture.valid() ||
        w <= 0 ||
        h <= 0
    )
    {
        return;
    }


    for (
        int dy = 0;
        dy < h;
        ++dy
    )
    {
        int screenY =
            y + dy;


        if (
            screenY < 0 ||
            screenY >= SCREEN_H
        )
        {
            continue;
        }


        int sourceY =
            (
                dy *
                texture.height
            ) / h;


        if (
            sourceY >=
            texture.height
        )
        {
            sourceY =
                texture.height - 1;
        }


        for (
            int dx = 0;
            dx < w;
            ++dx
        )
        {
            int screenX =
                x + dx;


            if (
                screenX < 0 ||
                screenX >= SCREEN_W
            )
            {
                continue;
            }


            int sourceX =
                (
                    dx *
                    texture.width
                ) / w;


            if (
                sourceX >=
                texture.width
            )
            {
                sourceX =
                    texture.width - 1;
            }


            uint32_t source =
                texture.pixels[
                    sourceY *
                    texture.width +
                    sourceX
                ];


            uint32_t* destination =
                gVram +
                screenY * BUFFER_W +
                screenX;


            *destination =
                alphaBlend(
                    source,
                    *destination
                );
        }
    }
}
